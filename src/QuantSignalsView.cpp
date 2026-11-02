#include "QuantViews.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace quant {

namespace {

// The timeframes scanned: Yahoo's interval and range for each; 4h candles are built from the hourly ones.
struct Timeframe {
    QString label, interval, range;
};
const QVector<Timeframe> &timeframes()
{
    static const QVector<Timeframe> tfs {{"5m", "5m", "5d"}, {"15m", "15m", "5d"}, {"1h", "60m", "1mo"}, {"1d", "1d", "6mo"}};
    return tfs;
}

constexpr int StreakMin = 5;   // candles in a row for a streak signal

double gapThreshold(const QString &tf)
{
    if (tf == "1d") return 2.0;
    if (tf == "4h") return 1.0;
    if (tf == "1h") return 0.75;
    if (tf == "15m") return 0.5;
    return 0.3;
}

// Four hourly candles of the same day into one.
QVector<research::Bar> fourHour(const QVector<research::Bar> &hourly)
{
    QVector<research::Bar> out;
    QDate day;
    int count = 0;
    for (const research::Bar &b : hourly) {
        const QDate d = b.time.toLocalTime().date();
        if (d != day || count == 4) {
            out.push_back(b);
            day = d;
            count = 1;
            continue;
        }
        research::Bar &last = out.last();
        last.high = std::max(last.high, b.high);
        last.low = std::min(last.low, b.low);
        last.close = b.close;
        last.volume += b.volume;
        ++count;
    }
    return out;
}

QString ago(const QDateTime &t)
{
    const qint64 s = t.secsTo(QDateTime::currentDateTime());
    if (s < 3600) return QString("%1m ago").arg(std::max<qint64>(1, s / 60));
    if (s < 86400) return QString("%1h ago").arg(s / 3600);
    return QString("%1d ago").arg(s / 86400);
}

} // namespace

QStringList SignalsView::universe()
{
    // Large US stocks (the Dow and the biggest of the S&P 500), plus the Compare tickers.
    QStringList list {"AAPL", "MSFT", "NVDA", "AMZN", "GOOGL", "META", "TSLA", "BRK-B", "AVGO", "JPM", "V", "UNH", "XOM", "LLY",
                      "JNJ", "WMT", "MA", "PG", "HD", "COST", "ORCL", "ABBV", "BAC", "CVX", "KO", "MRK", "PEP", "ADBE", "CRM",
                      "NFLX", "AMD", "TMO", "CSCO", "MCD", "ACN", "WFC", "DIS", "ABT", "INTC", "IBM", "CAT", "GS", "BA", "NKE",
                      "GE", "AMGN", "QCOM", "TXN"};
    for (const QString &t : QSettings().value("quant/compare/tickers").toStringList())
        if (!list.contains(t))
            list << t;
    return list;
}

SignalsView::SignalsView(QWidget *parent)
    : View(parent)
{
    QHBoxLayout *h;
    auto *filterBar = bar(&h);
    m_filter = new QLineEdit;
    m_filter->setObjectName("qtInput");
    m_filter->setPlaceholderText(tr("Filter by symbol (e.g., AAPL, TSLA)…"));
    h->addWidget(m_filter);
    content()->addWidget(filterBar);

    auto *filters = bar(&h);
    h->addWidget(segments({tr("ALL"), "RSI", tr("STREAK"), tr("GAP")}, &m_indicator));
    h->addStretch(1);
    h->addWidget(segments({tr("ALL TF"), "5M", "15M", "1H", "4H", "1D"}, &m_timeframe));
    content()->addWidget(filters);

    QVBoxLayout *body;
    auto *tableCard = card({}, &body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    m_table = new QTableWidget(0, 7);
    m_table->setObjectName("qtTable");
    m_table->setHorizontalHeaderLabels({tr("RANK"), tr("SYMBOL"), tr("TIMEFRAME"), tr("INDICATOR"), tr("SIGNAL"), tr("VALUE"), tr("TIME")});
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int col = 0; col < 7; ++col)
        m_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    body->addWidget(m_table);
    m_empty = label(tr("No signals match."), "qtEmpty");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setMinimumHeight(120);
    body->addWidget(m_empty);
    content()->addWidget(tableCard);
    content()->addStretch(1);

    m_refill.setSingleShot(true);
    m_refill.setInterval(250);
    connect(&m_refill, &QTimer::timeout, this, &SignalsView::fillTable);
    m_rescan.setInterval(5 * 60 * 1000);
    connect(&m_rescan, &QTimer::timeout, this, &SignalsView::scan);
    connect(m_filter, &QLineEdit::textChanged, this, [this] { m_refill.start(); });
    connect(m_indicator, &QButtonGroup::idClicked, this, &SignalsView::fillTable);
    connect(m_timeframe, &QButtonGroup::idClicked, this, &SignalsView::fillTable);
    fillTable();
}

