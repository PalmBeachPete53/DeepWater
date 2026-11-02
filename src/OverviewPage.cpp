#include "OverviewPage.h"

#include <QCoreApplication>

#include "OverviewWidgets.h"
#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace overview;
using research::ok;

namespace {

struct Instrument {
    QString symbol;
    QString name;
};

// The four ETFs of the chart: the S&P 500, the Nasdaq-100, the Dow and the Russell 2000.
const QStringList &chartSymbols()
{
    static const QStringList s {"SPY", "QQQ", "DIA", "IWM"};
    return s;
}

QColor chartColor(const QString &symbol)
{
    if (symbol == "SPY") return research::palette::yellow();
    if (symbol == "QQQ") return research::palette::green();
    if (symbol == "DIA") return research::palette::blue();
    return Theme::current().Text;
}

struct ChartRange {
    const char *label, *range, *interval, *format;
};
const ChartRange chartRanges[] {
    {"TODAY", "2d", "5m", "HH:mm"},  {"1W", "5d", "15m", "MMM d"},  {"1M", "1mo", "60m", "MMM d"},
    {"3M", "3mo", "1d", "MMM d"},    {"YTD", "ytd", "1d", "MMM d"}, {"1Y", "1y", "1d", "MMM d"},
    {"3Y", "3y", "1wk", "MMM yyyy"}, {"5Y", "5y", "1wk", "MMM yyyy"},
};
constexpr int chartRangeCount = int(std::size(chartRanges));

const QVector<Instrument> &usIndices()
{
    static const QVector<Instrument> v {
        {"^GSPC", "S&P 500"}, {"^NDX", "Nasdaq 100"}, {"^IXIC", "Nasdaq"}, {"^DJI", "Dow Jones"}, {"^RUT", "Russell 2000"},
    };
    return v;
}

const QVector<Instrument> &globalIndices()
{
    static const QVector<Instrument> v {
        {"^N225", "Nikkei 225"}, {"^STOXX50E", "Euro Stoxx 50"}, {"^GDAXI", "DAX"},       {"^FTSE", "FTSE 100"},
        {"^FCHI", "CAC 40"},     {"^HSI", "Hang Seng"},          {"^GSPTSE", "TSX"},      {"^IBEX", "IBEX 35"},
        {"DX-Y.NYB", "Dollar Index"}, {"^VIX", "VIX"}, {"^VXN", "Nasdaq VXN"},
    };
    return v;
}

const QVector<Instrument> &cryptos()
{
    static const QVector<Instrument> v {
        {"BTC-USD", "Bitcoin"},     {"ETH-USD", "Ethereum"},          {"BNB-USD", "BNB"},      {"XRP-USD", "XRP"},
        {"SOL-USD", "Solana"},      {"TRX-USD", "TRON"},              {"DOGE-USD", "Dogecoin"}, {"ADA-USD", "Cardano"},
        {"LINK-USD", "Chainlink"},  {"HYPE32196-USD", "Hyperliquid"}, {"XLM-USD", "Stellar"},  {"SUI20947-USD", "Sui"},
        {"ZEC-USD", "Zcash"},       {"LTC-USD", "Litecoin"},
    };
    return v;
}

const QVector<Instrument> &currencies()
{
    static const QVector<Instrument> v {
        {"EURUSD=X", "EUR/USD"}, {"JPY=X", "USD/JPY"}, {"GBPUSD=X", "GBP/USD"}, {"CHF=X", "USD/CHF"},
        {"CAD=X", "USD/CAD"},    {"AUDUSD=X", "AUD/USD"}, {"NZDUSD=X", "NZD/USD"}, {"HKD=X", "USD/HKD"},
        {"SEK=X", "USD/SEK"},    {"NOK=X", "USD/NOK"}, {"TRY=X", "USD/TRY"},    {"ZAR=X", "USD/ZAR"},
    };
    return v;
}

const QVector<Instrument> &commodities()
{
    static const QVector<Instrument> v {
        {"CL=F", "Crude Oil (WTI)"}, {"BZ=F", "Brent Crude Oil"}, {"NG=F", "Natural Gas"}, {"RB=F", "Gasoline (RBOB)"},
        {"HO=F", "Heating Oil"},     {"GC=F", "Gold"},            {"SI=F", "Silver"},      {"HG=F", "Copper"},
        {"ZC=F", "Corn"},            {"ZS=F", "Soybeans"},        {"KE=F", "Wheat (KC)"},  {"SB=F", "Sugar"},
    };
    return v;
}

// The S&P 500's sectors, through State Street's Select Sector SPDR funds.
QVector<Instrument> sectorFunds()
{
    return {
        {"XLK", OverviewPage::tr("Technology")},        {"XLF", OverviewPage::tr("Financials")},
        {"XLV", OverviewPage::tr("Health Care")},       {"XLY", OverviewPage::tr("Consumer Discretionary")},
        {"XLP", OverviewPage::tr("Consumer Staples")},  {"XLE", OverviewPage::tr("Energy")},
        {"XLI", OverviewPage::tr("Industrials")},       {"XLB", OverviewPage::tr("Materials")},
        {"XLU", OverviewPage::tr("Utilities")},         {"XLRE", OverviewPage::tr("Real Estate")},
        {"XLC", OverviewPage::tr("Communication Services")},
    };
}

// Treasury tenors: the CSV's column, the name shown.
const QVector<Instrument> &tenors()
{
    static const QVector<Instrument> v {
        {"3 Mo", "US3M"}, {"6 Mo", "US6M"}, {"1 Yr", "US1Y"}, {"2 Yr", "US2Y"},
        {"3 Yr", "US3Y"}, {"5 Yr", "US5Y"}, {"10 Yr", "US10Y"}, {"30 Yr", "US30Y"},
    };
    return v;
}

double num(const QJsonObject &q, const char *key)
{
    return research::raw(q, key);
}

QString currencySign(const QString &currency)
{
    static const QHash<QString, QString> signs {
        {"CHF", "Fr"}, {"CAD", "C$"}, {"AUD", "A$"}, {"NZD", "NZ$"}, {"HKD", "HK$"}, {"SEK", "kr"}, {"NOK", "kr"},
        {"DKK", "kr"}, {"TRY", "₺"}, {"ZAR", "R"}, {"SGD", "S$"}, {"MXN", "Mex$"}, {"BRL", "R$"}, {"TWD", "NT$"},
    };
    if (signs.contains(currency))
        return signs.value(currency);
    return research::currencySymbol(currency);
}

// $6,855.58, ¥156.15, $0.2811, kr10.22, 447.50¢ (grains are quoted in US cents); FX quotes below 10 with four
// decimals.
QString price(double v, const QString &currency, bool fx = false)
{
    if (!ok(v))
        return "—";
    const double a = std::abs(v);
    const int decimals = fx ? (a < 10 ? 4 : 2) : a >= 1 ? 2 : a >= 0.01 ? 4 : 6;
    if (currency == "USX")
        return QLocale().toString(v, 'f', decimals) + "¢";
    return currencySign(currency) + QLocale().toString(v, 'f', decimals);
}

// v in percent: +1.23%.
QString pct(double v, int decimals = 2)
{
    if (!ok(v))
        return "—";
    return QString("%1%2%").arg(v > 0 ? "+" : "").arg(QLocale().toString(v, 'f', decimals));
}

QColor signColor(double v)
{
    const Palette &c = Theme::current();
    return !ok(v) ? c.TextFaint : v > 0 ? c.Up : v < 0 ? c.Down : c.TextDim;
}

Table::Row quoteRow(const QString &name, const QJsonObject &q, bool fx = false)
{
    const double chg = num(q, "regularMarketChangePercent");
    Table::Row row;
    row.cells = {{name}, {price(num(q, "regularMarketPrice"), q.value("currency").toString(), fx)},
                 {pct(chg), signColor(chg)}};
    const QString full = q.value("longName").toString(q.value("shortName").toString());
    row.tooltip = q.isEmpty() ? QString() : QString("%1 · %2").arg(q.value("symbol").toString(), full);
    return row;
}

QVector<Table::Row> listRows(const QVector<Instrument> &list, const QHash<QString, QJsonObject> &quotes, bool fx = false)
{
    QVector<Table::Row> rows;
    for (const Instrument &i : list)   // commodity names are translated in i18n/deepwater_data_fr.ts (MarketData)
        rows << quoteRow(QCoreApplication::translate("MarketData", i.name.toUtf8().constData()), quotes.value(i.symbol), fx);
    return rows;
}

QVector<Table::Column> listColumns()
{
    return {{"", 1, 0, Qt::AlignLeft}, {"", 1, 0, Qt::AlignRight}, {"", 1, 54, Qt::AlignRight}};
}

QString shortName(const QJsonObject &q)
{
    QString n = q.value("shortName").toString(q.value("longName").toString());
    return n.isEmpty() ? q.value("symbol").toString() : n;
}

// The New York date of a moment.
QDate nyDate(const QDateTime &t)
{
    static const QTimeZone ny("America/New_York");
    return t.toTimeZone(ny).date();
}

double mean(const QVector<double> &v)
{
    double s = 0;
    for (double x : v)
        s += x;
    return v.isEmpty() ? research::NaN : s / v.size();
}

} // namespace

