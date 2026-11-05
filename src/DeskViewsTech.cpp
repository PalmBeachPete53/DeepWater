// The Desk's analysis sections: Technicals, Liquidity, Notes, Workspaces.
#include "DeskViews.h"
#include "NewsReader.h"
#include "quant/ui/Notifications.h"

#include "Myfxbook.h"
#include "QuantData.h"
#include "MacroData.h"
#include "DeskData.h"
#include "desk/Positioning.h"
#include "desk/Rates.h"
#include <QTimer>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QToolButton>
#include <QShortcut>
#include <QFrame>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
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
        m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);   // "Plus bas décroissants" too
        m_table->setCursor(Qt::PointingHandCursor);
        body->addWidget(m_table);
        row->addWidget(listCard, 3, Qt::AlignTop);
        auto *fileCard = quant::card(tr("Signal file"), &body);
        fileCard->setMinimumWidth(360);
        m_fileTitle = quant::label("—", "qtTitle");
        m_fileTitle->setWordWrap(true);
        body->addWidget(m_fileTitle);
        m_fileHint = quant::label({}, "qtNote");
        m_fileHint->setWordWrap(true);
        body->addWidget(m_fileHint);
        m_compared = QSettings().value("desk/technical/compare").toString();
        m_radar = new RadarChart;
        m_radar->setMinimumHeight(280);
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
                clickMarket(item->data(Qt::UserRole).toString());
        });
        m_table->viewport()->installEventFilter(this);
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

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // Shift as it was for this very click: cellClicked doesn't say.
        if (watched == m_table->viewport() && event->type() == QEvent::MouseButtonRelease)
            m_shiftClick = static_cast<QMouseEvent *>(event)->modifiers().testFlag(Qt::ShiftModifier);
        return View::eventFilter(watched, event);
    }

    void showEvent(QShowEvent *e) override
    {
        View::showEvent(e);
        // The Markets section chooses the market before showing this one: it comes alone.
        if (!m_signals.isEmpty()) {
            const QString chosen = QSettings().value("desk/technical/selected", "EURUSD").toString();
            if (chosen != m_selected)
                setCompared({});
            select(chosen);
        }
    }

public:
    static QString trendName(desk::Trend t) { return t == desk::Trend::Bullish ? tr("Bullish") : t == desk::Trend::Bearish ? tr("Bearish") : tr("Neutral"); }
    static QString momentumName(desk::Momentum m) { return m == desk::Momentum::Overbought ? tr("Overbought") : m == desk::Momentum::Oversold ? tr("Oversold") : tr("Neutral"); }
    static QString volName(desk::Volatility v) { return v == desk::Volatility::High ? tr("High") : v == desk::Volatility::Low ? tr("Low") : tr("Normal"); }
    static QString volumeName(desk::VolumeState v) { return v == desk::VolumeState::Above ? tr("Above avg") : v == desk::VolumeState::Below ? tr("Below avg") : v == desk::VolumeState::Average ? tr("Average") : QString("—"); }
    static QString structureName(desk::Structure s) { return s == desk::Structure::Bullish ? tr("Higher highs") : s == desk::Structure::Bearish ? tr("Lower lows") : tr("Ranging"); }
    static QString liquidityName(const desk::Signals &s)
    {
        switch (s.liquidity) {
        case desk::LiquidityState::PoolAbove: return tr("Pool above %1").arg(pct(s.liquidityDistancePct, 1));
        case desk::LiquidityState::PoolBelow: return tr("Pool below %1").arg(pct(s.liquidityDistancePct, 1));
        case desk::LiquidityState::Near: return tr("Near a pool %1").arg(pct(s.liquidityDistancePct, 1));
        case desk::LiquidityState::Swept: return tr("Swept");
        default: return QString("—");
        }
    }
    static QStringList signalNames() { return {tr("Trend"), tr("Momentum"), tr("Volatility"), tr("Volume"), tr("Structure"), tr("Liquidity")}; }
    static QVector<double> radarValues(const desk::Signals &v) { return {v.radar[0], v.radar[1], v.radar[2], v.radar[3], v.radar[4], v.radar[5]}; }

