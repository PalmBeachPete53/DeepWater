#pragma once

#include "GeoData.h"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QThreadPool>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>

class ChokepointMonitor;
class MarketData;
class VesselStore;
class QToolButton;

// Interactive orthographic globe rendered with QPainter.
// Drag to spin, scroll to zoom, click a marker to select it.
class GlobeWidget : public QWidget
{
    Q_OBJECT
public:
    GlobeWidget(MarketData *data, VesselStore *vessels, ChokepointMonitor *chokepoints,
                QWidget *parent = nullptr);

    struct ViewState {
        double lon;
        double lat;
        double zoom;
        bool autoSpin;
    };
    ViewState viewState() const { return {m_lon, m_lat, m_zoom, m_autoSpin}; }
    void setViewState(const ViewState &state);

    void setAutoSpin(bool on);
    bool autoSpin() const { return m_autoSpin; }
    // The user's own choice (button, Space): saved with QSettings for the next launch, unlike
    // the automatic stops when flying to a selection.
    void toggleAutoSpin();

    void zoomBy(double factor);
    void resetView();
    void flyTo(double latDeg, double lonDeg, double zoom = -1);
    void flyToCountry(const QString &iso3);

    // Country fills for a choropleth map, with a legend. Empty colors clear it.
    struct Heatmap {
        QString title;
        QHash<QString, QColor> colors;    // ISO3 -> fill
        QHash<QString, QString> labels;   // ISO3 -> tooltip text
        QHash<QString, double> heights;   // ISO3 -> 0..1, prism height in the prism view
        QVector<QColor> ramp;             // legend colors, low to high
        QString lowLabel;
        QString highLabel;
    };
    void setHeatmap(const Heatmap &heatmap);
    // Prism map: countries of the heatmap raised by their height, seen with a tilted
    // camera (saved with QSettings).
    void setPrismMode(bool on);
    bool prismMode() const { return m_prism; }

    // Flow map: arcs between countries, from exporter to importer, shown while the flow view
    // is on (saved with QSettings).
    struct FlowArc {
        QString from;    // ISO3
        QString to;
        double value;    // sets the width, relative to the largest arc
        QColor color;
        QString label;   // tooltip text
    };
    struct FlowMap {
        QString title;
        QVector<QPair<QColor, QString>> keys;   // legend entries
        QVector<FlowArc> arcs;                  // largest first
    };
    void setFlowMap(const FlowMap &map);
    void setFlowMode(bool on);
    bool flowMode() const { return m_flowMode; }

    // Points of interest shown while the supply chain layer is on (saved with QSettings): ports, oil storage
    // sites, disruptions. A point is drawn from `minZoom` on, so that small ports wait for a closer view.
    struct MapPoint {
        enum Shape { Dot, Diamond, Ring };
        QString key;          // what pointSelected() hands back
        double lat = 0;
        double lon = 0;
        Shape shape = Dot;
        QColor color;
        double size = 3;      // radius in pixels at zoom 1
        double minZoom = 0;
        QString tip;          // tooltip, rich text
    };
    void setPoints(const QVector<MapPoint> &points);
    void setPointsMode(bool on);
    bool pointsMode() const { return m_pointsMode; }

public slots:
    void setSelection(const QVector<int> &siteIndices);
    void setSelectedVessel(qint64 mmsi);   // 0 clears
    void setSelectedChokepoint(int zone);  // -1 clears
    void setSelectedCountry(const QString &iso3);   // empty clears
    void setSelectedPoint(const QString &key);      // empty clears
    // Probable route of the selected vessel; an empty path clears it.
    void setRoute(const QVector<QVector3D> &path, const QString &destinationLabel);

signals:
    // Emitted when the user clicks a marker or cluster (empty when clicking empty space).
    void selectionChanged(const QVector<int> &siteIndices);
    void vesselSelected(qint64 mmsi);
    void chokepointSelected(int zone);
    // Click on land away from any marker.
    void countrySelected(const QString &iso3);
    void pointSelected(const QString &key);
    void pointsModeChanged(bool on);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    struct Projected {
        QPointF pos;
        float depth;   // > 0 means on the visible hemisphere
    };
    struct Cluster {
        QPointF center;
        double radius = 0;
        float depth = 1;
        QVector<int> sites;
    };

    void updateRotation();
    Projected project(const QVector3D &v) const;
    double radius() const;
    QPointF globeCenter() const;

