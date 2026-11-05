// The Desk's analysis sections: Technicals, Liquidity, Notes, Workspaces.
#include "DeskViews.h"

#include <QButtonGroup>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QTableWidget>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

namespace deskui {

namespace {

// ------------------------------------------------------------------ Technicals

class TechnicalView : public quant::View
{
    Q_OBJECT
public:
    explicit TechnicalView(QWidget *parent = nullptr)
        : View(parent)
    {
        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Bullish"), &m_bullish, &m_bullishSub), 1);
        tiles->addWidget(tile(tr("Bearish"), &m_bearish, &m_bearishSub), 1);
        tiles->addWidget(tile(tr("Overbought"), &m_overbought, &m_overboughtSub), 1);
        tiles->addWidget(tile(tr("Oversold"), &m_oversold, &m_oversoldSub), 1);
        tiles->addWidget(tile(tr("Near liquidity"), &m_near, &m_nearSub), 1);
        tiles->addWidget(tile(tr("Market lean"), &m_lean, &m_leanSub), 1);
        content()->addLayout(tiles);

        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        m_search = new QLineEdit;
        m_search->setObjectName("qtInput");
        m_search->setPlaceholderText(tr("Filter"));
        m_search->setClearButtonEnabled(true);
        h->addWidget(m_search, 1);
        h->addWidget(quant::segments({tr("All"), tr("FX"), tr("Indices"), tr("Commodities"), tr("Stocks"), tr("Crypto")}, &m_class));
        h->addWidget(quant::segments({tr("All trends"), tr("Bullish"), tr("Neutral"), tr("Bearish")}, &m_trend));
        content()->addWidget(top);

        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        QVBoxLayout *body;
        auto *listCard = quant::card({}, &body);
        body->setContentsMargins(4, 4, 4, 4);
        m_table = table({tr("Market"), tr("Last"), tr("1D"), tr("Trend"), tr("Momentum"), tr("Volatility"), tr("Volume"), tr("Structure"), tr("Liquidity")});
        m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);   // "Pool above +0,12%" stays whole
        m_table->setCursor(Qt::PointingHandCursor);
        body->addWidget(m_table);
        row->addWidget(listCard, 3, Qt::AlignTop);
        auto *fileCard = quant::card(tr("Signal file"), &body);
        fileCard->setMinimumWidth(360);
        m_fileTitle = quant::label("—", "qtTitle");
        body->addWidget(m_fileTitle);
        m_radar = new RadarChart;
        m_radar->setMinimumHeight(250);
        body->addWidget(m_radar);
        m_fileText = quant::label({}, "qtCardLabel");
        m_fileText->setWordWrap(true);
        body->addWidget(m_fileText);
        row->addWidget(fileCard, 1, Qt::AlignTop);
        content()->addLayout(row);
        auto *note = quant::label(tr("Computed here from Yahoo Finance's daily candles: EMA 20 against EMA 50, RSI 14, 20-day volatility against the year's, volume over 5 days against 60 (a listed fund stands in for markets without volume, or none), the last swings, the nearest stop pool. Plain readings, not signals to trade."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        connect(m_search, &QLineEdit::textChanged, this, &TechnicalView::fill);
        connect(m_class, &QButtonGroup::idClicked, this, &TechnicalView::fill);
        connect(m_trend, &QButtonGroup::idClicked, this, &TechnicalView::fill);
        connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            if (auto *item = m_table->item(row, 0))
                select(item->data(Qt::UserRole).toString());
        });
    }

protected:
    void load() override
    {
        QStringList symbols;
        for (const desk::Instrument &i : desk::instruments()) {
            symbols << i.symbol;
            if (!i.volumeProxy.isEmpty() && !symbols.contains(i.volumeProxy))
                symbols << i.volumeProxy;
        }
        setStatus(tr("Loading the markets: 0 of %1…").arg(symbols.size()));
        deskdata::loadCandles(symbols, "2y", "1d", this,
                              [this](const deskdata::Candles &c, const QString &error) {
                                  m_candles = c;
                                  m_signals.clear();
                                  for (const desk::Instrument &i : desk::instruments())
                                      if (c.contains(i.symbol))
                                          m_signals.insert(i.id, desk::computeSignals(c.value(i.symbol), c.value(i.volumeProxy)));
                                  setStatus(c.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
                                  fill();
                                  select(QSettings().value("desk/technical/selected", "EURUSD").toString());
                              },
                              [this](int finished, int total) {
                                  if (finished % 10 == 0)
                                      setStatus(tr("Loading the markets: %1 of %2…").arg(finished).arg(total));
                              });
    }

    void showEvent(QShowEvent *e) override
    {
        View::showEvent(e);
        // The Markets section chooses the market before showing this one.
        if (!m_signals.isEmpty())
            select(QSettings().value("desk/technical/selected", "EURUSD").toString());
    }

private:
    QString trendName(desk::Trend t) const { return t == desk::Trend::Bullish ? tr("Bullish") : t == desk::Trend::Bearish ? tr("Bearish") : tr("Neutral"); }
    QString momentumName(desk::Momentum m) const { return m == desk::Momentum::Overbought ? tr("Overbought") : m == desk::Momentum::Oversold ? tr("Oversold") : tr("Neutral"); }
    QString volName(desk::Volatility v) const { return v == desk::Volatility::High ? tr("High") : v == desk::Volatility::Low ? tr("Low") : tr("Normal"); }
    QString volumeName(desk::VolumeState v) const { return v == desk::VolumeState::Above ? tr("Above avg") : v == desk::VolumeState::Below ? tr("Below avg") : v == desk::VolumeState::Average ? tr("Average") : QString("—"); }
    QString structureName(desk::Structure s) const { return s == desk::Structure::Bullish ? tr("Higher highs") : s == desk::Structure::Bearish ? tr("Lower lows") : tr("Ranging"); }
    QString liquidityName(const desk::Signals &s) const
    {
        switch (s.liquidity) {
        case desk::LiquidityState::PoolAbove: return tr("Pool above %1").arg(pct(s.liquidityDistancePct, 1));
        case desk::LiquidityState::PoolBelow: return tr("Pool below %1").arg(pct(s.liquidityDistancePct, 1));
        case desk::LiquidityState::Near: return tr("Near a pool %1").arg(pct(s.liquidityDistancePct, 1));
        case desk::LiquidityState::Swept: return tr("Swept");
        default: return QString("—");
        }
    }

    void fill()
    {
        const Palette &pal = Theme::current();
        const QString needle = m_search->text().trimmed().toLower();
        const int cls = m_class->checkedId(), trendFilter = m_trend->checkedId();
        int bullish = 0, bearish = 0, overbought = 0, oversold = 0, near = 0, total = 0;
        QVector<const desk::Instrument *> shown;
        for (const desk::Instrument &i : desk::instruments()) {
            const auto it = m_signals.constFind(i.id);
            if (it == m_signals.constEnd() || !it->valid)
                continue;
            ++total;
            bullish += it->trend == desk::Trend::Bullish;
            bearish += it->trend == desk::Trend::Bearish;
            overbought += it->momentum == desk::Momentum::Overbought;
            oversold += it->momentum == desk::Momentum::Oversold;
            near += it->liquidity == desk::LiquidityState::Near;
            if (cls > 0 && int(i.cls) != cls - 1)
                continue;
            if (trendFilter == 1 && it->trend != desk::Trend::Bullish) continue;
            if (trendFilter == 2 && it->trend != desk::Trend::Neutral) continue;
            if (trendFilter == 3 && it->trend != desk::Trend::Bearish) continue;
            if (!needle.isEmpty() && !i.label.toLower().contains(needle) && !i.id.toLower().contains(needle))
                continue;
            shown << &i;
        }
        const auto share = [&](int n) { return total ? tr("%1% of %2").arg(qRound(100.0 * n / total)).arg(total) : QString(); };
        setSigned(m_bullish, QString::number(bullish), 1);
        m_bullishSub->setText(share(bullish));
        setSigned(m_bearish, QString::number(bearish), -1);
        m_bearishSub->setText(share(bearish));
        m_overbought->setText(QString::number(overbought));
        m_overboughtSub->setText(tr("RSI at 70 or more"));
        m_oversold->setText(QString::number(oversold));
        m_oversoldSub->setText(tr("RSI at 30 or less"));
        m_near->setText(QString::number(near));
        m_nearSub->setText(tr("within one daily range of a stop pool"));
        const int lean = total ? qRound(100.0 * (bullish - bearish) / total) : 0;
        setSigned(m_lean, QString("%1%2").arg(lean > 0 ? "+" : "").arg(lean), lean);
        m_leanSub->setText(lean > 10 ? tr("leaning bullish") : lean < -10 ? tr("leaning bearish") : tr("balanced"));

        m_table->setRowCount(shown.size());
        for (int r = 0; r < shown.size(); ++r) {
            const desk::Instrument &i = *shown[r];
            const desk::Signals s = m_signals.value(i.id);
            auto *name = cell(i.label, {}, Qt::AlignLeft, true);
            name->setData(Qt::UserRole, i.id);
            m_table->setItem(r, 0, name);
            m_table->setItem(r, 1, cell(QLocale().toString(s.last, 'f', priceDecimals(s.last))));
            m_table->setItem(r, 2, cell(pct(s.changePct), s.changePct >= 0 ? pal.Up : pal.Down));
            m_table->setItem(r, 3, cell(trendName(s.trend), s.trend == desk::Trend::Bullish ? pal.Up : s.trend == desk::Trend::Bearish ? pal.Down : pal.TextDim));
            m_table->setItem(r, 4, cell(QString("%1 %2").arg(momentumName(s.momentum), amount(s.rsi, 0)), s.momentum == desk::Momentum::Overbought ? pal.Down : s.momentum == desk::Momentum::Oversold ? pal.Up : pal.TextDim));
            m_table->setItem(r, 5, cell(QString("%1 %2%").arg(volName(s.volatility), amount(s.volAnnual, 0)), s.volatility == desk::Volatility::High ? pal.Accent : pal.TextDim));
            m_table->setItem(r, 6, cell(volumeName(s.volume), s.volume == desk::VolumeState::Above ? pal.Accent : pal.TextDim));
            m_table->setItem(r, 7, cell(structureName(s.structure), s.structure == desk::Structure::Bullish ? pal.Up : s.structure == desk::Structure::Bearish ? pal.Down : pal.TextDim));
            m_table->setItem(r, 8, cell(liquidityName(s), s.liquidity == desk::LiquidityState::Near || s.liquidity == desk::LiquidityState::Swept ? pal.Accent : pal.TextDim));
        }
        fitHeight(m_table);
    }

    void select(const QString &id)
    {
        const desk::Instrument *i = desk::findInstrument(id);
        if (!i || !m_signals.contains(i->id))
            return;
        QSettings().setValue("desk/technical/selected", i->id);
        const desk::Signals s = m_signals.value(i->id);
        m_fileTitle->setText(i->label.toUpper());
        m_radar->setData({tr("Trend"), tr("Momentum"), tr("Volatility"), tr("Volume"), tr("Structure"), tr("Liquidity")},
                         {s.radar[0], s.radar[1], s.radar[2], s.radar[3], s.radar[4], s.radar[5]});
        QStringList lines;
        lines << tr("Trend: %1, EMA 20 at %2 from EMA 50.").arg(trendName(s.trend), pct(s.trendGapPct));
        lines << tr("Momentum: RSI %1, %2.").arg(amount(s.rsi, 0), momentumName(s.momentum).toLower());
        lines << tr("Volatility: %1% a year on 20 days, %2% over the period.").arg(amount(s.volAnnual, 1), amount(s.volBaseline, 1));
        if (s.volume != desk::VolumeState::NoData)
            lines << tr("Volume: %1 times its 60-day average.").arg(amount(s.volumeRatio));
        lines << tr("Structure: %1.").arg(structureName(s.structure).toLower());
        if (s.liquidity != desk::LiquidityState::None)
            lines << tr("Liquidity: %1.").arg(liquidityName(s));
        m_fileText->setText(lines.join("\n"));
    }

    QLabel *m_bullish, *m_bullishSub, *m_bearish, *m_bearishSub, *m_overbought, *m_overboughtSub, *m_oversold, *m_oversoldSub, *m_near, *m_nearSub, *m_lean, *m_leanSub;
    QLineEdit *m_search;
    QButtonGroup *m_class, *m_trend;
    QTableWidget *m_table;
    QLabel *m_fileTitle, *m_fileText;
    RadarChart *m_radar;
    deskdata::Candles m_candles;
    QHash<QString, desk::Signals> m_signals;
};

// ------------------------------------------------------------------ Liquidity

class LiquidityView : public quant::View
{
    Q_OBJECT
public:
    explicit LiquidityView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        m_instrument = deskCombo();
        for (const desk::Instrument &i : desk::instruments())
            m_instrument->addItem(i.label, i.id);
        m_instrument->setCurrentIndex(std::max(0, m_instrument->findData(QSettings().value("desk/liquidity/instrument", "EURUSD"))));
        h->addWidget(m_instrument);
        h->addWidget(quant::segments({"1H", tr("Daily")}, &m_timeframe));
        h->addStretch(1);
        content()->addWidget(top);
        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Stop-run likelihood"), &m_stopRun, &m_stopRunSub), 1);
        tiles->addWidget(tile(tr("Untested pools"), &m_untested, &m_untestedSub), 1);
        tiles->addWidget(tile(tr("Pool above"), &m_above, &m_aboveSub), 1);
        tiles->addWidget(tile(tr("Pool below"), &m_below, &m_belowSub), 1);
        tiles->addWidget(tile(tr("Last sweep"), &m_sweep, &m_sweepSub), 1);
        content()->addLayout(tiles);
        QVBoxLayout *body;
        auto *chartCard = quant::card({}, &body);
        m_chart = new LevelsChart;
        body->addWidget(m_chart);
        content()->addWidget(chartCard);
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        auto *poolsCard = quant::card(tr("Liquidity pools"), &body);
        m_pools = table({tr("Side"), tr("Level"), tr("Touches"), tr("Distance"), tr("State")});
        body->addWidget(m_pools);
        row->addWidget(poolsCard, 1, Qt::AlignTop);
        auto *sweepCard = quant::card(tr("Sweep alerts"), &body);
        m_sweeps = table({tr("When"), tr("Side"), tr("Level"), tr("Touches")});
        body->addWidget(m_sweeps);
        row->addWidget(sweepCard, 1, Qt::AlignTop);
        auto *daysCard = quant::card(tr("Session extremes"), &body);
        m_days = table({tr("Day"), tr("High"), tr("Low")});
        body->addWidget(m_days);
        row->addWidget(daysCard, 1, Qt::AlignTop);
        content()->addLayout(row);
        auto *note = quant::label(tr("Stops are assumed to rest beyond swing highs and lows: swings close together make one pool, and a pool is swept when a later bar trades through it. Inferred from price alone, for every market, with no order data. On Binance pairs, the Charts page's liquidity heatmap shows the real order book."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        connect(m_instrument, &QComboBox::currentIndexChanged, this, &LiquidityView::fetch);
        connect(m_timeframe, &QButtonGroup::idClicked, this, &LiquidityView::fetch);
    }

protected:
    void load() override { fetch(); }

private:
    void fetch()
    {
        const desk::Instrument *i = desk::findInstrument(m_instrument->currentData().toString());
        if (!i)
            return;
        QSettings().setValue("desk/liquidity/instrument", i->id);
        const bool hourly = m_timeframe->checkedId() == 0;
        setStatus(tr("Loading %1…").arg(i->label));
        const int serial = ++m_serial;
        deskdata::loadCandles({i->symbol}, hourly ? "1mo" : "1y", hourly ? "60m" : "1d", this, [this, serial, sym = i->symbol, label = i->label, hourly](const deskdata::Candles &c, const QString &error) {
            if (serial != m_serial)
                return;
            const QVector<desk::Candle> bars = c.value(sym);
            setStatus(bars.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
            if (!bars.isEmpty())
                fill(bars, label, hourly);
        });
    }

    void fill(const QVector<desk::Candle> &bars, const QString &label, bool hourly)
    {
        const Palette &pal = Theme::current();
        const desk::Liquidity l = desk::findLiquidity(bars, hourly ? 4 : 3);
        const double last = bars.last().close;
        const int d = priceDecimals(last);
        const QLocale loc;
        const auto price = [&](double v) { return loc.toString(v, 'f', d); };
        QVector<qint64> times;
        QVector<double> closes;
        for (const desk::Candle &b : bars) {
            times << b.time;
            closes << b.close;
        }
        QVector<LevelsChart::Level> levels;
        for (const desk::Pool &p : l.pools) {
            if (p.swept)
                continue;
            levels.push_back({p.price, p.buySide ? pal.Up : pal.Down, QString("%1 ×%2").arg(p.buySide ? "BSL" : "SSL").arg(p.touches), p.touches < 2});
            if (levels.size() >= 12)
                break;
        }
        if (l.previousDayHigh > 0) {
            levels.push_back({l.previousDayHigh, pal.Accent, "PDH", true});
            levels.push_back({l.previousDayLow, pal.Accent, "PDL", true});
        }
        m_chart->setTitle({{label, pal.Text}});
        m_chart->setData(times, closes, levels, d);

        const QStringList runs {tr("Low"), tr("Moderate"), tr("High")};
        m_stopRun->setText(runs[int(l.stopRun)]);
        m_stopRun->setStyleSheet(QString("color: %1").arg((l.stopRun == desk::Liquidity::High ? pal.Accent : pal.Text).name()));
        m_stopRunSub->setText(tr("the nearest pool is %1, by distance in average ranges").arg(l.stopRunBuySideCloser ? tr("above") : tr("below")));
        m_untested->setText(QString::number(l.untested));
        m_untestedSub->setText(tr("of %1 pools found").arg(l.pools.size()));
        const desk::Pool *above = nullptr, *below = nullptr;
        for (const desk::Pool &p : l.pools) {
            if (p.swept) continue;
            if (p.buySide && p.price > last && !above) above = &p;
            if (!p.buySide && p.price < last && !below) below = &p;
        }
        m_above->setText(above ? price(above->price) : "—");
        m_aboveSub->setText(above ? pct(above->distancePct) : QString());
        m_below->setText(below ? price(below->price) : "—");
        m_belowSub->setText(below ? pct(below->distancePct) : QString());
        const auto when = [&](qint64 t) { return loc.toString(QDateTime::fromSecsSinceEpoch(t).toLocalTime(), hourly ? "d MMM HH:mm" : "d MMM yy"); };
        m_sweep->setText(l.sweeps.isEmpty() ? "—" : price(l.sweeps.first().price));
        m_sweepSub->setText(l.sweeps.isEmpty() ? QString() : tr("%1, %2").arg(l.sweeps.first().buySide ? tr("above") : tr("below"), when(l.sweeps.first().time)));

        m_pools->setRowCount(std::min<int>(16, l.pools.size()));
        for (int r = 0; r < m_pools->rowCount(); ++r) {
            const desk::Pool &p = l.pools[r];
            m_pools->setItem(r, 0, cell(p.buySide ? tr("Buy-side") : tr("Sell-side"), p.buySide ? pal.Up : pal.Down));
            m_pools->setItem(r, 1, cell(price(p.price), {}, Qt::AlignCenter, true));
            m_pools->setItem(r, 2, cell(QString::number(p.touches)));
            m_pools->setItem(r, 3, cell(pct(p.distancePct)));
            m_pools->setItem(r, 4, cell(p.swept ? tr("Swept") : tr("Untested"), p.swept ? pal.TextDim : pal.Accent));
        }
        fitHeight(m_pools);
        m_sweeps->setRowCount(std::min<int>(16, l.sweeps.size()));
        for (int r = 0; r < m_sweeps->rowCount(); ++r) {
            const desk::Sweep &s = l.sweeps[r];
            m_sweeps->setItem(r, 0, cell(when(s.time), pal.TextDim));
            m_sweeps->setItem(r, 1, cell(s.buySide ? tr("Buy-side") : tr("Sell-side"), s.buySide ? pal.Up : pal.Down));
            m_sweeps->setItem(r, 2, cell(price(s.price)));
            m_sweeps->setItem(r, 3, cell(QString::number(s.touches)));
        }
        fitHeight(m_sweeps);
        m_days->setRowCount(std::min<int>(16, l.days.size()));
        for (int r = 0; r < m_days->rowCount(); ++r) {
            m_days->setItem(r, 0, cell(loc.toString(l.days[r].date, "ddd d MMM"), pal.TextDim));
            m_days->setItem(r, 1, cell(price(l.days[r].high)));
            m_days->setItem(r, 2, cell(price(l.days[r].low)));
        }
        fitHeight(m_days);
    }

    QComboBox *m_instrument;
    QButtonGroup *m_timeframe;
    QLabel *m_stopRun, *m_stopRunSub, *m_untested, *m_untestedSub, *m_above, *m_aboveSub, *m_below, *m_belowSub, *m_sweep, *m_sweepSub;
    LevelsChart *m_chart;
    QTableWidget *m_pools, *m_sweeps, *m_days;
    int m_serial = 0;
};

// ------------------------------------------------------------------ Notes

struct NoteFeed {
    const char *group, *name, *url;
};
const NoteFeed NoteFeeds[] = {{"ing", "ING Think", "https://think.ing.com/rss/"},
                              {"fed", "Federal Reserve", "https://www.federalreserve.gov/feeds/press_monetary.xml"},
                              {"ecb", "ECB", "https://www.ecb.europa.eu/rss/press.html"},
                              {"boe", "Bank of England", "https://www.bankofengland.co.uk/rss/news"},
                              {"bis", "BIS speeches", "https://www.bis.org/doclist/cbspeeches.rss"}};

class NotesView : public quant::View
{
    Q_OBJECT
public:
    explicit NotesView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        m_publisher = deskCombo();
        m_publisher->addItem(tr("All publishers"), QString());
        for (const NoteFeed &f : NoteFeeds)
            m_publisher->addItem(f.name, QString(f.name));
        h->addWidget(m_publisher);
        m_search = new QLineEdit;
        m_search->setObjectName("qtInput");
        m_search->setPlaceholderText(tr("Search the titles"));
        m_search->setClearButtonEnabled(true);
        h->addWidget(m_search, 1);
        content()->addWidget(top);
        QVBoxLayout *body;
        auto *card = quant::card({}, &body);
        body->setContentsMargins(4, 4, 4, 4);
        m_table = table({tr("Date"), tr("Publisher"), tr("Title")});
        fitColumns(m_table, 2);
        m_table->setCursor(Qt::PointingHandCursor);
        body->addWidget(m_table);
        content()->addWidget(card);
        auto *note = quant::label(tr("Only what is free and offered as a public feed: ING's research and the central banks' own releases and speeches. The large banks' notes and their FX forecasts by quarter are for clients, with no public feed, so they aren't here. A click opens the note in your browser."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        connect(m_publisher, &QComboBox::currentIndexChanged, this, &NotesView::fill);
        connect(m_search, &QLineEdit::textChanged, this, &NotesView::fill);
        connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            const QString url = m_table->item(row, 0)->data(Qt::UserRole).toString();
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        });
    }

protected:
    void load() override
    {
        setStatus(tr("Loading the feeds…"));
        auto left = std::make_shared<int>(int(std::size(NoteFeeds)));
        for (const NoteFeed &f : NoteFeeds)
            deskdata::loadFeed(f.group, f.url, 3600, this, [this, left, name = QString(f.name)](const QVector<news::Item> &items, const QString &error) {
                for (news::Item i : items) {
                    i.publisher = name;
                    m_items << i;
                }
                if (items.isEmpty())
                    m_failed << QString("%1 (%2)").arg(name, error);
                if (--*left == 0) {
                    std::sort(m_items.begin(), m_items.end(), [](const news::Item &a, const news::Item &b) { return a.published > b.published; });
                    setStatus(m_failed.isEmpty() ? QString() : tr("Unavailable: %1").arg(m_failed.join(", ")));
                    fill();
                }
            });
    }

private:
    void fill()
    {
        const Palette &pal = Theme::current();
        const QString publisher = m_publisher->currentData().toString(), needle = m_search->text().trimmed().toLower();
        QVector<const news::Item *> shown;
        for (const news::Item &i : m_items)
            if ((publisher.isEmpty() || i.publisher == publisher) && (needle.isEmpty() || i.title.toLower().contains(needle)))
                shown << &i;
        m_table->setRowCount(std::min<int>(120, shown.size()));
        for (int r = 0; r < m_table->rowCount(); ++r) {
            auto *date = cell(QLocale().toString(shown[r]->published.toLocalTime().date(), "d MMM yyyy"), pal.TextDim);
            date->setData(Qt::UserRole, shown[r]->url);
            m_table->setItem(r, 0, date);
            m_table->setItem(r, 1, cell(shown[r]->publisher, pal.Accent));
            m_table->setItem(r, 2, cell(shown[r]->title, {}, Qt::AlignLeft));
        }
        fitHeight(m_table);
    }

    QComboBox *m_publisher;
    QLineEdit *m_search;
    QTableWidget *m_table;
    QVector<news::Item> m_items;
    QStringList m_failed;
};

// ------------------------------------------------------------------ Workspaces

class WorkspacesView : public QWidget
{
    Q_OBJECT
public:
    explicit WorkspacesView(const Context &c, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_context(c)
    {
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(12, 10, 12, 10);
        v->setSpacing(10);
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        h->addWidget(quant::label(tr("WORKSPACE"), "qtFieldLabel"));
        m_list = deskCombo();
        m_list->setMinimumWidth(190);
        h->addWidget(m_list);
        auto *add = deskAction("+");
        add->setToolTip(tr("New workspace"));
        auto *rename = deskAction(tr("Rename"));
        auto *remove = deskAction("×");
        remove->setToolTip(tr("Delete this workspace"));
        h->addWidget(add);
        h->addWidget(rename);
        h->addWidget(remove);
        h->addSpacing(12);
        h->addWidget(quant::label(tr("ADD A PANEL"), "qtFieldLabel"));
        m_panelChoice = deskCombo();
        const QStringList names = sectionNames();
        for (int i = 0; i < sectionCount(); ++i)
            m_panelChoice->addItem(names[i], i);
        h->addWidget(m_panelChoice);
        auto *addPanel = deskAction(tr("Add"));
        h->addWidget(addPanel);
        h->addStretch(1);
        h->addWidget(quant::label(tr("Each panel is a live copy of a Desk section."), "qtNote"));
        v->addWidget(top);
        m_gridHost = new QWidget;
        m_grid = new QGridLayout(m_gridHost);
        m_grid->setContentsMargins(0, 0, 0, 0);
        m_grid->setSpacing(10);
        v->addWidget(m_gridHost, 1);

        load();
        connect(m_list, &QComboBox::currentIndexChanged, this, [this](int i) {
            if (i >= 0 && !m_syncing) {
                m_current = i;
                save();
                build();
            }
        });
        connect(add, &QPushButton::clicked, this, [this] {
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("New workspace"), tr("Name"), QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok || name.isEmpty())
                return;
            m_spaces.push_back({name, {}});
            m_current = int(m_spaces.size()) - 1;
            save();
            sync();
            build();
        });
        connect(rename, &QPushButton::clicked, this, [this] {
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("Rename the workspace"), tr("Name"), QLineEdit::Normal, m_spaces[m_current].name, &ok).trimmed();
            if (!ok || name.isEmpty())
                return;
            m_spaces[m_current].name = name;
            save();
            sync();
        });
        connect(remove, &QPushButton::clicked, this, [this] {
            if (QMessageBox::question(this, tr("Delete the workspace"), tr("Delete “%1”?").arg(m_spaces[m_current].name)) != QMessageBox::Yes)
                return;
            m_spaces.removeAt(m_current);
            if (m_spaces.isEmpty())
                m_spaces = defaults();
            m_current = std::clamp(m_current, 0, int(m_spaces.size()) - 1);
            save();
            sync();
            build();
        });
        connect(addPanel, &QPushButton::clicked, this, [this] {
            if (m_spaces[m_current].panels.size() >= 6) {
                QMessageBox::information(this, tr("Workspaces"), tr("A workspace holds six panels at most."));
                return;
            }
            m_spaces[m_current].panels << m_panelChoice->currentData().toInt();
            save();
            build();
        });
        sync();
        build();
    }

private:
    struct Space {
        QString name;
        QVector<int> panels;
    };