private:
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
        markRows();
    }

    void setCompared(const QString &id)
    {
        m_compared = id;
        QSettings().setValue("desk/technical/compare", id);
    }

    // A click shows a market alone. Shift+click adds one to the comparison, or takes the place of the second; on one of
    // the two, it takes it out and the other stays. There is always one market in the file.
    void clickMarket(const QString &id)
    {
        if (!m_shiftClick) {
            setCompared({});
            select(id);
            return;
        }
        if (id == m_selected && m_compared.isEmpty())
            return;
        if (id == m_selected) {
            const QString remaining = m_compared;
            setCompared({});
            select(remaining);
            return;
        }
        setCompared(id == m_compared ? QString() : id);
        select(m_selected);
    }

    // The two markets stand out in the list, with the point and the colour they have on the radar.
    void markRows()
    {
        const auto marker = [](const QColor &color, bool square) {
            QPixmap pixmap(20, 20);
            pixmap.setDevicePixelRatio(2);
            pixmap.fill(Qt::transparent);
            if (color.isValid()) {
                QPainter p(&pixmap);
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(Qt::NoPen);
                p.setBrush(color);
                if (square)
                    p.drawRect(QRectF(1.5, 1.5, 7, 7));
                else
                    p.drawEllipse(QRectF(1, 1, 8, 8));
            }
            return QIcon(pixmap);
        };
        const QColor first = Theme::current().Accent, second = RadarChart::overlayColor();
        const QIcon none = marker({}, false), firstMark = marker(first, false), secondMark = marker(second, true);
        const bool comparing = compared() != nullptr;
        for (int r = 0; r < m_table->rowCount(); ++r) {
            QTableWidgetItem *name = m_table->item(r, 0);
            if (!name)
                continue;
            const QString id = name->data(Qt::UserRole).toString();
            const bool isFirst = id == m_selected, isSecond = comparing && id == m_compared;
            QColor tint = isFirst ? first : second;
            tint.setAlpha(36);
            for (int col = 0; col < m_table->columnCount(); ++col)
                if (QTableWidgetItem *item = m_table->item(r, col))
                    item->setBackground(isFirst || isSecond ? QBrush(tint) : QBrush());
            name->setIcon(isFirst ? firstMark : isSecond ? secondMark : none);
        }
    }

    // The market compared with the selected one, if there is one to show.
    const desk::Instrument *compared() const
    {
        const desk::Instrument *other = desk::findInstrument(m_compared);
        return other && other->id != m_selected && m_signals.value(other->id).valid ? other : nullptr;
    }

    void select(const QString &id)
    {
        const desk::Instrument *i = desk::findInstrument(id);
        if (!i || !m_signals.contains(i->id))
            return;
        m_selected = i->id;
        QSettings().setValue("desk/technical/selected", i->id);
        const desk::Signals s = m_signals.value(i->id);
        const desk::Instrument *other = compared();
        const desk::Signals o = other ? m_signals.value(other->id) : desk::Signals {};
        m_fileTitle->setText(other ? tr("%1 vs %2").arg(i->label, other->label).toUpper() : i->label.toUpper());
        m_radar->setData(signalNames(), radarValues(s));
        m_radar->setOverlay(i->label, other ? other->label : QString(), other ? radarValues(o) : QVector<double> {});
        m_fileText->setText(other ? QString("%1\n%2\n\n%3\n%4").arg(i->label, fileLines(s), other->label, fileLines(o)) : fileLines(s));
        m_fileHint->setText(other ? tr("Shift+click another market to replace %1, or one of these two to take it out.").arg(other->label)
                                  : tr("Shift+click another market in the list to compare it with this one."));
        markRows();
    }

