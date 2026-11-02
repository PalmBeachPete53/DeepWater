#pragma once

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QPixmap>
#include <QWidget>

#include <functional>

// Charts of the Quant page, painted with QPainter in the active theme. Each takes plain data and redraws;
// hovering shows the values under the mouse.
namespace quant {

using Format = std::function<QString(double)>;

QFont chartFont(int px, bool bold = false);
QString thousands(double v, int decimals = 0);        // 1,987,373
QString signedThousands(double v, int decimals = 0);  // +32,315
QString shortNumber(double v);                        // 150k, 1.2M, -50k

// A piece of a chart title: text in a colour (the default colour when invalid).
struct TitlePart {
    QString text;
    QColor color;
};

// Base: margins, the plot rectangle, axes, tooltip and hover tracking over a row of dates.
class Chart : public QWidget
{
    Q_OBJECT
public:
    explicit Chart(QWidget *parent = nullptr);
    void setTitle(const QVector<TitlePart> &parts);
    void setDateFormat(const QString &f) { m_dateFormat = f; update(); }

protected:
    struct Axis {
        double lo = 0, hi = 1;
        QVector<double> ticks;
    };
    static Axis axisFor(double lo, double hi, int count = 5, double pad = 0.06);
    static Axis axisAround(double lo, double hi, int count = 5);   // without padding

    QRectF plotRect() const;
    void paintEvent(QPaintEvent *) override final;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    virtual void paint(QPainter &p, const QRectF &plot) = 0;
    void drawTitle(QPainter &p);
    void drawEmpty(QPainter &p, const QString &text);
    void drawValueAxis(QPainter &p, const QRectF &plot, const Axis &axis, const Format &format, bool right, bool grid,
                       const QColor &color = QColor());
    void drawDateAxis(QPainter &p, const QRectF &plot, const QVector<QDate> &dates, bool grid);
    void drawLabelAxis(QPainter &p, const QRectF &plot, const QStringList &labels, bool grid);
    void drawTooltip(QPainter &p, const QPointF &at, const QStringList &lines, const QVector<QColor> &swatches);
    double xOf(const QRectF &plot, int i, int n) const;
    int indexAt(const QRectF &plot, double x, int n) const;

    QMargins m_margins {56, 30, 56, 28};
    QVector<TitlePart> m_title;
    QString m_dateFormat;   // empty: chosen from the span
    QPointF m_mouse;
    bool m_hovering = false;
};

// Several series over the same dates: a right axis, a dotted zero line and the last value of each series written
// at its end. Two series can be filled between (green where the first is above, red below); one series can be
// filled against zero.
class MultiLineChart : public Chart
{
    Q_OBJECT
public:
    struct Line {
        QString name;
        QColor color;
        QVector<double> values;   // one per date, NaN for none
    };
    explicit MultiLineChart(QWidget *parent = nullptr);
    void setData(const QVector<QDate> &dates, const QVector<Line> &lines, Format format);
    // Labels instead of dates on the x axis (one per point): the chart then ignores the dates.
    void setXLabels(const QStringList &labels) { m_xLabels = labels; changed(); }
    void setRightAxis(bool on) { m_rightAxis = on; changed(); }
    void setZeroLine(bool on) { m_zeroLine = on; changed(); }
    void setEndLabels(bool on) { m_endLabels = on; changed(); }
    void setFillBetween(bool on) { m_fillBetween = on; changed(); }
    void setSignedArea(bool on) { m_signedArea = on; changed(); }
    void setLegend(bool on) { m_legend = on; changed(); }

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    // Everything but the hover crosshair and tooltip, kept as an image: moving the mouse over a long
    // series redrew all of it (areas, lines, axes) at every move.
    void paintLayer(QPainter &p, const QRectF &plot, const Axis &axis, int n, bool byLabel);
    void changed() { m_layer = QPixmap(); update(); }
    QPixmap m_layer;
    QRectF m_layerPlot;
    QString m_layerTheme, m_layerDateFormat;

