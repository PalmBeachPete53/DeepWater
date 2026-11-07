// The Desk's macro sections: Rates, Positioning, Risk appetite, Seasonality, Fundamentals.
#include "DeskViews.h"

#include "DeskHistory.h"
#include "EconomyData.h"

#include "desk/CentralBanks.h"
#include "desk/Fundamentals.h"
#include "KeyedSources.h"
#include "MacroData.h"
#include "ResearchCharts.h"
#include "Myfxbook.h"
#include "desk/Rates.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMap>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

namespace deskui {

namespace {

// ------------------------------------------------------------------ Rates

class RatesView : public quant::View
{
    Q_OBJECT
public:
    explicit RatesView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
    {
        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Fed funds, implied today"), &m_current, &m_currentSub), 1);
        tiles->addWidget(tile(tr("Next FOMC"), &m_next, &m_nextSub), 1);
        tiles->addWidget(tile(tr("Priced for it"), &m_priced, &m_pricedSub), 1);
        tiles->addWidget(tile(tr("Priced by the end of the path"), &m_end, &m_endSub), 1);
        content()->addLayout(tiles);

        auto *charts = new QHBoxLayout;
        charts->setSpacing(10);
        QVBoxLayout *body;
        auto *curveCard = quant::card(tr("Implied fed funds rate after each meeting"), &body);
        m_curve = new quant::MultiLineChart;
        m_curve->setMinimumHeight(240);
        m_curve->setZeroLine(false);
        body->addWidget(m_curve);
        charts->addWidget(curveCard, 1);
        auto *pointsCard = quant::card(tr("Implied change at each meeting, basis points"), &body);
        m_points = new BarsChart;
        body->addWidget(m_points);
        charts->addWidget(pointsCard, 1);
        content()->addLayout(charts);

        auto *tableCard = quant::card(tr("Meeting by meeting"), &body);
        m_meetings = table({tr("Meeting"), tr("In"), tr("Rate before"), tr("Rate after"), tr("Change"), tr("Cumulative"), "−50", "−25", tr("Hold"), "+25", "+50"});
        body->addWidget(m_meetings);
        auto *method = quant::label(tr("Read from CME 30-day fed funds futures, delayed, with the Fed's own calendar: each contract is 100 minus the month's average rate, so a meeting's move is what the month after it must be for the average to hold. Each move is spread on the two nearest quarter-point steps, the usual simplification. It reads what is priced, not what will happen."), "qtNote");
        method->setWordWrap(true);
        body->addWidget(method);
        content()->addWidget(tableCard);

        auto *low = new QHBoxLayout;
        low->setSpacing(10);
        auto *banksCard = quant::card(tr("Central banks"), &body);
        m_banks = table({tr("Bank"), tr("Rate"), tr("Last move"), tr("On"), tr("1 year"), tr("Last vote"), tr("2Y − rate"), tr("Next decision"), tr("In")});
        body->addWidget(m_banks);
        auto *banksNote = quant::label(tr("Policy rates from the BIS; decisions and votes from the banks' own pages, where they publish them. The RBA and the RBNZ refuse programs, so their dates are kept in the app and updated with it. 2Y − rate is the 2-year government yield against the policy rate: where the market sees rates, with a term premium. No free source that allows it publishes short rate futures for the other banks; ING's forecasts for them are in Forecasts."), "qtNote");
        banksNote->setWordWrap(true);
        body->addWidget(banksNote);
        low->addWidget(banksCard, 7);
        auto *yieldsCard = quant::card(tr("Sovereign yield curves"), &body);
        m_yields = new quant::MultiLineChart;
        m_yields->setMinimumHeight(230);
        m_yields->setZeroLine(false);
        m_yields->setLegend(true);
        body->addWidget(m_yields);
        auto *yieldsNote = quant::label(tr("Latest published: US Treasury and Japan's finance ministry par yields, the ECB's AAA euro area curve, the Bank of England's gilt zero-coupon curve, the Bank of Canada's benchmark bonds, long-term shown at 30 years, the SNB's Confederation spot rates. Methods differ, so levels compare roughly."), "qtNote");
        yieldsNote->setWordWrap(true);
        body->addWidget(yieldsNote);
        low->addWidget(yieldsCard, 3);
        content()->addLayout(low);
        auto *ukRow = new QHBoxLayout;
        ukRow->setSpacing(10);
        auto *oisCard = quant::card(tr("United Kingdom: rate implied by the OIS curve"), &body);
        m_ois = new quant::MultiLineChart;
        m_ois->setMinimumHeight(220);
        m_ois->setZeroLine(false);
        body->addWidget(m_ois);
        ukRow->addWidget(oisCard, 2);
        auto *oisMeetings = quant::card(tr("Priced at the next Bank of England meetings"), &body);
        m_oisTable = table({tr("Meeting"), tr("Implied rate"), tr("Against Bank Rate"), "−50", "−25", tr("Hold"), "+25", "+50"});
        body->addWidget(m_oisTable);
        auto *oisNote = quant::label(tr("The Bank of England's daily OIS forward curve (Bloomberg Finance L.P. and Bank calculations), shown for your own use. Each meeting's move is the forward a week after it against the one before, spread on the two nearest quarter points as for the Fed: a simplification with a small premium in it, not a survey of odds."), "qtNote");
        oisNote->setWordWrap(true);
        body->addWidget(oisNote);
        ukRow->addWidget(oisMeetings, 3);
        content()->addLayout(ukRow);
        auto *pricingCard = quant::card(tr("How the next meeting's pricing moved"), &body);
        m_pricingHistory = new quant::MultiLineChart;
        m_pricingHistory->setMinimumHeight(200);
        m_pricingHistory->setZeroLine(true);
        body->addWidget(m_pricingHistory);
        m_pricingNote = quant::label({}, "qtNote");
        m_pricingNote->setWordWrap(true);
        body->addWidget(m_pricingNote);
        content()->addWidget(pricingCard);
        connect(DeskHistory::instance(), &DeskHistory::changed, this, &RatesView::fillPricingHistory);
        auto *estrRow = new QHBoxLayout;
        estrRow->setSpacing(10);
        auto *estrCard = quant::card(tr("Euro area: €STR futures, by quarter"), &body);
        m_estr = table({tr("Period"), tr("Implied rate"), tr("Against €STR today"), tr("Moves of 25 bp"), tr("Volume")});
        body->addWidget(m_estr);
        auto *estrNote = quant::label(tr("CME's three-month €STR futures, delayed, against the ECB's latest €STR. A quarter holds about two ECB meetings, so this reads the path, not one meeting's odds. Most of this market trades elsewhere, so a low volume means a thin price."), "qtNote");
        estrNote->setWordWrap(true);
        body->addWidget(estrNote);
        estrRow->addWidget(estrCard, 3);
        auto *oceaniaCard = quant::card(tr("Australia and New Zealand"), &body);
        m_oceania = table({tr("Economy"), "3M", "10Y", tr("Slope"), tr("Month")});
        body->addWidget(m_oceania);
        auto *oceaniaNote = quant::label(tr("The OECD's monthly interbank and long-term rates: their central banks refuse programs and no free daily curve was found, so only these two points, some weeks old."), "qtNote");
        oceaniaNote->setWordWrap(true);
        body->addWidget(oceaniaNote);
        estrRow->addWidget(oceaniaCard, 2, Qt::AlignTop);
        content()->addLayout(estrRow);
        content()->addStretch(1);
        if (c.rates)
            connect(c.rates, &RemoteDataSet::updated, this, &RatesView::fillBanks);
    }

protected:
    void load() override
    {
        fetchFed();
        fetchYields();
        if (m_context.rates)
            m_context.rates->refreshIfStale();
        fillBanks();
        fetchDecisions();
        fetchOfficial();
        deskdata::loadBoeOis(this, [this](const desk::OisCurve &curve) {
            m_oisCurve = curve;
            fillOis();
        });
        fillPricingHistory();
    }

private:
    void fillOis()
    {
        const Palette &pal = Theme::current();
        if (m_oisCurve.points.isEmpty()) {
            m_oisTable->setRowCount(0);
            return;
        }
        const QLocale loc;
        QVector<QDate> dates;
        QVector<double> rates;
        for (const auto &point : std::as_const(m_oisCurve.points))
            if (point.first <= 36) {
                dates << m_oisCurve.date.addDays(qint64(point.first * 30.44));
                rates << point.second;
            }
        m_ois->setData(dates, {{tr("Implied rate"), pal.Accent, rates}}, [](double v) { return QString("%1%").arg(amount(v)); });
        const double bankRate = m_context.rates && m_context.rates->hasRate("GBP") ? m_context.rates->current("GBP") : std::nan("");
        QVector<QDate> next;
        for (const QDate &d : m_meetings_.value("GBP"))
            if (d >= m_oisCurve.date && next.size() < 6)   // the curve may predate today's meeting
                next << d;
        const desk::FedPath path = desk::oisMeetingPath(m_oisCurve, bankRate, next, m_oisCurve.date);
        m_oisTable->setRowCount(path.meetings.size());
        for (int r = 0; r < path.meetings.size(); ++r) {
            const desk::MeetingPricing &m = path.meetings[r];
            m_oisTable->setItem(r, 0, cell(loc.toString(m.date, "d MMM yyyy"), {}, Qt::AlignCenter, true));
            m_oisTable->setItem(r, 1, cell(QString("%1%").arg(amount(m.rateAfter))));
            m_oisTable->setItem(r, 2, cell(tr("%1 bp").arg(signedAmount(m.cumulativeBp, 0)), m.cumulativeBp >= 0 ? pal.Up : pal.Down));
            for (int k = 0; k < 5; ++k) {
                const double v = m.probability[k];
                m_oisTable->setItem(r, 3 + k, cell(v >= 0.5 ? QString("%1%").arg(amount(v, 0)) : "—", v >= 50 ? pal.Text : pal.TextDim, Qt::AlignCenter, v >= 50));
            }
        }
        fitHeight(m_oisTable);
    }

    // The banks' own calendars and how the last decision was voted. A page that changes gives a dash, not a guess.
    void fetchOfficial()
    {
        const auto page = [this](const QString &key, const QString &url, std::function<void(const QString &)> done) {
            research::Client::instance()->raw(key, QUrl(url), 24 * 3600, false, this, [done](const QByteArray &body, const QString &) { done(QString::fromUtf8(body)); });
        };
        const auto dates = [this](const QString &currency, const QVector<QDate> &list) {
            m_meetings_.insert(currency, list);
            fillBanks();
        };
        for (const char *currency : {"AUD", "NZD", "EUR", "JPY", "CHF", "CAD"})
            deskdata::loadMeetings(currency, this, [dates, currency = QString(currency)](const QVector<QDate> &list) { dates(currency, list); });
        deskdata::loadMeetings("GBP", this, [this, page, dates](const QVector<QDate> &list) {
            dates("GBP", list);
            fillOis();
            QDate last;
            for (const QDate &d : list)
                if (d < QDate::currentDate())
                    last = d;
            if (!last.isValid())
                return;
            page("boe:summary:" + last.toString(Qt::ISODate), desk::boeSummaryUrl(last), [this](const QString &summary) {
                m_votes.insert("GBP", desk::parseBoeVote(summary));
                fillBanks();
            });
        });
        page("fed:feed", "https://www.federalreserve.gov/feeds/press_monetary.xml", [this, page](const QString &feed) {
            const QString url = desk::latestFomcStatementUrl(feed);
            if (url.isEmpty())
                return;
            page("fed:statement:" + url.section('/', -1), url, [this](const QString &statement) {
                m_votes.insert("USD", desk::parseFedVote(statement));
                fillBanks();
            });
        });
    }


    // The calendar page holds a week around today; a decision is often further away.
    void fetchDecisions()
    {
        if (!m_context.calendar)
            return;
        const QDate today = QDate::currentDate();
        m_context.calendar->fetchRange(today, today.addDays(125), this, [this](const QVector<EconomicCalendar::Event> &events, const QString &) {
            m_decisions = events;
            fillBanks();
        });
    }

    void fetchFed()
    {
        setStatus(tr("Loading the futures and the Fed's calendar…"));
        deskdata::loadFedPath(this, [this](const desk::FedPath &path, const QVector<QDate> &meetings, const QString &error) {
            if (meetings.isEmpty()) {
                setStatus(tr("The Fed's calendar is unavailable: %1").arg(error));
                return;
            }
            for (const QDate &m : meetings)
                if (m >= QDate::currentDate() && (!m_nextFomc.isValid() || m < m_nextFomc))
                    m_nextFomc = m;
            fillBanks();
            fillFed(path, error);
        });
    }

