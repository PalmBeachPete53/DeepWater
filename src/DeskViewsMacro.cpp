// The Desk's macro sections: Rates, Positioning, Risk appetite, Seasonality, Fundamentals.
#include "DeskViews.h"

#include "desk/Fundamentals.h"
#include "desk/Rates.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
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
        m_banks = table({tr("Bank"), tr("Rate"), tr("Last move"), tr("On"), tr("1 year"), tr("Next decision"), tr("In")});
        body->addWidget(m_banks);
        auto *banksNote = quant::label(tr("Policy rates from the BIS; decisions from the Fed's own calendar and the economic calendar, which only lists some of them ahead. No free source publishes rate futures for the other banks, so only the Fed has probabilities here."), "qtNote");
        banksNote->setWordWrap(true);
        body->addWidget(banksNote);
        low->addWidget(banksCard, 3);
        auto *yieldsCard = quant::card(tr("Sovereign yield curves"), &body);
        m_yields = new quant::MultiLineChart;
        m_yields->setMinimumHeight(230);
        m_yields->setZeroLine(false);
        m_yields->setLegend(true);
        body->addWidget(m_yields);
        auto *yieldsNote = quant::label(tr("US Treasury par yields and the ECB's AAA euro area spot curve, latest published."), "qtNote");
        body->addWidget(yieldsNote);
        low->addWidget(yieldsCard, 2);
        content()->addLayout(low);
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
    }