OverviewPage::OverviewPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("overviewPage");
    setAttribute(Qt::WA_StyledBackground);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("ovScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto *content = new QWidget;
    content->setObjectName("ovContent");
    content->setMinimumWidth(1180);
    scroll->setWidget(content);
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(6);
    QSettings settings;

    // --- Index performance and statistics.
    auto *top = new QHBoxLayout;
    top->setSpacing(6);
    auto *perf = new Panel(tr("Index performance"));
    perf->setFixedHeight(330);
    QStringList rangeLabels;
    for (const ChartRange &r : chartRanges)
        rangeLabels << (QString(r.label) == "TODAY" ? tr("TODAY") : QString(r.label));
    perf->head()->addWidget(periodTabs(rangeLabels, &m_chartRange,
                                       std::clamp(settings.value("overview/chartRange", 0).toInt(), 0, chartRangeCount - 1)));
    auto *checks = new QHBoxLayout;
    checks->setContentsMargins(8, 4, 8, 0);
    checks->setSpacing(18);
    const QStringList hidden = settings.value("overview/hiddenSeries").toStringList();
    m_chart = new PerfChart;
    for (const QString &s : chartSymbols()) {
        auto *box = new QCheckBox(s);
        box->setObjectName("ovSeries");
        box->setChecked(!hidden.contains(s));
        box->setCursor(Qt::PointingHandCursor);
        box->setFocusPolicy(Qt::NoFocus);
        box->setStyleSheet(QString("QCheckBox { color: %1; }").arg(chartColor(s).name()));
        checks->addWidget(box);
        connect(box, &QCheckBox::toggled, this, [this, s](bool on) {
            m_chart->setSeriesVisible(s, on);
            QStringList h = QSettings().value("overview/hiddenSeries").toStringList();
            h.removeAll(s);
            if (!on)
                h << s;
            QSettings().setValue("overview/hiddenSeries", h);
        });
    }
    checks->addStretch(1);
    perf->body()->addLayout(checks);
    perf->body()->addWidget(m_chart, 1);
    top->addWidget(perf, 4);
    auto *statsPanel = new Panel(tr("Statistics"));
    statsPanel->setFixedHeight(330);
    m_stats = new StatsView;
    statsPanel->body()->addWidget(m_stats, 1);
    top->addWidget(statsPanel, 1);
    v->addLayout(top);
    connect(m_chartRange, &QButtonGroup::idClicked, this, [this](int id) {
        QSettings().setValue("overview/chartRange", id);
        fetchChart();
    });

    // --- Sectors, US indices and bonds, world, crypto, currencies, commodities.
    auto *board = new QHBoxLayout;
    board->setSpacing(6);
    auto makeList = [](Panel *panel, int minRows) {
        auto *t = new Table;
        t->setColumns(listColumns(), false);
        t->setMinimumRows(minRows);
        t->setMessage(tr("Loading…"));
        panel->body()->addWidget(t);
        panel->body()->addStretch(1);
        return t;
    };

    auto *sectors = new Panel(tr("Sectors"));
    sectors->head()->addWidget(periodTabs({tr("TODAY"), "1W", "1M", "3M", "YTD"}, &m_sectorRange,
                                          std::clamp(settings.value("overview/sectorRange", 0).toInt(), 0, 4)));
    m_sectors = new Table;
    m_sectors->setColumns({{"", 2.4, 0, Qt::AlignLeft}, {"", 1.3, 0, Qt::AlignLeft}, {"", 1, 60, Qt::AlignRight}}, false);
    m_sectors->setMessage(tr("Loading…"));
    sectors->body()->addWidget(m_sectors);
    sectors->body()->addStretch(1);
    board->addWidget(sectors, 6);
    connect(m_sectorRange, &QButtonGroup::idClicked, this, [this](int id) {
        QSettings().setValue("overview/sectorRange", id);
        fillSectors();
    });

    auto *usColumn = new QVBoxLayout;
    usColumn->setSpacing(6);
    auto *us = new Panel(tr("US indices"));
    m_usIndices = makeList(us, 5);
    usColumn->addWidget(us);
    m_bondsPanel = new Panel(tr("US bonds"));
    m_bonds = new Table;
    m_bonds->setColumns(listColumns(), false);
    m_bonds->setMinimumRows(int(tenors().size()));
    m_bonds->setMessage(tr("Loading…"));
    m_bondsPanel->body()->addWidget(m_bonds);
    m_bondsPanel->body()->addStretch(1);
    usColumn->addWidget(m_bondsPanel, 1);
    board->addLayout(usColumn, 5);

    auto *global = new Panel(tr("Global"));
    m_global = makeList(global, 14);
    board->addWidget(global, 5);
    auto *crypto = new Panel(tr("Crypto"));
    m_crypto = makeList(crypto, 14);
    board->addWidget(crypto, 5);
    auto *fx = new Panel("FX");
    m_fx = makeList(fx, 14);
    board->addWidget(fx, 5);
    auto *cmdty = new Panel(tr("Commodities"));
    m_commodities = makeList(cmdty, 14);
    board->addWidget(cmdty, 5);
    v->addLayout(board);

    // --- Largest companies and the day's movers.
    auto *movers = new QHBoxLayout;
    movers->setSpacing(6);
    auto *largest = new Panel(tr("Largest companies"));
    m_prevPage = new QPushButton("‹");
    m_nextPage = new QPushButton("›");
    m_largestPage = new QLabel("1/3");
    m_largestPage->setObjectName("ovNote");
    for (QPushButton *b : {m_prevPage, m_nextPage}) {
        b->setObjectName("ovPager");
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(18, 18);
    }
    largest->head()->addWidget(m_prevPage);
    largest->head()->addWidget(m_largestPage);
    largest->head()->addWidget(m_nextPage);
    m_largest = new Table;
    m_largest->setColumns({{"#", 1, 24, Qt::AlignRight},
                           {tr("Sym"), 1, 62, Qt::AlignLeft},
                           {tr("Name"), 2.2, 0, Qt::AlignLeft},
                           {tr("Price"), 1, 0, Qt::AlignRight},
                           {tr("Chg%"), 1, 62, Qt::AlignRight},
                           {tr("Mcap"), 1, 62, Qt::AlignRight}});
    m_largest->setMinimumRows(10);
    m_largest->setMessage(tr("Loading…"));
    largest->body()->addWidget(m_largest);
    largest->body()->addStretch(1);
    movers->addWidget(largest, 3);
    connect(m_prevPage, &QPushButton::clicked, this, [this] {
        m_page = std::max(0, m_page - 1);
        fillLargest();
    });
    connect(m_nextPage, &QPushButton::clicked, this, [this] {
        m_page = std::min(2, m_page + 1);
        fillLargest();
    });
    connect(m_largest, &Table::symbolClicked, this, &OverviewPage::openResearch);

    auto moverTable = [&](const QString &title, bool volume) {
        auto *panel = new Panel(title);
        auto *t = new Table;
        QVector<Table::Column> cols {{tr("Symbol"), 1, 0, Qt::AlignLeft}, {tr("Price"), 1, 0, Qt::AlignRight}};
        if (volume)
            cols << Table::Column {tr("Volume"), 1, 62, Qt::AlignRight};
        cols << Table::Column {tr("Chg%"), 1, 62, Qt::AlignRight};
        t->setColumns(cols);
        t->setMinimumRows(10);
        t->setMessage(tr("Loading…"));
        panel->body()->addWidget(t);
        panel->body()->addStretch(1);
        movers->addWidget(panel, 2);
        connect(t, &Table::symbolClicked, this, &OverviewPage::openResearch);
        return t;
    };
    m_gainers = moverTable(tr("Gainers"), false);
    m_losers = moverTable(tr("Losers"), false);
    m_active = moverTable(tr("Active"), true);
    v->addLayout(movers);

    // --- The best and worst members of an index today.
    auto *indexMovers = new QHBoxLayout;
    indexMovers->setSpacing(6);
    auto indexTable = [&](const QString &title, QButtonGroup **group, const char *key) {
        auto *panel = new Panel(title);
        panel->head()->addWidget(periodTabs({"SP500", "NDX", "DJIA"}, group,
                                            std::clamp(QSettings().value(key, 0).toInt(), 0, 2)));
        auto *t = new Table;
        t->setColumns({{"#", 1, 24, Qt::AlignRight},
                       {tr("Sym"), 1, 62, Qt::AlignLeft},
                       {tr("Name"), 3, 0, Qt::AlignLeft},
                       {tr("Price"), 1, 0, Qt::AlignRight},
                       {tr("Chg%"), 1, 70, Qt::AlignRight}});
        t->setMinimumRows(12);
        t->setMessage(tr("Loading…"));
        panel->body()->addWidget(t);
        panel->body()->addStretch(1);
        indexMovers->addWidget(panel, 1);
        connect(t, &Table::symbolClicked, this, &OverviewPage::openResearch);
        connect(*group, &QButtonGroup::idClicked, this, [this, key](int id) {
            QSettings().setValue(key, id);
            fetchIndexMembers(id);
            fillIndexMovers();
        });
        return t;
    };
    m_indexGainers = indexTable(tr("Top index gainers"), &m_gainersIndex, "overview/gainersIndex");
    m_indexLosers = indexTable(tr("Top index losers"), &m_losersIndex, "overview/losersIndex");
    v->addLayout(indexMovers);
    v->addStretch(1);

    // Every minute while shown; answers younger than that come from the client's cache.
    m_timer.setInterval(60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &OverviewPage::refresh);
}