    QVector<QDate> m_dates;
    QStringList m_xLabels;
    bool m_legend = false;
    QVector<Line> m_lines;
    Format m_format;
    bool m_rightAxis = true, m_zeroLine = true, m_endLabels = true, m_fillBetween = false, m_signedArea = false;
};

// Overlaid histograms of several samples on one scale, with a legend.
class HistogramChart : public Chart
{
    Q_OBJECT
public:
    struct Sample {
        QString name;
        QColor color;
        QVector<double> values;
    };
    explicit HistogramChart(QWidget *parent = nullptr);
    void setData(const QVector<Sample> &samples, int bins, Format format);

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<Sample> m_samples;
    int m_bins = 40;
    Format m_format;
};

// Two price series, each on its own axis (the first on the left), with an area under each and optional
// markers on their highs and lows.
class OverlayChart : public Chart
{
    Q_OBJECT
public:
    explicit OverlayChart(QWidget *parent = nullptr);
    void setData(const QVector<QDate> &dates, const QString &nameA, const QColor &colorA, const QVector<double> &a,
                 const QString &nameB, const QColor &colorB, const QVector<double> &b);
    void setMinMax(bool on) { m_minMax = on; update(); }

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QString m_nameA, m_nameB;
    QColor m_colorA, m_colorB;
    QVector<double> m_a, m_b;
    bool m_minMax = true;
};

// A spread, its moving average and a ±2σ band.
class SpreadChart : public Chart
{
    Q_OBJECT
public:
    explicit SpreadChart(QWidget *parent = nullptr);
    void setData(const QVector<QDate> &dates, const QVector<double> &spread, const QVector<double> &mean,
                 const QVector<double> &upper, const QVector<double> &lower, int window);
    // Names of the three legend entries (line, mean line, band) and the axis format, for other uses of the shape.
    void setLegend(const QStringList &names, Format format = {}) { m_legend = names; m_format = std::move(format); update(); }

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QVector<double> m_spread, m_mean, m_upper, m_lower;
    int m_window = 20;
    QStringList m_legend;
    Format m_format;
};

// A z-score between -3 and +3 with the sell zone above +2σ and the buy zone below -2σ; the line is coloured by
// how far it is from zero.
class ZScoreChart : public Chart
{
    Q_OBJECT
public:
    explicit ZScoreChart(QWidget *parent = nullptr);
    void setData(const QVector<QDate> &dates, const QVector<double> &z);

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QVector<double> m_z;
};

// Returns of one series against another, the regression line, an optional box of statistics and optional
// histograms of each axis along the sides.
class ScatterChart : public Chart
{
    Q_OBJECT
public:
    explicit ScatterChart(QWidget *parent = nullptr);
    void setData(const QVector<QDate> &dates, const QVector<double> &x, const QVector<double> &y, double alpha, double beta);
    void setLabels(const QString &xLabel, const QString &yLabel, const QColor &xColor, const QColor &yColor);
    void setStats(const QVector<std::pair<QString, QString>> &stats) { m_stats = stats; update(); }
    void setMarginals(bool on);
    void setPointColor(const QColor &c) { m_pointColor = c; update(); }

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QVector<double> m_x, m_y;
    double m_alpha = 0, m_beta = 0;
    QString m_xLabel, m_yLabel;
    QColor m_xColor, m_yColor, m_pointColor;
    QVector<std::pair<QString, QString>> m_stats;
    bool m_marginals = false;
};

// Long against short, as shares of each group's positions, one bar per group.
class LongShortBars : public Chart
{
    Q_OBJECT
public:
    struct Group {
        QString label;
        double longPos = 0, shortPos = 0;
    };
    explicit LongShortBars(QWidget *parent = nullptr);
    void setData(const QVector<Group> &groups);

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<Group> m_groups;
};

// Weekly series drawn as steps, with a legend.
class StepChart : public Chart
{
    Q_OBJECT
public:
    struct Line {
        QString name;
        QColor color;
        QVector<double> values;
    };
    explicit StepChart(QWidget *parent = nullptr);
    void setData(const QVector<QDate> &dates, const QVector<Line> &lines);

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QVector<Line> m_lines;
};

// A price line coloured by the regime of each step, with the regime shaded behind it and a legend.
class RegimeChart : public Chart
{
    Q_OBJECT
public:
    explicit RegimeChart(QWidget *parent = nullptr);
    // regime: one index per date into labels / colors; probability: that regime's probability (for the tooltip).
    void setData(const QVector<QDate> &dates, const QVector<double> &price, const QVector<int> &regime,
                 const QVector<double> &probability, const QStringList &labels, const QVector<QColor> &colors);

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QVector<double> m_price, m_probability;
    QVector<int> m_regime;
    QStringList m_labels;
    QVector<QColor> m_colors;
};

// Probabilities of each regime over time, stacked to one.
class StackedAreaChart : public Chart
{
    Q_OBJECT
public:
    explicit StackedAreaChart(QWidget *parent = nullptr);
    // series: one per regime, one value per date, summing to 1 at each date.
    void setData(const QVector<QDate> &dates, const QVector<QVector<double>> &series, const QStringList &labels,
                 const QVector<QColor> &colors);

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<QDate> m_dates;
    QVector<QVector<double>> m_series;
    QStringList m_labels;
    QVector<QColor> m_colors;
};

} // namespace quant
