#include "DeskStrip.h"

#include "DeskViews.h"
#include "EconomyData.h"
#include "desk/Instruments.h"
#include "desk/Sessions.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimeZone>

namespace {

// Sections of the Desk, as in deskui::sectionNames().
enum { StrengthSection = 1, SessionsSection = 2, WireSection = 4, AppetiteSection = 7 };

} // namespace

DeskStrip::DeskStrip(EconomicCalendar *calendar, QWidget *parent)
    : QWidget(parent)
    , m_calendar(calendar)
{
    setFont(quant::mono(12));
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    const auto add = [&](const QString &title, QLabel **value, QLabel **sub, int section, const QString &tip) {
        QFrame *tile = deskui::tile(title, value, sub);
        tile->setCursor(Qt::PointingHandCursor);
        tile->setToolTip(tip);
        tile->installEventFilter(this);
        m_targets.insert(tile, section);
        row->addWidget(tile, 1);
    };
    add(tr("FX sessions"), &m_sessions, &m_sessionsSub, SessionsSection, tr("Open the Desk's sessions"));
    add(tr("Strongest currency, 24 hours"), &m_strength, &m_strengthSub, StrengthSection, tr("Open the Desk's currency strength"));
    add(tr("Risk appetite"), &m_appetite, &m_appetiteSub, AppetiteSection, tr("Open the Desk's risk appetite"));
    add(tr("Next high-impact release"), &m_next, &m_nextSub, WireSection, tr("Open the Desk's wire"));
    m_clock.setInterval(30 * 1000);
    connect(&m_clock, &QTimer::timeout, this, &DeskStrip::fillClock);
    m_markets.setInterval(5 * 60 * 1000);
    connect(&m_markets, &QTimer::timeout, this, &DeskStrip::fetchMarkets);
    if (m_calendar)
        connect(m_calendar, &RemoteDataSet::updated, this, &DeskStrip::fillClock);
}

void DeskStrip::showEvent(QShowEvent *)
{
    if (m_calendar)
        m_calendar->refreshIfStale();
    fillClock();
    fetchMarkets();
    m_clock.start();
    m_markets.start();
}

void DeskStrip::hideEvent(QHideEvent *)
{
    m_clock.stop();
    m_markets.stop();
}

bool DeskStrip::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease && m_targets.contains(watched))
        emit openSection(m_targets.value(watched));
    return QWidget::eventFilter(watched, event);
}

void DeskStrip::fillClock()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto duration = [](qint64 secs) {
        return secs < 3600 ? tr("%1 min").arg(std::max<qint64>(1, secs / 60))
             : secs < 86400 ? tr("%1 h %2").arg(secs / 3600).arg((secs % 3600) / 60, 2, 10, QLatin1Char('0'))
                            : tr("%1 d %2 h").arg(secs / 86400).arg((secs % 86400) / 3600);
    };
    QStringList open;
    qint64 soonest = -1;
    QString soonestName;
    for (const desk::FxSession &s : desk::fxSessions()) {
        const desk::SessionState state = desk::sessionState(s, now);
        if (state.open) {
            open << s.name;
        } else if (state.nextOpen.isValid()) {
            const qint64 left = now.secsTo(state.nextOpen);
            if (left > 0 && (soonest < 0 || left < soonest)) {
                soonest = left;
                soonestName = s.name;
            }
        }
    }
    m_sessions->setText(open.isEmpty() ? tr("Closed") : open.join(" + "));
    m_sessionsSub->setText(soonest >= 0 ? tr("%1 opens in %2").arg(soonestName, duration(soonest)) : QString());

    const EconomicCalendar::Event *next = nullptr;
    if (m_calendar)
        for (const EconomicCalendar::Event &e : m_calendar->events())
            if (e.impact == 2 && e.time > now && (!next || e.time < next->time))
                next = &e;
    m_next->setText(next ? next->title : QStringLiteral("—"));
    m_nextSub->setText(next ? tr("%1, in %2").arg(next->country, duration(now.secsTo(next->time))) : tr("None this week"));
}

void DeskStrip::fetchMarkets()
{
    QStringList pairs;
    for (const QString &c : desk::majorCurrencies()) {
        const QString pair = desk::dollarPair(c);
        if (!pair.isEmpty())
            pairs << pair;
    }
    deskdata::loadCandles(pairs, "5d", "60m", this, [this](const deskdata::Candles &candles, const QString &) {
        QVector<desk::CurrencySeries> series;
        for (const QString &c : desk::majorCurrencies()) {
            desk::CurrencySeries s;
            s.currency = c;
            bool direct = true;
            const QString pair = desk::dollarPair(c, &direct);
            if (!pair.isEmpty())
                for (const desk::Candle &b : candles.value(pair)) {
                    s.times << b.time;
                    s.dollarsPerUnit << (direct ? b.close : b.close > 0 ? 1.0 / b.close : 0.0);
                }
            series << s;
        }
        const desk::Strength strength = desk::currencyStrength(series);
        const auto ranks = desk::rankStrength(strength, std::max<int>(0, strength.times.size() - 24));
        if (ranks.size() < 2)
            return;
        deskui::setSigned(m_strength, QString("%1 %2").arg(ranks.first().first, deskui::pct(ranks.first().second)), ranks.first().second);
        m_strengthSub->setText(tr("weakest: %1 %2").arg(ranks.last().first, deskui::pct(ranks.last().second)));
    });
    deskdata::loadCandles(deskui::appetiteSymbols(), "2y", "1d", this, [this](const deskdata::Candles &candles, const QString &) {
        const desk::Appetite a = deskui::appetiteComposite(candles);
        if (!a.valid)
            return;
        const QStringList zones {tr("extreme risk-off"), tr("risk-off"), tr("neutral"), tr("risk-on"), tr("extreme risk-on")};
        const desk::AppetiteZone zone = desk::zoneOf(a.now);
        deskui::setSigned(m_appetite, deskui::amount(a.now, 0), zone == desk::AppetiteZone::Neutral ? 0 : a.now - 50);
        m_appetiteZone = zones[int(zone)];
        m_appetite->parentWidget()->setToolTip(tr("Deepwater's composite of seven markets, %1 a week ago; not CNN's Fear & Greed index, "
                                                  "whose estimate follows. Click for the Desk's risk appetite").arg(deskui::amount(a.weekAgo, 0)));
        fillAppetiteSub();
    });
    economy::stockFearGreed(this, [this](const economy::FearGreed &f, const QString &) {
        if (!f.index.isEmpty())
            m_fearGreed = f.index.last().value;
        fillAppetiteSub();
    });
}

void DeskStrip::fillAppetiteSub()
{
    if (m_appetiteZone.isEmpty())
        return;
    m_appetiteSub->setText(m_fearGreed < 0 ? m_appetiteZone
                                           : tr("%1 · Fear & Greed %2").arg(m_appetiteZone, deskui::amount(m_fearGreed, 0)));
}