void OverviewPage::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!m_lastRefresh.isValid() || m_lastRefresh.secsTo(QDateTime::currentDateTimeUtc()) >= 55)
        refresh();
    m_timer.start();
}

void OverviewPage::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    m_timer.stop();
}

void OverviewPage::refresh()
{
    m_lastRefresh = QDateTime::currentDateTimeUtc();
    fetchChart();
    fetchBoard();
    fetchSectorHistory();
    fetchBonds();
    fetchLargest();
    fetchMovers();
    fetchIndexMembers(m_gainersIndex->checkedId());
    if (m_losersIndex->checkedId() != m_gainersIndex->checkedId())
        fetchIndexMembers(m_losersIndex->checkedId());
}

// ---------------------------------------------------------------- index performance

void OverviewPage::fetchChart()
{
    const int gen = ++m_chartGeneration;
    const ChartRange &r = chartRanges[std::clamp(m_chartRange->checkedId(), 0, chartRangeCount - 1)];
    m_chartPending = int(chartSymbols().size());
    m_chartError.clear();
    auto bars = std::make_shared<QHash<QString, QVector<research::Bar>>>();
    for (const QString &s : chartSymbols()) {
        research::Client::instance()->candles(s, r.range, r.interval, this,
            [this, gen, s, bars](const QVector<research::Bar> &b, const QString &err) {
                if (gen != m_chartGeneration)
                    return;
                bars->insert(s, b);
                if (!err.isEmpty())
                    m_chartError = err;
                if (--m_chartPending == 0) {
                    m_bars = *bars;
                    buildChart();
                }
            });
    }
}

