#include "DeskHistory.h"

#include "DeskData.h"
#include "MacroData.h"
#include "Myfxbook.h"
#include "desk/CentralBanks.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString historyPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/desk/history.json";
}

} // namespace

DeskHistory *DeskHistory::instance()
{
    static auto *history = new DeskHistory;
    return history;
}

DeskHistory::DeskHistory(QObject *parent)
    : QObject(parent)
{
}

void DeskHistory::start(PolicyRates *rates)
{
    if (m_sample)
        return;
    m_rates = rates;
    QFile file(historyPath());
    if (file.open(QIODevice::ReadOnly))
        m_history = desk::historyFromJson(file.readAll());
    connect(Myfxbook::instance(), &Myfxbook::changed, this, &DeskHistory::noteCrowd);
    connect(&m_clock, &QTimer::timeout, this, &DeskHistory::collect);
    m_clock.start(60 * 60 * 1000);
    QTimer::singleShot(90 * 1000, this, &DeskHistory::collect);   // the start of the app has enough to do
}

void DeskHistory::loadSampleForThisRun(const QString &path)
{
    m_sample = true;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
        m_history = desk::historyFromJson(file.readAll());
    emit changed();
}

void DeskHistory::collect()
{
    const QDate today = QDate::currentDate();
    if (m_sample || today.dayOfWeek() > 5)   // markets are shut: the weekend would repeat Friday
        return;
    if (Myfxbook::instance()->isConnected() && desk::lastDate(m_history, "crowd/") != today) {
        noteCrowd();   // figures fetched less than half an hour ago bring no new answer, so no signal
        Myfxbook::instance()->refreshIfStale();
    }
    if (desk::lastDate(m_history, "rate/USD/") != today) {
        deskdata::loadFedPath(this, [this, today](const desk::FedPath &path, const QVector<QDate> &, const QString &) {
            QVector<std::pair<QDate, double>> moves;
            for (const desk::MeetingPricing &m : path.meetings)
                moves.push_back({m.date, m.changeBp});
            noteMeetings("USD", moves, today);
        });
    }
    if (m_rates && m_rates->hasRate("GBP") && desk::lastDate(m_history, "rate/GBP/") != today) {
        deskdata::loadBoeOis(this, [this](const desk::OisCurve &curve) {
            if (curve.points.isEmpty())
                return;
            deskdata::loadMeetings("GBP", this, [this, curve](const QVector<QDate> &meetings) {
                QVector<QDate> next;
                for (const QDate &d : meetings)
                    if (d >= curve.date && next.size() < 6)
                        next << d;
                QVector<std::pair<QDate, double>> moves;
                for (const desk::MeetingPricing &m : desk::oisMeetingPath(curve, m_rates->current("GBP"), next, curve.date).meetings)
                    moves.push_back({m.date, m.changeBp});
                noteMeetings("GBP", moves, curve.date);
            });
        });
    }
}

void DeskHistory::noteCrowd()
{
    if (m_sample)
        return;
    const Myfxbook *m = Myfxbook::instance();
    // The day the figures were fetched: at launch they can be yesterday's, kept on disk.
    const QDate date = m->fetchedAt().toLocalTime().date();
    if (!m->outlook().ok || !date.isValid() || date.dayOfWeek() > 5)
        return;
    bool any = false;
    for (const desk::OutlookRow &row : m->outlook().rows)
        if (row.positions() >= desk::OutlookMinPositions) {
            desk::addPoint(m_history, desk::crowdKey(row.symbol), date, row.longPct);
            any = true;
        }
    if (any) {
        save();
        emit changed();
    }
}

void DeskHistory::noteMeetings(const QString &currency, const QVector<std::pair<QDate, double>> &moves, const QDate &date)
{
    if (m_sample || moves.isEmpty())
        return;
    for (const auto &[meeting, move] : moves)
        desk::addPoint(m_history, desk::meetingKey(currency, meeting), date, move);
    save();
    emit changed();
}

void DeskHistory::save()
{
    if (m_sample)
        return;
    const QString path = historyPath();
    QDir().mkpath(QFileInfo(path).path());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(desk::historyToJson(m_history));
        file.commit();
    }
}