    // How the next meeting's pricing moved, from what this computer noted: no free source keeps it.
    void fillPricingHistory()
    {
        const desk::History &history = DeskHistory::instance()->history();
        const QDate today = QDate::currentDate();
        QMap<QDate, bool> days;
        const auto series = [&](const QString &currency) { return history.value(desk::nextMeetingKey(history, currency, today)); };
        const QVector<desk::DayPoint> usd = series("USD"), gbp = series("GBP");
        for (const desk::DayPoint &p : usd) days.insert(p.date, true);
        for (const desk::DayPoint &p : gbp) days.insert(p.date, true);
        const QVector<QDate> dates = days.keys().toVector();
        const auto line = [&](const QVector<desk::DayPoint> &points) {
            QVector<double> values(dates.size(), std::nan(""));
            for (const desk::DayPoint &p : points)
                values[dates.indexOf(p.date)] = p.value;
            return values;
        };
        const auto meetingName = [&](const QString &name, const QString &currency) {
            const QDate meeting = desk::meetingOfKey(desk::nextMeetingKey(history, currency, today));
            return meeting.isValid() ? tr("%1, %2").arg(name, QLocale().toString(meeting, "d MMM")) : name;
        };
        QVector<quant::MultiLineChart::Line> lines;
        if (!usd.isEmpty())
            lines.push_back({meetingName(tr("Fed"), "USD"), quant::seriesColor(1), line(usd)});
        if (!gbp.isEmpty())
            lines.push_back({meetingName(tr("Bank of England"), "GBP"), quant::seriesColor(6), line(gbp)});
        m_pricingHistory->setData(dates, lines, [](double v) { return tr("%1 bp").arg(signedAmount(v, 0)); });
        m_pricingHistory->setVisible(dates.size() >= 2);
        m_pricingNote->setText(dates.size() < 2
                                   ? tr("Noted on this computer each weekday while the app is open, since no free source keeps it. The line appears after a second day.")
                                   : tr("Noted on this computer each weekday while the app is open, since %1. The move priced for each bank's next meeting; a day the app stayed shut is a gap.").arg(QLocale().toString(dates.first(), QLocale::ShortFormat)));
    }

    void fillFed(const desk::FedPath &path, const QString &error)
    {
        const Palette &pal = Theme::current();
        if (path.meetings.isEmpty()) {
            setStatus(tr("The fed funds futures are unavailable: %1").arg(error));
            return;
        }
        setStatus({});
        const QLocale loc;
        const QDate today = QDate::currentDate();
        m_current->setText(QString("%1%").arg(amount(path.currentRate)));
        m_currentSub->setText(tr("100 minus the nearest future"));
        const desk::MeetingPricing &first = path.meetings.first();
        m_next->setText(loc.toString(first.date, "d MMM yyyy"));
        m_nextSub->setText(tr("in %1 days").arg(today.daysTo(first.date)));
        const double top = std::max({first.pCut, first.pHold, first.pHike});
        const QString call = top == first.pHold ? tr("Hold") : top == first.pHike ? tr("Hike") : tr("Cut");
        setSigned(m_priced, QString("%1 %2%").arg(call, amount(top, 0)), first.changeBp);
        m_pricedSub->setText(tr("%1 bp implied").arg(signedAmount(first.changeBp, 1)));
        const desk::MeetingPricing &last = path.meetings.last();
        setSigned(m_end, QString("%1%").arg(amount(last.rateAfter)), last.cumulativeBp);
        m_endSub->setText(tr("%1 bp by %2").arg(signedAmount(last.cumulativeBp, 1), loc.toString(last.date, "MMM yyyy")));

        QStringList labels;
        QVector<QDate> dates;
        QVector<double> rates, changes;
        m_meetings->setRowCount(path.meetings.size());
        for (int r = 0; r < path.meetings.size(); ++r) {
            const desk::MeetingPricing &m = path.meetings[r];
            labels << loc.toString(m.date, "MMM yy");
            dates << m.date;
            rates << m.rateAfter;
            changes << m.changeBp;
            m_meetings->setItem(r, 0, cell(loc.toString(m.date, "d MMM yyyy"), {}, Qt::AlignCenter, true));
            m_meetings->setItem(r, 1, cell(tr("%1 d").arg(today.daysTo(m.date)), pal.TextDim));
            m_meetings->setItem(r, 2, cell(QString("%1%").arg(amount(m.rateBefore))));
            m_meetings->setItem(r, 3, cell(QString("%1%").arg(amount(m.rateAfter))));
            m_meetings->setItem(r, 4, cell(signedAmount(m.changeBp, 1), m.changeBp >= 0 ? pal.Up : pal.Down));
            m_meetings->setItem(r, 5, cell(signedAmount(m.cumulativeBp, 1), m.cumulativeBp >= 0 ? pal.Up : pal.Down));
            for (int k = 0; k < 5; ++k) {
                const double v = m.probability[k];
                m_meetings->setItem(r, 6 + k, cell(v >= 0.5 ? QString("%1%").arg(amount(v, 0)) : "—", v >= 50 ? pal.Text : pal.TextDim, Qt::AlignCenter, v >= 50));
            }
        }
        fitHeight(m_meetings);
        m_curve->setXLabels(labels);
        m_curve->setData(dates, {{tr("Implied rate"), pal.Accent, rates}}, [](double v) { return QString("%1%").arg(amount(v)); });
        m_points->setData(labels, changes, [](double v) { return signedAmount(v, 1); });
    }

    void fillBanks()
    {
        if (!m_context.rates)
            return;
        const Palette &pal = Theme::current();
        const QLocale loc;
        const auto &banks = m_context.rates->banks();
        m_banks->setRowCount(banks.size());
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QHash<QString, QString> countries {{"USD", "US"}, {"EUR", "EU"}, {"JPY", "JP"}, {"GBP", "GB"}, {"CHF", "CH"}, {"CAD", "CA"}, {"AUD", "AU"}, {"NZD", "NZ"}, {"CNY", "CN"}};
        for (int r = 0; r < banks.size(); ++r) {
            const PolicyRates::Bank &b = banks[r];
            m_banks->setItem(r, 0, cell(QString("%1 · %2").arg(b.currency, b.name), {}, Qt::AlignLeft, true));
            const bool has = m_context.rates->hasRate(b.currency);
            m_banks->setItem(r, 1, cell(has ? QString("%1%").arg(amount(m_context.rates->current(b.currency))) : "—"));
            QDate when;
            double delta = 0;
            const bool moved = has && m_context.rates->lastChange(b.currency, &when, &delta);
            m_banks->setItem(r, 2, cell(moved ? tr("%1 bp").arg(signedAmount(delta * 100, 0)) : "—", delta > 0 ? pal.Up : delta < 0 ? pal.Down : pal.TextDim));
            m_banks->setItem(r, 3, cell(moved ? loc.toString(when, "d MMM yyyy") : "—", pal.TextDim));
            const double year = has ? m_context.rates->changeOver(b.currency, 365) : 0;
            m_banks->setItem(r, 4, cell(has ? tr("%1 bp").arg(signedAmount(year * 100, 0)) : "—", year > 0 ? pal.Up : year < 0 ? pal.Down : pal.TextDim));
            QDateTime next;
            for (const EconomicCalendar::Event &e : m_decisions)
                if (e.country == countries.value(b.currency) && e.time > now && isRateDecision(e.title) && (!next.isValid() || e.time < next))
                    next = e.time;
            if (b.currency == "USD" && m_nextFomc.isValid() && (!next.isValid() || m_nextFomc < next.date()))
                next = m_nextFomc.startOfDay(QTimeZone::UTC);
            for (const QDate &d : m_meetings_.value(b.currency))
                if (d >= now.toLocalTime().date() && (!next.isValid() || d < next.toLocalTime().date())) {
                    next = d.startOfDay(QTimeZone::UTC);
                    break;
                }
            const desk::Vote vote = m_votes.value(b.currency);
            QString voteText = QStringLiteral("—");
            if (vote.known && vote.unanimous)
                voteText = tr("Unanimous");
            else if (vote.known && vote.majority)
                voteText = QString("%1–%2").arg(vote.majority).arg(vote.minority);
            else if (vote.known)
                voteText = vote.dissenters.size() == 1 ? tr("1 dissent") : tr("%1 dissents").arg(vote.dissenters.size());
            auto *voteCell = cell(voteText, vote.known ? pal.Text : pal.TextDim);
            if (!vote.dissenters.isEmpty())
                voteCell->setToolTip(vote.dissenters.join(", "));
            m_banks->setItem(r, 5, voteCell);
            const double twoYear = m_twoYear.value(b.currency, std::nan(""));
            const double spread = has ? (twoYear - m_context.rates->current(b.currency)) * 100.0 : std::nan("");
            m_banks->setItem(r, 6, cell(std::isnan(spread) ? QStringLiteral("—") : tr("%1 bp").arg(signedAmount(spread, 0)), std::isnan(spread) ? pal.TextDim : spread >= 0 ? pal.Up : pal.Down));
            m_banks->setItem(r, 7, cell(next.isValid() ? loc.toString(next.toLocalTime().date(), "d MMM yyyy") : "—"));
            m_banks->setItem(r, 8, cell(next.isValid() ? tr("%1 d").arg(now.daysTo(next)) : "—", pal.TextDim));
        }
        fitColumns(m_banks, 0);
        fitHeight(m_banks);
    }

    void fillOceania(const QString &csv)
    {
        const Palette &pal = Theme::current();
        const QStringList lines = csv.split('\n', Qt::SkipEmptyParts);
        if (lines.size() < 2)
            return;
        const QStringList head = lines.first().trimmed().split(',');
        const int area = head.indexOf("REF_AREA"), measure = head.indexOf("MEASURE"), period = head.indexOf("TIME_PERIOD"), value = head.indexOf("OBS_VALUE");
        if (area < 0 || measure < 0 || period < 0 || value < 0)
            return;
        QHash<QString, std::pair<QString, double>> latest;   // "AUS/IRLT" -> month, value
        for (int i = 1; i < lines.size(); ++i) {
            const QStringList c = lines[i].trimmed().split(',');
            if (c.size() <= std::max({area, measure, period, value}))
                continue;
            const QString key = c[area] + '/' + c[measure];
            if (!latest.contains(key) || latest.value(key).first < c[period])
                latest.insert(key, {c[period], c[value].toDouble()});
        }
        const struct { const char *code, *name; } rows[] = {{"AUS", QT_TR_NOOP("Australia")}, {"NZL", QT_TR_NOOP("New Zealand")}};
        m_oceania->setRowCount(2);
        int r = 0;
        for (const auto &row : rows) {
            const auto shortRate = latest.value(QString(row.code) + "/IR3TIB"), longRate = latest.value(QString(row.code) + "/IRLT");
            const bool both = !shortRate.first.isEmpty() && !longRate.first.isEmpty();
            m_oceania->setItem(r, 0, cell(tr(row.name), {}, Qt::AlignLeft, true));
            m_oceania->setItem(r, 1, cell(shortRate.first.isEmpty() ? QStringLiteral("—") : QString("%1%").arg(amount(shortRate.second))));
            m_oceania->setItem(r, 2, cell(longRate.first.isEmpty() ? QStringLiteral("—") : QString("%1%").arg(amount(longRate.second))));
            const double slope = (longRate.second - shortRate.second) * 100.0;
            m_oceania->setItem(r, 3, cell(both ? tr("%1 bp").arg(signedAmount(slope, 0)) : QStringLiteral("—"), !both ? pal.TextDim : slope >= 0 ? pal.Up : pal.Down));
            m_oceania->setItem(r, 4, cell(longRate.first, pal.TextDim));
            ++r;
        }
        fitHeight(m_oceania);
    }

    // CME's three-month €STR futures: thin, but the only free market reading of the ECB's path found.
    void fetchEstr()
    {
        const QVector<QDate> quarters = desk::nextQuarterMonths(QDate::currentDate(), 8);
        QStringList symbols;
        for (const QDate &q : quarters)
            symbols << desk::estrFutureSymbol(q);
        research::Client *client = research::Client::instance();
        client->raw("ecb:estr", QUrl("https://data-api.ecb.europa.eu/service/data/EST/B.EU000A2X2A25.WT?lastNObservations=1&format=csvdata"), 6 * 3600, false, this,
                    [this, symbols, quarters, client](const QByteArray &body, const QString &) {
            const QStringList lines = QString::fromUtf8(body).split('\n', Qt::SkipEmptyParts);
            double today = std::nan("");
            if (lines.size() >= 2) {
                const int col = lines.first().split(',').indexOf("OBS_VALUE");
                const QStringList cells = lines.last().split(',');
                if (col >= 0 && col < cells.size())
                    today = cells[col].toDouble();
            }
            client->quotes(symbols, 15 * 60, this, [this, symbols, quarters, today](const QHash<QString, QJsonObject> &q, const QString &) {
                const Palette &pal = Theme::current();
                const QLocale loc;
                m_estr->setRowCount(0);
                for (int i = 0; i < symbols.size(); ++i) {
                    const double price = research::raw(q.value(symbols[i]), "regularMarketPrice");
                    if (!research::ok(price) || price < 50)
                        continue;
                    const double rate = 100.0 - price, bp = (rate - today) * 100.0;
                    const double volume = research::raw(q.value(symbols[i]), "regularMarketVolume");
                    const int r = m_estr->rowCount();
                    m_estr->insertRow(r);
                    m_estr->setItem(r, 0, cell(tr("%1 to %2").arg(loc.toString(quarters[i], "MMM yy"), loc.toString(quarters[i].addMonths(3), "MMM yy")), {}, Qt::AlignLeft, true));
                    m_estr->setItem(r, 1, cell(QString("%1%").arg(amount(rate, 3))));
                    m_estr->setItem(r, 2, cell(std::isnan(bp) ? QStringLiteral("—") : tr("%1 bp").arg(signedAmount(bp, 0)), std::isnan(bp) ? pal.TextDim : bp >= 0 ? pal.Up : pal.Down));
                    m_estr->setItem(r, 3, cell(std::isnan(bp) ? QStringLiteral("—") : signedAmount(bp / 25.0, 1)));
                    m_estr->setItem(r, 4, cell(research::ok(volume) ? amount(volume, 0) : QStringLiteral("—"), pal.TextDim));
                }
                fitHeight(m_estr);
            });
        });
    }