void SignalsView::load()
{
    scan();
}

void SignalsView::showEvent(QShowEvent *e)
{
    View::showEvent(e);
    m_rescan.start();
    if (m_lastScan.isValid() && m_lastScan.secsTo(QDateTime::currentDateTime()) > 5 * 60 && m_pending == 0)
        scan();
}

void SignalsView::hideEvent(QHideEvent *e)
{
    View::hideEvent(e);
    m_rescan.stop();
}

void SignalsView::scan()
{
    const int gen = ++m_generation;
    const QStringList symbols = universe();
    m_pending = m_total = symbols.size() * timeframes().size();
    m_lastScan = QDateTime::currentDateTime();
    setStatus(tr("Scanning %1 symbols on 5 timeframes… 0/%2").arg(symbols.size()).arg(m_total));
    for (const QString &sym : symbols)
        for (const Timeframe &tf : timeframes())
            research::Client::instance()->candles(sym, tf.range, tf.interval, this,
                [this, gen, sym, tf](const QVector<research::Bar> &bars, const QString &) {
                    if (gen != m_generation)
                        return;
                    scanned(sym, tf.label, bars);
                    if (tf.label == "1h")
                        scanned(sym, "4h", fourHour(bars));
                    --m_pending;
                    if (m_pending > 0)
                        setStatus(tr("Scanning… %1/%2").arg(m_total - m_pending).arg(m_total));
                    else
                        setStatus(tr("Scanned %1 symbols on 5 timeframes at %2 · RSI(14) beyond 30/70, streaks of %3+ candles, gaps")
                                      .arg(universe().size()).arg(QLocale().toString(m_lastScan.time(), "HH:mm")).arg(StreakMin));
                    m_refill.start();
                });
}

void SignalsView::scanned(const QString &symbol, const QString &timeframe, const QVector<research::Bar> &bars)
{
    QVector<Signal> found;
    const int n = bars.size();
    if (n < 16) {
        m_signals.insert(symbol + " " + timeframe, found);
        return;
    }
    QVector<double> close(n);
    for (int i = 0; i < n; ++i)
        close[i] = bars[i].close;
    // RSI: the most recent candle beyond 30 or 70.
    const QVector<double> r = rsi(close, 14);
    for (int i = n - 1; i >= 0; --i) {
        if (!ok(r[i]) || (r[i] > 30 && r[i] < 70))
            continue;
        const bool over = r[i] >= 70;
        found.push_back({symbol, timeframe, "RSI", over ? tr("RSI OVERBOUGHT") : tr("RSI OVERSOLD"),
                         QString("RSI %1").arg(QString::number(r[i], 'f', 1)), bars[i].time, over});
        break;
    }
    // Streak: the most recent run of StreakMin or more candles closing higher (or lower) than the one before.
    {
        QVector<int> dir(n, 0);
        for (int i = 1; i < n; ++i)
            dir[i] = close[i] > close[i - 1] ? 1 : close[i] < close[i - 1] ? -1 : 0;
        int i = n - 1;
        while (i >= 1) {
            if (dir[i] == 0) {
                --i;
                continue;
            }
            int j = i, len = 0;
            while (j >= 1 && dir[j] == dir[i]) {
                ++len;
                --j;
            }
            if (len >= StreakMin) {
                found.push_back({symbol, timeframe, tr("STREAK"), dir[i] > 0 ? tr("STREAK UP") : tr("STREAK DOWN"),
                                 tr("%1 candles").arg(len), bars[i].time, dir[i] < 0});
                break;
            }
            i = j;
        }
    }
    // Gap: the most recent candle opening away from the previous close.
    const double threshold = gapThreshold(timeframe);
    for (int i = n - 1; i > 0; --i) {
        if (!ok(bars[i].open) || close[i - 1] <= 0)
            continue;
        const double gap = 100 * (bars[i].open / close[i - 1] - 1);
        if (std::fabs(gap) >= threshold) {
            found.push_back({symbol, timeframe, tr("GAP"), gap > 0 ? tr("GAP UP") : tr("GAP DOWN"),
                             (gap > 0 ? "+" : "") + QString::number(gap, 'f', 2) + "%", bars[i].time, gap < 0});
            break;
        }
    }
    m_signals.insert(symbol + " " + timeframe, found);
}

