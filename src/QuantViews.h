#pragma once

#include "QuantCharts.h"
#include "QuantData.h"
#include "ResearchData.h"

#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QTimer>
#include <QWidget>

class QComboBox;

class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;

namespace quant {

class MarketStateStrip;

// --- Small building blocks shared by the sections.

QFont mono(int px = 12, bool bold = false);
// Segmented buttons ("1 YEAR | 6 MONTH"): the group's ids are the indexes; `checked` is checked.
QWidget *segments(const QStringList &labels, QButtonGroup **group, int checked = 0);
// A framed bar of controls (the row under the sub-header).
QFrame *bar(QHBoxLayout **layout);
// A framed block with a small title.
QFrame *card(const QString &title, QVBoxLayout **body, QHBoxLayout **head = nullptr);
QLabel *label(const QString &text, const char *objectName);
// A coloured badge ("WATCH", "STREAK DOWN"): text in `color` on a tint of it.
QLabel *badge(const QString &text, const QColor &color, bool filled = false);
void setBadge(QLabel *l, const QString &text, const QColor &color, bool filled = false);
QLineEdit *symbolEdit(const QString &text, const QColor &color);
// A symbol as typed: upper case, trimmed; empty when it isn't one.
QString cleanSymbol(const QString &text);
// Yahoo's range words for the ranges offered: "1y", "6mo"…
QString yahooRange(const QString &label);
// Colours for up to eight series.
QColor seriesColor(int i);
QColor changeColor(double v);

// A section of the Quant page: loads when first shown.
class View : public QWidget
{
    Q_OBJECT
public:
    explicit View(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *) override;
    virtual void load() = 0;
    void setStatus(const QString &text);   // a line at the top, hidden when empty
    QVBoxLayout *content() const { return m_content; }

private:
    bool m_loaded = false;
    QLabel *m_status;
    QVBoxLayout *m_content;
};

// Compare: several tickers' % change over a range on one chart, and their key figures side by side, the best
// of each row in green and the worst in red.
class CompareView : public View
{
    Q_OBJECT
public:
    explicit CompareView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void addTicker(const QString &symbol);
    void removeTicker(const QString &symbol);
    void saveTickers();
    void rebuildChips();
    void fetch();
    void fillChart();
    void fillTable();

    QButtonGroup *m_range;
    QPushButton *m_changeMode;
    QLineEdit *m_add;
    QHBoxLayout *m_chips;
    MultiLineChart *m_chart;
    QTableWidget *m_table;
    QStringList m_tickers;
    QHash<QString, research::Series> m_prices;
    QHash<QString, QJsonObject> m_summaries;
    QHash<QString, QString> m_errors;
    int m_generation = 0;
};

// Correlation: a pair for pairs trading, Y against X: hedge ratio, cointegration (ADF), half-life of the spread,
// z-score signal; the prices, the spread with its band, the z-score and the returns scatter.
class CorrelationView : public View
{
    Q_OBJECT
public:
    explicit CorrelationView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fetch();
    void compute();

    QButtonGroup *m_range;
    QSpinBox *m_window;
    QLineEdit *m_y, *m_x;
    QCheckBox *m_minMax, *m_zToggle;
    MarketStateStrip *m_state;
    QLabel *m_adfP, *m_adfStat, *m_stationary, *m_halfLife, *m_beta, *m_formula, *m_z, *m_signal;
    OverlayChart *m_overlay;
    SpreadChart *m_spread;
    ZScoreChart *m_zChart;
    ScatterChart *m_scatter;
    QFrame *m_zPanel;
    Prices m_py, m_px;
    QString m_symY, m_symX;
    int m_generation = 0;
    bool m_haveY = false, m_haveX = false;
};

// Risk & beta: the regression of a stock's returns on an index's, daily or weekly over 1 to 5 years, with the
// distributions of both, the regression statistics and the cumulative performance of the two.
class RiskBetaView : public View
{
    Q_OBJECT
public:
    explicit RiskBetaView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fetch();
    void compute();

    QButtonGroup *m_range, *m_period, *m_perfMode;
    QLineEdit *m_y, *m_x;
    MarketStateStrip *m_state;
    ScatterChart *m_scatter;
    QGridLayout *m_stats;
    QVector<QLabel *> m_statValues;
    QLabel *m_statY, *m_statX;
    MultiLineChart *m_perf;
    Prices m_py, m_px;
    QString m_symY, m_symX;
    int m_generation = 0;
    bool m_haveY = false, m_haveX = false;
};

class CotTable;

// COT: the CFTC's weekly Commitments of Traders report (legacy, futures only) for a few index, crypto and metal
// contracts: positions, changes, shares of open interest and traders by group, long against short, net positions
// over two years and the weekly history.
class CotView : public View
{
    Q_OBJECT
public:
    explicit CotView(CotReport *report, QWidget *parent = nullptr);

signals:
    void askAi(const QString &prompt);

protected:
    void load() override;

private:
    void fill();
    void fillReading(const CotReport::Series &s);
    void fetchTff(const CotReport::Market &market);

    CotReport *m_report;
    QComboBox *m_market;
    QLabel *m_specNet, *m_specNetSub, *m_specLong, *m_specLongSub, *m_specRange, *m_specRangeSub, *m_commRange, *m_commRangeSub, *m_reading;
    QFrame *m_tffCard;
    QTableWidget *m_tff;
    MultiLineChart *m_tffChart;
    QLabel *m_tffDate;
    int m_tffSerial = 0;
    QLabel *m_title, *m_code, *m_asOf, *m_release, *m_units, *m_totals, *m_historyTitle;
    CotTable *m_table;
    LongShortBars *m_longShort;
    StepChart *m_net;
    QPushButton *m_historyToggle;
    QTableWidget *m_history;
};

// Signals: a scan of large US stocks on five timeframes for RSI extremes, streaks of candles and gaps, newest
// first, filtered by symbol, indicator and timeframe.
class SignalsView : public View
{
    Q_OBJECT
public:
    explicit SignalsView(QWidget *parent = nullptr);

protected:
    void load() override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    struct Signal {
        QString symbol, timeframe, indicator, signal, value;
        QDateTime time;
        bool down = false;
    };
    void scan();
    void scanned(const QString &symbol, const QString &timeframe, const QVector<research::Bar> &bars);
    void fillTable();
    static QStringList universe();

    QLineEdit *m_filter;
    QButtonGroup *m_indicator, *m_timeframe;
    QTableWidget *m_table;
    QLabel *m_empty;
    QHash<QString, QVector<Signal>> m_signals;   // by "symbol tf"
    QTimer m_refill, m_rescan;
    int m_generation = 0;
    int m_pending = 0, m_total = 0;
    QDateTime m_lastScan;
};

} // namespace quant
