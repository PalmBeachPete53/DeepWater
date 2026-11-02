#pragma once

#include <QColor>
#include <QDate>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

// Charts of the Research page, painted with QPainter in the active theme. Each takes plain data and redraws;
// hovering shows the values under the mouse.
namespace research {

using Format = std::function<QString(double)>;

namespace palette {
QColor blue();
QColor yellow();
QColor red();
QColor green();
QColor purple();
QColor cyan();
QColor pink();
QColor orange();
QColor darkGreen();
}

// "Nice" axis ticks covering [lo, hi].
QVector<double> niceTicks(double lo, double hi, int count = 5);

// Groups of bars (one group per category, one bar per series) with the change against the previous
// group above each bar.
class BarChart : public QWidget
{
    Q_OBJECT
public:
    struct Series {
        QString name;
        QColor color;
        QVector<double> values;   // one per category, NaN for none
    };
    explicit BarChart(QWidget *parent = nullptr);
    void setData(const QStringList &categories, const QVector<Series> &series, Format format,
                 bool showChange = true, bool changeInPoints = false);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF plotRect() const;
    QStringList m_categories;
    QVector<Series> m_series;
    Format m_format;
    bool m_showChange = true;
    bool m_changeInPoints = false;
    int m_hover = -1;
};

// Lines over categories (dates or periods). A series may end in a dashed projection.
class LineChart : public QWidget
{
    Q_OBJECT
public:
    struct Series {
        QString name;
        QColor color;
        QVector<double> values;   // one per category, NaN for none
        bool dots = true;
        bool area = false;
        bool dashed = false;
    };
    explicit LineChart(QWidget *parent = nullptr);
    void setData(const QStringList &categories, const QVector<Series> &series, Format format);
    void setLegend(bool on) { m_legend = on; update(); }
    void setLabelEvery(int n) { m_labelEvery = n; update(); }
    void setTitleForTooltip(const QString &t) { m_tooltipTitle = t; }
    void setRotatedLabels(bool on) { m_rotated = on; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF plotRect() const;
    QStringList m_categories;
    QVector<Series> m_series;
    Format m_format;
    bool m_legend = false;
    bool m_rotated = false;
    int m_labelEvery = 0;
    int m_hover = -1;
    QString m_tooltipTitle;
};

// A ring of slices, with text in the middle and, optionally, labels on leader lines.
class DonutChart : public QWidget
{
    Q_OBJECT
public:
    struct Slice {
        QString label;
        double value = 0;
        QColor color;
        QString detail;           // under the label
        QColor detailColor;
    };
    explicit DonutChart(QWidget *parent = nullptr);
    void setData(const QVector<Slice> &slices, const QString &centerTitle = {}, const QString &centerSub = {},
                 const QString &centerValue = {}, const QColor &centerColor = {});
    void setLeaderLabels(bool on) { m_leaders = on; update(); }
    void setThickness(double t) { m_thickness = t; update(); }
    void setStartAngle(double deg) { m_start = deg; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QVector<Slice> m_slices;
    QString m_title, m_sub, m_value;
    QColor m_valueColor;
    bool m_leaders = false;
    double m_thickness = 0.32;
    double m_start = 90;
    int m_hover = -1;
};

// Analysts' consensus as a needle on a half dial (strong sell … strong buy).
class Gauge : public QWidget
{
    Q_OBJECT
public:
    explicit Gauge(QWidget *parent = nullptr);
    void setValue(double score, const QString &label);   // score 1..5, 5 = strong buy
    QSize sizeHint() const override { return QSize(240, 180); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_score = 3;
    QString m_label;
};

// Horizontal bars with a label on the left and the count inside.
class HBars : public QWidget
{
    Q_OBJECT
public:
    struct Bar {
        QString label;
        double value = 0;
        QColor color;
        QString text;             // inside the bar, else the value
    };
    explicit HBars(QWidget *parent = nullptr);
    void setData(const QVector<Bar> &bars, double max = 0);
    void setLabelWidth(int w) { m_labelWidth = w; updateGeometry(); update(); }
    void setRowHeight(int h) { m_rowHeight = h; updateGeometry(); update(); }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<Bar> m_bars;
    double m_max = 0;
    int m_labelWidth = 90;
    int m_rowHeight = 25;
};

// A marker on a red-to-green scale (average analyst rating, 1..5).
class RatingScale : public QWidget
{
    Q_OBJECT
public:
    explicit RatingScale(QWidget *parent = nullptr);
    void setValue(double v) { m_value = v; update(); }
    QSize sizeHint() const override { return QSize(400, 40); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_value = 3;
};

// One bar split in proportions.
class ProportionBar : public QWidget
{
    Q_OBJECT
public:
    struct Part {
        double value = 0;
        QColor color;
    };
    explicit ProportionBar(QWidget *parent = nullptr);
    void setParts(const QVector<Part> &parts) { m_parts = parts; update(); }
    QSize sizeHint() const override { return QSize(400, 30); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<Part> m_parts;
};

// Stacked bars per period (counts by category) with a line on a second axis.
class StackedBarChart : public QWidget
{
    Q_OBJECT
public:
    struct Stack {
        QString name;
        QColor color;
        QVector<double> values;
    };
    explicit StackedBarChart(QWidget *parent = nullptr);
    void setData(const QStringList &categories, const QVector<Stack> &stacks, const QVector<double> &line,
                 const QString &lineName, const QStringList &lineAxisLabels, double lineMin, double lineMax,
                 std::function<QString(double)> lineFormat);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF plotRect() const;
    QStringList m_categories;
    QVector<Stack> m_stacks;
    QVector<double> m_line;
    QString m_lineName;
    QStringList m_lineLabels;
    double m_lineMin = 1, m_lineMax = 5;
    std::function<QString(double)> m_lineFormat;
    int m_hover = -1;
};

// The supply chain: suppliers on the left, customers on the right, the company in the middle.
class SupplyChainGraph : public QWidget
{
    Q_OBJECT
public:
    struct Node {
        QString symbol, name, sector, product, url;
        double price = qQNaN(), changePct = qQNaN();
        double share = qQNaN();   // the relationship's share of revenue, 0..1
        QString shareText;
        QString label;            // shown instead of the symbol (unlisted companies)
        QString currency = "USD";
        bool openable = true;     // clicking it opens the symbol
    };
    // The measure under each node: "REVENUE IMPACT" by default.
    void setMetricLabel(const QString &label) { m_metricLabel = label; update(); }
    void setNote(const QString &note) { m_note = note; update(); }
    explicit SupplyChainGraph(QWidget *parent = nullptr);
    void setData(const Node &center, const QVector<Node> &suppliers, const QVector<Node> &customers);

signals:
    void symbolActivated(const QString &symbol);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF nodeRect(int side, int index) const;   // side 0 suppliers, 1 customers
    Node m_center;
    QVector<Node> m_suppliers, m_customers;
    QString m_metricLabel;
    QString m_note;               // in the middle when a side is empty
    int m_hoverSide = -1, m_hoverIndex = -1;
};

} // namespace research