public:
    static QString fileLines(const desk::Signals &s)
    {
        QStringList lines;
        lines << tr("Trend: %1, EMA 20 at %2 from EMA 50.").arg(trendName(s.trend), pct(s.trendGapPct));
        lines << tr("Momentum: RSI %1, %2.").arg(amount(s.rsi, 0), momentumName(s.momentum).toLower());
        lines << tr("Volatility: %1% a year on 20 days, %2% over the period.").arg(amount(s.volAnnual, 1), amount(s.volBaseline, 1));
        if (s.volume != desk::VolumeState::NoData)
            lines << tr("Volume: %1 times its 60-day average.").arg(amount(s.volumeRatio));
        else
            lines << tr("Volume: none for this market, drawn at the middle of the web.");
        lines << tr("Structure: %1.").arg(structureName(s.structure).toLower());
        if (s.liquidity != desk::LiquidityState::None)
            lines << tr("Liquidity: %1.").arg(liquidityName(s));
        return lines.join("\n");
    }

private:
    QLabel *m_bullish, *m_bullishSub, *m_bearish, *m_bearishSub, *m_overbought, *m_overboughtSub, *m_oversold, *m_oversoldSub, *m_near, *m_nearSub, *m_lean, *m_leanSub;
    QLineEdit *m_search;
    QButtonGroup *m_class, *m_trend;
    QTableWidget *m_table;
    QLabel *m_fileTitle, *m_fileHint, *m_fileText;
    QString m_selected, m_compared;
    bool m_shiftClick = false;
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
        m_notify = new QCheckBox(tr("Notify me of a new sweep"));
        m_notify->setObjectName("qtCheck");
        m_notify->setToolTip(tr("For the market and timeframe shown, checked every 15 minutes while Deepwater is open, on delayed prices"));
        m_notify->setChecked(QSettings().value("desk/liquidity/notify", false).toBool());
        h->addWidget(m_notify);
        connect(m_notify, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue("desk/liquidity/notify", on);
            if (on)
                qtm::Notifications::instance()->requestPermission();
            updateWatch();
        });
        m_watch.setInterval(15 * 60 * 1000);
        connect(&m_watch, &QTimer::timeout, this, &LiquidityView::fetch);
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
    void load() override
    {
        fetch();
        updateWatch();
    }