    void renderBase();
    // Calls `paint` for horizontal bands of `height` rows [top, bottom), on the render pool and
    // this thread.
    void paintBands(int height, const std::function<void(int, int)> &paint);
    // Paints rows [top, bottom) of `image`, whose pixels start at `rows`, with a painter in
    // widget coordinates; `paint` also gets the band in widget coordinates.
    void paintRows(const QImage &image, uchar *rows, int top, int bottom,
                   const std::function<void(QPainter &, const QRectF &)> &paint) const;
    // Projects what the base image shows into m_scene (and the hit-test shapes), once per render.
    void layoutBase();
    void layoutLand();
    void layoutPrisms();
    void layoutSelectedCountry();
    void layoutFlows();
    // Paints m_scene; called by each band of renderBase (`band`, in widget coordinates), so
    // only reads members.
    void paintBase(QPainter &p, const QRectF &band) const;
    void paintBackdrop(QPainter &p) const;    // background, halo and ocean, under everything
    void paintLimbShade(QPainter &p) const;   // over the land, on a transparent image
    void drawBackgroundGrid(QPainter &p) const;
    void drawGlobe(QPainter &p, const QRectF &band) const;
    void drawMarkers(QPainter &p);
    void drawVessels(QPainter &p);
    void drawChokepoints(QPainter &p);
    void drawRoute(QPainter &p);
    void drawPoints(QPainter &p);
    int pointAt(const QPointF &pos) const;
    int chokepointAt(const QPointF &pos) const;
    QString countryAt(const QPointF &pos) const;
    void drawSelectedCountry(QPainter &p, const QRectF &band) const;
    void drawPrisms(QPainter &p, const QRectF &band) const;
    void drawFlowTrails(QPainter &p, const QRectF &band) const;   // in the base image
    void drawFlows(QPainter &p);               // moving particles and legend, every frame
    int flowArcAt(const QPointF &pos) const;
    const GeoRing *mainRing(int country) const;   // the mainland rather than overseas islands
    void animatePrisms();   // toward the state set by the prism mode and the heatmap
    double prismHeight(const QString &iso3) const;   // in globe radii, 0 when flat
    void drawLegend(QPainter &p);
    qint64 vesselAt(const QPointF &pos) const;
    void drawCluster(QPainter &p, const Cluster &c, bool hovered, bool selected);
    void buildClusters();

    int clusterAt(const QPointF &pos) const;
    void animateTo(double lon, double lat, double zoom, int ms);
    void layoutControls();
    void tick();
    // The frame timer runs only while something moves (spin, inertia, a selection's pulse, flow
    // particles) and the globe is shown: wake() restarts it when one of these starts.
    bool isAnimating() const;
    void wake();

    MarketData *m_data;
    VesselStore *m_vessels;
    ChokepointMonitor *m_chokepoints;
    QVector<Country> m_countries;
    // Country outlines in lon/lat degrees with their bounds, for hit testing.
    QVector<QVector<QPolygonF>> m_countryShapes;
    QVector<QVector<QRectF>> m_countryBounds;
    QHash<QString, int> m_countryIndex;   // ISO3 -> index in m_countries
    Heatmap m_heatmap;
    bool m_prism = false;
    double m_prismScale = 0;   // 0..1, animated when toggling the prism view; also drives the tilt
    double m_tiltSin = 0;      // camera tilt, from m_prismScale
    double m_tiltCos = 1;
    QVariantAnimation m_prismAnim;
    struct PrismHit {
        QString iso;
        QPolygonF shape;
        QRectF bounds;
    };
    QVector<PrismHit> m_prismHits;   // drawn prism faces, back to front, from the last base render
    // A polyline or polygon on screen with its bounding box: each band of renderBase paints
    // only the shapes that cross it, since Qt strokes a whole line before clipping it. Long
    // lines are cut into short pieces (addLine) for that.
    struct Shape {
        QPolygonF points;
        QRectF bounds;
        bool crosses(const QRectF &band, double margin) const
        {
            return bounds.adjusted(-margin, -margin, margin, margin).intersects(band);
        }
    };
    static Shape makeShape(QPolygonF points);
    static void addLine(QVector<Shape> &out, const QPolygonF &line);
    // Screen geometry of the base image, from layoutBase. Polygons only: the bands paint it
    // from several threads, and a QPainterPath builds a cache on first use that isn't thread-safe.
    struct BaseScene {
        QVector<Shape> graticule;
        QVector<Shape> fills;
        QVector<QColor> fillColors;
        QVector<Shape> borders;
        struct Face {
            Shape shape;
            QColor color;
            bool cap;   // top of a prism (outlined), otherwise a wall
        };
        QVector<Face> prismFaces;              // back to front
        QVector<Shape> outline, outlineGlow;   // selected country
    };
    BaseScene m_scene;
    FlowMap m_flowMap;
    bool m_flowMode = false;
    struct FlowPath {
        QPolygonF points;           // on screen, exporter to importer
        QVector<bool> visible;      // per point
        QVector<Shape> lines;       // visible runs of points, in pieces
        double width = 0;
    };
    QVector<FlowPath> m_flowPaths;   // per arc (empty when not drawn), from the last base render
    QString m_selectedCountry;
    QVector<QVector<QVector3D>> m_graticule;
    QVector<QVector3D> m_siteVectors;

