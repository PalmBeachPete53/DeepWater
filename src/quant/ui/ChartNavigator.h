#pragma once

// The range slider under a chart pane (a Deepwater addition, not in Quant Terminal): a thin strip with a mini line of
// the closes of every bar loaded, and a window over the part the chart shows. Dragging the window scrolls the chart at
// the same zoom; a click outside it centres it there (keep dragging without letting go); dragging its left or right
// edge zooms (narrower = in), never under 10 bars nor past the data. Only the time axis moves: the chart's price scale
// fits the visible bars itself (autoscale). The gestures' rules are core/RangeSlider (pure, tested).
//
// One source of truth: the chart's visible logical range (its time scale). The slider only reads it (redrawn when the
// chart says it changed: Chart::visibleLogicalRangeChanged, at most once a frame) and writes it
// (TimeScale::setVisibleLogicalRange, applied with the chart's next frame). So scrolling or zooming the chart itself,
// new bars (the chart follows them when the last bar is in view, and the window with it), a new symbol or timeframe (the
// chart shows its last 160 bars) all show up here with nothing else to do.
//
// The strip spans the chart's time points (every series' times: a compared symbol can add some) plus the chart's right
// margin (to 9 bars after the last one, where a fresh chart's view ends); each bar's close sits at its own logical index.
// Ranges here are edges (bar i spans i − ½ … i + ½); the chart's own range names the edge bars' centres.

#include "../core/Bar.h"
#include "../core/RangeSlider.h"

#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

#include <functional>
#include <optional>

namespace lw {
class Chart;
}

namespace qtm {

class ChartNavigator : public QWidget
{
public:
    static constexpr int HEIGHT = 36;
    static rangeslider::Params params() { return {10, 9, 8}; }   // min bars, right margin (ChartPane's fresh view), grab px

    // `bars`: the bars the chart shows (their closes make the line); `toChart`: a bar's time as the chart has it.
    ChartNavigator(lw::Chart *chart, std::function<const Bars &()> bars, std::function<double(double)> toChart,
                   QWidget *parent = nullptr);

    // For the tests: the strip and where the window is drawn (px), or none.
    rangeslider::Span span() const;
    std::optional<std::pair<double, double>> windowX() const;
    int curveBuilds() const { return m_curveBuilds; }
    int curvePoints() const { return m_curvePoints; }   // bars placed on the line by the last build
    double curveLastX() const { return m_curveLastX; }   // where the line ends (px on the track)

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

private:
    struct CurveKey {
        int n = -1, points = 0;
        double first = 0, last = 0, lastClose = 0, w = 0, h = 0, dpr = 1;
        QRgb theme = 0;
        bool operator==(const CurveKey &) const = default;
    };
public:
    // The chart's visible range as edges (its own range names the bars at the edges: their centres, ± ½).
    std::optional<rangeslider::Range> range() const;

private:
    void set(const rangeslider::Range &r);
    void updateCursor(double x);
    const QPixmap &trackImage(const rangeslider::Span &s, const QRectF &track);

    lw::Chart *m_chart;
    std::function<const Bars &()> m_bars;
    std::function<double(double)> m_toChart;
    rangeslider::Gesture m_gesture{params()};
    // The track with its line, drawn again only when the bars, the size or the theme change.
    QPixmap m_track;
    CurveKey m_trackKey;
    int m_curveBuilds = 0, m_curvePoints = 0;
    double m_curveLastX = 0;
};

} // namespace qtm