private:
    void updateWatch()
    {
        if (m_notify->isChecked())
            m_watch.start();
        else
            m_watch.stop();
    }

    // Only what appeared since the last look: the first look at a market sets the mark and says nothing.
    void notifySweeps(const QString &id, const QString &label, const desk::Liquidity &l, bool hourly)
    {
        if (l.sweeps.isEmpty())
            return;
        const QString key = QString("desk/liquidity/seen/%1/%2").arg(id, hourly ? "1h" : "1d");
        QSettings settings;
        const bool known = settings.contains(key);
        const QVector<desk::Sweep> fresh = desk::sweepsSince(l.sweeps, settings.value(key).toLongLong());
        settings.setValue(key, qint64(l.sweeps.first().time));
        if (!known || !m_notify->isChecked())
            return;
        for (const desk::Sweep &s : fresh)
            qtm::Notifications::instance()->notify(tr("%1: stops swept").arg(label),
                                                   tr("The pool %1 at %2 was taken out").arg(s.buySide ? tr("above") : tr("below"), QLocale().toString(s.price, 'f', priceDecimals(s.price))));
    }

    void fetch()
    {
        const desk::Instrument *i = desk::findInstrument(m_instrument->currentData().toString());
        if (!i)
            return;
        QSettings().setValue("desk/liquidity/instrument", i->id);
        const bool hourly = m_timeframe->checkedId() == 0;
        setStatus(tr("Loading %1…").arg(i->label));
        const int serial = ++m_serial;
        deskdata::loadCandles({i->symbol}, hourly ? "1mo" : "1y", hourly ? "60m" : "1d", this, [this, serial, sym = i->symbol, id = i->id, label = i->label, hourly](const deskdata::Candles &c, const QString &error) {
            if (serial != m_serial)
                return;
            const QVector<desk::Candle> bars = c.value(sym);
            setStatus(bars.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
            if (!bars.isEmpty())
                fill(bars, label, hourly);
            if (!bars.isEmpty())
                notifySweeps(id, label, desk::findLiquidity(bars, hourly ? 4 : 3), hourly);
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

    QCheckBox *m_notify;
    QTimer m_watch;
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
    explicit NotesView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
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
        auto *note = quant::label(tr("Only what is free and offered as a public feed: ING's research and the central banks' own releases and speeches. A click opens the note here. The large banks' client research has no public feed; ING's forecasts are in Forecasts."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        connect(m_publisher, &QComboBox::currentIndexChanged, this, &NotesView::fill);
        connect(m_search, &QLineEdit::textChanged, this, &NotesView::fill);
        connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            const QString url = m_table->item(row, 0)->data(Qt::UserRole).toString();
            if (url.isEmpty())
                return;
            news::Item item;
            item.url = url;
            item.title = m_table->item(row, 2)->text();
            item.publisher = m_table->item(row, 1)->text();
            item.site = QUrl(url).host();
            if (!m_reader) {
                m_reader = new newsui::Reader(this);
                if (m_context.page)
                    connect(m_reader, &newsui::Reader::askAi, m_context.page, &DeskPage::askAi);
            }
            m_reader->open(item);
        });
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        View::resizeEvent(e);
        if (m_reader && m_reader->isOpen())
            m_reader->setGeometry(rect());
    }
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

    Context m_context;
    newsui::Reader *m_reader = nullptr;
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

    static QVector<Workspace> defaults() { return {{tr("Macro desk"), {1, 2, 4, 5}}}; }

    void reload()
    {
        m_spaces = loadWorkspaces(&m_current);
        sync();
        build();
    }

private:
    void load() { m_spaces = loadWorkspaces(&m_current); }
    void save() const { saveWorkspaces(m_spaces, m_current); }

    void sync()
    {
        m_syncing = true;
        m_list->clear();
        for (const Workspace &s : m_spaces)
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
    QVector<Workspace> m_spaces;
    int m_current = 0;
    bool m_syncing = false;
};

// ------------------------------------------------------------------ One market's sheet

class MarketSheet : public QWidget
{
    Q_OBJECT
public:
    MarketSheet(const Context &c, QWidget *parent)
        : QWidget(parent)
        , m_context(c)
    {
        setGeometry(parent->rect());
        parent->installEventFilter(this);
        m_panel = new QFrame(this);
        m_panel->setObjectName("newsReader");
        m_panel->setAttribute(Qt::WA_StyledBackground);
        auto *outer = new QVBoxLayout(m_panel);
        outer->setContentsMargins(16, 12, 16, 14);
        outer->setSpacing(10);

        auto *head = new QHBoxLayout;
        auto *names = new QVBoxLayout;
        names->setSpacing(1);
        m_title = quant::label({}, "qtTitle");
        m_sub = quant::label({}, "qtCardLabel");
        names->addWidget(m_title);
        names->addWidget(m_sub);
        head->addLayout(names, 1);
        auto *technicals = deskAction(tr("Technicals"));
        auto *rates = deskAction(tr("Rates"));
        head->addWidget(technicals);
        head->addWidget(rates);
        auto *close = new QToolButton;
        close->setObjectName("newsReaderButton");
        close->setText("✕");
        close->setToolTip(tr("Close (Esc)"));
        close->setCursor(Qt::PointingHandCursor);
        head->addWidget(close);
        outer->addLayout(head);

        QVBoxLayout *body;
        auto *top = new QHBoxLayout;
        top->setSpacing(10);
        auto *chartCard = quant::card(QStringLiteral("—"), &body);
        m_chartTitle = chartCard->findChild<QLabel *>();
        m_chart = new LevelsChart;
        m_chart->setMinimumHeight(250);
        body->addWidget(m_chart, 1);
        top->addWidget(chartCard, 3);
        auto *signalCard = quant::card(tr("Signals"), &body);
        auto *signalRow = new QHBoxLayout;
        m_radar = new RadarChart;
        m_radar->setMinimumWidth(240);
        signalRow->addWidget(m_radar, 3);
        m_signals = quant::label({}, "qtCardLabel");
        m_signals->setWordWrap(true);
        signalRow->addWidget(m_signals, 2);
        body->addLayout(signalRow, 1);
        top->addWidget(signalCard, 3);
        outer->addLayout(top, 1);

        auto *bottom = new QHBoxLayout;
        bottom->setSpacing(10);
        auto *crowdCard = quant::card(tr("Crowd"), &body);
        m_crowd = table({{}, tr("Long"), tr("Short")});
        m_crowd->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        body->addWidget(m_crowd);
        m_crowdNote = quant::label({}, "qtNote");
        m_crowdNote->setWordWrap(true);
        body->addWidget(m_crowdNote);
        body->addStretch(1);
        bottom->addWidget(crowdCard, 4, Qt::AlignTop);
        auto *cotCard = quant::card(tr("Commitments of Traders"), &body);
        m_cot = table({tr("Contract"), tr("Spec. net"), tr("Week"), tr("Range")});
        m_cot->setToolTip(tr("Range: where the speculators' net stands in its last 52 weeks"));
        for (int col = 1; col < 4; ++col)
            m_cot->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
        body->addWidget(m_cot);
        m_cotNote = quant::label({}, "qtNote");
        m_cotNote->setWordWrap(true);
        body->addWidget(m_cotNote);
        body->addStretch(1);
        bottom->addWidget(cotCard, 5, Qt::AlignTop);
        auto *ratesCard = quant::card(tr("Policy rates"), &body);
        m_rates = table({tr("Currency"), tr("Rate"), tr("Decision"), tr("In")});
        m_rates->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        body->addWidget(m_rates);
        m_ratesNote = quant::label({}, "qtNote");
        m_ratesNote->setWordWrap(true);
        body->addWidget(m_ratesNote);
        body->addStretch(1);
        bottom->addWidget(ratesCard, 4, Qt::AlignTop);
        outer->addLayout(bottom);

        connect(close, &QToolButton::clicked, this, &QWidget::hide);
        connect(technicals, &QPushButton::clicked, this, [this] {
            QSettings().setValue("desk/technical/selected", m_id);
            hide();
            if (m_context.page)
                m_context.page->showSection(10);
        });
        connect(rates, &QPushButton::clicked, this, [this] {
            hide();
            if (m_context.page)
                m_context.page->showSection(5);
        });
        auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        esc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(esc, &QShortcut::activated, this, &QWidget::hide);
        connect(Myfxbook::instance(), &Myfxbook::changed, this, [this] {
            if (isVisible())
                fillCrowd();
        });
        hide();
    }

    void open(const desk::Instrument &i)
    {
        m_id = i.id;
        m_bars.clear();
        m_last = 0;
        const int serial = ++m_serial;
        m_title->setText(i.label.toUpper());
        m_sub->setText(tr("Loading the prices…"));
        m_chart->setData({}, {}, {}, 2);
        m_radar->setData({}, {});
        m_signals->clear();
        fillCrowd();
        fillCot(i);
        fillRates(i);
        placePanel();
        show();
        raise();
        setFocus();
        Myfxbook::instance()->refreshIfStale();
        deskdata::loadCandles({i.symbol, i.volumeProxy}, "2y", "1d", this, [this, serial, i](const deskdata::Candles &c, const QString &error) {
            if (serial != m_serial)
                return;
            m_bars = c.value(i.symbol);
            const desk::Signals s = desk::computeSignals(m_bars, c.value(i.volumeProxy));
            if (!s.valid) {
                m_sub->setText(tr("Prices unavailable: %1").arg(error));
                return;
            }
            m_last = s.last;
            m_sub->setText(tr("%1 · last %2 · %3 on the day, delayed").arg(className(i.cls), QLocale().toString(s.last, 'f', priceDecimals(s.last)), pct(s.changePct)));
            m_radar->setData(TechnicalView::signalNames(), TechnicalView::radarValues(s));
            m_signals->setText(TechnicalView::fileLines(s));
            fillCrowd();
        });
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QColor dim = Theme::current().Background;
        dim.setAlpha(170);
        p.fillRect(rect(), dim);
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (!m_panel->geometry().contains(e->pos()))
            hide();
    }
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parent() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
            placePanel();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    static QString className(desk::AssetClass c)
    {
        switch (c) {
        case desk::AssetClass::Fx: return tr("Forex");
        case desk::AssetClass::Index: return tr("Indices");
        case desk::AssetClass::Commodity: return tr("Commodities");
        case desk::AssetClass::Stock: return tr("Stocks");
        default: return tr("Crypto");
        }
    }

    // A table with nothing to show leaves only its note.
    static void fitTable(QTableWidget *t)
    {
        t->setVisible(t->rowCount() > 0);
        fitHeight(t);
    }

    void placePanel()
    {
        const int w = std::min(1240, width() - 32), h = std::min(800, height() - 24);
        m_panel->setGeometry((width() - w) / 2, (height() - h) / 2, std::max(320, w), std::max(240, h));
    }

    // The year of closes, with where the crowd entered when enough positions say so.
    void drawChart(const desk::OutlookRow *row)
    {
        const Palette &pal = Theme::current();
        // By date, not a count of bars: crypto trades every day.
        const qint64 yearAgo = QDateTime::currentDateTimeUtc().addYears(-1).toSecsSinceEpoch();
        QVector<qint64> times;
        QVector<double> closes;
        for (const desk::Candle &bar : m_bars) {
            if (bar.time < yearAgo)
                continue;
            times << bar.time;
            closes << bar.close;
        }
        QVector<LevelsChart::Level> levels;
        if (row && row->positions() >= desk::OutlookMinPositions) {
            if (row->avgLongPrice > 0)
                levels.push_back({row->avgLongPrice, pal.Up, tr("Long"), true});
            if (row->avgShortPrice > 0)
                levels.push_back({row->avgShortPrice, pal.Down, tr("Short"), true});
        }
        m_chart->setData(times, closes, levels, priceDecimals(m_last));
        m_chartTitle->setText((levels.isEmpty() ? tr("Price over a year") : tr("Price over a year, with the crowd's average entries")).toUpper());
    }

    void fillCrowd()
    {
        const desk::Instrument *i = desk::findInstrument(m_id);
        if (!i)
            return;
        const Palette &pal = Theme::current();
        m_crowd->setRowCount(0);
        m_crowdNote->clear();
        const auto addRow = [&](const QString &name, const QString &a, const QString &b, const QColor &ca = {}, const QColor &cb = {}) {
            const int r = m_crowd->rowCount();
            m_crowd->setRowCount(r + 1);
            m_crowd->setItem(r, 0, cell(name, {}, Qt::AlignLeft));
            m_crowd->setItem(r, 1, cell(a, ca));
            m_crowd->setItem(r, 2, cell(b, cb));
        };
        const desk::OutlookRow *row = nullptr;
        if (i->cls == desk::AssetClass::Crypto) {
            const QString pair = i->id.left(i->id.size() - 3) + "USDT";
            const int serial = m_serial;
            drawChart(nullptr);
            deskdata::loadLongShare(pair, this, [this, serial](const QVector<double> &longs, const QString &) {
                if (serial != m_serial)
                    return;
                if (longs.isEmpty()) {
                    m_crowdNote->setText(tr("Binance publishes no ratio for this market."));
                } else {
                    m_crowd->setRowCount(1);
                    m_crowd->setItem(0, 0, cell(tr("Accounts"), {}, Qt::AlignLeft));
                    m_crowd->setItem(0, 1, cell(QString("%1%").arg(amount(longs.last(), 0))));
                    m_crowd->setItem(0, 2, cell(QString("%1%").arg(amount(100 - longs.last(), 0))));
                    m_crowdNote->setText(tr("Binance's USDT perpetual: accounts, not money."));
                }
                fitTable(m_crowd);
            });
            fitTable(m_crowd);
            return;
        }
        for (const desk::OutlookRow &candidate : Myfxbook::instance()->outlook().rows) {
            const desk::Instrument *found = desk::findInstrument(candidate.symbol);
            if (found && found->id == i->id) {
                row = &candidate;
                break;
            }
        }
        drawChart(row);
        if (!row) {
            m_crowdNote->setText(Myfxbook::instance()->isConnected() ? tr("Myfxbook has no figure for this market.")
                                                                     : tr("Connect your Myfxbook account in Desk › Positioning to see the crowd here."));
            fitTable(m_crowd);
            return;
        }
        addRow(tr("Lots"), QString("%1%").arg(amount(row->longPct, 0)), QString("%1%").arg(amount(row->shortPct, 0)));
        addRow(tr("Positions"), QLocale().toString(row->longPositions), QLocale().toString(row->shortPositions));
        const int d = priceDecimals(m_last > 0 ? m_last : row->avgLongPrice);
        addRow(tr("Average entry"), row->avgLongPrice > 0 ? QLocale().toString(row->avgLongPrice, 'f', d) : QString("—"),
               row->avgShortPrice > 0 ? QLocale().toString(row->avgShortPrice, 'f', d) : QString("—"));
        if (m_last > 0) {
            const double up = desk::crowdLongGain(*row, m_last), down = desk::crowdShortGain(*row, m_last);
            addRow(tr("Against the last price"), row->avgLongPrice > 0 ? pct(up) : QString("—"), row->avgShortPrice > 0 ? pct(down) : QString("—"),
                   up >= 0 ? pal.Up : pal.Down, down >= 0 ? pal.Up : pal.Down);
        }
        m_crowdNote->setText(row->positions() < desk::OutlookMinPositions
                                 ? tr("Only %1 positions on Myfxbook: too few to read, and left off the chart.").arg(row->positions())
                                 : tr("Myfxbook's community, accounts it follows, some on demo money. The last line sets the average entry against the price, before costs: an estimate, not their real result."));
        fitTable(m_crowd);
    }

    void fillCot(const desk::Instrument &i)
    {
        const QVector<desk::CotLink> links = desk::cotLinks(i);
        m_cot->setRowCount(0);
        m_cotNote->clear();
        if (links.isEmpty() || !m_context.cot) {
            m_cotNote->setText(tr("The CFTC publishes no contract for this market."));
            fitTable(m_cot);
            return;
        }
        m_context.cot->waitCached();
        const Palette &pal = Theme::current();
        QDate reported;
        bool inverse = false;
        for (const desk::CotLink &link : links) {
            const CotReport::Market *market = nullptr;
            for (const CotReport::Market &m : m_context.cot->markets())
                if (m.id == link.market)
                    market = &m;
            if (!market)
                continue;
            const QVector<CotReport::Week> &weeks = m_context.cot->series(link.market).weeks;
            if (weeks.isEmpty())
                continue;
            QVector<double> nets;
            for (const CotReport::Week &w : weeks)
                nets << double(w.ncNet());
            const double change = weeks.size() > 1 ? nets.last() - nets[nets.size() - 2] : 0;
            const int r = m_cot->rowCount();
            m_cot->setRowCount(r + 1);
            m_cot->setItem(r, 0, cell(link.inverse ? tr("%1 (inv.)").arg(market->label) : market->label, {}, Qt::AlignLeft));
            m_cot->setItem(r, 1, cell(signedAmount(nets.last(), 0), nets.last() >= 0 ? pal.Up : pal.Down));
            m_cot->setItem(r, 2, cell(signedAmount(change, 0), change >= 0 ? pal.Up : pal.Down));
            m_cot->setItem(r, 3, cell(QString("%1%").arg(amount(desk::rangePosition(nets), 0))));
            reported = weeks.last().date;
            inverse = inverse || link.inverse;
        }
        if (m_cot->rowCount() == 0)
            m_cotNote->setText(tr("The CFTC's report isn't loaded yet."));
        else
            m_cotNote->setText((inverse ? tr("Inv.: speculators long that contract are short this pair's price. ") : QString())
                               + tr("Positions of %1, in contracts. Range: 0% is the year's lowest net, 100% the highest.").arg(QLocale().toString(reported, QLocale::ShortFormat)));
        fitTable(m_cot);
    }

    void fillRates(const desk::Instrument &i)
    {
        const int serial = m_serial;
        const QStringList currencies = desk::rateCurrencies(i);
        QStringList known;
        for (const QString &c : currencies)
            if (m_context.rates && m_context.rates->hasRate(c))
                known << c;
        m_rates->setRowCount(known.size());
        const QDate today = QDate::currentDate();
        for (int r = 0; r < known.size(); ++r) {
            const QString currency = known[r];
            m_rates->setItem(r, 0, cell(currency, {}, Qt::AlignLeft, true));
            m_rates->setItem(r, 1, cell(QString("%1%").arg(amount(m_context.rates->current(currency)))));
            m_rates->setItem(r, 2, cell("—"));
            m_rates->setItem(r, 3, cell("—"));
            deskdata::loadMeetings(currency, this, [this, serial, r, today](const QVector<QDate> &dates) {
                if (serial != m_serial)
                    return;
                for (const QDate &d : dates) {
                    if (d < today)
                        continue;
                    m_rates->setItem(r, 2, cell(QLocale().toString(d, "d MMM")));
                    m_rates->setItem(r, 3, cell(d == today ? tr("today") : tr("%1 d").arg(today.daysTo(d))));
                    break;
                }
            });
        }
        m_ratesNote->setText(known.size() == 2
                                 ? tr("%1 − %2: %3 points. Policy rates from the BIS, dates from the banks. Rates has what the market prices.")
                                       .arg(known[0], known[1], signedAmount(m_context.rates->current(known[0]) - m_context.rates->current(known[1])))
                                 : known.isEmpty() ? tr("The BIS publishes no policy rate for this market's currency.")
                                                   : tr("The rate of the currency this market is priced in, from the BIS; dates from the bank."));
        fitTable(m_rates);
    }

    Context m_context;
    QFrame *m_panel;
    QLabel *m_title, *m_sub, *m_chartTitle, *m_signals, *m_crowdNote, *m_cotNote, *m_ratesNote;
    LevelsChart *m_chart;
    RadarChart *m_radar;
    QTableWidget *m_crowd, *m_cot, *m_rates;
    QString m_id;
    QVector<desk::Candle> m_bars;
    double m_last = 0;
    int m_serial = 0;
};

} // namespace

QVector<Workspace> loadWorkspaces(int *current)
{
    QVector<Workspace> spaces;
    const QJsonObject root = QJsonDocument::fromJson(QSettings().value("desk/workspaces").toByteArray()).object();
    for (const QJsonValue &v : root.value("spaces").toArray()) {
        Workspace s;
        s.name = v.toObject().value("name").toString();
        for (const QJsonValue &p : v.toObject().value("panels").toArray())
            if (p.toInt() >= 0 && p.toInt() < sectionCount())
                s.panels << p.toInt();
        spaces << s;
    }
    if (spaces.isEmpty())
        spaces = WorkspacesView::defaults();
    if (current)
        *current = std::clamp(root.value("current").toInt(), 0, int(spaces.size()) - 1);
    return spaces;
}

void saveWorkspaces(const QVector<Workspace> &spaces, int current)
{
    QJsonArray list;
    for (const Workspace &s : spaces) {
        QJsonArray panels;
        for (int p : s.panels)
            panels.append(p);
        list.append(QJsonObject {{"name", s.name}, {"panels", panels}});
    }
    QSettings().setValue("desk/workspaces", QJsonDocument(QJsonObject {{"spaces", list}, {"current", current}}).toJson(QJsonDocument::Compact));
}

void refreshWorkspaces(QWidget *view)
{
    if (auto *workspaces = qobject_cast<WorkspacesView *>(view))
        workspaces->reload();
}

void openMarketSheet(const QString &id, const Context &c, QWidget *view)
{
    const desk::Instrument *i = desk::findInstrument(id);
    if (!i)
        return;
    QWidget *host = c.page ? static_cast<QWidget *>(c.page) : view;
    auto *sheet = host->findChild<MarketSheet *>(QString(), Qt::FindDirectChildrenOnly);
    if (!sheet)
        sheet = new MarketSheet(c, host);
    sheet->open(*i);
}

QWidget *makeTechnical(const Context &) { return new TechnicalView; }
QWidget *makeLiquidity(const Context &) { return new LiquidityView; }
QWidget *makeNotes(const Context &c) { return new NotesView(c); }
QWidget *makeWorkspaces(const Context &c) { return new WorkspacesView(c); }

} // namespace deskui

#include "DeskViewsTech.moc"