void OverviewPage::buildChart()
{
    const int rangeIndex = std::clamp(m_chartRange->checkedId(), 0, chartRangeCount - 1);
    const ChartRange &r = chartRanges[rangeIndex];
    const bool today = rangeIndex == 0;

    // Each series' bars over the range and its base: today, the previous session's last close; otherwise the
    // range's first open.
    struct Prepared {
        QVector<qint64> times;
        QVector<double> closes;
        double base = research::NaN;
    };
    QHash<QString, Prepared> prepared;
    QVector<qint64> times;
    for (const QString &s : chartSymbols()) {
        const QVector<research::Bar> &bars = m_bars.value(s);
        if (bars.isEmpty())
            continue;
        Prepared p;
        int first = 0;
        if (today) {
            const QDate session = nyDate(bars.last().time);
            while (first < bars.size() && nyDate(bars[first].time) != session)
                ++first;
            if (first > 0)
                p.base = bars[first - 1].close;
        }
        if (first >= bars.size())
            continue;
        if (!ok(p.base))
            p.base = ok(bars[first].open) ? bars[first].open : bars[first].close;
        for (int i = first; i < bars.size(); ++i) {
            p.times << bars[i].time.toSecsSinceEpoch();
            p.closes << bars[i].close;
        }
        times += p.times;
        prepared.insert(s, p);
    }
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    if (times.size() < 2) {
        m_chart->setMessage(m_chartError.isEmpty() ? tr("No prices for this range") : m_chartError);
        m_chart->setData({}, {}, r.format);
        m_stats->setData({}, {});
        return;
    }

    // On the common time axis, carried forward over a series' missing bars.
    QVector<PerfChart::Series> series;
    QHash<QString, QVector<double>> returns;   // bar to bar, aligned on the axis
    QVector<StatsView::Stat> stats;
    for (const QString &s : chartSymbols()) {
        const Prepared p = prepared.value(s);
        PerfChart::Series out;
        out.name = s;
        out.color = chartColor(s);
        out.values.fill(research::NaN, times.size());
        QVector<double> rets(times.size(), research::NaN);
        StatsView::Stat st;
        st.name = s;
        st.color = out.color;
        if (!p.times.isEmpty() && p.base > 0) {
            int j = 0;
            double last = research::NaN, prevClose = research::NaN;
            QVector<double> barReturns;
            double high = -1e300, low = 1e300;
            for (int i = 0; i < times.size(); ++i) {
                bool fresh = false;
                while (j < p.times.size() && p.times[j] <= times[i]) {
                    last = p.closes[j++];
                    fresh = true;
                }
                if (!ok(last))
                    continue;
                const double v = (last / p.base - 1) * 100;
                out.values[i] = v;
                high = std::max(high, v);
                low = std::min(low, v);
                if (fresh && ok(prevClose) && prevClose > 0) {
                    rets[i] = last / prevClose - 1;
                    barReturns << rets[i];
                }
                if (fresh)
                    prevClose = last;
            }
            const double m = mean(barReturns);
            double var = 0;
            for (double x : barReturns)
                var += (x - m) * (x - m);
            st.ret = out.values.last();
            st.high = high;
            st.low = low;
            st.vol = barReturns.size() > 1 ? std::sqrt(var / (barReturns.size() - 1)) * 100 : research::NaN;
        }
        series << out;
        returns.insert(s, rets);
        stats << st;
    }

    QVector<StatsView::Correlation> correlations;
    const QStringList &syms = chartSymbols();
    for (int a = 0; a < syms.size(); ++a)
        for (int b = a + 1; b < syms.size(); ++b) {
            const QVector<double> &x = returns[syms[a]], &y = returns[syms[b]];
            QVector<double> xs, ys;
            for (int i = 0; i < x.size(); ++i)
                if (ok(x[i]) && ok(y[i])) {
                    xs << x[i];
                    ys << y[i];
                }
            double corr = research::NaN;
            if (xs.size() > 2) {
                const double mx = mean(xs), my = mean(ys);
                double sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < xs.size(); ++i) {
                    sxy += (xs[i] - mx) * (ys[i] - my);
                    sxx += (xs[i] - mx) * (xs[i] - mx);
                    syy += (ys[i] - my) * (ys[i] - my);
                }
                if (sxx > 0 && syy > 0)
                    corr = sxy / std::sqrt(sxx * syy);
            }
            correlations << StatsView::Correlation {syms[a], syms[b], corr};
        }

    const QStringList hidden = QSettings().value("overview/hiddenSeries").toStringList();
    for (PerfChart::Series &s : series)
        s.visible = !hidden.contains(s.name);
    m_chart->setData(times, series, r.format);
    m_stats->setData(stats, correlations);
}