private:
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
        const QDate today = QDate::currentDate();
        QStringList symbols;
        QHash<QString, QDate> months;
        for (int i = 0; i < 15; ++i) {
            const QDate m = QDate(today.year(), today.month(), 1).addMonths(i);
            const QString symbol = desk::fedFutureSymbol(m);
            symbols << symbol;
            months.insert(symbol, m);
        }
        research::Client::instance()->raw("fed:fomc", QUrl("https://www.federalreserve.gov/monetarypolicy/fomccalendars.htm"), 24 * 3600, false, this,
                                          [this, symbols, months](const QByteArray &body, const QString &error) {
            const QVector<QDate> meetings = desk::parseFomcCalendar(QString::fromUtf8(body));
            if (meetings.isEmpty()) {
                setStatus(tr("The Fed's calendar is unavailable: %1").arg(error));
                return;
            }
            for (const QDate &m : meetings)
                if (m >= QDate::currentDate() && (!m_nextFomc.isValid() || m < m_nextFomc))
                    m_nextFomc = m;
            fillBanks();
            research::Client::instance()->quotes(symbols, 15 * 60, this, [this, meetings, months](const QHash<QString, QJsonObject> &q, const QString &err) {
                QVector<desk::FedContract> contracts;
                for (auto it = months.begin(); it != months.end(); ++it) {
                    const double price = research::raw(q.value(it.key()), "regularMarketPrice");
                    if (research::ok(price) && price > 50)
                        contracts.push_back({it.value(), price});
                }
                fillFed(desk::fedPath(contracts, meetings, QDate::currentDate()), err);
            });
        });
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
            m_banks->setItem(r, 5, cell(next.isValid() ? loc.toString(next.toLocalTime().date(), "d MMM yyyy") : "—"));
            m_banks->setItem(r, 6, cell(next.isValid() ? tr("%1 d").arg(now.daysTo(next)) : "—", pal.TextDim));
        }
        fitColumns(m_banks, 0);
        fitHeight(m_banks);
    }

    void fetchYields()
    {
        struct State {
            QHash<QString, double> us, euro;
            int left = 0;
        };
        const QStringList tenors {"3M", "1Y", "2Y", "5Y", "10Y", "20Y", "30Y"};
        const QHash<QString, QString> treasury {{"3M", "3 Mo"}, {"1Y", "1 Yr"}, {"2Y", "2 Yr"}, {"5Y", "5 Yr"}, {"10Y", "10 Yr"}, {"20Y", "20 Yr"}, {"30Y", "30 Yr"}};
        auto state = std::make_shared<State>();
        state->left = tenors.size() + 1;
        const auto finish = [this, state, tenors] {
            --state->left;   // each answer redraws: one slow source doesn't hide the other
            const Palette &pal = Theme::current();
            QVector<double> us, euro;
            for (const QString &t : tenors) {
                us << state->us.value(t, research::NaN);
                euro << state->euro.value(t, research::NaN);
            }
            QVector<QDate> dates(tenors.size(), QDate::currentDate());
            m_yields->setXLabels(tenors);
            m_yields->setData(dates, {{tr("United States"), pal.Accent, us}, {tr("Euro area"), pal.Up, euro}}, [](double v) { return QString("%1%").arg(amount(v)); });
        };
        research::Client::instance()->treasuryYields(this, [state, treasury, finish](const QVector<research::YieldCurveDay> &days, const QString &) {
            if (!days.isEmpty())
                for (auto it = treasury.begin(); it != treasury.end(); ++it)
                    if (days.first().yields.contains(it.value()))
                        state->us.insert(it.key(), days.first().yields.value(it.value()));
            finish();
        });
        for (const QString &t : tenors) {
            const QString url = QString("https://data-api.ecb.europa.eu/service/data/YC/B.U2.EUR.4F.G_N_A.SV_C_YM.SR_%1?lastNObservations=1&format=csvdata").arg(t);
            research::Client::instance()->raw("ecb:yc:" + t, QUrl(url), 6 * 3600, false, this, [state, t, finish](const QByteArray &body, const QString &) {
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
    }

    // The calendar's names for a policy decision differ by country ("Interest Rate", "Marginal Lending Rate"…).
    static bool isRateDecision(const QString &title)
    {
        return title == "Interest Rate" || title.contains("Interest Rate Decision", Qt::CaseInsensitive) || title.contains("Marginal Lending Rate", Qt::CaseInsensitive)
            || title.contains("Deposit Facility Rate", Qt::CaseInsensitive) || title.contains("Loan Prime Rate 1Y", Qt::CaseInsensitive);
    }

    Context m_context;
    QVector<EconomicCalendar::Event> m_decisions;
    QDate m_nextFomc;
    QLabel *m_current, *m_currentSub, *m_next, *m_nextSub, *m_priced, *m_pricedSub, *m_end, *m_endSub;
    quant::MultiLineChart *m_curve, *m_yields;
    BarsChart *m_points;
    QTableWidget *m_meetings, *m_banks;
};

// ------------------------------------------------------------------ Positioning

class PositioningView : public quant::View
{
    Q_OBJECT
public:
    explicit PositioningView(QWidget *parent = nullptr)
        : View(parent)
    {
        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Crowded long"), &m_crowdedLong, &m_crowdedLongSub), 1);
        tiles->addWidget(tile(tr("Crowded short"), &m_crowdedShort, &m_crowdedShortSub), 1);
        tiles->addWidget(tile(tr("Most long"), &m_mostLong, &m_mostLongSub), 1);
        tiles->addWidget(tile(tr("Most short"), &m_mostShort, &m_mostShortSub), 1);
        tiles->addWidget(tile(tr("Crowd lean"), &m_lean, &m_leanSub), 1);
        content()->addLayout(tiles);
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
        content()->addLayout(row);
        auto *note = quant::label(tr("Binance's public ratio of accounts long and short on its USDT perpetuals, a measure of the crowd, not of money. Forex and CFD positioning isn't here: the brokers' and communities' figures aren't published for free to a program (Myfxbook refuses programs). COT positions are in Quant › COT."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
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
            request(pair, [this, s, pair, left, first](const QVector<double> &longs, const QString &error) {
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

    void request(const QString &pair, std::function<void(const QVector<double> &, const QString &)> done)
    {
        const QString url = QString("https://fapi.binance.com/futures/data/globalLongShortAccountRatio?symbol=%1&period=1d&limit=30").arg(pair);
        research::Client::instance()->raw("binance:ratio:" + pair, QUrl(url), 3600, false, this, [done](const QByteArray &body, const QString &error) {
            QVector<double> longs;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).array())
                longs << v.toObject().value("longAccount").toString().toDouble() * 100.0;
            done(longs, error);
        });
    }

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
        request(pair, [this, pair](const QVector<double> &longs, const QString &) {
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
        deskdata::loadCandles({"^VIX", "^GSPC", "HYG", "LQD", "AUDJPY=X", "HG=F", "GC=F", "BTC-USD"}, "2y", "1d", this, [this](const deskdata::Candles &c, const QString &error) {
            compute(c, error);
        });
    }

private:
    void compute(const deskdata::Candles &c, const QString &error)
    {
        const QVector<desk::Candle> spx = c.value("^GSPC");
        if (spx.size() < 300) {
            setStatus(tr("Prices unavailable: %1").arg(error));
            return;
        }
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
        const desk::Appetite a = desk::computeAppetite(dates, comps);
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
    quant::MultiLineChart *m_history;
    QTableWidget *m_components;
};

// ------------------------------------------------------------------ Seasonality

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
        m_path = new quant::MultiLineChart;
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
        connect(m_period, &QButtonGroup::idClicked, this, &SeasonalityView::compute);
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
        o.firstMonth = firstMonths[m_period->checkedId()];
        o.lastMonth = lastMonths[m_period->checkedId()];
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
    quant::MultiLineChart *m_path;
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
        auto *method = quant::label(tr("From each economy's latest releases on the economic calendar and the BIS's policy rates. Each theme reads strong, neutral or weak by fixed rules (a hike, inflation above 2.5%, growth above 0.3%, unemployment falling, a PMI above 50.5, confidence rising are strong) and the score is their mean, from −100 to +100. This app's own simple reading: not a forecast, and a theme without a recent release is left out."), "qtNote");
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
QWidget *makePositioning(const Context &) { return new PositioningView; }
QWidget *makeAppetite(const Context &) { return new AppetiteView; }
QWidget *makeSeasonality(const Context &) { return new SeasonalityView; }
QWidget *makeFundamentals(const Context &c) { return new FundamentalsView(c); }

} // namespace deskui

#include "DeskViewsMacro.moc"