    void fetchYields()
    {
        struct State {
            QHash<QString, double> us, euro, uk, japan, canada, swiss;
        };
        const QStringList tenors {"3M", "1Y", "2Y", "5Y", "10Y", "20Y", "30Y"};
        const QHash<QString, QString> treasury {{"3M", "3 Mo"}, {"1Y", "1 Yr"}, {"2Y", "2 Yr"}, {"5Y", "5 Yr"}, {"10Y", "10 Yr"}, {"20Y", "20 Yr"}, {"30Y", "30 Yr"}};
        auto state = std::make_shared<State>();
        // Each answer redraws: one slow source doesn't hide the others.
        const auto finish = [this, state, tenors] {
            const Palette &pal = Theme::current();
            const auto column = [&](const QHash<QString, double> &values) {
                QVector<double> out;
                for (const QString &t : tenors)
                    out << values.value(t, research::NaN);
                return out;
            };
            const auto twoYear = [](const QHash<QString, double> &values) { return values.value("2Y", std::nan("")); };
            m_twoYear = {{"USD", twoYear(state->us)}, {"EUR", twoYear(state->euro)}, {"JPY", twoYear(state->japan)}, {"CAD", twoYear(state->canada)}, {"CHF", twoYear(state->swiss)}};
            fillBanks();
            QVector<QDate> dates(tenors.size(), QDate::currentDate());
            m_yields->setXLabels(tenors);
            m_yields->setData(dates, {{tr("United States"), pal.Accent, column(state->us)}, {tr("Euro area"), pal.Up, column(state->euro)},
                                      {tr("United Kingdom"), research::palette::red(), column(state->uk)}, {tr("Japan"), QColor(0xE8, 0xB0, 0x4B), column(state->japan)},
                                      {tr("Canada"), research::palette::blue(), column(state->canada)}, {tr("Switzerland"), pal.TextDim, column(state->swiss)}},
                              [](double v) { return QString("%1%").arg(amount(v)); });
        };
        research::Client *client = research::Client::instance();
        client->treasuryYields(this, [state, treasury, finish](const QVector<research::YieldCurveDay> &days, const QString &) {
            if (!days.isEmpty())
                for (auto it = treasury.begin(); it != treasury.end(); ++it)
                    if (days.first().yields.contains(it.value()))
                        state->us.insert(it.key(), days.first().yields.value(it.value()));
            finish();
        });
        for (const QString &t : tenors) {
            const QString url = QString("https://data-api.ecb.europa.eu/service/data/YC/B.U2.EUR.4F.G_N_A.SV_C_YM.SR_%1?lastNObservations=1&format=csvdata").arg(t);
            client->raw("ecb:yc:" + t, QUrl(url), 6 * 3600, false, this, [state, t, finish](const QByteArray &body, const QString &) {
                const QStringList lines = QString::fromUtf8(body).split('\n', Qt::SkipEmptyParts);
                if (lines.size() >= 2) {
                    const int col = lines.first().split(',').indexOf("OBS_VALUE");
                    const QStringList cells = lines.last().split(',');
                    if (col >= 0 && col < cells.size())
                        state->euro.insert(t, cells[col].toDouble());
                }
                finish();
            });
        }
        QUrl gilts("https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp");
        gilts.setQuery(QString("csv.x=yes&Datefrom=%1&Dateto=now&SeriesCodes=IUDSNZC,IUDMNZC,IUDLNZC&CSVF=TN&UsingCodes=Y&VPD=Y&VFD=N").arg(QLocale::c().toString(QDate::currentDate().addDays(-14), "dd/MMM/yyyy")));
        client->raw("boe:gilts", gilts, 6 * 3600, false, this, [state, finish](const QByteArray &body, const QString &) {
            state->uk = desk::parseBoeYields(QString::fromUtf8(body));
            finish();
        });
        client->raw("mof:jgb", QUrl("https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/jgbcme.csv"), 6 * 3600, false, this, [state, finish](const QByteArray &body, const QString &) {
            state->japan = desk::parseMofYields(QString::fromUtf8(body));
            finish();
        });
        QUrl swiss("https://data.snb.ch/api/cube/rendeiduebd/data/csv/en");
        swiss.setQuery("fromDate=" + QDate::currentDate().addDays(-21).toString(Qt::ISODate));
        client->raw("snb:yields", swiss, 6 * 3600, false, this, [state, finish](const QByteArray &body, const QString &) {
            state->swiss = desk::parseSnbYields(QString::fromUtf8(body));
            finish();
        });
        // Australia and New Zealand: the central banks refuse programs, so the OECD's monthly rates, two points only.
        QUrl oceania("https://sdmx.oecd.org/public/rest/data/OECD.SDD.STES,DSD_KEI@DF_KEI,4.0/AUS+NZL.M.IRLT+IR3TIB.......");
        oceania.setQuery("startPeriod=" + QDate::currentDate().addMonths(-5).toString("yyyy-MM") + "&format=csvfile");
        client->raw("oecd:kei:oceania", oceania, 24 * 3600, false, {{"Accept-Language", "en"}}, this, [this](const QByteArray &body, const QString &) {
            fillOceania(QString::fromUtf8(body));
        });
        fetchEstr();
        client->raw("boc:yields", QUrl("https://www.bankofcanada.ca/valet/observations/group/bond_yields_all/json?recent=1"), 6 * 3600, false, this, [state, finish](const QByteArray &body, const QString &) {
            state->canada = desk::parseValetYields(body);
            finish();
        });
    }

    // The calendar's names for a policy decision differ by country ("Interest Rate", "Marginal Lending Rate"…).
    static bool isRateDecision(const QString &title)
    {
        return title == "Interest Rate" || title.contains("Interest Rate Decision", Qt::CaseInsensitive) || title.contains("Marginal Lending Rate", Qt::CaseInsensitive)
            || title.contains("Deposit Facility Rate", Qt::CaseInsensitive) || title.contains("Loan Prime Rate 1Y", Qt::CaseInsensitive);
    }

    Context m_context;
    QVector<EconomicCalendar::Event> m_decisions;
    QHash<QString, QVector<QDate>> m_meetings_;   // by currency, from the banks' own pages
    QHash<QString, desk::Vote> m_votes;
    QHash<QString, double> m_twoYear;
    QTableWidget *m_estr, *m_oceania;
    desk::OisCurve m_oisCurve;
    quant::MultiLineChart *m_ois;
    QTableWidget *m_oisTable;
    QDate m_nextFomc;
    quant::MultiLineChart *m_pricingHistory;
    QLabel *m_pricingNote;
    QLabel *m_current, *m_currentSub, *m_next, *m_nextSub, *m_priced, *m_pricedSub, *m_end, *m_endSub;
    quant::MultiLineChart *m_curve, *m_yields;
    BarsChart *m_points;
    QTableWidget *m_meetings, *m_banks;
};

// ------------------------------------------------------------------ Positioning

// Forex and CFD positioning of Myfxbook's community, with the user's free account.
class FxPositioningPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FxPositioningPanel(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(10);

        QVBoxLayout *body;
        m_login = quant::card(tr("Connect your Myfxbook account"), &body);
        auto *explain = quant::label(tr("Myfxbook publishes its community's positions to accounts through its official API. A free account is enough. Your password goes once to Myfxbook, over HTTPS, to open a session, and isn't kept: only the session is. It ends after a month or when your internet address changes, and then this asks again. The figures are asked at most every half hour."), "qtNote");
        explain->setWordWrap(true);
        body->addWidget(explain);
        auto *form = new QHBoxLayout;
        form->setSpacing(8);
        m_email = new QLineEdit;
        m_email->setObjectName("qtInput");
        m_email->setPlaceholderText(tr("Email of the account"));
        form->addWidget(m_email, 1);
        m_password = new QLineEdit;
        m_password->setObjectName("qtInput");
        m_password->setEchoMode(QLineEdit::Password);
        m_password->setPlaceholderText(tr("Password"));
        form->addWidget(m_password, 1);
        m_connect = deskAction(tr("Connect"));
        form->addWidget(m_connect);
        body->addLayout(form);
        m_loginError = quant::label({}, "qtNote");
        m_loginError->setWordWrap(true);
        body->addWidget(m_loginError);
        v->addWidget(m_login);

        m_board = new QWidget;
        auto *bv = new QVBoxLayout(m_board);
        bv->setContentsMargins(0, 0, 0, 0);
        bv->setSpacing(10);
        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Crowded long"), &m_crowdedLong, &m_crowdedLongSub), 1);
        tiles->addWidget(tile(tr("Crowded short"), &m_crowdedShort, &m_crowdedShortSub), 1);
        tiles->addWidget(tile(tr("Most long"), &m_mostLong, &m_mostLongSub), 1);
        tiles->addWidget(tile(tr("Most short"), &m_mostShort, &m_mostShortSub), 1);
        tiles->addWidget(tile(tr("Crowd lean"), &m_lean, &m_leanSub), 1);
        bv->addLayout(tiles);
        auto *historyCard = quant::card(tr("The crowd's long share, day after day"), &body);
        m_historyTitle = historyCard->findChild<QLabel *>();
        m_history = new quant::MultiLineChart;
        m_history->setMinimumHeight(190);
        m_history->setZeroLine(false);
        body->addWidget(m_history);
        m_historyNote = quant::label({}, "qtNote");
        m_historyNote->setWordWrap(true);
        body->addWidget(m_historyNote);
        bv->addWidget(historyCard);
        QHBoxLayout *head;
        auto *card = quant::card(tr("Myfxbook community: positions long and short"), &body, &head);
        m_updated = quant::label({}, "qtNote");
        head->addWidget(m_updated);
        auto *disconnect = deskAction(tr("Disconnect"));
        head->addWidget(disconnect);
        m_table = table({tr("Symbol"), tr("Positioning"), tr("Long"), tr("Short"), tr("Long lots"), tr("Short lots"), tr("Positions"), tr("Avg long price"), tr("Avg short price"), tr("Status")});
        useSplitBars(m_table, 1);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        body->addWidget(m_table);
        m_table->setCursor(Qt::PointingHandCursor);
        connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            if (auto *item = m_table->item(row, 0)) {
                m_historySymbol = item->text();
                fillHistory();
            }
        });
        bv->addWidget(card);
        v->addWidget(m_board);
        m_message = quant::label({}, "qtNote");
        m_message->setWordWrap(true);
        v->addWidget(m_message);
        auto *note = quant::label(tr("The share of lots long and short of the traders whose accounts are connected to Myfxbook, demo accounts included: one community of retail traders, not the whole market, and it says nothing of when a crowded trade turns. Symbols with few positions stay in the list, dimmed, but don't count in the tiles. Myfxbook's own figures, under its terms of use. COT positions are in Quant › COT."), "qtNote");
        note->setWordWrap(true);
        v->addWidget(note);
        v->addStretch(1);

        Myfxbook *m = Myfxbook::instance();
        const auto submit = [this, m] {
            const QString email = m_email->text().trimmed(), password = m_password->text();
            if (email.isEmpty() || password.isEmpty())
                return;
            m_connect->setEnabled(false);
            m_loginError->setText(tr("Connecting…"));
            m->logIn(email, password);
            m_password->clear();
        };
        connect(m_connect, &QPushButton::clicked, this, submit);
        connect(m_password, &QLineEdit::returnPressed, this, submit);
        connect(disconnect, &QPushButton::clicked, m, &Myfxbook::logOut);
        connect(m, &Myfxbook::changed, this, &FxPositioningPanel::fill);
        connect(DeskHistory::instance(), &DeskHistory::changed, this, &FxPositioningPanel::fillHistory);
        connect(m, &Myfxbook::loginFailed, this, [this](const QString &message) {
            m_connect->setEnabled(true);
            m_loginError->setText(message);
        });
        m_clock.setInterval(5 * 60 * 1000);
        connect(&m_clock, &QTimer::timeout, m, &Myfxbook::refreshIfStale);
        fill();
    }

protected:
    void showEvent(QShowEvent *) override
    {
        Myfxbook::instance()->refreshIfStale();
        m_clock.start();
    }
    void hideEvent(QHideEvent *) override { m_clock.stop(); }