// ---------------------------------------------------------------- quotes of the fixed lists

void OverviewPage::fetchBoard()
{
    QStringList symbols;
    for (const auto *list : {&usIndices(), &globalIndices(), &cryptos(), &currencies(), &commodities()})
        for (const Instrument &i : *list)
            symbols << i.symbol;
    for (const Instrument &i : sectorFunds())
        symbols << i.symbol;
    research::Client::instance()->quotes(symbols, 55, this, [this](const QHash<QString, QJsonObject> &q, const QString &err) {
        if (!q.isEmpty())
            m_board = q;
        m_boardError = err;
        fillBoard();
        fillSectors();
    });
}

void OverviewPage::fillBoard()
{
    const QString message = m_boardError.isEmpty() ? tr("Loading…") : m_boardError;
    auto fill = [&](Table *t, const QVector<Instrument> &list, bool fx = false) {
        if (m_board.isEmpty())
            t->setMessage(message);
        else
            t->setRows(listRows(list, m_board, fx));
    };
    fill(m_usIndices, usIndices());
    fill(m_global, globalIndices());
    fill(m_crypto, cryptos());
    fill(m_fx, currencies(), true);
    fill(m_commodities, commodities());
}

// ---------------------------------------------------------------- sectors

void OverviewPage::fetchSectorHistory()
{
    QStringList symbols;
    for (const Instrument &i : sectorFunds())
        symbols << i.symbol;
    research::Client::instance()->spark(symbols, "1y", this, [this](const QHash<QString, research::Series> &s, const QString &) {
        if (!s.isEmpty())
            m_sectorHistory = s;
        fillSectors();
    });
}

