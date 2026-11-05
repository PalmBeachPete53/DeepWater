#pragma once

#include "AiRead.h"
#include "QuantCharts.h"

#include <QFrame>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QTableWidget;
class QTableWidgetItem;

// Small pieces shared by the Desk and Journal tabs: charts in the Quant page's look, tiles, tables.
namespace deskui {

// A figure with a small title and a line under it.
QFrame *tile(const QString &title, QLabel **value, QLabel **sub = nullptr);
void setSigned(QLabel *value, const QString &text, double sign);   // coloured by the sign of `sign`

QTableWidget *table(const QStringList &headers);
// Rows as tall as the Quant page's; `stretch` is the column that takes the room left (-1: all equally).
void fitColumns(QTableWidget *t, int stretch = -1);
// A table in a scroll page shows every row: it takes the height of its rows.
void fitHeight(QTableWidget *t, int maxRows = 0);
QTableWidgetItem *cell(const QString &text, const QColor &color = QColor(), Qt::Alignment align = Qt::AlignCenter, bool bold = false);

QString pct(double v, int decimals = 2);       // +1.23%
QString amount(double v, int decimals = 2);    // 1,234.50
QString signedAmount(double v, int decimals = 2);

// Vertical bars around zero, one per label.
class BarsChart : public quant::Chart
{
    Q_OBJECT
public:
    explicit BarsChart(QWidget *parent = nullptr);
    void setData(const QStringList &labels, const QVector<double> &values, quant::Format format);
    QString aiRead() const override;

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QStringList m_labels;
    QVector<double> m_values;
    quant::Format m_format;
};

// Horizontal bars, largest first as given, with the name on the left.
class HBarsChart : public quant::Chart
{
    Q_OBJECT
public:
    explicit HBarsChart(QWidget *parent = nullptr);
    void setData(const QStringList &labels, const QVector<double> &values, quant::Format format, const QStringList &tips = {});
    void setNeutralColor(bool on) { m_neutral = on; update(); }   // counts: no green or red
    QString aiRead() const override;

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QStringList m_labels, m_tips;
    QVector<double> m_values;
    quant::Format m_format;
    bool m_neutral = false;
};

// Points on two axes: green above zero, red below.
class DotsChart : public quant::Chart
{
    Q_OBJECT
public:
    explicit DotsChart(QWidget *parent = nullptr);
    void setData(const QVector<double> &x, const QVector<double> &y, quant::Format xFormat, quant::Format yFormat, const QStringList &tips = {});
    void setLogX(bool on) { m_logX = on; update(); }
    void setXRange(double lo, double hi) { m_xLo = lo; m_xHi = hi; update(); }
    QString aiRead() const override;

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    double mapX(const QRectF &plot, double x, double lo, double hi) const;
    QVector<double> m_x, m_y;
    QStringList m_tips;
    quant::Format m_xFormat, m_yFormat;
    bool m_logX = false;
    double m_xLo = 0, m_xHi = 0;
};

// Values from 0 to 100 on spokes, with an optional second shape over the first and a legend naming both.
class RadarChart : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    explicit RadarChart(QWidget *parent = nullptr);
    void setData(const QStringList &names, const QVector<double> &values);
    // Empty values take the second shape away. `label` and `overlayLabel` name the two shapes in the legend.
    void setOverlay(const QString &label, const QString &overlayLabel, const QVector<double> &values);
    QString aiRead() const override;
    QSize minimumSizeHint() const override { return {200, 190}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QStringList m_names;
    QVector<double> m_values;
    QString m_label, m_overlayLabel;
    QVector<double> m_overlay;
};

// A half dial from 0 to 100, red to the accent, with the value in the middle.
class GaugeDial : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    explicit GaugeDial(QWidget *parent = nullptr);
    void setValue(double value, const QString &caption);
    QString aiRead() const override;
    QSize minimumSizeHint() const override { return {260, 170}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_value = -1;
    QString m_caption;
};

// A price line with horizontal levels over it, named at the right edge.
class LevelsChart : public quant::Chart
{
    Q_OBJECT
public:
    struct Level {
        double price = 0;
        QColor color;
        QString label;
        bool dashed = false;
    };
    explicit LevelsChart(QWidget *parent = nullptr);
    void setData(const QVector<qint64> &times, const QVector<double> &closes, const QVector<Level> &levels, int decimals);
    QString aiRead() const override;

protected:
    void paint(QPainter &p, const QRectF &plot) override;

private:
    QVector<qint64> m_times;
    QVector<double> m_closes;
    QVector<Level> m_levels;
    int m_decimals = 2;
};

// A long / short split, drawn in a table cell's colours: the delegate for a column whose items hold the long share (0-100).
class SplitBarDelegate;
void useSplitBars(QTableWidget *t, int column);

} // namespace deskui