private:
    // Myfxbook only gives today's figures: the history is what this computer noted. A click on a row chooses the symbol.
    void fillHistory()
    {
        const QVector<desk::DayPoint> points = DeskHistory::instance()->history().value(desk::crowdKey(m_historySymbol));
        m_historyTitle->setText(tr("%1: the crowd's long share, day after day").arg(m_historySymbol).toUpper());
        QVector<QDate> dates;
        QVector<double> values;
        for (const desk::DayPoint &p : points) {
            dates << p.date;
            values << p.value;
        }
        m_history->setData(dates, {{tr("Long"), Theme::current().Up, values}}, [](double v) { return QString("%1%").arg(amount(v, 0)); });
        m_history->setVisible(points.size() >= 2);
        m_historyNote->setText(points.size() < 2
                                   ? tr("Noted on this computer each weekday while the app is open, since Myfxbook keeps no history. The line appears after a second day. A click on a row chooses the symbol.")
                                   : tr("Noted on this computer each weekday while the app is open, since %1; a day the app stayed shut is a gap. Symbols with fewer than %2 positions aren't noted. A click on a row chooses the symbol.")
                                         .arg(QLocale().toString(points.first().date, QLocale::ShortFormat)).arg(desk::OutlookMinPositions));
    }

    void fill()
    {
        Myfxbook *m = Myfxbook::instance();
        const Palette &pal = Theme::current();
        const bool connected = m->isConnected();
        m_login->setVisible(!connected);
        m_connect->setEnabled(true);
        if (connected)
            m_loginError->clear();
        else if (!m->errorString().isEmpty())
            m_loginError->setText(m->errorString());
        QVector<desk::OutlookRow> rows = m->outlook().rows;
        // Without a session the last figures stay hidden: they would look current.
        m_board->setVisible(connected && !rows.isEmpty());
        m_message->setVisible(connected);
        m_message->setText(m->errorString().isEmpty() ? (rows.isEmpty() ? tr("Loading Myfxbook's figures…") : QString()) : m->errorString());
        if (!connected || rows.isEmpty())
            return;
        // The most traded first: the thin symbols, at 0% or 100% on a handful of positions, would otherwise lead.
        std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.positions() > b.positions(); });
        const double demo = m->outlook().demoPct;
        m_updated->setText(demo >= 0 ? tr("%1% of the community's accounts are demo · updated %2").arg(amount(demo, 0), QLocale().toString(m->fetchedAt().toLocalTime().time(), "HH:mm"))
                                     : tr("updated %1").arg(QLocale().toString(m->fetchedAt().toLocalTime().time(), "HH:mm")));
        m_table->setRowCount(rows.size());
        for (int r = 0; r < rows.size(); ++r) {
            const desk::OutlookRow &row = rows[r];
            const bool thin = row.positions() < desk::OutlookMinPositions;
            QString status = tr("BALANCED");
            QColor color = pal.TextDim;
            if (thin) { status = tr("FEW POSITIONS"); color = pal.TextFaint; }
            else if (row.longPct >= 70) { status = tr("CROWDED LONG"); color = pal.Up; }
            else if (row.longPct <= 30) { status = tr("CROWDED SHORT"); color = pal.Down; }
            else if (row.longPct >= 55) { status = tr("LEANING LONG"); color = pal.Up; }
            else if (row.longPct <= 45) { status = tr("LEANING SHORT"); color = pal.Down; }
            m_table->setItem(r, 0, cell(row.symbol, thin ? pal.TextDim : QColor(), Qt::AlignLeft, !thin));
            auto *split = cell({});
            split->setData(Qt::UserRole, row.longPct);
            m_table->setItem(r, 1, split);
            m_table->setItem(r, 2, cell(QString("%1%").arg(amount(row.longPct, 0)), thin ? pal.TextDim : pal.Up));
            m_table->setItem(r, 3, cell(QString("%1%").arg(amount(row.shortPct, 0)), thin ? pal.TextDim : pal.Down));
            m_table->setItem(r, 4, cell(amount(row.longVolume, 0)));
            m_table->setItem(r, 5, cell(amount(row.shortVolume, 0)));
            m_table->setItem(r, 6, cell(amount(row.positions(), 0), pal.TextDim));
            const int decimals = priceDecimals(row.avgLongPrice > 0 ? row.avgLongPrice : row.avgShortPrice);
            m_table->setItem(r, 7, cell(row.avgLongPrice > 0 ? QLocale().toString(row.avgLongPrice, 'f', decimals) : "—"));
            m_table->setItem(r, 8, cell(row.avgShortPrice > 0 ? QLocale().toString(row.avgShortPrice, 'f', decimals) : "—"));
            m_table->setItem(r, 9, cell(status, color, Qt::AlignCenter, !thin));
        }
        fitHeight(m_table);
        if (m_historySymbol.isEmpty() && !rows.isEmpty())
            m_historySymbol = rows.first().symbol;
        fillHistory();
        const desk::OutlookSummary sum = desk::summarizeOutlook(rows);
        m_crowdedLong->setText(QString::number(sum.crowdedLong));
        m_crowdedLongSub->setText(tr("of %1 symbols with %2 positions or more, at 70% or more").arg(sum.counted).arg(desk::OutlookMinPositions));
        m_crowdedShort->setText(QString::number(sum.crowdedShort));
        m_crowdedShortSub->setText(tr("of %1 symbols with %2 positions or more, at 30% or less").arg(sum.counted).arg(desk::OutlookMinPositions));
        m_mostLong->setText(sum.counted ? sum.mostLong.symbol : QStringLiteral("—"));
        m_mostLongSub->setText(sum.counted ? tr("%1% long").arg(amount(sum.mostLong.longPct, 0)) : QString());
        m_mostShort->setText(sum.counted ? sum.mostShort.symbol : QStringLiteral("—"));
        m_mostShortSub->setText(sum.counted ? tr("%1% short").arg(amount(sum.mostShort.shortPct, 0)) : QString());
        m_lean->setText(sum.counted ? tr("%1% long").arg(amount(sum.meanLong, 0)) : QStringLiteral("—"));
        m_leanSub->setText(tr("the mean of these %1 symbols").arg(sum.counted));
    }

    QFrame *m_login;
    QLineEdit *m_email, *m_password;
    QPushButton *m_connect;
    QLabel *m_loginError, *m_updated, *m_message, *m_historyTitle, *m_historyNote;
    quant::MultiLineChart *m_history;
    QString m_historySymbol;
    QWidget *m_board;
    QLabel *m_crowdedLong, *m_crowdedLongSub, *m_crowdedShort, *m_crowdedShortSub, *m_mostLong, *m_mostLongSub, *m_mostShort, *m_mostShortSub, *m_lean, *m_leanSub;
    QTableWidget *m_table;
    QTimer m_clock;
};

class PositioningView : public quant::View
{
    Q_OBJECT
public:
    explicit PositioningView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        QButtonGroup *markets;
        h->addWidget(quant::segments({tr("Forex and CFDs · Myfxbook"), tr("Crypto · Binance")}, &markets, std::clamp(QSettings().value("desk/positioning/market", 0).toInt(), 0, 1)));
        h->addStretch(1);
        content()->addWidget(top);
        auto *stack = new QStackedWidget;
        stack->addWidget(new FxPositioningPanel);
        auto *crypto = new QWidget;
        auto *page = new QVBoxLayout(crypto);
        page->setContentsMargins(0, 0, 0, 0);
        page->setSpacing(10);
        stack->addWidget(crypto);
        stack->setCurrentIndex(markets->checkedId());
        content()->addWidget(stack);
        connect(markets, &QButtonGroup::idClicked, this, [this, stack](int id) {
            QSettings().setValue("desk/positioning/market", id);
            stack->setCurrentIndex(id);
        });
        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Crowded long"), &m_crowdedLong, &m_crowdedLongSub), 1);
        tiles->addWidget(tile(tr("Crowded short"), &m_crowdedShort, &m_crowdedShortSub), 1);
        tiles->addWidget(tile(tr("Most long"), &m_mostLong, &m_mostLongSub), 1);
        tiles->addWidget(tile(tr("Most short"), &m_mostShort, &m_mostShortSub), 1);
        tiles->addWidget(tile(tr("Crowd lean"), &m_lean, &m_leanSub), 1);
        page->addLayout(tiles);
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        QVBoxLayout *body;
        auto *listCard = quant::card(tr("Crypto perpetuals: accounts long and short"), &body);
        m_table = table({tr("Symbol"), tr("Positioning"), tr("Long"), tr("Short"), tr("24h Δ long"), tr("Status")});
        useSplitBars(m_table, 1);
        m_table->setCursor(Qt::PointingHandCursor);
        body->addWidget(m_table);
        row->addWidget(listCard, 3);
        auto *historyCard = quant::card(tr("Share of accounts long, 30 days"), &body);
        m_history = new quant::MultiLineChart;
        m_history->setMinimumHeight(280);
        m_history->setZeroLine(false);
        body->addWidget(m_history);
        row->addWidget(historyCard, 2);
        page->addLayout(row);
        auto *note = quant::label(tr("Binance's public ratio of accounts long and short on its USDT perpetuals, a measure of the crowd, not of money. COT positions are in Quant › COT."), "qtNote");
        note->setWordWrap(true);
        page->addWidget(note);
        page->addStretch(1);
        content()->addStretch(1);
        connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            if (row >= 0 && row < m_rows.size())
                showHistory(m_rows[row].symbol);
        });
    }

protected:
    void load() override
    {
        setStatus(tr("Loading Binance's ratios…"));
        const QStringList symbols {"BTC", "ETH", "SOL", "XRP", "BNB", "ADA", "DOGE", "LINK", "AVAX", "LTC", "DOT", "TRX"};
        auto left = std::make_shared<int>(symbols.size());
        auto first = std::make_shared<QString>();
        for (const QString &s : symbols) {
            const QString pair = s + "USDT";
            deskdata::loadLongShare(pair, this, [this, s, pair, left, first](const QVector<double> &longs, const QString &error) {
                if (!longs.isEmpty())
                    m_rows.push_back({s, pair, longs.last(), longs.size() > 1 ? longs.last() - longs[longs.size() - 2] : 0.0});
                else if (first->isEmpty())
                    *first = error;
                if (--*left == 0) {
                    setStatus(m_rows.isEmpty() ? tr("Binance's ratios are unavailable: %1").arg(*first) : QString());
                    fill();
                }
            });
        }
    }

private:
    struct Row {
        QString name, symbol;
        double longShare, change;
    };

    void fill()
    {
        const Palette &pal = Theme::current();
        std::sort(m_rows.begin(), m_rows.end(), [](const Row &a, const Row &b) { return std::abs(a.longShare - 50) > std::abs(b.longShare - 50); });
        m_table->setRowCount(m_rows.size());
        int crowdedLong = 0, crowdedShort = 0;
        double sum = 0;
        const Row *mostLong = nullptr, *mostShort = nullptr;
        for (int r = 0; r < m_rows.size(); ++r) {
            const Row &row = m_rows[r];
            sum += row.longShare;
            if (!mostLong || row.longShare > mostLong->longShare) mostLong = &row;
            if (!mostShort || row.longShare < mostShort->longShare) mostShort = &row;
            crowdedLong += row.longShare >= 70;
            crowdedShort += row.longShare <= 30;
            QString status = tr("BALANCED");
            QColor color = pal.TextDim;
            if (row.longShare >= 70) { status = tr("CROWDED LONG"); color = pal.Up; }
            else if (row.longShare <= 30) { status = tr("CROWDED SHORT"); color = pal.Down; }
            else if (row.longShare >= 55) { status = tr("LEANING LONG"); color = pal.Up; }
            else if (row.longShare <= 45) { status = tr("LEANING SHORT"); color = pal.Down; }
            m_table->setItem(r, 0, cell(row.name + "/USDT", {}, Qt::AlignLeft, true));
            auto *split = cell({});
            split->setData(Qt::UserRole, row.longShare);
            m_table->setItem(r, 1, split);
            m_table->setItem(r, 2, cell(QString("%1%").arg(amount(row.longShare, 0)), pal.Up));
            m_table->setItem(r, 3, cell(QString("%1%").arg(amount(100 - row.longShare, 0)), pal.Down));
            m_table->setItem(r, 4, cell(tr("%1 pt").arg(signedAmount(row.change, 1)), row.change >= 0 ? pal.Up : pal.Down));
            m_table->setItem(r, 5, cell(status, color, Qt::AlignCenter, true));
        }
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        fitHeight(m_table);
        m_crowdedLong->setText(QString::number(crowdedLong));
        m_crowdedLongSub->setText(tr("of %1, at 70% or more").arg(m_rows.size()));
        m_crowdedShort->setText(QString::number(crowdedShort));
        m_crowdedShortSub->setText(tr("of %1, at 30% or less").arg(m_rows.size()));
        if (mostLong) {
            m_mostLong->setText(mostLong->name);
            m_mostLongSub->setText(tr("%1% long").arg(amount(mostLong->longShare, 0)));
            m_mostShort->setText(mostShort->name);
            m_mostShortSub->setText(tr("%1% short").arg(amount(100 - mostShort->longShare, 0)));
            m_lean->setText(tr("%1% long").arg(amount(sum / m_rows.size(), 0)));
            m_leanSub->setText(tr("the mean of these markets"));
            showHistory(m_rows.first().symbol);
        }
    }

    void showHistory(const QString &pair)
    {
        deskdata::loadLongShare(pair, this, [this, pair](const QVector<double> &longs, const QString &) {
            QVector<QDate> dates;
            const QDate today = QDate::currentDate();
            for (int i = 0; i < longs.size(); ++i)
                dates << today.addDays(i - longs.size() + 1);
            m_history->setTitle({{pair, Theme::current().Text}});
            m_history->setData(dates, {{tr("Long"), Theme::current().Accent, longs}}, [](double v) { return QString("%1%").arg(amount(v, 0)); });
        });
    }

    QLabel *m_crowdedLong, *m_crowdedLongSub, *m_crowdedShort, *m_crowdedShortSub, *m_mostLong, *m_mostLongSub, *m_mostShort, *m_mostShortSub, *m_lean, *m_leanSub;
    QTableWidget *m_table;
    quant::MultiLineChart *m_history;
    QVector<Row> m_rows;
};

// ------------------------------------------------------------------ Forecasts