void OverviewPage::fillSectors()
{
    const int period = m_sectorRange->checkedId();
    const QDate today = nyDate(QDateTime::currentDateTimeUtc());
    // The last close on or before the period's start.
    const QDate start = period == 1   ? today.addDays(-7)
                        : period == 2 ? today.addMonths(-1)
                        : period == 3 ? today.addMonths(-3)
                                      : QDate(today.year() - 1, 12, 31);
    struct Item {
        Instrument fund;
        double change = research::NaN;   // %
    };
    QVector<Item> items;
    for (const Instrument &f : sectorFunds()) {
        const QJsonObject q = m_board.value(f.symbol);
        Item it {f};
        if (period == 0) {
            it.change = num(q, "regularMarketChangePercent");
        } else {
            const research::Series &s = m_sectorHistory.value(f.symbol);
            double base = research::NaN;
            for (const research::Point &p : s)
                if (p.date <= start)
                    base = p.value;
            double last = num(q, "regularMarketPrice");
            if (!ok(last) && !s.isEmpty())
                last = s.last().value;
            if (ok(base) && base > 0 && ok(last))
                it.change = (last / base - 1) * 100;
        }
        items << it;
    }
    if (std::none_of(items.begin(), items.end(), [](const Item &i) { return ok(i.change); })) {
        m_sectors->setRows({});
        m_sectors->setMessage(m_boardError.isEmpty() ? tr("Loading…") : m_boardError);
        return;
    }
    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        return (ok(a.change) ? a.change : -1e9) > (ok(b.change) ? b.change : -1e9);
    });
    double scale = 0;
    for (const Item &i : items)
        if (ok(i.change))
            scale = std::max(scale, std::abs(i.change));
    QVector<Table::Row> rows;
    for (const Item &i : items) {
        Table::Row row;
        Table::Cell bar;
        bar.bar = ok(i.change) && scale > 0 ? i.change / scale : research::NaN;
        row.cells = {{i.fund.name}, bar, {pct(i.change), signColor(i.change)}};
        row.marker = signColor(i.change);
        row.tooltip = tr("%1 · Select Sector SPDR fund").arg(i.fund.symbol);
        rows << row;
    }
    m_sectors->setRows(rows);
}

// ---------------------------------------------------------------- bonds

