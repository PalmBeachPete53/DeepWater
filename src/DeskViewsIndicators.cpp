#include "DeskPage.h"
#include "DeskData.h"
#include "DeskViews.h"
#include "DeskWidgets.h"
#include "QuantCharts.h"
#include "QuantViews.h"
#include "Theme.h"
#include "desk/IndicatorRead.h"
#include "desk/Instruments.h"
#include "quant/core/Indicators.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QSettings>
#include <QSpinBox>
#include <QTimeZone>
#include <QVBoxLayout>

#include <cmath>

namespace deskui {

namespace {

constexpr int ShownBars = 130;   // about six months: enough to see the indicator, the price still legible

// One market through six usual indicators, each with its settings and its state in words.
class IndicatorsView : public quant::View
{
    Q_OBJECT
public:
    explicit IndicatorsView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        h->addWidget(quant::label(tr("MARKET"), "qtFieldLabel"));
        m_market = deskCombo();
        for (const desk::Instrument &i : desk::instruments())
            m_market->addItem(i.label, i.id);
        m_market->setCurrentIndex(std::max(0, m_market->findData(QSettings().value("desk/indicators/market", "EURUSD"))));
        h->addWidget(m_market);
        h->addStretch(1);
        content()->addWidget(top);

        auto *grid = new QGridLayout;
        grid->setSpacing(10);
        addCard(grid, 0, 0, "supertrend", tr("Supertrend"), {{"atrLength", tr("ATR"), 10, 1, 100, false}, {"factor", tr("Factor"), 3, 0.5, 10, true}});
        addCard(grid, 0, 1, "bb", tr("Bollinger Bands"), {{"length", tr("Length"), 20, 2, 200, false}, {"mult", tr("Deviations"), 2, 0.5, 5, true}});
        addCard(grid, 0, 2, "ema", tr("Moving averages"), {{"fast", tr("Fast"), 20, 2, 200, false}, {"slow", tr("Slow"), 50, 3, 400, false}});
        addCard(grid, 1, 0, "rsi", tr("RSI"), {{"length", tr("Length"), 14, 2, 100, false}, {"oversold", tr("Low line"), 30, 1, 49, false}, {"overbought", tr("High line"), 70, 51, 99, false}});
        addCard(grid, 1, 1, "macd", tr("MACD"), {{"fast", tr("Fast"), 12, 2, 100, false}, {"slow", tr("Slow"), 26, 3, 200, false}, {"signal", tr("Signal"), 9, 2, 100, false}});
        addCard(grid, 1, 2, "dmi", tr("ADX and directional movement"), {{"diLength", tr("Length"), 14, 2, 100, false}});
        content()->addLayout(grid);
        auto *note = quant::label(tr("Daily closes from Yahoo Finance, delayed, computed here with the indicators of the Charts tab; the last six months are shown. The settings are remembered. Plain readings, not signals to trade."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        connect(m_market, &QComboBox::currentIndexChanged, this, [this] {
            QSettings().setValue("desk/indicators/market", m_market->currentData());
            fetch();
        });
    }

protected:
    void load() override { fetch(); }

private:
    struct Param {
        const char *key;
        QString label;
        double value, low, high;
        bool real;
    };
    struct Card {
        quant::MultiLineChart *chart = nullptr;
        QLabel *state = nullptr;
        QHash<QString, QAbstractSpinBox *> params;
    };

    void addCard(QGridLayout *grid, int row, int column, const QString &id, const QString &title, const QVector<Param> &params)
    {
        QVBoxLayout *body;
        QHBoxLayout *head;
        auto *frame = quant::card(title, &body, &head);
        Card card;
        for (const Param &p : params) {
            const QString key = QString("desk/indicators/%1/%2").arg(id, p.key);
            head->addWidget(quant::label(p.label, "qtNote"));
            if (p.real) {
                auto *spin = new QDoubleSpinBox;
                spin->setObjectName("qtSpin");
                spin->setDecimals(1);
                spin->setSingleStep(0.1);
                spin->setRange(p.low, p.high);
                spin->setValue(QSettings().value(key, p.value).toDouble());
                connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key](double v) {
                    QSettings().setValue(key, v);
                    fill();
                });
                head->addWidget(spin);
                card.params.insert(p.key, spin);
            } else {
                auto *spin = new QSpinBox;
                spin->setObjectName("qtSpin");
                spin->setRange(int(p.low), int(p.high));
                spin->setValue(QSettings().value(key, p.value).toInt());
                connect(spin, &QSpinBox::valueChanged, this, [this, key](int v) {
                    QSettings().setValue(key, v);
                    fill();
                });
                head->addWidget(spin);
                card.params.insert(p.key, spin);
            }
        }
        card.state = quant::label({}, "qtCardLabel");
        card.state->setWordWrap(true);
        body->addWidget(card.state);
        card.chart = new quant::MultiLineChart;
        card.chart->setMinimumHeight(220);
        card.chart->setZeroLine(false);
        body->addWidget(card.chart);
        grid->addWidget(frame, row, column);
        m_cards.insert(id, card);
    }