// Other people's forecasts, side by side: ING's by quarter, the IMF's and the OECD's by year, the Fed's own projections.
class ForecastsView : public quant::View
{
    Q_OBJECT
public:
    explicit ForecastsView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
    {
        QHBoxLayout *h;
        QVBoxLayout *body;
        auto *ing = quant::card(tr("ING Think, by quarter"), &body, &h);
        m_updated = quant::label({}, "qtNote");
        h->addWidget(m_updated);
        auto *open = deskAction(tr("Open the page"));
        connect(open, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://think.ing.com/forecasts/")); });
        h->addWidget(open);
        m_tab = deskCombo();
        m_tab->setMinimumWidth(240);
        body->addWidget(m_tab, 0, Qt::AlignLeft);
        m_ing = table({tr("Group"), tr("Name")});
        body->addWidget(m_ing);
        auto *ingNote = quant::label(tr("Source: ING Bank N.V., think.ing.com. The bank's own forecasts, end of period unless the table says otherwise; they are revised and aren't advice."), "qtNote");
        ingNote->setWordWrap(true);
        body->addWidget(ingNote);
        content()->addWidget(ing);

        m_priceCard = quant::card(tr("ING's path on the price"), &body);
        m_priceTitle = quant::label({}, "qtCardValue");
        body->addWidget(m_priceTitle);
        m_price = new quant::MultiLineChart;
        m_price->setMinimumHeight(260);
        m_price->setZeroLine(false);
        m_price->setLegend(true);
        body->addWidget(m_price);
        auto *priceNote = quant::label(tr("A year of daily closes from Yahoo Finance, then a straight line through ING's end-of-quarter forecasts. Click a pair in the table to see it."), "qtNote");
        priceNote->setWordWrap(true);
        body->addWidget(priceNote);
        content()->addWidget(m_priceCard);
        connect(m_ing, &QTableWidget::cellClicked, this, [this](int row, int) {
            for (int c = 0; c < m_ing->columnCount(); ++c)
                if (const QTableWidgetItem *it = m_ing->item(row, c); it && it->text().contains('/')) {
                    showPair(it->text());
                    return;
                }
        });

        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        auto *macro = quant::card(tr("Growth, inflation and unemployment, by year"), &body);
        const int year = QDate::currentDate().year();
        const int now = year % 100, next = now + 1;
        m_macro = table({tr("Economy"), tr("GDP %1 IMF").arg(now), tr("GDP %1 IMF").arg(next), tr("GDP %1 OECD").arg(now), tr("GDP %1 OECD").arg(next),
                         tr("CPI %1 IMF").arg(now), tr("CPI %1 IMF").arg(next), tr("Unemp. %1 IMF").arg(next), tr("Unemp. %1 OECD").arg(next)});
        body->addWidget(m_macro);
        auto *macroNote = quant::label(tr("IMF World Economic Outlook and OECD Economic Outlook, in % a year. In the second row, the IMF's figures are Germany's and the OECD's the euro area's."), "qtNote");
        macroNote->setWordWrap(true);
        body->addWidget(macroNote);
        QHBoxLayout *macroBar = new QHBoxLayout;
        m_macroTitle = quant::label({}, "qtCardValue");
        macroBar->addWidget(m_macroTitle);
        macroBar->addStretch(1);
        macroBar->addWidget(quant::segments({tr("GDP"), tr("CPI"), tr("Unemployment")}, &m_macroMeasure));
        body->addLayout(macroBar);
        m_macroChart = new quant::MultiLineChart;
        m_macroChart->setMinimumHeight(220);
        m_macroChart->setLegend(true);
        body->addWidget(m_macroChart);
        connect(m_macroMeasure, &QButtonGroup::idClicked, this, [this] { showEconomy(m_economyIndex); });
        connect(m_macro, &QTableWidget::cellClicked, this, [this](int row, int) { showEconomy(row); });
        row->addWidget(macro, 5);
        auto *fed = quant::card(tr("The Fed's own projections, median"), &body);
        m_fed = table({tr("Measure"), QString::number(year), QString::number(year + 1), QString::number(year + 2)});
        body->addWidget(m_fed);
        m_fedNote = quant::label(tr("The FOMC's Summary of Economic Projections, through FRED."), "qtNote");
        m_fedNote->setWordWrap(true);
        body->addWidget(m_fedNote);
        row->addWidget(fed, 2, Qt::AlignTop);
        content()->addLayout(row);

        auto *rangeCard = quant::card(tr("How far apart forecasters are"), &body);
        m_range = table({tr("Economy"), tr("Measure"), tr("Year"), tr("Lowest"), tr("Median"), tr("Highest"), tr("Forecasters"), tr("Survey")});
        body->addWidget(m_range);
        auto *rangeNote = quant::label(tr("Each forecaster's own answer in the latest Survey of Professional Forecasters: the ECB's, quarterly, and the Philadelphia Fed's, quarterly. Yearly figures, not the range for one release: no free source publishes that one for a calendar event."), "qtNote");
        rangeNote->setWordWrap(true);
        body->addWidget(rangeNote);
        content()->addWidget(rangeCard);

        auto *others = quant::card(tr("Other banks, on their own sites"), &body);
        auto *linkNote = quant::label(tr("TD, KBC, MUFG and RBC publish FX tables too. RBC and MUFG don't allow theirs to be extracted or quoted without consent, and TD and KBC state no such permission, so their figures stay on their pages."), "qtNote");
        linkNote->setWordWrap(true);
        body->addWidget(linkNote);
        auto *links = new QHBoxLayout;
        const struct { const char *name, *url; } sites[] = {{"TD Economics", "https://economics.td.com/ca-forecast-tables"}, {"KBC Economics", "https://www.kbc.com/en/economics/forecasts/world.html"},
                                                            {"MUFG Research", "https://www.mufgresearch.com/forecasts/"}, {"RBC Economics", "https://www.rbc.com/en/economics/monthly-forecast-update/"}};
        for (const auto &site : sites) {
            auto *b = deskAction(site.name);
            b->setToolTip(tr("Opens in your browser"));
            connect(b, &QPushButton::clicked, this, [url = QString(site.url)] { QDesktopServices::openUrl(QUrl(url)); });
            links->addWidget(b);
        }
        links->addStretch(1);
        body->addLayout(links);
        content()->addWidget(others);
        content()->addStretch(1);
        connect(m_tab, &QComboBox::currentIndexChanged, this, [this](int i) {
            if (i >= 0 && !m_filling)
                QSettings().setValue("desk/forecasts/tab", m_tab->currentData().toString());
            fillIng();
        });
    }

protected:
    void load() override
    {
        setStatus(tr("Loading the forecasts…"));
        research::Client *client = research::Client::instance();
        client->raw("ing:forecasts", QUrl("https://think.ing.com/forecasts/"), 12 * 3600, false, this, [this](const QByteArray &body, const QString &error) {
            m_tabs = desk::parseIngForecasts(QString::fromUtf8(body));
            setStatus(m_tabs.isEmpty() ? tr("ING's forecasts are unavailable: %1").arg(error.isEmpty() ? tr("the page changed") : error) : QString());
            const QString saved = QSettings().value("desk/forecasts/tab", "fx").toString();
            m_filling = true;
            m_tab->clear();
            for (const desk::ForecastTab &t : std::as_const(m_tabs))
                m_tab->addItem(t.title, t.id);
            m_tab->setCurrentIndex(std::max(0, m_tab->findData(saved)));
            m_filling = false;
            fillIng();
        });
        const int year = QDate::currentDate().year();
        QUrl oecd("https://sdmx.oecd.org/public/rest/data/OECD.ECO.MAD,DSD_EO@DF_EO,/USA+EA17+JPN+GBR+CHE+CAN+AUS+NZL.GDPV_ANNPCT+UNR.A");
        oecd.setQuery(QString("startPeriod=%1&format=csvfile").arg(year));
        client->raw("oecd:eo", oecd, 24 * 3600, false, {{"Accept-Language", "en"}}, this, [this](const QByteArray &body, const QString &) {
            const QStringList lines = QString::fromUtf8(body).split('\n', Qt::SkipEmptyParts);
            if (lines.size() < 2)
                return;
            const QStringList head = lines.first().trimmed().split(',');
            const int area = head.indexOf("REF_AREA"), measure = head.indexOf("MEASURE"), period = head.indexOf("TIME_PERIOD"), value = head.indexOf("OBS_VALUE");
            if (area < 0 || measure < 0 || period < 0 || value < 0)
                return;
            for (int i = 1; i < lines.size(); ++i) {
                const QStringList cells = lines[i].trimmed().split(',');
                if (cells.size() > std::max({area, measure, period, value}))
                    m_oecd.insert(cells[area] + '/' + cells[measure] + '/' + cells[period], cells[value].toDouble());
            }
            fillMacro();
        });
        if (m_context.economy) {
            connect(m_context.economy, &RemoteDataSet::updated, this, &ForecastsView::fillMacro, Qt::UniqueConnection);
            m_context.economy->refreshIfStale();
        }
        fillMacro();
        fetchFed();
        fetchRanges();
    }

private:
    void fillIng()
    {
        const int index = m_tab->currentIndex();
        if (index < 0 || index >= m_tabs.size()) {
            m_ing->setRowCount(0);
            return;
        }
        const desk::ForecastTab &tab = m_tabs[index];
        m_updated->setText(tr("updated %1").arg(tab.updated));
        int columns = 0, labels = 0, rows = 0;
        for (const desk::ForecastTable &t : tab.tables) {
            columns = std::max(columns, int(t.columns.size()));
            rows += int(t.rows.size());
            for (const auto &r : t.rows)
                labels = std::max(labels, int(r.labels.size()));
        }
        labels = std::max(labels, 1);
        QStringList headers(labels + columns, QString());
        headers[0] = tr("Name");
        if (!tab.tables.isEmpty())
            for (int c = 0; c < tab.tables.first().columns.size(); ++c)
                headers[labels + c] = tab.tables.first().columns[c];
        m_ing->setColumnCount(labels + columns);
        m_ing->setHorizontalHeaderLabels(headers);
        fitColumns(m_ing);
        // Each table of the tab under a line naming its group, with its own periods when they differ.
        m_ing->setRowCount(rows + int(tab.tables.size()));
        int r = 0;
        const Palette &pal = Theme::current();
        for (const desk::ForecastTable &t : tab.tables) {
            for (int c = 0; c < m_ing->columnCount(); ++c)
                m_ing->setItem(r, c, cell({}));
            m_ing->setItem(r, 0, cell(t.group, pal.Accent, Qt::AlignLeft, true));
            // The header row already names the first table's periods.
            if (t.columns != tab.tables.first().columns)
                for (int c = 0; c < t.columns.size(); ++c)
                    m_ing->setItem(r, labels + c, cell(t.columns[c], pal.TextDim));
            ++r;
            for (const auto &row : t.rows) {
                for (int c = 0; c < m_ing->columnCount(); ++c)
                    m_ing->setItem(r, c, cell({}));
                for (int c = 0; c < row.labels.size(); ++c)
                    m_ing->setItem(r, c, cell(row.labels[c], {}, Qt::AlignLeft, c == row.labels.size() - 1));
                for (int c = 0; c < row.values.size(); ++c)
                    m_ing->setItem(r, labels + c, cell(row.values[c]));
                ++r;
            }
        }
        fitHeight(m_ing);
        const bool fx = tab.id == "fx";
        m_priceCard->setVisible(fx);
        if (fx && m_price->property("shown").isNull()) {
            m_price->setProperty("shown", true);
            showPair("EUR/USD");
        }
    }

    struct Economy { const char *imf, *oecd, *name; };
    static const QVector<Economy> &economyList()
    {
        static const QVector<Economy> list {
            {"USA", "USA", QT_TR_NOOP("United States")}, {"DEU", "EA17", QT_TR_NOOP("Germany · euro area")}, {"JPN", "JPN", QT_TR_NOOP("Japan")}, {"GBR", "GBR", QT_TR_NOOP("United Kingdom")},
            {"CHE", "CHE", QT_TR_NOOP("Switzerland")}, {"CAN", "CAN", QT_TR_NOOP("Canada")}, {"AUS", "AUS", QT_TR_NOOP("Australia")}, {"NZL", "NZL", QT_TR_NOOP("New Zealand")}};
        return list;
    }

    // The IMF's published years, then its projections, with the OECD's forecasts when it has this measure.
    void showEconomy(int index)
    {
        const QVector<Economy> &list = economyList();
        if (index < 0 || index >= list.size() || !m_context.economy)
            return;
        m_economyIndex = index;
        const Economy &e = list[index];
        const int measure = std::max(0, m_macroMeasure->checkedId());
        const WorldEconomy::Indicator indicator = measure == 0 ? WorldEconomy::GdpGrowth : measure == 1 ? WorldEconomy::Inflation : WorldEconomy::Unemployment;
        const QMap<int, double> series = m_context.economy->series(e.imf, indicator);
        const int year = QDate::currentDate().year();
        QVector<QDate> dates;
        QVector<double> past, projected, oecd;
        for (int y = year - 12; y <= year + 5; ++y) {
            const double v = series.value(y, std::nan(""));
            dates << QDate(y, 7, 1);
            past << (y < year ? v : std::nan(""));
            projected << (y >= year - 1 ? v : std::nan(""));   // from the last published year, so the lines meet
            const char *code = measure == 0 ? "GDPV_ANNPCT" : measure == 2 ? "UNR" : nullptr;
            oecd << (code ? m_oecd.value(QString("%1/%2/%3").arg(e.oecd, code).arg(y), std::nan("")) : std::nan(""));
        }
        const Palette &pal = Theme::current();
        m_macroTitle->setText(tr(e.name));
        QVector<quant::MultiLineChart::Line> lines {{tr("IMF, published"), pal.Text, past}, {tr("IMF, projected"), pal.Accent, projected}};
        if (measure != 1)
            lines.push_back({tr("OECD, forecast"), pal.Up, oecd});
        m_macroChart->setData(dates, lines, [](double v) { return QString("%1%").arg(amount(v, 1)); });
    }

    void showPair(const QString &pair)
    {
        const desk::Instrument *instrument = desk::findInstrument(pair);
        m_priceTitle->setText(pair);
        const desk::ForecastTable::Row *found = nullptr;
        const desk::ForecastTable *table = nullptr;
        for (const desk::ForecastTab &tab : std::as_const(m_tabs))
            for (const desk::ForecastTable &t : tab.tables)
                for (const auto &row : t.rows)
                    if (!found && row.labels.contains(pair)) {
                        found = &row;
                        table = &t;
                    }
        if (!instrument || !found) {
            m_priceTitle->setText(tr("%1: no price for this pair in the Desk's markets").arg(pair));
            m_price->setData({}, {}, {});
            return;
        }
        QVector<std::pair<QDate, double>> targets;
        for (int c = 0; c < found->values.size() && c < table->columns.size(); ++c) {
            bool good = false;
            const double v = found->values[c].toDouble(&good);
            const QDate end = desk::forecastPeriodEnd(table->columns[c]);
            if (good && end.isValid())
                targets.push_back({end, v});
        }
        const int serial = ++m_priceSerial;
        const QString symbol = instrument->symbol;
        deskdata::loadCandles({symbol}, "1y", "1d", this, [this, serial, symbol, targets](const deskdata::Candles &c, const QString &) {
            if (serial != m_priceSerial)
                return;
            const QVector<desk::Candle> bars = c.value(symbol);
            if (bars.isEmpty() || targets.isEmpty())
                return;
            QVector<QDate> dates;
            QVector<double> price, path;
            for (const desk::Candle &b : bars) {
                dates << QDateTime::fromSecsSinceEpoch(b.time, QTimeZone::UTC).date();
                price << b.close;
                path << std::nan("");
            }
            path.last() = price.last();
            // A straight line from the last close through each quarter's end, on weekdays like the prices.
            QDate from = dates.last();
            double level = price.last();
            for (const auto &target : targets) {
                const qint64 days = from.daysTo(target.first);
                if (days <= 0)
                    continue;
                for (QDate d = from.addDays(1); d <= target.first; d = d.addDays(1)) {
                    if (d.dayOfWeek() > 5)
                        continue;
                    dates << d;
                    price << std::nan("");
                    path << level + (target.second - level) * double(from.daysTo(d)) / double(days);
                }
                from = target.first;
                level = target.second;
            }
            const Palette &pal = Theme::current();
            const int decimals = priceDecimals(price.first());
            m_price->setData(dates, {{tr("Price"), pal.Text, price}, {tr("ING forecast"), pal.Accent, path}},
                             [decimals](double v) { return QLocale().toString(v, 'f', decimals); });
        });
    }

    void fillMacro()
    {
        const QVector<Economy> &economies = economyList();
        const int year = QDate::currentDate().year();
        m_macro->setRowCount(int(economies.size()));
        const auto figure = [](double v) { return std::isnan(v) ? QStringLiteral("—") : amount(v, 1); };
        const auto imf = [&](const char *iso, WorldEconomy::Indicator ind, int y) {
            return m_context.economy ? m_context.economy->value(iso, ind, y) : std::nan("");
        };
        const auto oecd = [&](const char *area, const char *measure, int y) {
            return m_oecd.value(QString("%1/%2/%3").arg(area, measure).arg(y), std::nan(""));
        };
        int r = 0;
        for (const auto &e : economies) {
            m_macro->setItem(r, 0, cell(tr(e.name), {}, Qt::AlignLeft, true));
            m_macro->setItem(r, 1, cell(figure(imf(e.imf, WorldEconomy::GdpGrowth, year))));
            m_macro->setItem(r, 2, cell(figure(imf(e.imf, WorldEconomy::GdpGrowth, year + 1))));
            m_macro->setItem(r, 3, cell(figure(oecd(e.oecd, "GDPV_ANNPCT", year))));
            m_macro->setItem(r, 4, cell(figure(oecd(e.oecd, "GDPV_ANNPCT", year + 1))));
            m_macro->setItem(r, 5, cell(figure(imf(e.imf, WorldEconomy::Inflation, year))));
            m_macro->setItem(r, 6, cell(figure(imf(e.imf, WorldEconomy::Inflation, year + 1))));
            m_macro->setItem(r, 7, cell(figure(imf(e.imf, WorldEconomy::Unemployment, year + 1))));
            m_macro->setItem(r, 8, cell(figure(oecd(e.oecd, "UNR", year + 1))));
            ++r;
        }
        fitColumns(m_macro);
        m_macro->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);   // names stay whole
        fitHeight(m_macro);
        showEconomy(m_economyIndex);
    }

