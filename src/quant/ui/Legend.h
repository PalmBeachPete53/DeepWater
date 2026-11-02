#pragma once

// The chart legend of chart.js (div.legend): painted like the HTML (rows of 20px, flex gaps, the
// hover background and action buttons), in two widgets: Legend paints and lets the mouse through
// (pointer-events: none), LegendInput covers only the parts that take the mouse in the CSS
// (the title and the indicator/compare/note rows) and handles hover and clicks.

#include <QFont>
#include <QPointer>
#include <QRectF>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace qtm {

class ChartPane;
class LegendInput;

struct LegendModel {
    QString name;               // "BTCUSDT · 1D · Binance"
    bool hasBar = false;
    QString o, h, l, c, chg;
    QString cls;                // "", "up", "down"
    struct Compare { QString sym, label; };
    QVector<Compare> compare;
    struct Val { QString text, color; };   // color: CSS colour or "--var"
    struct Ind {
        QString uid;
        QString name;
        QVector<Val> vals;
        bool hidden = false;
    };
    QVector<Ind> inds;
    QStringList notes;
};

class Legend : public QWidget
{
    Q_OBJECT
public:
    explicit Legend(ChartPane *pane);
    void setModel(const LegendModel &m);
    void relayout();
    void setInput(LegendInput *in) { m_input = in; }

    // Layout results (legend coordinates).
    struct Row {
        enum Kind { Main, Compare, Ind, Note } kind = Main;
        QString key;            // "c:<sym>" or "i:<uid>"
        double y = 0, h = 20;
        double width = 0;       // the row's box width
        QRectF title;           // Main: the title span
        QVector<QRectF> acts;   // action buttons when shown
        QStringList actNames;   // "settings", "hide", "remove", "rmcompare"
        int index = 0;          // into compare / inds / notes
        QStringList lines;      // Note: wrapped lines
    };
    QVector<Row> rows;
    double legendWidth = 0;
    QString hoverKey;
    QString hoverAct;
    LegendModel model;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    ChartPane *m_pane;
    LegendInput *m_input = nullptr;
};

class LegendInput : public QWidget
{
    Q_OBJECT
public:
    LegendInput(ChartPane *pane, Legend *legend);
    void sync();   // follow the legend's geometry and interactive parts

protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void hoverAt(const QPointF &p);
    QString actAt(const QPointF &p, QString *key) const;
    ChartPane *m_pane;
    Legend *m_legend;
    QString m_pressAct, m_pressKey;
};

} // namespace qtm