void SignalsView::fillTable()
{
    const Palette &c = Theme::current();
    const QString filter = m_filter->text().trimmed().toUpper();
    const int ind = m_indicator->checkedId(), tf = m_timeframe->checkedId();
    static const QStringList indicators {{}, "RSI", "STREAK", "GAP"};
    static const QStringList tfs {{}, "5m", "15m", "1h", "4h", "1d"};
    QVector<Signal> rows;
    for (const QVector<Signal> &list : std::as_const(m_signals))
        for (const Signal &s : list) {
            if (!filter.isEmpty() && !s.symbol.startsWith(filter))
                continue;
            if (ind > 0 && s.indicator != indicators[ind])
                continue;
            if (tf > 0 && s.timeframe != tfs[tf])
                continue;
            rows.push_back(s);
        }
    std::sort(rows.begin(), rows.end(), [](const Signal &a, const Signal &b) {
        return a.time != b.time ? a.time > b.time : a.symbol < b.symbol;
    });
    m_table->setRowCount(rows.size());
    m_table->setVisible(!rows.isEmpty());
    m_empty->setVisible(rows.isEmpty());
    const QColor purple = research::palette::purple(), teal = research::palette::cyan();
    for (int r = 0; r < rows.size(); ++r) {
        const Signal &s = rows[r];
        auto item = [&](const QString &text, const QColor &color, bool bold = false) {
            auto *it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::AlignCenter);
            it->setForeground(color);
            if (bold)
                it->setFont(mono(12, true));
            return it;
        };
        auto centered = [&](QWidget *w) {
            auto *box = new QWidget;
            auto *l = new QHBoxLayout(box);
            l->setContentsMargins(0, 0, 0, 0);
            w->setFixedHeight(20);
            l->addWidget(w, 0, Qt::AlignCenter);
            return box;
        };
        m_table->setItem(r, 0, item(QString::number(r + 1), c.Text));
        m_table->setItem(r, 1, item(s.symbol, c.Text, true));
        m_table->setCellWidget(r, 2, centered(badge(s.timeframe, purple)));
        m_table->setCellWidget(r, 3, centered(badge(s.indicator, teal)));
        m_table->setCellWidget(r, 4, centered(badge(s.signal, s.down ? c.Down : c.Up)));
        m_table->setItem(r, 5, item(s.value, s.down ? c.Down : c.Up, true));
        auto *time = new QLabel(QString("<div style='text-align:center'><b style='color:%1'>%2</b><br><span style='color:%3; font-size:9px'>(%4)</span></div>")
                                    .arg(c.Text.name(), QLocale().toString(s.time.toLocalTime(), "MMM d, hh:mm AP"),
                                         c.TextDim.name(), ago(s.time)));
        time->setAlignment(Qt::AlignCenter);
        time->setTextFormat(Qt::RichText);
        m_table->setCellWidget(r, 6, time);
    }
    int h = m_table->horizontalHeader()->height();
    for (int r = 0; r < m_table->rowCount(); ++r)
        h += m_table->rowHeight(r);
    m_table->setFixedHeight(h + 4);
}

} // namespace quant
