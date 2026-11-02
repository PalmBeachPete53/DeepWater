#pragma once

// Port of quant-terminal/public/js/drawings.js: drawing tools rendered as a chart series primitive.
// Points are stored as { time (UTC seconds), price } so drawings stay put when you change timeframe.

#include "../chart/Chart.h"

#include <QJsonArray>
#include <QFont>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

namespace qtm {

class ChartPane;

struct DrawingTool {
    QString id;
    QString label;
    int points = 0;     // 0 for the cursor
    QString icon;       // inner SVG of a 24x24 viewBox
};

// JS TOOLS, TOOL_BY_ID, FIB_LEVELS.
const QVector<DrawingTool> &drawingTools();
const DrawingTool *toolById(const QString &id);
extern const QVector<double> FIB_LEVELS;

struct DrawPoint {
    double time = 0;
    double price = 0;
};

// { id, type, points, text, color, rr }. Missing fields stay missing when saved.
struct Drawing {
    QString id;
    QString type;
    QVector<DrawPoint> points;
    std::optional<QString> text;
    std::optional<QString> color;
    std::optional<double> rr;

    QJsonObject toJson() const;
    static Drawing fromJson(const QJsonObject &o);
};

QJsonArray drawingsToJson(const QVector<Drawing> &items);
QVector<Drawing> drawingsFromJson(const QJsonArray &a);

class DrawingLayer : public lw::Primitive
{
public:
    explicit DrawingLayer(ChartPane *pane);

    QVector<Drawing> items;
    std::optional<Drawing> temp;   // the drawing being placed
    QString selected;              // id of the selected drawing (JS keeps the object)

    Drawing *selectedItem();
    void setItems(const QVector<Drawing> &items);
    void update() { requestUpdate(); }
    // Id of the nearest drawing within a few pixels of (x, y), empty when none.
    QString hitTest(double x, double y) const;

    ZOrder zOrder() const override { return ZOrder::Top; }
    void draw(QPainter &p, const QSizeF &size) override;

private:
    std::optional<double> x(double t) const;
    std::optional<double> y(double price) const;
    void drawOne(QPainter &p, const QSizeF &size, const Drawing &d, bool active);
    ChartPane *m_pane;
    QFont m_font;
};

} // namespace qtm