    void fetchRanges()
    {
        research::Client *client = research::Client::instance();
        const int year = QDate::currentDate().year();
        client->raw("ecb:spf", QUrl(QString("https://data-api.ecb.europa.eu/service/data/SPF/Q.U2.HICP+RGDP+UNEM.POINT.%1+%2+%3.Q.?lastNObservations=1&detail=dataonly&format=csvdata").arg(year).arg(year + 1).arg(year + 2)), 24 * 3600, false, this,
                    [this](const QByteArray &body, const QString &) {
            m_euroRanges = desk::parseEcbSpf(QString::fromUtf8(body));
            fillRanges();
        });
        for (const char *variable : {"CPI", "RGDP", "UNEMP"}) {
            const QUrl url(QString("https://www.philadelphiafed.org/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/Individual_%1.xlsx").arg(variable));
            client->raw(QString("philly:spf:") + variable, url, 24 * 3600, false, this, [this, v = QString(variable)](const QByteArray &xlsx, const QString &) {
                m_usRanges.insert(v, desk::parsePhillyIndividual(unzipFile(xlsx, "xl/worksheets/sheet1.xml"), unzipFile(xlsx, "xl/sharedStrings.xml"), v));
                fillRanges();
            });
        }
    }

    void fillRanges()
    {
        const Palette &pal = Theme::current();
        const int year = QDate::currentDate().year();
        const QHash<QString, QString> names {{"CPI", tr("Inflation")}, {"GDP", tr("Real GDP growth")}, {"UNEMP", tr("Unemployment")}};
        m_range->setRowCount(0);
        const auto add = [&](const QString &economy, const QVector<desk::ForecastRange> &list) {
            for (const desk::ForecastRange &f : list) {
                if (f.year < year || f.year > year + 2 || !names.contains(f.topic) || f.count < 5)
                    continue;
                const int r = m_range->rowCount();
                m_range->insertRow(r);
                m_range->setItem(r, 0, cell(economy, {}, Qt::AlignLeft, true));
                m_range->setItem(r, 1, cell(names.value(f.topic), {}, Qt::AlignLeft));
                m_range->setItem(r, 2, cell(QString::number(f.year)));
                m_range->setItem(r, 3, cell(QString("%1%").arg(amount(f.low, 1))));
                m_range->setItem(r, 4, cell(QString("%1%").arg(amount(f.median, 1)), {}, Qt::AlignCenter, true));
                m_range->setItem(r, 5, cell(QString("%1%").arg(amount(f.high, 1))));
                m_range->setItem(r, 6, cell(QString::number(f.count), pal.TextDim));
                m_range->setItem(r, 7, cell(f.survey, pal.TextDim));
            }
        };
        add(tr("Euro area"), m_euroRanges);
        for (const char *v : {"CPI", "RGDP", "UNEMP"})
            add(tr("United States"), m_usRanges.value(v));
        fitHeight(m_range);
    }

    void fetchFed()
    {
        const QString key = sourceKey("fred");
        const struct { const char *series, *name; } measures[] = {{"FEDTARMD", QT_TR_NOOP("Fed funds rate")}, {"GDPC1MD", QT_TR_NOOP("Real GDP growth")},
                                                                  {"UNRATEMD", QT_TR_NOOP("Unemployment")}, {"PCECTPIMD", QT_TR_NOOP("PCE inflation")}};
        m_fed->setRowCount(int(std::size(measures)));
        const int year = QDate::currentDate().year();
        int r = 0;
        for (const auto &m : measures) {
            m_fed->setItem(r, 0, cell(tr(m.name), {}, Qt::AlignLeft, true));
            for (int c = 1; c <= 3; ++c)
                m_fed->setItem(r, c, cell("—", Theme::current().TextDim));
            ++r;
        }
        fitColumns(m_fed, 0);
        fitHeight(m_fed);
        if (key.isEmpty()) {
            m_fedNote->setText(tr("The FOMC's Summary of Economic Projections needs a free FRED key, in Sources."));
            return;
        }
        r = 0;
        for (const auto &m : measures) {
            QUrl url("https://api.stlouisfed.org/fred/series/observations");
            url.setQuery(QString("series_id=%1&api_key=%2&file_type=json&sort_order=desc&limit=8").arg(m.series, key));
            research::Client::instance()->raw(QString("fred:sep:") + m.series, url, 12 * 3600, true, this, [this, r, year](const QByteArray &body, const QString &) {
                for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("observations").toArray()) {
                    const QJsonObject o = v.toObject();
                    const int y = o.value("date").toString().left(4).toInt();
                    bool good = false;
                    const double value = o.value("value").toString().toDouble(&good);
                    if (good && y >= year && y <= year + 2)
                        m_fed->setItem(r, 1 + y - year, cell(QString("%1%").arg(amount(value, 1))));
                }
            });
            ++r;
        }
    }

    Context m_context;
    QComboBox *m_tab;
    QLabel *m_updated, *m_fedNote;
    QTableWidget *m_ing, *m_macro, *m_fed;
    QTableWidget *m_range;
    QVector<desk::ForecastRange> m_euroRanges;
    QHash<QString, QVector<desk::ForecastRange>> m_usRanges;
    QFrame *m_priceCard;
    QLabel *m_priceTitle, *m_macroTitle;
    quant::MultiLineChart *m_price, *m_macroChart;
    QButtonGroup *m_macroMeasure = nullptr;
    int m_economyIndex = 0, m_priceSerial = 0;
    QVector<desk::ForecastTab> m_tabs;
    QHash<QString, double> m_oecd;
    bool m_filling = false;
};

// ------------------------------------------------------------------ Risk appetite