void OverviewPage::fetchBonds()
{
    research::Client::instance()->treasuryYields(this, [this](const QVector<research::YieldCurveDay> &days, const QString &err) {
        if (days.isEmpty()) {
            m_bonds->setMessage(err.isEmpty() ? tr("No yields") : err);
            return;
        }
        const research::YieldCurveDay &last = days.first();
        const research::YieldCurveDay *prev = days.size() > 1 ? &days[1] : nullptr;
        QVector<Table::Row> rows;
        for (const Instrument &t : tenors()) {
            const double y = last.yields.value(t.symbol, research::NaN);
            const double p = prev ? prev->yields.value(t.symbol, research::NaN) : research::NaN;
            const double chg = ok(y) && ok(p) ? y - p : research::NaN;
            Table::Row row;
            row.cells = {{t.name},
                         {ok(y) ? QLocale().toString(y, 'f', 2) + "%" : "—"},
                         {ok(chg) ? QString("%1%2").arg(chg > 0 ? "+" : "").arg(QLocale().toString(chg, 'f', 2)) : "—",
                          signColor(chg)}};
            row.tooltip = tr("%1 Treasury par yield, %2; change in points since %3")
                              .arg(t.symbol, QLocale().toString(last.date, QLocale::ShortFormat),
                                   prev ? QLocale().toString(prev->date, QLocale::ShortFormat) : "—");
            rows << row;
        }
        m_bonds->setRows(rows);
        m_bondsPanel->setNote(QLocale().toString(last.date, "MMM d"));
    });
}

// ---------------------------------------------------------------- largest companies

void OverviewPage::fetchLargest()
{
    research::Client::instance()->listings(this, [this](const QVector<research::Listing> &all, const QString &err) {
        QVector<const research::Listing *> sorted;
        for (const research::Listing &l : all)
            if (ok(l.marketCap) && l.marketCap > 0)
                sorted << &l;
        std::sort(sorted.begin(), sorted.end(),
                  [](const research::Listing *a, const research::Listing *b) { return a->marketCap > b->marketCap; });
        // One line per company (GOOGL and GOOG).
        QStringList symbols;
        QSet<QString> names;
        for (const research::Listing *l : sorted) {
            if (names.contains(l->name))
                continue;
            names.insert(l->name);
            symbols << l->symbol;
            if (symbols.size() == 34)   // a few spare: share classes the screener names differently
                break;
        }
        if (symbols.isEmpty()) {
            m_largest->setMessage(err.isEmpty() ? tr("No listings") : err);
            return;
        }
        m_largestSymbols = symbols;
        research::Client::instance()->quotes(symbols, 55, this, [this](const QHash<QString, QJsonObject> &q, const QString &qerr) {
            if (!q.isEmpty())
                m_largestQuotes = q;
            else
                m_largest->setMessage(qerr);
            fillLargest();
        });
    });
}

void OverviewPage::fillLargest()
{
    QStringList sorted = m_largestSymbols;
    // By today's market cap (Yahoo's), the screener's order otherwise; one line per company (GOOGL, not GOOG too).
    std::stable_sort(sorted.begin(), sorted.end(), [this](const QString &a, const QString &b) {
        const double ca = num(m_largestQuotes.value(a), "marketCap"), cb = num(m_largestQuotes.value(b), "marketCap");
        return (ok(ca) ? ca : 0) > (ok(cb) ? cb : 0);
    });
    QStringList symbols;
    QSet<QString> companies;
    for (const QString &sym : std::as_const(sorted)) {
        const QJsonObject q = m_largestQuotes.value(sym);
        const QString company = q.value("longName").toString(shortName(q));
        if (!q.isEmpty() && companies.contains(company))
            continue;
        companies.insert(company);
        symbols << sym;
        if (symbols.size() == 30)
            break;
    }
    const int pages = std::max(1, int(std::ceil(symbols.size() / 10.0)));
    m_page = std::clamp(m_page, 0, pages - 1);
    m_largestPage->setText(QString("%1/%2").arg(m_page + 1).arg(pages));
    m_prevPage->setEnabled(m_page > 0);
    m_nextPage->setEnabled(m_page < pages - 1);
    if (m_largestQuotes.isEmpty())
        return;
    QVector<Table::Row> rows;
    for (int i = m_page * 10; i < std::min<int>(symbols.size(), m_page * 10 + 10); ++i) {
        const QJsonObject q = m_largestQuotes.value(symbols[i]);
        const double chg = num(q, "regularMarketChangePercent");
        Table::Row row;
        row.symbol = symbols[i];
        row.cells = {{QString::number(i + 1), Theme::current().TextFaint},
                     {symbols[i], {}, true},
                     {shortName(q)},
                     {price(num(q, "regularMarketPrice"), q.value("currency").toString())},
                     {pct(chg), signColor(chg)},
                     {research::compact(num(q, "marketCap"))}};
        row.tooltip = tr("%1 · open in Research").arg(q.value("longName").toString(shortName(q)));
        rows << row;
    }
    m_largest->setRows(rows);
}

// ---------------------------------------------------------------- movers