    double param(const QString &id, const QString &key) const
    {
        QAbstractSpinBox *box = m_cards.value(id).params.value(key);
        if (auto *d = qobject_cast<QDoubleSpinBox *>(box))
            return d->value();
        if (auto *i = qobject_cast<QSpinBox *>(box))
            return i->value();
        return 0;
    }

    void fetch()
    {
        const desk::Instrument *i = desk::findInstrument(m_market->currentData().toString());
        if (!i)
            return;
        setStatus(tr("Loading %1…").arg(i->label));
        const int serial = ++m_serial;
        deskdata::loadCandles({i->symbol}, "2y", "1d", this, [this, serial, symbol = i->symbol, label = i->label](const deskdata::Candles &c, const QString &error) {
            if (serial != m_serial)
                return;
            m_bars.clear();
            for (const desk::Candle &bar : c.value(symbol)) {
                qtm::Bar b;
                b.time = double(bar.time);
                b.open = bar.open;
                b.high = bar.high;
                b.low = bar.low;
                b.close = bar.close;
                b.volume = bar.volume;
                m_bars << b;
            }
            setStatus(m_bars.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
            fill();
        });
    }

    // The last bars of a series, for the chart.
    static QVector<double> recent(const QVector<double> &values)
    {
        return values.mid(std::max<qsizetype>(0, values.size() - ShownBars));
    }

    static QString sessions(int n) { return n == 1 ? tr("1 session") : tr("%1 sessions").arg(n); }

    static void setState(const Card &card, const QString &text, int sign)
    {
        setSigned(card.state, text, sign);
    }

    void fill()
    {
        if (m_bars.isEmpty())
            return;
        const Palette &pal = Theme::current();
        QVector<double> closes;
        QVector<QDate> allDates;
        for (const qtm::Bar &b : std::as_const(m_bars)) {
            closes << b.close;
            allDates << QDateTime::fromSecsSinceEpoch(qint64(b.time), QTimeZone::UTC).date();
        }
        const QVector<QDate> dates = allDates.mid(std::max<qsizetype>(0, allDates.size() - ShownBars));
        const quant::Format priceFormat = [d = priceDecimals(closes.last())](double v) { return QLocale().toString(v, 'f', d); };
        const quant::Format plain = [](double v) { return amount(v, 1); };
        const int digits = priceDecimals(closes.last());
        const auto compute = [&](const QString &id, const QJsonObject &params) -> qtm::IndicatorResult {
            try {
                return qtm::compute(id, m_bars, params);
            } catch (const std::exception &) {
                return {};
            }
        };
        const auto constant = [&](double v) { return QVector<double>(dates.size(), v); };

        // Supertrend
        {
            const Card &card = m_cards["supertrend"];
            const qtm::IndicatorResult r = compute("supertrend", {{"atrLength", param("supertrend", "atrLength")}, {"factor", param("supertrend", "factor")}});
            const desk::TrendReading t = desk::readSupertrend(r.values.value("direction"));
            card.chart->setData(dates, {{tr("Price"), pal.Text, recent(closes)}, {tr("Up"), pal.Up, recent(r.values.value("up"))}, {tr("Down"), pal.Down, recent(r.values.value("down"))}}, priceFormat);
            setState(card, t.side > 0 ? tr("Up for %1").arg(sessions(t.bars)) : t.side < 0 ? tr("Down for %1").arg(sessions(t.bars)) : QString("—"), t.side);
        }
        // Bollinger
        {
            const Card &card = m_cards["bb"];
            const qtm::IndicatorResult r = compute("bb", {{"length", param("bb", "length")}, {"mult", param("bb", "mult")}});
            const desk::BandsReading b = desk::readBands(closes, r.values.value("upper"), r.values.value("lower"));
            card.chart->setData(dates, {{tr("Price"), pal.Text, recent(closes)}, {tr("Upper"), quant::seriesColor(1), recent(r.values.value("upper"))}, {tr("Middle"), pal.TextFaint, recent(r.values.value("basis"))}, {tr("Lower"), quant::seriesColor(1), recent(r.values.value("lower"))}}, priceFormat);
            QString text = "—";
            if (b.valid) {
                text = b.position >= 100 ? tr("Above the upper band") : b.position <= 0 ? tr("Below the lower band")
                     : b.position >= 80 ? tr("Near the upper band") : b.position <= 20 ? tr("Near the lower band") : tr("Inside the bands");
                text += tr(", at %1% of their width").arg(amount(b.position, 0));
                if (b.squeeze)
                    text += tr("; narrow: the lowest fifth of six months");
            }
            setState(card, text, 0);
        }
        // Moving averages
        {
            const Card &card = m_cards["ema"];
            const qtm::IndicatorResult fast = compute("ema", {{"length", param("ema", "fast")}});
            const qtm::IndicatorResult slow = compute("ema", {{"length", param("ema", "slow")}});
            const QVector<double> f = fast.values.value("value"), s = slow.values.value("value");
            const desk::TrendReading t = desk::readCross(f, s);
            card.chart->setData(dates, {{tr("Price"), pal.Text, recent(closes)}, {tr("EMA %1").arg(int(param("ema", "fast"))), quant::seriesColor(1), recent(f)}, {tr("EMA %1").arg(int(param("ema", "slow"))), quant::seriesColor(6), recent(s)}}, priceFormat);
            setState(card, t.side > 0 ? tr("Fast above slow for %1, %2 apart").arg(sessions(t.bars), QLocale().toString(t.gap, 'f', digits))
                         : t.side < 0 ? tr("Fast below slow for %1, %2 apart").arg(sessions(t.bars), QLocale().toString(-t.gap, 'f', digits)) : QString("—"), t.side);
        }
        // RSI
        {
            const Card &card = m_cards["rsi"];
            const double low = param("rsi", "oversold"), high = param("rsi", "overbought");
            const qtm::IndicatorResult r = compute("rsi", {{"length", param("rsi", "length")}, {"maLength", 0}});
            const desk::RsiReading v = desk::readRsi(r.values.value("value"), low, high);
            card.chart->setData(dates, {{tr("RSI"), quant::seriesColor(4), recent(r.values.value("value"))}, {tr("High line"), pal.Down, constant(high)}, {tr("Low line"), pal.Up, constant(low)}}, plain);
            const QString direction = v.rising ? tr("rising") : tr("falling");
            setState(card, !v.valid ? QString("—") : v.zone > 0 ? tr("Overbought at %1, %2").arg(amount(v.value, 0), direction)
                         : v.zone < 0 ? tr("Oversold at %1, %2").arg(amount(v.value, 0), direction) : tr("Between its lines at %1, %2").arg(amount(v.value, 0), direction), -v.zone);
        }
        // MACD
        {
            const Card &card = m_cards["macd"];
            const qtm::IndicatorResult r = compute("macd", {{"fast", param("macd", "fast")}, {"slow", param("macd", "slow")}, {"signal", param("macd", "signal")}});
            const QVector<double> macd = r.values.value("macd"), signal = r.values.value("signal"), hist = r.values.value("hist");
            const desk::TrendReading t = desk::readCross(macd, signal);
            card.chart->setZeroLine(true);
            card.chart->setData(dates, {{tr("MACD"), quant::seriesColor(1), recent(macd)}, {tr("Signal"), quant::seriesColor(6), recent(signal)}}, [d = digits](double v) { return QLocale().toString(v, 'f', std::min(d + 1, 5)); });
            const bool widening = hist.size() >= 2 && std::abs(hist.last()) > std::abs(hist[hist.size() - 2]);
            const QString gap = widening ? tr("gap widening") : tr("gap narrowing");
            setState(card, t.side > 0 ? tr("Above its signal for %1, %2").arg(sessions(t.bars), gap)
                         : t.side < 0 ? tr("Below its signal for %1, %2").arg(sessions(t.bars), gap) : QString("—"), t.side);
        }
        // ADX
        {
            const Card &card = m_cards["dmi"];
            const qtm::IndicatorResult r = compute("dmi", {{"diLength", param("dmi", "diLength")}, {"adxSmoothing", param("dmi", "diLength")}});
            const desk::DmiReading d = desk::readDmi(r.values.value("adx"), r.values.value("plus"), r.values.value("minus"));
            card.chart->setData(dates, {{tr("ADX"), pal.Text, recent(r.values.value("adx"))}, {tr("+DI"), pal.Up, recent(r.values.value("plus"))}, {tr("−DI"), pal.Down, recent(r.values.value("minus"))}}, plain);
            setState(card, !d.valid ? QString("—") : d.strong ? (d.side > 0 ? tr("Strong trend up, ADX %1").arg(amount(d.adx, 0)) : tr("Strong trend down, ADX %1").arg(amount(d.adx, 0)))
                                                              : tr("No strong trend, ADX %1").arg(amount(d.adx, 0)), d.strong ? d.side : 0);
        }
    }

    QComboBox *m_market;
    QHash<QString, Card> m_cards;
    qtm::Bars m_bars;
    int m_serial = 0;
};

} // namespace

QWidget *makeIndicators(const Context &) { return new IndicatorsView; }

} // namespace deskui

#include "DeskViewsIndicators.moc"