class AppetiteView : public quant::View
{
    Q_OBJECT
public:
    explicit AppetiteView(QWidget *parent = nullptr)
        : View(parent)
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        auto *left = new QVBoxLayout;
        left->setSpacing(10);
        QVBoxLayout *body;
        auto *dialCard = quant::card(tr("Composite risk appetite"), &body);
        m_dial = new GaugeDial;
        m_dial->setMinimumHeight(250);
        body->addWidget(m_dial);
        m_change = quant::label({}, "qtCardLabel");
        m_change->setAlignment(Qt::AlignCenter);
        body->addWidget(m_change);
        left->addWidget(dialCard);
        // The index traders quote is CNN's Fear & Greed, a narrower one: its estimate here, so the two aren't mistaken.
        auto *fgCard = quant::card(tr("Stock market Fear & Greed (estimate)"), &body);
        m_fearGreed = quant::label(tr("Loading…"), "qtCardLabel");
        m_fearGreed->setWordWrap(true);
        body->addWidget(m_fearGreed);
        left->addWidget(fgCard);
        auto *historyCard = quant::card(tr("Composite, last year"), &body);
        m_history = new quant::MultiLineChart;
        m_history->setMinimumHeight(220);
        m_history->setZeroLine(false);
        body->addWidget(m_history);
        left->addWidget(historyCard);
        row->addLayout(left, 1);
        auto *compCard = quant::card(tr("Components: percentile against their own past year"), &body);
        m_components = table({tr("Component"), tr("Measure"), tr("Percentile"), tr("What it reads")});
        fitColumns(m_components, 3);
        body->addWidget(m_components);
        body->addStretch(1);
        row->addWidget(compCard, 1);
        content()->addLayout(row);
        auto *note = quant::label(tr("Seven markets, daily closes, each ranked against its last 252 days and averaged. Under 40 is risk-off, over 60 risk-on. This app's own composite: an estimate, not a published index."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
    }

protected:
    void load() override
    {
        setStatus(tr("Loading seven markets…"));
        economy::stockFearGreed(this, [this](const economy::FearGreed &f, const QString &error) {
            if (f.index.isEmpty()) {
                m_fearGreed->setText(tr("Unavailable: %1").arg(error));
                return;
            }
            const double v = f.index.last().value;
            m_fearGreed->setText(tr("<b>%1</b>, %2. Built the way CNN builds the index that financial sites quote (momentum, volatility, "
                                    "safe haven demand, breadth, junk bonds against their last year); not CNN's own figure. The gauge above "
                                    "is another index, broader: credit, currencies, metals and crypto too, so the two can differ.")
                                     .arg(amount(v, 0), economy::zoneLabel(economy::Zones::FearGreed, v).toLower()));
        });
        deskdata::loadCandles(symbols(), "2y", "1d", this, [this](const deskdata::Candles &c, const QString &error) {
            compute(c, error);
        });
    }

public:
    static QStringList symbols() { return {"^VIX", "^GSPC", "HYG", "LQD", "AUDJPY=X", "HG=F", "GC=F", "BTC-USD"}; }

    // The composite from the candles of symbols(); also read by the strip on Overview and Forex.
    static desk::Appetite composite(const deskdata::Candles &c)
    {
        const QVector<desk::Candle> spx = c.value("^GSPC");
        if (spx.size() < 300)
            return {};
        // Every market on the S&P 500's days, the last close known carried forward.
        QVector<QDate> dates;
        const auto dayOf = [](qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date(); };
        for (const desk::Candle &b : spx)
            dates << dayOf(b.time);
        const auto aligned = [&](const QString &symbol) {
            QVector<double> out(dates.size(), std::nan(""));
            const QVector<desk::Candle> bars = c.value(symbol);
            int k = 0;
            double last = std::nan("");
            for (int i = 0; i < dates.size(); ++i) {
                while (k < bars.size() && dayOf(bars[k].time) <= dates[i])
                    last = bars[k++].close;
                out[i] = last;
            }
            return out;
        };
        const int n = dates.size();
        const auto change = [&](const QVector<double> &v, int days) {
            QVector<double> out(n, std::nan(""));
            for (int i = days; i < n; ++i)
                if (v[i - days] > 0)
                    out[i] = (v[i] / v[i - days] - 1) * 100.0;
            return out;
        };
        const auto ratio = [&](const QVector<double> &a, const QVector<double> &b) {
            QVector<double> out(n, std::nan(""));
            for (int i = 0; i < n; ++i)
                if (b[i] > 0)
                    out[i] = a[i] / b[i];
            return out;
        };
        const QVector<double> close = aligned("^GSPC"), gold = aligned("GC=F");
        QVector<double> trend(n, std::nan(""));
        for (int i = 125; i < n; ++i) {
            double sum = 0;
            for (int k = i - 124; k <= i; ++k)
                sum += close[k];
            trend[i] = (close[i] / (sum / 125) - 1) * 100.0;
        }
        const QVector<double> spx20 = change(close, 20), gold20 = change(gold, 20);
        QVector<double> stocksGold(n, std::nan(""));
        for (int i = 0; i < n; ++i)
            stocksGold[i] = spx20[i] - gold20[i];
        QVector<desk::AppetiteComponent> comps {
            {"vix", tr("Volatility (VIX)"), tr("Lower volatility, more appetite"), aligned("^VIX"), true},
            {"trend", tr("Equity trend"), tr("S&P 500 against its 125-day average"), trend},
            {"credit", tr("Credit appetite"), tr("High yield against investment grade bonds, 20-day change"), change(ratio(aligned("HYG"), aligned("LQD")), 20)},
            {"audjpy", tr("AUD/JPY"), tr("Australian dollar against the yen, 20-day change"), change(aligned("AUDJPY=X"), 20)},
            {"copper", tr("Copper / Gold"), tr("Growth against safety, 20-day change of the ratio"), change(ratio(aligned("HG=F"), gold), 20)},
            {"stocks", tr("Stocks vs safe haven"), tr("S&P 500's 20-day return minus gold's"), stocksGold},
            {"crypto", tr("Crypto momentum"), tr("Bitcoin's 20-day change"), change(aligned("BTC-USD"), 20)},
        };
        return desk::computeAppetite(dates, comps);
    }

private:
    void compute(const deskdata::Candles &c, const QString &error)
    {
        if (c.value("^GSPC").size() < 300) {
            setStatus(tr("Prices unavailable: %1").arg(error));
            return;
        }
        const desk::Appetite a = composite(c);
        if (!a.valid) {
            setStatus(tr("Not enough history yet."));
            return;
        }
        setStatus({});
        const Palette &pal = Theme::current();
        const QStringList zones {tr("EXTREME RISK-OFF"), tr("RISK-OFF"), tr("NEUTRAL"), tr("RISK-ON"), tr("EXTREME RISK-ON")};
        m_dial->setValue(a.now, zones[int(desk::zoneOf(a.now))]);
        m_change->setText(tr("1 week ago %1 · 1 month ago %2").arg(amount(a.weekAgo, 0), amount(a.monthAgo, 0)));
        const int keep = std::min<int>(252, a.history.size());
        m_history->setData(a.dates.mid(a.dates.size() - keep), {{tr("Composite"), pal.Accent, a.history.mid(a.history.size() - keep)}}, [](double v) { return amount(v, 0); });
        m_components->setRowCount(a.components.size());
        for (int r = 0; r < a.components.size(); ++r) {
            const desk::AppetiteComponent &comp = a.components[r];
            m_components->setItem(r, 0, cell(comp.name, {}, Qt::AlignLeft, true));
            m_components->setItem(r, 1, cell(comp.id == "vix" ? amount(comp.value) : comp.id == "stocks" ? tr("%1 pts").arg(signedAmount(comp.value)) : pct(comp.value)));
            m_components->setItem(r, 2, cell(amount(comp.percentile, 0), comp.percentile < 40 ? pal.Down : comp.percentile > 60 ? pal.Up : pal.Text, Qt::AlignCenter, true));
            m_components->setItem(r, 3, cell(comp.note, pal.TextDim, Qt::AlignLeft));
        }
        fitHeight(m_components);
    }

    GaugeDial *m_dial;
    QLabel *m_change;
    QLabel *m_fearGreed;
    quant::MultiLineChart *m_history;
    QTableWidget *m_components;
};

// ------------------------------------------------------------------ Seasonality

// The year's path, on which dragging picks the months of the period.
class SeasonalPathChart : public quant::MultiLineChart
{
    Q_OBJECT
public:
    using MultiLineChart::MultiLineChart;
    void setHighlight(int firstMonth, int lastMonth)
    {
        m_first = firstMonth;
        m_last = lastMonth;
        update();
    }

signals:
    void monthsPicked(int firstMonth, int lastMonth);

protected:
    void paint(QPainter &p, const QRectF &plot) override
    {
        MultiLineChart::paint(p, plot);
        const int from = m_dragging ? std::min(m_anchor, m_current) : m_first;
        const int to = m_dragging ? std::max(m_anchor, m_current) : m_last;
        if (from <= 0 || (from == 1 && to == 12))
            return;
        QColor shade = Theme::current().Accent;
        shade.setAlpha(36);
        const double w = plot.width() / 12.0;
        p.fillRect(QRectF(plot.left() + (from - 1) * w, plot.top(), (to - from + 1) * w, plot.height()), shade);
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        m_dragging = true;
        m_anchor = m_current = monthAt(e->position().x());
        update();
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_dragging)
            m_current = monthAt(e->position().x());
        MultiLineChart::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (!m_dragging)
            return;
        m_dragging = false;
        m_current = monthAt(e->position().x());
        emit monthsPicked(std::min(m_anchor, m_current), std::max(m_anchor, m_current));
    }

private:
    int monthAt(double x) const
    {
        const QRectF plot = plotRect();
        return std::clamp(int((x - plot.left()) / plot.width() * 12.0) + 1, 1, 12);
    }

    bool m_dragging = false;
    int m_anchor = 0, m_current = 0, m_first = 0, m_last = 0;
};

class SeasonalityView : public quant::View
{
    Q_OBJECT
public:
    explicit SeasonalityView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        m_instrument = deskCombo();
        for (const desk::Instrument &i : desk::instruments())
            m_instrument->addItem(i.label, i.id);
        m_instrument->setCurrentIndex(std::max(0, m_instrument->findData(QSettings().value("desk/seasonal/instrument", "XAUUSD"))));
        h->addWidget(m_instrument);
        h->addWidget(quant::label(tr("YEARS"), "qtFieldLabel"));
        h->addWidget(quant::segments({"5", "10", "15", "20"}, &m_years, 1));
        h->addWidget(quant::label(tr("PERIOD"), "qtFieldLabel"));
        h->addWidget(quant::segments({tr("Full"), "H1", "H2", "Q1", "Q2", "Q3", "Q4"}, &m_period));
        m_outliers = new QCheckBox(tr("Leave out extreme years (beyond 1.5σ)"));
        m_outliers->setObjectName("qtCheck");
        h->addWidget(m_outliers);
        h->addStretch(1);
        content()->addWidget(top);

        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Win rate"), &m_win, &m_winSub), 1);
        tiles->addWidget(tile(tr("Mean return of the period"), &m_mean, &m_meanSub), 1);
        tiles->addWidget(tile(tr("Best month"), &m_best, &m_bestSub), 1);
        tiles->addWidget(tile(tr("Worst month"), &m_worst, &m_worstSub), 1);
        tiles->addWidget(tile(tr("Volatility ratio"), &m_vol, &m_volSub), 1);
        content()->addLayout(tiles);

        QVBoxLayout *body;
        auto *pathCard = quant::card(tr("Average path of the year and this year, % from 1 January"), &body);
        m_path = new SeasonalPathChart;
        m_path->setToolTip(tr("Drag across the months to study them"));
        m_path->setCursor(Qt::CrossCursor);
        connect(m_path, &SeasonalPathChart::monthsPicked, this, [this](int first, int last) {
            m_customFirst = first;
            m_customLast = last;
            // A period of its own: no segment stands for it.
            m_period->setExclusive(false);
            if (QAbstractButton *b = m_period->checkedButton())
                b->setChecked(false);
            m_period->setExclusive(true);
            compute();
        });
        m_path->setMinimumHeight(290);
        m_path->setDateFormat("MMM");
        m_path->setLegend(true);
        body->addWidget(m_path);
        content()->addWidget(pathCard);
        auto *low = new QHBoxLayout;
        low->setSpacing(10);
        auto *monthCard = quant::card(tr("Mean return by month"), &body);
        m_months = new BarsChart;
        body->addWidget(m_months);
        low->addWidget(monthCard, 2);
        auto *dayCard = quant::card(tr("Mean daily return by weekday, in the period"), &body);
        m_weekdays = new BarsChart;
        body->addWidget(m_weekdays);
        low->addWidget(dayCard, 1);
        auto *yearCard = quant::card(tr("The period, year by year"), &body);
        m_yearBars = new BarsChart;
        body->addWidget(m_yearBars);
        low->addWidget(yearCard, 2);
        content()->addLayout(low);
        auto *note = quant::label(tr("Daily closes from Yahoo Finance; the current year is left out of the averages. History shorter than asked for is used as it is. Past seasonality doesn't predict a given year."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);

        connect(m_instrument, &QComboBox::currentIndexChanged, this, &SeasonalityView::fetch);
        connect(m_years, &QButtonGroup::idClicked, this, &SeasonalityView::compute);
        connect(m_period, &QButtonGroup::idClicked, this, [this] {
            m_customFirst = 0;
            compute();
        });
        connect(m_outliers, &QCheckBox::toggled, this, &SeasonalityView::compute);
    }

protected:
    void load() override { fetch(); }

private:
    void fetch()
    {
        const desk::Instrument *i = desk::findInstrument(m_instrument->currentData().toString());
        if (!i)
            return;
        QSettings().setValue("desk/seasonal/instrument", i->id);
        setStatus(tr("Loading %1's history…").arg(i->label));
        const int serial = ++m_serial;
        // Not "max": over that range Yahoo answers monthly bars whatever the interval asked for.
        deskdata::loadCandles({i->symbol}, "25y", "1d", this, [this, serial, symbol = i->symbol](const deskdata::Candles &c, const QString &error) {
            if (serial != m_serial)
                return;
            m_bars = c.value(symbol);
            setStatus(m_bars.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
            compute();
        });
    }

    void compute()
    {
        const Palette &pal = Theme::current();
        const int yearsList[] = {5, 10, 15, 20};
        const int firstMonths[] = {1, 1, 7, 1, 4, 7, 10}, lastMonths[] = {12, 6, 12, 3, 6, 9, 12};
        desk::SeasonalOptions o;
        o.years = yearsList[m_years->checkedId()];
        const int preset = std::max(0, m_period->checkedId());
        o.firstMonth = m_customFirst > 0 ? m_customFirst : firstMonths[preset];
        o.lastMonth = m_customFirst > 0 ? m_customLast : lastMonths[preset];
        m_path->setHighlight(o.firstMonth, o.lastMonth);
        o.excludeOutliers = m_outliers->isChecked();
        const int year = QDate::currentDate().year();
        const desk::Seasonal s = desk::computeSeasonal(m_bars, o, year);
        if (!s.valid) {
            if (!m_bars.isEmpty())
                setStatus(tr("Not enough history for this period."));
            return;
        }
        const QLocale loc;
        int used = 0;
        for (const desk::SeasonalYear &y : s.years)
            used += !y.excluded;
        m_win->setText(QString("%1%").arg(amount(s.winRate, 0)));
        m_winSub->setText(tr("%1 up, %2 down, over %3 years").arg(s.positive).arg(s.negative).arg(used));
        setSigned(m_mean, pct(s.meanReturn), s.meanReturn);
        m_meanSub->setText(tr("compounded over the period"));
        m_best->setText(loc.standaloneMonthName(s.bestMonth + 1, QLocale::ShortFormat));
        m_bestSub->setText(pct(s.byMonth[s.bestMonth]));
        m_worst->setText(loc.standaloneMonthName(s.worstMonth + 1, QLocale::ShortFormat));
        m_worstSub->setText(pct(s.byMonth[s.worstMonth]));
        m_vol->setText(amount(s.volatilityRatio));
        m_volSub->setText(tr("the period's daily volatility over the whole year's"));

        QVector<QDate> dates;
        for (int d = 0; d < 366; ++d)
            dates << QDate(2024, 1, 1).addDays(d);
        // This year so far, on the same axis.
        QVector<double> now(366, std::nan(""));
        double base = 0;
        for (const desk::Candle &b : m_bars) {
            const QDate d = QDateTime::fromSecsSinceEpoch(b.time, QTimeZone::UTC).date();
            if (d.year() == year - 1)
                base = b.close;
            if (d.year() == year && base > 0)
                now[std::min(d.dayOfYear(), 366) - 1] = (b.close / base - 1) * 100.0;
        }
        QVector<double> filled = now;
        double last = std::nan("");
        const int today = QDate::currentDate().dayOfYear();
        for (int d = 0; d < 366; ++d) {
            if (!std::isnan(filled[d]))
                last = filled[d];
            else if (d < today)
                filled[d] = last;
        }
        m_path->setData(dates, {{tr("Average, %1 years").arg(used), pal.Accent, s.path}, {QString::number(year), pal.Text, filled}}, [](double v) { return pct(v); });

        QStringList months;
        QVector<double> byMonth;
        for (int m = 0; m < 12; ++m) {
            months << loc.standaloneMonthName(m + 1, QLocale::ShortFormat);
            byMonth << s.byMonth[m];
        }
        m_months->setData(months, byMonth, [](double v) { return pct(v); });
        QStringList days;
        QVector<double> byDay;
        for (int d = 0; d < 5; ++d) {
            days << loc.standaloneDayName(d + 1, QLocale::ShortFormat);
            byDay << s.byWeekday[d];
        }
        m_weekdays->setData(days, byDay, [](double v) { return pct(v, 3); });
        QStringList yearsText;
        QVector<double> byYear;
        for (const desk::SeasonalYear &y : s.years)
            if (!y.excluded) {
                yearsText << QString::number(y.year);
                byYear << y.periodReturn;
            }
        m_yearBars->setData(yearsText, byYear, [](double v) { return pct(v, 1); });
    }

    QComboBox *m_instrument;
    QButtonGroup *m_years, *m_period;
    QCheckBox *m_outliers;
    QLabel *m_win, *m_winSub, *m_mean, *m_meanSub, *m_best, *m_bestSub, *m_worst, *m_worstSub, *m_vol, *m_volSub;
    SeasonalPathChart *m_path = nullptr;
    int m_customFirst = 0, m_customLast = 0;
    BarsChart *m_months, *m_weekdays, *m_yearBars;
    QVector<desk::Candle> m_bars;
    int m_serial = 0;
};

// ------------------------------------------------------------------ Fundamentals

class FundamentalsView : public quant::View
{
    Q_OBJECT
public:
    explicit FundamentalsView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
    {
        QVBoxLayout *body;
        QHBoxLayout *head;
        auto *mapCard = quant::card(tr("Currencies on their fundamentals, strongest first"), &body, &head);
        head->addWidget(askAiButton(c, tr("ASK THE AI TO READ IT"),
                                    tr("Use read_page on the Desk page, section Fundamentals, then say which currencies the table favours and why, in a few sentences. Use only what the page shows and say what is missing.")));
        m_map = table({tr("Currency"), tr("Score"), tr("Bias"), tr("Rates"), tr("Inflation"), tr("Growth"), tr("Labour"), tr("Activity"), tr("Sentiment")});
        body->addWidget(m_map);
        auto *method = quant::label(tr("From each economy's latest releases on the economic calendar and the BIS's policy rates. Each theme reads strong, neutral or weak by fixed rules (a hike, inflation above 2.5%, growth above 0.3%, unemployment falling, a PMI above 50.5, confidence rising are strong) and the score is their mean, from −100 to +100. The bias compares it with the mean of the eight currencies. This app's own simple reading: not a forecast, and a theme without a recent release is left out."), "qtNote");
        method->setWordWrap(true);
        body->addWidget(method);
        content()->addWidget(mapCard);

        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        auto *compareCard = quant::card(tr("Pair comparator"), &body);
        auto *pick = new QHBoxLayout;
        m_a = deskCombo();
        m_b = deskCombo();
        for (const QString &cur : desk::majorCurrencies()) {
            m_a->addItem(cur);
            m_b->addItem(cur);
        }
        m_a->setCurrentText("EUR");
        m_b->setCurrentText("USD");
        pick->addWidget(m_a);
        pick->addWidget(quant::label("/", "qtCardValue"));
        pick->addWidget(m_b);
        pick->addStretch(1);
        body->addLayout(pick);
        m_verdict = quant::label("—", "qtMidValue");
        m_verdict->setWordWrap(true);
        body->addWidget(m_verdict);
        m_pair = table({tr("Theme"), "A", "B", tr("Difference")});
        body->addWidget(m_pair);
        row->addWidget(compareCard, 1);
        auto *fileCard = quant::card(tr("Latest figures"), &body);
        m_figures = table({tr("Currency"), tr("Policy rate"), tr("CPI"), tr("GDP"), tr("Unemployment"), tr("PMI"), tr("Confidence")});
        body->addWidget(m_figures);
        body->addStretch(1);
        row->addWidget(fileCard, 2);
        content()->addLayout(row);
        content()->addStretch(1);
        connect(m_a, &QComboBox::currentIndexChanged, this, &FundamentalsView::comparePair);
        connect(m_b, &QComboBox::currentIndexChanged, this, &FundamentalsView::comparePair);
        if (c.rates)
            connect(c.rates, &RemoteDataSet::updated, this, &FundamentalsView::score);
    }

protected:
    void load() override
    {
        if (!m_context.calendar)
            return;
        setStatus(tr("Loading five months of releases…"));
        const QDate today = QDate::currentDate();
        if (m_context.rates)
            m_context.rates->refreshIfStale();
        m_context.calendar->fetchRange(today.addDays(-150), today.addDays(1), this, [this](const QVector<EconomicCalendar::Event> &events, const QString &error) {
            m_events = events;
            setStatus(events.isEmpty() ? tr("The calendar is unavailable: %1").arg(error) : QString());
            score();
        });
    }

private:
    // The latest release of a country whose title matches; false when there is none.
    bool latest(const QString &country, const QRegularExpression &pattern, double *value, double *previous) const
    {
        QDateTime best;
        for (const EconomicCalendar::Event &e : m_events) {
            if (e.country != country || std::isnan(e.actual) || !pattern.match(e.title).hasMatch())
                continue;
            if (!best.isValid() || e.time > best) {
                best = e.time;
                *value = e.actual;
                *previous = e.previous;
            }
        }
        return best.isValid();
    }

