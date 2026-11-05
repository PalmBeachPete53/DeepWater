#pragma once

#include "AiRead.h"

#include <QColor>
#include <QFrame>
#include <QVector>
#include <QWidget>

#include <limits>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

// The building blocks of the Overview page: framed panels, painted tables, the index performance chart and its
// statistics. Everything is painted with the current theme's colours, read at paint time.
namespace overview {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// A framed block: its title in the accent colour and in capitals, controls on the right of the title, a body.
class Panel : public QFrame
{
    Q_OBJECT
public:
    explicit Panel(const QString &title, QWidget *parent = nullptr);
    QHBoxLayout *head() const { return m_head; }
    QVBoxLayout *body() const { return m_body; }
    void setTitle(const QString &title);
    // A faint line under the title ("Treasury, Oct 30"), hidden when empty.
    void setNote(const QString &note);

private:
    QLabel *m_title, *m_note;
    QHBoxLayout *m_head;
    QVBoxLayout *m_body;
};

// Small text tabs ("TODAY 1W 1M…"), the checked one filled with the accent colour. The group's ids are the
// indexes.
QWidget *periodTabs(const QStringList &labels, QButtonGroup **group, int checked);

// A painted table: an optional header, rows of cells, a hover highlight; a row with a symbol is clickable.
// One column can be a bar (sectors): its cells' `bar` value (−1…1) is drawn as a bar from the left, green or red.
class Table : public QWidget
{
    Q_OBJECT
public:
    struct Column {
        QString title;
        double stretch = 1;          // share of the width left once the fixed columns are placed
        int fixed = 0;               // a fixed width in pixels instead (0: stretch)
        Qt::Alignment align = Qt::AlignLeft;
    };
    struct Cell {
        QString text;
        QColor color;                // invalid: the text colour
        bool bold = false;
        double bar = NaN;            // the bar column: −1…1
    };
    struct Row {
        QVector<Cell> cells;
        QString symbol;              // clickable when set
        QString tooltip;
        QColor marker;               // a thin coloured edge on the left (sectors), invalid: none
    };

    explicit Table(QWidget *parent = nullptr);
    void setColumns(const QVector<Column> &columns, bool header = true);
    void setRows(const QVector<Row> &rows);
    // Shown greyed in place of the rows while there are none ("Loading…", an error).
    void setMessage(const QString &message);
    // The table keeps room for this many rows (the panels of a line stay aligned).
    void setMinimumRows(int rows);
    int rowHeight() const { return 19; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void symbolClicked(const QString &symbol);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    bool event(QEvent *) override;

private:
    int rowAt(int y) const;
    int headerHeight() const { return m_header ? 18 : 0; }
    QVector<int> columnWidths() const;

    QVector<Column> m_columns;
    QVector<Row> m_rows;
    QString m_message;
    bool m_header = true;
    int m_minRows = 0;
    int m_hover = -1;
};

// The cumulative change of a few series over one time axis (each series as % from its base), the area between
// zero and the highest series filled green, between zero and the lowest filled red, each line named at its end,
// a crosshair with every series' value under the mouse.
class PerfChart : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;
    struct Series {
        QString name;
        QColor color;
        QVector<double> values;      // % change, one per time, NaN where missing
        bool visible = true;
    };
    explicit PerfChart(QWidget *parent = nullptr);
    // times: seconds since the epoch, ascending; shown in `zone` with `format` ("HH:mm", "MMM d"…).
    void setData(const QVector<qint64> &times, const QVector<Series> &series, const QString &format);
    void setSeriesVisible(const QString &name, bool visible);
    void setMessage(const QString &message);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QVector<qint64> m_times;
    QVector<Series> m_series;
    QString m_format;
    QString m_message;
    int m_hover = -1;
};

// The statistics next to the chart: one card per series (return, high, low and volatility of the bar returns
// over the range) and the correlations of their bar returns, pair by pair.
class StatsView : public QWidget
{
    Q_OBJECT
public:
    struct Stat {
        QString name;
        QColor color;
        double ret = NaN, high = NaN, low = NaN, vol = NaN;   // in %
    };
    struct Correlation {
        QString a, b;
        double value = NaN;
    };
    explicit StatsView(QWidget *parent = nullptr);
    void setData(const QVector<Stat> &stats, const QVector<Correlation> &correlations);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<Stat> m_stats;
    QVector<Correlation> m_correlations;
};

} // namespace overview