void OverviewPage::fetchMovers()
{
    const struct {
        const char *id;
        Table *table;
        bool volume;
    } screens[] {{"day_gainers", m_gainers, false}, {"day_losers", m_losers, false}, {"most_actives", m_active, true}};
    for (const auto &s : screens) {
        Table *table = s.table;
        const bool volume = s.volume;
        research::Client::instance()->screener(s.id, 25, this, [table, volume](const QVector<QJsonObject> &quotes, const QString &err) {
            if (quotes.isEmpty()) {
                table->setMessage(err);
                return;
            }
            QVector<Table::Row> rows;
            for (const QJsonObject &q : quotes) {
                if (rows.size() == 10)
                    break;
                const double chg = num(q, "regularMarketChangePercent");
                Table::Row row;
                row.symbol = q.value("symbol").toString();
                row.cells = {{row.symbol, {}, true}, {price(num(q, "regularMarketPrice"), q.value("currency").toString())}};
                if (volume)
                    row.cells << Table::Cell {research::compact(num(q, "regularMarketVolume"), 1)};
                row.cells << Table::Cell {pct(chg), signColor(chg)};
                row.tooltip = tr("%1 · open in Research").arg(shortName(q));
                rows << row;
            }
            table->setRows(rows);
        });
    }
}

// ---------------------------------------------------------------- index gainers and losers

void OverviewPage::fetchIndexMembers(int index)
{
    research::Client::instance()->indexes(this, [this, index](const research::Indexes &idx, const QString &err) {
        if (!idx.sp500.isEmpty())
            m_indexes = idx;
        const QVector<research::IndexMember> &members = index == 0   ? m_indexes.sp500
                                                        : index == 1 ? m_indexes.nasdaq100
                                                                     : m_indexes.dow;
        if (members.isEmpty()) {
            m_memberErrors[index] = err.isEmpty() ? tr("No index members") : err;
            fillIndexMovers();
            return;
        }
        QStringList symbols;
        for (const research::IndexMember &m : members)
            symbols << m.symbol;
        symbols.sort();
        symbols.removeDuplicates();
        // A hundred symbols per request.
        struct State {
            QHash<QString, QJsonObject> quotes;
            int left = 0;
            QString error;
        };
        auto state = std::make_shared<State>();
        state->left = int(std::ceil(symbols.size() / 100.0));
        for (int i = 0; i < symbols.size(); i += 100) {
            research::Client::instance()->quotes(symbols.mid(i, 100), 55, this,
                [this, index, state](const QHash<QString, QJsonObject> &q, const QString &qerr) {
                    state->quotes.insert(q);
                    if (!qerr.isEmpty())
                        state->error = qerr;
                    if (--state->left > 0)
                        return;
                    if (!state->quotes.isEmpty())
                        m_memberQuotes[index] = state->quotes;
                    m_memberErrors[index] = state->error;
                    fillIndexMovers();
                });
        }
    });
}

void OverviewPage::fillIndexMovers()
{
    auto fill = [this](Table *table, int index, bool gainers) {
        const QHash<QString, QJsonObject> quotes = m_memberQuotes.value(index);
        if (quotes.isEmpty()) {
            table->setRows({});
            const QString err = m_memberErrors.value(index);
            table->setMessage(err.isEmpty() ? tr("Loading…") : err);
            return;
        }
        const QVector<research::IndexMember> &members = index == 0   ? m_indexes.sp500
                                                        : index == 1 ? m_indexes.nasdaq100
                                                                     : m_indexes.dow;
        QHash<QString, QString> names;
        for (const research::IndexMember &m : members)
            names.insert(m.symbol, m.name);
        QVector<QJsonObject> list;
        for (const QJsonObject &q : quotes)
            if (ok(num(q, "regularMarketChangePercent")))
                list << q;
        std::sort(list.begin(), list.end(), [gainers](const QJsonObject &a, const QJsonObject &b) {
            const double ca = num(a, "regularMarketChangePercent"), cb = num(b, "regularMarketChangePercent");
            return gainers ? ca > cb : ca < cb;
        });
        QVector<Table::Row> rows;
        for (int i = 0; i < list.size() && i < 12; ++i) {
            const QJsonObject &q = list[i];
            const double chg = num(q, "regularMarketChangePercent");
            Table::Row row;
            row.symbol = q.value("symbol").toString();
            const QString name = names.value(row.symbol, shortName(q));
            row.cells = {{QString::number(i + 1), Theme::current().TextFaint},
                         {row.symbol, {}, true},
                         {name},
                         {price(num(q, "regularMarketPrice"), q.value("currency").toString())},
                         {pct(chg), signColor(chg)}};
            row.tooltip = tr("%1 · open in Research").arg(name);
            rows << row;
        }
        table->setRows(rows);
    };
    fill(m_indexGainers, m_gainersIndex->checkedId(), true);
    fill(m_indexLosers, m_losersIndex->checkedId(), false);
}