    void score()
    {
        if (m_events.isEmpty())
            return;
        const QHash<QString, QString> countries {{"USD", "US"}, {"EUR", "EU"}, {"JPY", "JP"}, {"GBP", "GB"}, {"CHF", "CH"}, {"CAD", "CA"}, {"AUD", "AU"}, {"NZD", "NZ"}};
        const auto rx = [](const char *p) { return QRegularExpression(p, QRegularExpression::CaseInsensitiveOption); };
        const QRegularExpression cpi = rx("^(?:Inflation Rate|CPI) YoY|^Inflation Rate$"), gdp = rx("^GDP Growth Rate QoQ"), jobless = rx("^Unemployment Rate"),
                                 composite = rx("Composite PMI"), manufacturing = rx("Manufacturing PMI|^ISM Manufacturing PMI"),
                                 confidence = rx("Consumer Confidence|Consumer Sentiment|Consumer Climate|Westpac");
        QVector<desk::Macro> macros;
        for (const QString &cur : desk::majorCurrencies()) {
            const QString country = countries.value(cur);
            desk::Macro m;
            m.currency = cur;
            if (m_context.rates && m_context.rates->hasRate(cur)) {
                m.policyRate = m_context.rates->current(cur);
                QDate when;
                double delta = 0;
                m.policyChange = m_context.rates->lastChange(cur, &when, &delta) && when.daysTo(QDate::currentDate()) < 200 ? delta : 0;
            }
            latest(country, cpi, &m.cpi, &m.cpiPrevious);
            latest(country, gdp, &m.gdp, &m.gdpPrevious);
            latest(country, jobless, &m.unemployment, &m.unemploymentPrevious);
            if (!latest(country, composite, &m.pmi, &m.pmiPrevious))
                latest(country, manufacturing, &m.pmi, &m.pmiPrevious);
            latest(country, confidence, &m.confidence, &m.confidencePrevious);
            macros << m;
        }
        m_macros = macros;
        m_scores = desk::scoreAll(macros);
        fill();
        comparePair();
    }

    QString keyName(const QString &key) const
    {
        static const QHash<QString, const char *> names {
            {"hike", QT_TR_NOOP("Hike")}, {"hold", QT_TR_NOOP("Hold")}, {"cut", QT_TR_NOOP("Cut")}, {"above_target", QT_TR_NOOP("Above target")},
            {"near_target", QT_TR_NOOP("Near target")}, {"below_target", QT_TR_NOOP("Below target")}, {"expanding", QT_TR_NOOP("Expanding")},
            {"flat", QT_TR_NOOP("Flat")}, {"contracting", QT_TR_NOOP("Contracting")}, {"tight", QT_TR_NOOP("Tightening")}, {"stable", QT_TR_NOOP("Stable")},
            {"softening", QT_TR_NOOP("Softening")}, {"expansion", QT_TR_NOOP("Expansion")}, {"contraction", QT_TR_NOOP("Contraction")},
            {"improving", QT_TR_NOOP("Improving")}, {"weakening", QT_TR_NOOP("Weakening")}};
        return tr(names.value(key, ""));
    }

    void fill()
    {
        const Palette &pal = Theme::current();
        m_map->setRowCount(m_scores.size());
        for (int r = 0; r < m_scores.size(); ++r) {
            const desk::Score &s = m_scores[r];
            m_map->setItem(r, 0, cell(s.currency, {}, Qt::AlignCenter, true));
            m_map->setItem(r, 1, cell(s.covered ? QString("%1%2").arg(s.score > 0 ? "+" : "").arg(s.score) : "—", s.score > 0 ? pal.Up : s.score < 0 ? pal.Down : pal.Text, Qt::AlignCenter, true));
            m_map->setItem(r, 2, cell(s.bias == desk::Score::Bullish ? tr("BULLISH") : s.bias == desk::Score::Bearish ? tr("BEARISH") : tr("NEUTRAL"),
                                      s.bias == desk::Score::Bullish ? pal.Up : s.bias == desk::Score::Bearish ? pal.Down : pal.TextDim));
            for (int t = 0; t < int(desk::FundTheme::Count); ++t) {
                const desk::Cell &c = s.cells[t];
                auto *item = cell(c.reading == desk::Reading::None ? "—" : QString("%1\n%2").arg(keyName(c.key), std::isnan(c.value) ? QString() : amount(c.value)),
                                  c.reading == desk::Reading::None ? pal.TextFaint : pal.Text);
                if (c.reading == desk::Reading::Strong || c.reading == desk::Reading::Weak) {
                    QColor tint = c.reading == desk::Reading::Strong ? pal.Up : pal.Down;
                    tint.setAlpha(55);
                    item->setBackground(tint);
                }
                m_map->setItem(r, 3 + t, item);
            }
            m_map->setRowHeight(r, 40);
        }
        int h = m_map->horizontalHeader()->sizeHint().height() + 2;
        for (int r = 0; r < m_map->rowCount(); ++r)
            h += m_map->rowHeight(r);
        m_map->setFixedHeight(h);

        m_figures->setRowCount(m_macros.size());
        for (int r = 0; r < m_macros.size(); ++r) {
            const desk::Macro &m = m_macros[r];
            const auto figure = [](double v) { return std::isnan(v) ? QString("—") : amount(v); };
            m_figures->setItem(r, 0, cell(m.currency, {}, Qt::AlignCenter, true));
            m_figures->setItem(r, 1, cell(figure(m.policyRate)));
            m_figures->setItem(r, 2, cell(figure(m.cpi)));
            m_figures->setItem(r, 3, cell(figure(m.gdp)));
            m_figures->setItem(r, 4, cell(figure(m.unemployment)));
            m_figures->setItem(r, 5, cell(figure(m.pmi)));
            m_figures->setItem(r, 6, cell(figure(m.confidence)));
        }
        fitHeight(m_figures);
    }

    void comparePair()
    {
        if (m_scores.isEmpty())
            return;
        const Palette &pal = Theme::current();
        const auto find = [&](const QString &cur) -> const desk::Score & {
            for (const desk::Score &s : m_scores)
                if (s.currency == cur)
                    return s;
            return m_scores.first();
        };
        const desk::Score &a = find(m_a->currentText()), &b = find(m_b->currentText());
        const desk::PairVerdict v = desk::comparePair(a, b);
        m_verdict->setText(v.favoured.isEmpty() ? tr("%1/%2: too close to call on these figures").arg(a.currency, b.currency)
                                                : tr("%1/%2: the figures favour %3, %4 to %5 on the themes").arg(a.currency, b.currency, v.favoured).arg(std::max(v.leadsA, v.leadsB)).arg(std::min(v.leadsA, v.leadsB)));
        m_pair->setHorizontalHeaderLabels({tr("Theme"), a.currency, b.currency, tr("Difference")});
        const QStringList themes {tr("Rates"), tr("Inflation"), tr("Growth"), tr("Labour"), tr("Activity"), tr("Sentiment")};
        m_pair->setRowCount(themes.size());
        for (int t = 0; t < themes.size(); ++t) {
            const desk::Cell &ca = a.cells[t], &cb = b.cells[t];
            const double diff = ca.value - cb.value;
            m_pair->setItem(t, 0, cell(themes[t], {}, Qt::AlignLeft, true));
            m_pair->setItem(t, 1, cell(std::isnan(ca.value) ? "—" : amount(ca.value)));
            m_pair->setItem(t, 2, cell(std::isnan(cb.value) ? "—" : amount(cb.value)));
            m_pair->setItem(t, 3, cell(std::isnan(diff) ? "—" : signedAmount(diff), std::isnan(diff) ? pal.TextDim : diff >= 0 ? pal.Up : pal.Down));
        }
        fitHeight(m_pair);
    }

    Context m_context;
    QTableWidget *m_map, *m_pair, *m_figures;
    QComboBox *m_a, *m_b;
    QLabel *m_verdict;
    QVector<EconomicCalendar::Event> m_events;
    QVector<desk::Macro> m_macros;
    QVector<desk::Score> m_scores;
};

} // namespace

QWidget *makeRates(const Context &c) { return new RatesView(c); }
QWidget *makeForecasts(const Context &c) { return new ForecastsView(c); }
QWidget *makePositioning(const Context &) { return new PositioningView; }
QWidget *makeAppetite(const Context &) { return new AppetiteView; }
QWidget *makeSeasonality(const Context &) { return new SeasonalityView; }
QWidget *makeFundamentals(const Context &c) { return new FundamentalsView(c); }

} // namespace deskui

namespace deskui {
QStringList appetiteSymbols() { return AppetiteView::symbols(); }
desk::Appetite appetiteComposite(const deskdata::Candles &candles) { return AppetiteView::composite(candles); }
} // namespace deskui

#include "DeskViewsMacro.moc"
