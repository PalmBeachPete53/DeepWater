#pragma once

#include "AiRead.h"

#include "MacroIndicators.h"

#include <QMap>
#include <functional>
#include <QWidget>

class QLabel;

// Right sidebar on the map page: one card per macro indicator. Clicking a card
// colors the globe by that indicator and shows its ranking in the left sidebar.
class IndicatorPanel : public QWidget
{
    Q_OBJECT
public:
    explicit IndicatorPanel(MacroIndicators *macro, QWidget *parent = nullptr);

    void setActive(int id);   // -1 for none

signals:
    void indicatorClicked(int id);

private:
    void refresh();

    MacroIndicators *m_macro;
    QVector<class IndicatorItem *> m_items;
    QLabel *m_status;
};

// One indicator card: name, coverage, world median and a histogram of the
// country values in the heatmap colors.
class IndicatorItem : public QWidget
{
    Q_OBJECT
public:
    IndicatorItem(MacroIndicators *macro, MacroIndicators::Id id, QWidget *parent = nullptr);

    MacroIndicators::Id id() const { return m_id; }
    void setActive(bool active);
    void refresh();

signals:
    void clicked(int id);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    MacroIndicators *m_macro;
    MacroIndicators::Id m_id;
    QVector<double> m_values;   // sorted
    QVector<QColor> m_binColors;
    QVector<int> m_bins;
    double m_median = 0;
    bool m_active = false;
    bool m_hover = false;
};

// Countries ranked by one indicator, with a bar per country. Click a row to
// open the country.
class RankingList : public QWidget
{
    Q_OBJECT
public:
    struct Row {
        QString iso3;
        QString name;
        double value;
        QString text;
        QColor color;
    };
    explicit RankingList(QWidget *parent = nullptr);

    // Bars reach full length at `scaleMax` (absolute value); longer ones are clipped.
    void setRows(const QVector<Row> &rows, bool diverging, double scaleMax);

signals:
    void countryClicked(const QString &iso3);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    int rowAt(const QPointF &pos) const;

    QVector<Row> m_rows;
    bool m_diverging = false;
    double m_maxAbs = 1;
    int m_hover = -1;
};

// Annual bars, with years from `firstProjected` on drawn as projections.
class AnnualChart : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;
    explicit AnnualChart(QWidget *parent = nullptr);

    void setSeries(const QMap<int, double> &values, int firstProjected, const QColor &positive,
                   const QColor &negative, const std::function<QString(double)> &format);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;

private:
    QRectF plotRect() const;
    int yearAt(const QPointF &pos) const;

    QMap<int, double> m_values;
    int m_firstProjected = 0;
    QColor m_positive;
    QColor m_negative;
    std::function<QString(double)> m_format;
};

// A country's primary, secondary and tertiary sectors as stacked bars, one per measure (share of
// GDP, share of jobs). The part of GDP no sector accounts for (taxes on products less subsidies)
// stays blank.
class SectorChart : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;
    struct Bar {
        QString label;                // "Share of GDP · 2025"
        double shares[3];             // primary, secondary, tertiary, in %; NaN when not reported
        QString tips[3];              // tooltip of each segment
    };
    explicit SectorChart(QWidget *parent = nullptr);

    void setBars(const QVector<Bar> &bars);
    // The color of a sector (0 primary, 1 secondary, 2 tertiary) in the current theme.
    static QColor color(int sector);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    static constexpr int RowHeight = 40;   // label, bar and the gap below
    QRectF barRect(int bar) const;
    // The bar's segments: x from, x to, for each sector (empty when not reported).
    QVector<QPair<double, double>> segments(int bar) const;

    QVector<Bar> m_bars;
};

// The shares of GDP of the three sectors over the decades, one line each, with the numbered events of the
// country's history marked on the time axis (a band for a period).
class SectorHistoryChart : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;
    struct Event {
        int from = 0;
        int to = 0;   // 0 for a single year
        int number = 0;
    };
    explicit SectorHistoryChart(QWidget *parent = nullptr);

    // primary, secondary, tertiary: year -> % of GDP. `names` label the lines in tooltips.
    void setData(const QMap<int, double> (&shares)[3], const QStringList &names, const QVector<Event> &events);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF plotRect() const;
    double xOf(double year) const;

    QMap<int, double> m_shares[3];
    QStringList m_names;
    QVector<Event> m_events;
    int m_first = 0, m_last = 0;
    double m_top = 100;
    int m_hoverYear = 0;
};

// Lines over time (quarterly or annual, time as a fractional year), with an optional reference line (100 for
// an index, the long-term average…) and a tooltip giving every line's value at the hovered time.
class TimeSeriesChart : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;
    struct Line {
        QString name;
        QColor color;
        QMap<double, double> values;
    };
    explicit TimeSeriesChart(QWidget *parent = nullptr);

    // `format` writes a value for the axis and the tooltip; a NaN `reference` draws none.
    void setLines(const QVector<Line> &lines, const std::function<QString(double)> &format, double reference);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF plotRect() const;
    double xOf(double t) const;
    double yOf(double v) const;

    QVector<Line> m_lines;
    std::function<QString(double)> m_format;
    double m_reference = 0;
    double m_first = 0, m_last = 0, m_lo = 0, m_hi = 1;
    double m_hover = 0;
};