    QVector<Space> defaults() const { return {{tr("Macro desk"), {1, 2, 4, 5}}}; }

    void load()
    {
        const QJsonObject root = QJsonDocument::fromJson(QSettings().value("desk/workspaces").toByteArray()).object();
        for (const QJsonValue &v : root.value("spaces").toArray()) {
            Space s;
            s.name = v.toObject().value("name").toString();
            for (const QJsonValue &p : v.toObject().value("panels").toArray())
                if (p.toInt() >= 0 && p.toInt() < sectionCount())
                    s.panels << p.toInt();
            m_spaces << s;
        }
        if (m_spaces.isEmpty())
            m_spaces = defaults();
        m_current = std::clamp(root.value("current").toInt(), 0, int(m_spaces.size()) - 1);
    }

    void save() const
    {
        QJsonArray spaces;
        for (const Space &s : m_spaces) {
            QJsonArray panels;
            for (int p : s.panels)
                panels.append(p);
            spaces.append(QJsonObject {{"name", s.name}, {"panels", panels}});
        }
        QSettings().setValue("desk/workspaces", QJsonDocument(QJsonObject {{"spaces", spaces}, {"current", m_current}}).toJson(QJsonDocument::Compact));
    }

    void sync()
    {
        m_syncing = true;
        m_list->clear();
        for (const Space &s : m_spaces)
            m_list->addItem(s.name);
        m_list->setCurrentIndex(m_current);
        m_syncing = false;
    }