    // View state
    double m_lon = -30;   // longitude at the center of the view
    double m_lat = 30;    // latitude at the center of the view
    double m_zoom = 1.0;
    float m_rot[9] {};    // rows: depth, east, north

    // Interaction
    bool m_autoSpin = true;
    bool m_dragging = false;
    bool m_pressed = false;
    QPointF m_pressPos;
    QPointF m_lastPos;
    QElapsedTimer m_lastMoveTime;
    QPointF m_velocity;   // deg/frame, for inertia after drag
    QTimer m_frameTimer;
    int m_spinFrame = 0;   // frames since the last auto-rotation step
    QElapsedTimer m_clock;
    QVariantAnimation m_viewAnim;
    double m_animFrom[3] {};
    double m_animTo[3] {};

    QVector<Cluster> m_clusters;
    int m_hovered = -1;
    QVector<int> m_selection;
    qint64 m_selectedVessel = 0;
    int m_selectedZone = -1;
    QVector<QVector3D> m_route;
    QString m_routeLabel;
    QVector<QPair<QPointF, double>> m_zoneHits;     // projected center + radius, per zone
    QVector<QPair<QPointF, qint64>> m_vesselHits;   // screen positions from the last paint
    QVector<MapPoint> m_points;
    QVector<QVector3D> m_pointVectors;
    bool m_pointsMode = false;
    QString m_selectedPoint;
    struct PointHit {
        QPointF pos;
        double radius;
        int index;
    };
    QVector<PointHit> m_pointHits;   // drawn points, from the last paint, top last

    // Ships are drawn as sprites: each shape (a tanker or cargo triangle per heading, a moored dot,
    // an unidentified ship's dot) rasterized once at the screen's resolution, for each sub-pixel
    // phase, then copied. Filling thousands of small paths every frame cost tens of milliseconds.
    enum ShipSprite { TankerShip, CargoShip, TankerDot, CargoDot, UnknownDot, ShipSpriteKinds };
    struct ShipSprites {
        double size = 0;      // the ships' size (logical px, in quarter pixels)
        qreal dpr = 0;
        QRgb tanker = 0, cargo = 0, outline = 0, unknown = 0;
        int phases = 1;       // sub-pixel positions per axis
        int margin = 0;       // device px from the image's corner to the ship's anchor
        QVector<QImage> images;   // kind, heading, phase y, phase x; built on first use
    };
    const QImage &shipSprite(ShipSprites &set, int kind, int heading, int phaseX, int phaseY) const;
    ShipSprites &shipSprites(double size);
    QVector<ShipSprites> m_shipSprites;   // the last few sizes, most recent first (zooming)

    // Cached render of everything below the markers, painted in horizontal bands on several
    // threads (QImage, not QPixmap, for that).
    QImage m_base;
    QThreadPool m_renderPool;
    bool m_baseDirty = true;
    // The radial gradients of the base image don't turn with the globe: they are rendered once
    // per size, position on screen and theme, and copied into each render.
    struct BackdropKey {
        QSize pixels;
        qreal dpr = 0;
        QPointF center;
        double radius = 0;
        QString theme;
        bool operator==(const BackdropKey &) const = default;
    };
    BackdropKey m_backdropKey;
    QImage m_backdrop;
    QImage m_limbShade;

    QToolButton *m_spinButton;
    QToolButton *m_zoomInButton;
    QToolButton *m_zoomOutButton;
    QToolButton *m_resetButton;
    QToolButton *m_prismButton;
    QToolButton *m_flowButton;
    QToolButton *m_pointsButton;
};