    void build()
    {
        while (QLayoutItem *item = m_grid->takeAt(0)) {
            if (QWidget *w = item->widget())
                w->deleteLater();
            delete item;
        }
        for (int r = 0; r < 3; ++r)
            m_grid->setRowStretch(r, 0);
        for (int col = 0; col < 3; ++col)
            m_grid->setColumnStretch(col, 0);
        const QVector<int> panels = m_spaces[m_current].panels;
        const QStringList names = sectionNames();
        const int columns = panels.size() <= 2 ? std::max<int>(1, panels.size()) : panels.size() <= 4 ? 2 : 3;
        for (int i = 0; i < panels.size(); ++i) {
            QVBoxLayout *body;
            QHBoxLayout *head;
            auto *card = quant::card(names.value(panels[i]), &body, &head);
            body->setContentsMargins(2, 6, 2, 2);
            auto *close = new QPushButton("×");
            close->setObjectName("qtIconButton");
            close->setCursor(Qt::PointingHandCursor);
            close->setToolTip(tr("Remove this panel"));
            head->addWidget(close);
            connect(close, &QPushButton::clicked, this, [this, i] {
                m_spaces[m_current].panels.remove(i);
                save();
                build();
            });
            if (QWidget *panel = makeSection(panels[i], m_context))
                body->addWidget(panel, 1);
            m_grid->addWidget(card, i / columns, i % columns);
            m_grid->setRowStretch(i / columns, 1);
            m_grid->setColumnStretch(i % columns, 1);
        }
        if (panels.isEmpty()) {
            auto *empty = quant::label(tr("This workspace is empty: add a panel above."), "qtEmpty");
            empty->setAlignment(Qt::AlignCenter);
            m_grid->addWidget(empty, 0, 0);
            m_grid->setRowStretch(0, 1);
            m_grid->setColumnStretch(0, 1);
        }
    }

    Context m_context;
    QComboBox *m_list, *m_panelChoice;
    QWidget *m_gridHost;
    QGridLayout *m_grid;
    QVector<Space> m_spaces;
    int m_current = 0;
    bool m_syncing = false;
};

} // namespace

QWidget *makeTechnical(const Context &) { return new TechnicalView; }
QWidget *makeLiquidity(const Context &) { return new LiquidityView; }
QWidget *makeNotes(const Context &) { return new NotesView; }
QWidget *makeWorkspaces(const Context &c) { return new WorkspacesView(c); }

} // namespace deskui

#include "DeskViewsTech.moc"
