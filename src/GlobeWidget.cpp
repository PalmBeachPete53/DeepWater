#include "GlobeWidget.h"

#include "Chokepoints.h"
#include "MarketData.h"
#include "Theme.h"
#include "VesselStore.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QThread>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>
#include <QtMath>

#include <atomic>
#include <cstring>

namespace {

constexpr double MinZoom = 0.6;
constexpr double MaxZoom = 12.0;
constexpr double ClusterDistance = 26.0;  // px; markers closer than this merge
constexpr double DotRadius = 4.5;
constexpr double SpinSpeed = 0.035;       // degrees per frame
constexpr int FrameMs = 16;
constexpr double MaxPrismHeight = 0.10;   // globe radii, for the top of the scale
constexpr double MinPrismHeight = 0.08;   // fraction of the max, so low values still stand out from flat land
constexpr double FlowTripSeconds = 7.0;   // time for a flow particle to go from exporter to importer
constexpr double PrismTilt = 45.0;        // camera tilt in the prism view, degrees from straight down

// Prisms sink toward the horizon (depth 0): seen edge-on at the limb, full-height walls
// would turn the big countries there into a thick band around the globe.
float prismTaper(float depth)
{
    const float t = std::clamp((depth - 0.1f) / 0.7f, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

double wrapLon(double lon)
{
    while (lon > 180) lon -= 360;
    while (lon < -180) lon += 360;
    return lon;
}

QToolButton *makeControl(const QString &text, const QString &tip, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setObjectName("globeControl");
    b->setText(text);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(40, 40);
    return b;
}

} // namespace

GlobeWidget::GlobeWidget(MarketData *data, VesselStore *vessels, ChokepointMonitor *chokepoints,
                         QWidget *parent)
    : QWidget(parent)
    , m_data(data)
    , m_vessels(vessels)
    , m_chokepoints(chokepoints)
{
    setMouseTracking(true);
    setMinimumSize(400, 400);
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_countries = Geo::loadCountries(":/countries.json");
    for (int i = 0; i < m_countries.size(); ++i) {
        m_countryIndex.insert(m_countries[i].iso, i);
        QVector<QPolygonF> shapes;
        QVector<QRectF> bounds;
        for (const GeoRing &ring : m_countries[i].rings) {
            QPolygonF poly;
            for (const QVector3D &v : ring.points)
                poly << QPointF(qRadiansToDegrees(std::atan2(v.y(), v.x())),
                                qRadiansToDegrees(std::asin(std::clamp(v.z(), -1.f, 1.f))));
            bounds.push_back(poly.boundingRect());
            shapes.push_back(std::move(poly));
        }
        m_countryShapes.push_back(std::move(shapes));
        m_countryBounds.push_back(std::move(bounds));
    }

    // Graticule: meridians and parallels every 15 degrees, sampled every 2 degrees.
    for (int lon = -180; lon < 180; lon += 15) {
        QVector<QVector3D> line;
        for (int lat = -80; lat <= 80; lat += 2)
            line.push_back(Geo::toVector(lat, lon));
        m_graticule.push_back(line);
    }
    for (int lat = -75; lat <= 75; lat += 15) {
        QVector<QVector3D> line;
        for (int lon = -180; lon <= 180; lon += 2)
            line.push_back(Geo::toVector(lat, lon));
        m_graticule.push_back(line);
    }

    for (const Site &s : m_data->sites())
        m_siteVectors.push_back(Geo::toVector(s.lat, s.lon));

    m_spinButton = makeControl(QString::fromUtf8("■"), tr("Pause rotation"), this);
    m_zoomInButton = makeControl("+", tr("Zoom in"), this);
    m_zoomOutButton = makeControl(QString::fromUtf8("−"), tr("Zoom out"), this);
    m_resetButton = makeControl(QString::fromUtf8("⟲"), tr("Reset view"), this);
    connect(m_spinButton, &QToolButton::clicked, this, &GlobeWidget::toggleAutoSpin);
    connect(m_zoomInButton, &QToolButton::clicked, this, [this] { zoomBy(1.5); });
    connect(m_zoomOutButton, &QToolButton::clicked, this, [this] { zoomBy(1 / 1.5); });
    connect(m_resetButton, &QToolButton::clicked, this, &GlobeWidget::resetView);
    m_prismButton = makeControl("3D", tr("Prism map: raise countries by value"), this);
    m_prismButton->setCheckable(true);
    m_prismButton->hide();   // only with a heatmap
    connect(m_prismButton, &QToolButton::clicked, this, [this] { setPrismMode(!m_prism); });

    m_flowButton = makeControl(QString::fromUtf8("⇄"), QString(), this);
    m_flowButton->setCheckable(true);
    connect(m_flowButton, &QToolButton::clicked, this, [this] { setFlowMode(!m_flowMode); });
    setAutoSpin(QSettings().value("autoSpin", true).toBool());
    m_flowMode = QSettings().value("flowMap", false).toBool();
    m_flowButton->setChecked(m_flowMode);
    m_flowButton->setToolTip(m_flowMode ? tr("Hide trade flows") : tr("Flow map: trade in goods"));

    m_pointsButton = makeControl(QString::fromUtf8("⚓"), QString(), this);
    m_pointsButton->setCheckable(true);
    connect(m_pointsButton, &QToolButton::clicked, this, [this] { setPointsMode(!m_pointsMode); });
    m_pointsMode = QSettings().value("supplyLayer", false).toBool();
    m_pointsButton->setChecked(m_pointsMode);
    m_pointsButton->setToolTip(m_pointsMode ? tr("Hide the supply chain layer")
                                            : tr("Supply chains: ports, oil storage, disruptions"));

    m_prism = QSettings().value("prismMap", false).toBool();
    m_prismButton->setChecked(m_prism);
    m_prismButton->setToolTip(m_prism ? tr("Flat map") : tr("Prism map: raise countries by value"));
    m_prismAnim.setDuration(700);
    m_prismAnim.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_prismAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_prismScale = v.toDouble();
        updateRotation();   // the camera tilts with the prisms
    });

    // Threads painting bands of the base image alongside the GUI thread.
    m_renderPool.setMaxThreadCount(std::clamp(QThread::idealThreadCount() - 1, 1, 7));

    m_viewAnim.setStartValue(0.0);
    m_viewAnim.setEndValue(1.0);
    m_viewAnim.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_viewAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const double t = v.toDouble();
        m_lon = wrapLon(m_animFrom[0] + (m_animTo[0] - m_animFrom[0]) * t);
        m_lat = m_animFrom[1] + (m_animTo[1] - m_animFrom[1]) * t;
        m_zoom = m_animFrom[2] * qPow(m_animTo[2] / m_animFrom[2], t);
        updateRotation();
    });

    connect(m_data, &MarketData::filterChanged, this, [this] { update(); });
    connect(m_vessels, &VesselStore::changed, this, [this] { update(); });
    connect(m_vessels, &VesselStore::filterChanged, this, [this] { update(); });

    updateRotation();
    m_clock.start();
    connect(&m_frameTimer, &QTimer::timeout, this, &GlobeWidget::tick);
    m_frameTimer.setInterval(FrameMs);
    m_frameTimer.start();
}

bool GlobeWidget::isAnimating() const
{
    return !m_velocity.isNull() || m_autoSpin || !m_selection.isEmpty() || m_selectedVessel
           || (m_flowMode && !m_flowMap.arcs.isEmpty());
}

void GlobeWidget::wake()
{
    if (isVisible() && !m_frameTimer.isActive() && isAnimating())
        m_frameTimer.start();
}

void GlobeWidget::showEvent(QShowEvent *)
{
    wake();
}

void GlobeWidget::hideEvent(QHideEvent *)
{
    m_frameTimer.stop();
}

// ---------------------------------------------------------------- view control

void GlobeWidget::setAutoSpin(bool on)
{
    m_autoSpin = on;
    m_spinButton->setText(on ? QString::fromUtf8("■") : QString::fromUtf8("▶"));
    m_spinButton->setToolTip(on ? tr("Pause rotation") : tr("Resume rotation"));
    wake();
}

void GlobeWidget::toggleAutoSpin()
{
    setAutoSpin(!m_autoSpin);
    QSettings().setValue("autoSpin", m_autoSpin);
}

void GlobeWidget::setViewState(const ViewState &state)
{
    m_lon = state.lon;
    m_lat = state.lat;
    m_zoom = state.zoom;
    setAutoSpin(state.autoSpin);
    updateRotation();
}

void GlobeWidget::zoomBy(double factor)
{
    animateTo(m_lon, m_lat, std::clamp(m_zoom * factor, MinZoom, MaxZoom), 250);
}

void GlobeWidget::resetView()
{
    animateTo(-30, 30, 1.0, 700);
}

void GlobeWidget::flyTo(double latDeg, double lonDeg, double zoom)
{
    setAutoSpin(false);
    animateTo(lonDeg, latDeg, zoom > 0 ? zoom : std::max(m_zoom, 2.0), 900);
}

void GlobeWidget::flyToCountry(const QString &iso3)
{
    const GeoRing *main = mainRing(m_countryIndex.value(iso3, -1));
    if (!main)
        return;
    const QVector3D c = main->center;
    const double angle = std::acos(std::clamp(double(main->cosRadius), -1.0, 1.0));
    // Fit the country in about half the globe's radius.
    const double zoom = std::clamp(0.45 / std::max(std::sin(angle), 0.01), 1.0, 6.0);
    flyTo(qRadiansToDegrees(std::asin(std::clamp(c.z(), -1.f, 1.f))),
          qRadiansToDegrees(std::atan2(c.y(), c.x())), zoom);
}

const GeoRing *GlobeWidget::mainRing(int country) const
{
    if (country < 0 || m_countries[country].rings.isEmpty())
        return nullptr;
    const GeoRing *main = &m_countries[country].rings.first();
    for (const GeoRing &ring : m_countries[country].rings)
        if (ring.cosRadius < main->cosRadius)
            main = &ring;
    return main;
}

void GlobeWidget::setFlowMap(const FlowMap &map)
{
    m_flowMap = map;
    m_baseDirty = true;   // the trails are in the base image
    update();
    wake();
}

void GlobeWidget::setFlowMode(bool on)
{
    m_flowMode = on;
    m_flowButton->setChecked(on);
    m_flowButton->setToolTip(on ? tr("Hide trade flows") : tr("Flow map: trade in goods"));
    QSettings().setValue("flowMap", on);
    m_baseDirty = true;
    update();
    wake();
}

void GlobeWidget::setPoints(const QVector<MapPoint> &points)
{
    m_points = points;
    m_pointVectors.clear();
    for (const MapPoint &pt : points)
        m_pointVectors.push_back(Geo::toVector(pt.lat, pt.lon));
    update();
}

void GlobeWidget::setPointsMode(bool on)
{
    m_pointsMode = on;
    m_pointsButton->setChecked(on);
    m_pointsButton->setToolTip(on ? tr("Hide the supply chain layer") : tr("Supply chains: ports, oil storage, disruptions"));
    QSettings().setValue("supplyLayer", on);
    if (!on)
        m_selectedPoint.clear();
    update();
    emit pointsModeChanged(on);
}

void GlobeWidget::setSelectedPoint(const QString &key)
{
    m_selectedPoint = key;
    update();
}

void GlobeWidget::drawPoints(QPainter &p)
{
    m_pointHits.clear();
    if (!m_pointsMode)
        return;
    const Palette &pal = Theme::current();
    const double scale = std::clamp(std::sqrt(m_zoom), 1.0, 2.5);
    int selected = -1;
    for (int i = 0; i < m_points.size(); ++i) {
        const MapPoint &pt = m_points[i];
        if (pt.minZoom > m_zoom && pt.key != m_selectedPoint)
            continue;
        const Projected pr = project(m_pointVectors[i]);
        if (pr.depth < 0.03f)
            continue;
        const double r = pt.size * scale;
        if (pt.key == m_selectedPoint)
            selected = i;
        QColor outline = pal.Background;
        outline.setAlpha(200);
        switch (pt.shape) {
        case MapPoint::Dot:
            p.setPen(QPen(outline, 0.8));
            p.setBrush(pt.color);
            p.drawEllipse(pr.pos, r, r);
            break;
        case MapPoint::Diamond: {
            const QPointF c = pr.pos;
            const QPolygonF d {c + QPointF(0, -r * 1.35), c + QPointF(r, 0), c + QPointF(0, r * 1.35), c + QPointF(-r, 0)};
            p.setPen(QPen(outline, 1.0));
            p.setBrush(pt.color);
            p.drawPolygon(d);
            break;
        }
        case MapPoint::Ring: {
            QColor fill = pt.color;
            fill.setAlpha(40);
            p.setPen(QPen(pt.color, 1.6));
            p.setBrush(fill);
            p.drawEllipse(pr.pos, r, r);
            p.setPen(Qt::NoPen);
            p.setBrush(pt.color);
            p.drawEllipse(pr.pos, 2.0, 2.0);
            break;
        }
        }
        m_pointHits.push_back({pr.pos, std::max(r + 2, 6.0), i});
    }
    if (selected >= 0) {
        const Projected pr = project(m_pointVectors[selected]);
        const double r = m_points[selected].size * scale * (m_points[selected].shape == MapPoint::Diamond ? 1.35 : 1.0);
        QPen pen(pal.Text, 1.6);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pr.pos, r + 4, r + 4);
    }
}

int GlobeWidget::pointAt(const QPointF &pos) const
{
    int best = -1;
    double bestDist = 1e9;
    // The last drawn is on top: ties go to it.
    for (const PointHit &h : m_pointHits) {
        const double d = QLineF(h.pos, pos).length();
        if (d <= h.radius && d <= bestDist) {
            bestDist = d;
            best = h.index;
        }
    }
    return best;
}

void GlobeWidget::setHeatmap(const Heatmap &heatmap)
{
    m_heatmap = heatmap;
    m_prismButton->setVisible(!heatmap.colors.isEmpty());
    layoutControls();
    animatePrisms();
    m_baseDirty = true;
    update();
}

void GlobeWidget::setPrismMode(bool on)
{
    m_prism = on;
    m_prismButton->setChecked(on);
    m_prismButton->setToolTip(on ? tr("Flat map") : tr("Prism map: raise countries by value"));
    QSettings().setValue("prismMap", on);
    animatePrisms();
}

void GlobeWidget::animatePrisms()
{
    // Prisms (and the camera tilt) only show with a heatmap.
    const double target = m_prism && !m_heatmap.colors.isEmpty() ? 1.0 : 0.0;
    const bool running = m_prismAnim.state() == QAbstractAnimation::Running;
    if (running ? m_prismAnim.endValue().toDouble() == target : m_prismScale == target)
        return;
    m_prismAnim.stop();
    m_prismAnim.setStartValue(m_prismScale);
    m_prismAnim.setEndValue(target);
    m_prismAnim.start();
}

double GlobeWidget::prismHeight(const QString &iso3) const
{
    const auto it = m_heatmap.heights.constFind(iso3);
    if (it == m_heatmap.heights.cend() || m_prismScale <= 0)
        return 0;
    return MaxPrismHeight * (MinPrismHeight + (1 - MinPrismHeight) * it.value()) * m_prismScale;
}

void GlobeWidget::setSelectedCountry(const QString &iso3)
{
    m_selectedCountry = iso3;
    m_baseDirty = true;
    update();
}

void GlobeWidget::animateTo(double lon, double lat, double zoom, int ms)
{
    m_velocity = {};
    m_viewAnim.stop();
    m_animFrom[0] = m_lon;
    m_animFrom[1] = m_lat;
    m_animFrom[2] = m_zoom;
    // Take the short way around.
    m_animTo[0] = m_lon + wrapLon(lon - m_lon);
    m_animTo[1] = std::clamp(lat, -89.0, 89.0);
    m_animTo[2] = zoom;
    m_viewAnim.setDuration(ms);
    m_viewAnim.start();
}

void GlobeWidget::setSelection(const QVector<int> &siteIndices)
{
    m_selection = siteIndices;
    update();
    wake();
}

void GlobeWidget::setSelectedVessel(qint64 mmsi)
{
    m_selectedVessel = mmsi;
    update();
    wake();
}

void GlobeWidget::setSelectedChokepoint(int zone)
{
    m_selectedZone = zone;
    update();
}

void GlobeWidget::setRoute(const QVector<QVector3D> &path, const QString &destinationLabel)
{
    m_route = path;
    m_routeLabel = destinationLabel;
    update();
}

void GlobeWidget::tick()
{
    bool moved = false;
    if (!m_dragging && !m_velocity.isNull()) {
        // Inertia after a flick.
        m_lon = wrapLon(m_lon - m_velocity.x());
        m_lat = std::clamp(m_lat + m_velocity.y(), -89.0, 89.0);
        m_velocity *= 0.92;
        if (qAbs(m_velocity.x()) < 0.01 && qAbs(m_velocity.y()) < 0.01)
            m_velocity = {};
        moved = true;
    } else if (m_autoSpin && !m_pressed && m_viewAnim.state() != QAbstractAnimation::Running) {
        // Zoomed out, a step moves the globe a fraction of a pixel: turn it every other frame
        // by twice as much, at half the cost. The selection pulse and the flow particles still
        // move on the frames in between (below), without redrawing the globe.
        const int frames = radius() * qDegreesToRadians(SpinSpeed) < 0.5 ? 2 : 1;
        if (++m_spinFrame >= frames) {
            m_spinFrame = 0;
            m_lon = wrapLon(m_lon - SpinSpeed * frames);
            moved = true;
        }
    }

    if (moved)
        updateRotation();
    else if (!m_selection.isEmpty() || m_selectedVessel || (m_flowMode && !m_flowMap.arcs.isEmpty()))
        update();  // keep the selection pulse and the flow particles moving
    if (!isAnimating())
        m_frameTimer.stop();   // until wake()
}

void GlobeWidget::updateRotation()
{
    // Rotate so that (m_lat, m_lon) faces the viewer: Rz(-lon) then Ry(lat).
    const double l = qDegreesToRadians(m_lon);
    const double f = qDegreesToRadians(m_lat);
    const double cl = qCos(l), sl = qSin(l), cf = qCos(f), sf = qSin(f);
    // depth row (toward viewer)
    m_rot[0] = float(cf * cl); m_rot[1] = float(cf * sl); m_rot[2] = float(sf);
    // east row (screen x)
    m_rot[3] = float(-sl);     m_rot[4] = float(cl);      m_rot[5] = 0.f;
    // north row (screen -y)
    m_rot[6] = float(-sf * cl); m_rot[7] = float(-sf * sl); m_rot[8] = float(cf);
    // Tilt (prism view): the camera moves south of the view point and looks north at it,
    // a rotation of the depth and north rows about the east axis.
    const double tilt = qDegreesToRadians(PrismTilt * m_prismScale);
    m_tiltSin = std::sin(tilt);
    m_tiltCos = std::cos(tilt);
    for (int k = 0; k < 3; ++k) {
        const float d = m_rot[k], n = m_rot[6 + k];
        m_rot[k] = float(d * m_tiltCos - n * m_tiltSin);
        m_rot[6 + k] = float(d * m_tiltSin + n * m_tiltCos);
    }
    m_baseDirty = true;
    update();
}

double GlobeWidget::radius() const
{
    return std::min(width(), height()) * 0.44 * m_zoom;
}

QPointF GlobeWidget::globeCenter() const
{
    // With a tilt, the globe moves down to bring the view point back to the middle, but no
    // further than the top of the globe near the top of the view: zoomed out, the globe stays whole.
    const double r = radius();
    const double shift = std::min(m_tiltSin * r, std::max(0.0, r - height() * 0.44));
    return QPointF(width() / 2.0, height() / 2.0 + shift);
}

GlobeWidget::Projected GlobeWidget::project(const QVector3D &v) const
{
    const float d = m_rot[0] * v.x() + m_rot[1] * v.y() + m_rot[2] * v.z();
    const float e = m_rot[3] * v.x() + m_rot[4] * v.y() + m_rot[5] * v.z();
    const float n = m_rot[6] * v.x() + m_rot[7] * v.y() + m_rot[8] * v.z();
    const double r = radius();
    const QPointF c = globeCenter();
    return {QPointF(c.x() + e * r, c.y() - n * r), d};
}

// ---------------------------------------------------------------- rendering

void GlobeWidget::paintEvent(QPaintEvent *)
{
    const qreal dpr = devicePixelRatioF();
    if (m_baseDirty || m_base.size() != size() * dpr) {
        renderBase();
        m_baseDirty = false;
    }

    QPainter p(this);
    p.drawImage(0, 0, m_base);
    p.setRenderHint(QPainter::Antialiasing);
    drawChokepoints(p);
    drawPoints(p);
    drawRoute(p);
    drawVessels(p);
    drawFlows(p);
    buildClusters();
    drawMarkers(p);

    drawLegend(p);

    p.setPen(Theme::current().TextDim);
    QFont f = font();
    f.setPointSizeF(11);
    p.setFont(f);
    p.drawText(QRectF(18, height() - 34, width(), 24), Qt::AlignLeft | Qt::AlignVCenter,
               tr("Drag to spin · scroll to zoom · click a dot or a country"));
}

void GlobeWidget::layoutSelectedCountry()
{
    const int i = m_countryIndex.value(m_selectedCountry, -1);
    if (i < 0)
        return;
    // Outline the top of the prism in the prism view.
    const float height = float(prismHeight(m_selectedCountry));
    const QVector3D view(m_rot[0], m_rot[1], m_rot[2]);
    // Stroking the full outline of a big country (Russia, Canada…) is slow, so vertices closer
    // than a minimum step to the previous kept one are dropped (1 px for the line, more for the
    // soft glow under it), islands of a few pixels are skipped, and the pieces are stroked as
    // separate polylines with bevel joins: several times faster than one path with round joins.
    QVector<Shape> &line = m_scene.outline, &glow = m_scene.outlineGlow;
    for (const GeoRing &ring : m_countries[i].rings) {
        const float sinR = std::sqrt(std::max(0.f, 1.f - ring.cosRadius * ring.cosRadius));
        if (ring.cosRadius > 0 && QVector3D::dotProduct(ring.center, view) < -sinR)
            continue;  // ring entirely on the far side
        if (sinR * radius() < 1.5)
            continue;  // too small to see
        for (auto [pieces, minStep] : {std::pair(&line, 1.0), std::pair(&glow, 3.0)}) {
            QPolygonF piece;
            for (int k = 0; k <= ring.points.size(); ++k) {
                const QVector3D &v = ring.points[k % ring.points.size()];
                const float top = 1 + height * prismTaper(QVector3D::dotProduct(v, view));
                const Projected pr = project(v * top);
                if (pr.depth <= 0) {
                    addLine(*pieces, piece);
                    piece.clear();
                    continue;
                }
                if (!piece.isEmpty() && k < ring.points.size() && (pr.pos - piece.last()).manhattanLength() < minStep)
                    continue;
                piece << pr.pos;
            }
            addLine(*pieces, piece);
        }
    }
}

void GlobeWidget::drawSelectedCountry(QPainter &p, const QRectF &band) const
{
    // Text color stands out against any heatmap fill, light or dark theme.
    QColor halo = Theme::current().Background;
    halo.setAlpha(150);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(halo, 5, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin));
    for (const Shape &piece : m_scene.outlineGlow)
        if (piece.crosses(band, 3))
            p.drawPolyline(piece.points);
    p.setPen(QPen(Theme::current().Text, 1.6, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin));
    for (const Shape &piece : m_scene.outline)
        if (piece.crosses(band, 2))
            p.drawPolyline(piece.points);
}

void GlobeWidget::drawLegend(QPainter &p)
{
    if (m_heatmap.colors.isEmpty() || m_heatmap.ramp.size() < 2)
        return;
    const Palette &pal = Theme::current();
    const QRectF bar(18, height() - 76, 180, 8);
    QLinearGradient g(bar.topLeft(), bar.topRight());
    for (int i = 0; i < m_heatmap.ramp.size(); ++i)
        g.setColorAt(double(i) / (m_heatmap.ramp.size() - 1), m_heatmap.ramp[i]);
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(bar, 3, 3);

    QFont f = font();
    f.setPointSizeF(11);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(pal.Text);
    p.drawText(QRectF(bar.left(), bar.top() - 24, 400, 20), Qt::AlignLeft | Qt::AlignVCenter, m_heatmap.title);
    f.setWeight(QFont::Normal);
    f.setPointSizeF(10);
    p.setFont(f);
    p.setPen(pal.TextDim);
    p.drawText(QRectF(bar.left(), bar.bottom() + 2, bar.width(), 16), Qt::AlignLeft | Qt::AlignVCenter,
               m_heatmap.lowLabel);
    p.drawText(QRectF(bar.left(), bar.bottom() + 2, bar.width(), 16), Qt::AlignRight | Qt::AlignVCenter,
               m_heatmap.highLabel);
}

QString GlobeWidget::countryAt(const QPointF &pos) const
{
    // Prisms first, front to back: they stand over their neighbours.
    for (int i = m_prismHits.size() - 1; i >= 0; --i) {
        const PrismHit &hit = m_prismHits[i];
        if (hit.bounds.contains(pos) && hit.shape.containsPoint(pos, Qt::WindingFill))
            return hit.iso;
    }
    // Inverse projection: screen point -> unit vector -> lon/lat.
    const double r = radius();
    const QPointF c = globeCenter();
    const double e = (pos.x() - c.x()) / r;
    const double n = (c.y() - pos.y()) / r;
    const double d2 = 1.0 - e * e - n * n;
    if (d2 <= 0)
        return {};
    const double d = std::sqrt(d2);
    const double x = d * m_rot[0] + e * m_rot[3] + n * m_rot[6];
    const double y = d * m_rot[1] + e * m_rot[4] + n * m_rot[7];
    const double z = d * m_rot[2] + e * m_rot[5] + n * m_rot[8];
    const QPointF ll(qRadiansToDegrees(std::atan2(y, x)), qRadiansToDegrees(std::asin(std::clamp(z, -1.0, 1.0))));
    for (int i = 0; i < m_countryShapes.size(); ++i) {
        for (int k = 0; k < m_countryShapes[i].size(); ++k) {
            if (m_countryBounds[i][k].contains(ll) && m_countryShapes[i][k].containsPoint(ll, Qt::WindingFill))
                return m_countries[i].iso;
        }
    }
    return {};
}

void GlobeWidget::renderBase()
{
    const qreal dpr = devicePixelRatioF();
    const QSize pixels = size() * dpr;
    if (m_base.size() != pixels)
        m_base = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
    m_base.setDevicePixelRatio(dpr);
    if (m_base.isNull())
        return;
    layoutBase();

    // The backdrop and the limb shade are redrawn only when the globe moves on screen (zoom,
    // tilt, resize) or the theme changes, by each band for its own rows, before it uses them.
    const BackdropKey key {pixels, dpr, globeCenter(), radius(), Theme::current().id};
    const bool newBackdrop = key != m_backdropKey;
    if (newBackdrop) {
        m_backdropKey = key;
        for (QImage *image : {&m_backdrop, &m_limbShade}) {
            if (image->size() != pixels)
                *image = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
            image->setDevicePixelRatio(dpr);
        }
    }
    // Taken here: bits() may detach, which isn't safe from the band threads.
    uchar *base = m_base.bits(), *backdrop = m_backdrop.bits(), *shade = m_limbShade.bits();
    const qsizetype bytesPerLine = m_base.bytesPerLine();
    paintBands(pixels.height(), [&](int top, int bottom) {
        uchar *const rows[] = {base + top * bytesPerLine, backdrop + top * bytesPerLine, shade + top * bytesPerLine};
        const qsizetype bytes = (bottom - top) * bytesPerLine;
        if (newBackdrop) {
            paintRows(m_backdrop, rows[1], top, bottom, [this](QPainter &p, const QRectF &) { paintBackdrop(p); });
            std::memset(rows[2], 0, bytes);
            paintRows(m_limbShade, rows[2], top, bottom, [this](QPainter &p, const QRectF &) { paintLimbShade(p); });
        }
        std::memcpy(rows[0], rows[1], bytes);
        paintRows(m_base, rows[0], top, bottom, [this](QPainter &p, const QRectF &band) { paintBase(p, band); });
    });
}

void GlobeWidget::paintBands(int height, const std::function<void(int, int)> &paint)
{
    // Redrawing the globe is mostly filling pixels (gradients, land, borders), so it is split
    // into horizontal bands painted at the same time, each straight into its rows of the image.
    // The scene is projected once beforehand; every band paints the shapes that cross it. There
    // are more bands than threads, taken in turn by the pool and this thread, so that a few busy
    // bands (the continents, the prisms) don't leave the other threads idle.
    const int workers = m_renderPool.maxThreadCount() + 1;
    const int bands = workers * 3;
    std::atomic<int> next = 0;
    auto renderBands = [&] {
        for (int band; (band = next++) < bands;)
            paint(height * band / bands, height * (band + 1) / bands);
    };
    for (int i = 1; i < workers; ++i)
        m_renderPool.start(renderBands);
    renderBands();
    m_renderPool.waitForDone();
}

void GlobeWidget::paintRows(const QImage &image, uchar *rows, int top, int bottom,
                            const std::function<void(QPainter &, const QRectF &)> &paint) const
{
    QImage band(rows, image.width(), bottom - top, image.bytesPerLine(), image.format());
    const qreal dpr = image.devicePixelRatio();
    band.setDevicePixelRatio(dpr);
    QPainter p(&band);
    p.translate(0, -top / dpr);
    paint(p, QRectF(0, top / dpr, width(), (bottom - top) / dpr));
}

GlobeWidget::Shape GlobeWidget::makeShape(QPolygonF points)
{
    const QRectF bounds = points.boundingRect();
    return {std::move(points), bounds};
}

void GlobeWidget::addLine(QVector<Shape> &out, const QPolygonF &line)
{
    // Pieces share their end points, so the line stays continuous.
    constexpr int Piece = 32;
    for (int start = 0; start + 1 < line.size(); start += Piece)
        out.push_back(makeShape(line.mid(start, Piece + 1)));
}

void GlobeWidget::layoutBase()
{
    m_scene = BaseScene();
    m_prismHits.clear();
    layoutLand();
    layoutPrisms();
    layoutSelectedCountry();
    layoutFlows();
}

void GlobeWidget::paintBase(QPainter &p, const QRectF &band) const
{
    // Drawn over a copy of the backdrop.
    p.setRenderHint(QPainter::Antialiasing);
    drawGlobe(p, band);
    // Here rather than at every frame: the flow map's particles repaint the globe constantly.
    drawSelectedCountry(p, band);
    drawFlowTrails(p, band);
}

void GlobeWidget::drawBackgroundGrid(QPainter &p) const
{
    p.setPen(QPen(Theme::current().GridLine, 1));
    const int step = 36;
    for (int x = (width() / 2) % step; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = (height() / 2) % step; y < height(); y += step)
        p.drawLine(0, y, width(), y);
}

void GlobeWidget::layoutLand()
{
    const QPointF c = globeCenter();
    const double r = radius();

    // Graticule (front hemisphere only).
    for (const auto &line : std::as_const(m_graticule)) {
        QPolygonF run;
        for (const QVector3D &v : line) {
            const Projected pr = project(v);
            if (pr.depth > 0) {
                run << pr.pos;
                continue;
            }
            addLine(m_scene.graticule, run);
            run.clear();
        }
        addLine(m_scene.graticule, run);
    }

    // Land. Points on the far side are pushed radially onto the limb, which
    // clips filled polygons to the visible disc without a full clipping pass.
    // Borders are only stroked where they are actually visible.
    // Vertices closer than MinStep px to the previous kept one are dropped,
    // which keeps the vertex count proportional to on-screen detail.
    constexpr double MinStep = 0.7;
    const QVector3D view(m_rot[0], m_rot[1], m_rot[2]);
    const QRectF viewport = QRectF(rect()).adjusted(-2, -2, 2, 2);
    for (const Country &country : std::as_const(m_countries)) {
        for (const GeoRing &ring : country.rings) {
            const float sinR = std::sqrt(std::max(0.f, 1.f - ring.cosRadius * ring.cosRadius));
            if (ring.cosRadius > 0 && QVector3D::dotProduct(ring.center, view) < -sinR)
                continue;  // ring entirely on the far side

            QPolygonF fill;
            fill.reserve(ring.points.size());
            QPolygonF border;
            bool anyVisible = false;
            bool wasVisible = false;
            for (const QVector3D &v : ring.points) {
                Projected pr = project(v);
                const bool visible = pr.depth > 0;
                if (!visible) {
                    const QPointF d = pr.pos - c;
                    const double len = std::hypot(d.x(), d.y());
                    if (len > 1e-6)
                        pr.pos = c + d * (r / len);
                }
                const bool keep = fill.isEmpty() || visible != wasVisible
                                  || (fill.last() - pr.pos).manhattanLength() >= MinStep;
                if (keep)
                    fill.push_back(pr.pos);

                if (visible) {
                    anyVisible = true;
                    if (keep || !wasVisible)
                        border.push_back(pr.pos);
                } else if (wasVisible && border.size() > 1) {
                    addLine(m_scene.borders, border);
                    border.clear();
                } else {
                    border.clear();
                }
                wasVisible = visible;
            }
            addLine(m_scene.borders, border);
            if (Shape shape = makeShape(std::move(fill)); anyVisible && shape.points.size() > 2
                                                            && shape.bounds.intersects(viewport)) {
                m_scene.fills.push_back(std::move(shape));
                m_scene.fillColors.push_back(m_heatmap.colors.value(country.iso, Theme::current().Land));
            }
        }
    }
}

void GlobeWidget::paintBackdrop(QPainter &p) const
{
    p.fillRect(rect(), Theme::current().Background);
    drawBackgroundGrid(p);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c = globeCenter();
    const double r = radius();

    // Soft halo / atmosphere around the limb.
    QRadialGradient halo(c, r * 1.08);
    QColor h0 = Theme::current().Halo; h0.setAlpha(0);
    QColor h1 = Theme::current().Halo; h1.setAlpha(55);
    halo.setColorAt(0.0, h0);
    halo.setColorAt(r / (r * 1.08) - 0.01, h1);
    halo.setColorAt(1.0, h0);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(c, r * 1.08, r * 1.08);

    // Ocean.
    QRadialGradient ocean(c - QPointF(r * 0.15, r * 0.2), r * 1.1);
    ocean.setColorAt(0.0, Theme::current().OceanInner);
    ocean.setColorAt(1.0, Theme::current().OceanOuter);
    p.setBrush(ocean);
    p.drawEllipse(c, r, r);
}

void GlobeWidget::paintLimbShade(QPainter &p) const
{
    // Darkens toward the edge for a sense of curvature.
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c = globeCenter();
    const double r = radius();
    QPainterPath clip;
    clip.addEllipse(c, r, r);
    p.setClipPath(clip);
    QRadialGradient shade(c, r);
    QColor edge = Theme::current().LimbShade;
    QColor mid = edge;
    mid.setAlpha(edge.alpha() / 4);
    QColor clear = edge;
    clear.setAlpha(0);
    shade.setColorAt(0.0, clear);
    shade.setColorAt(0.75, mid);
    shade.setColorAt(1.0, edge);
    p.setPen(Qt::NoPen);
    p.setBrush(shade);
    p.drawEllipse(c, r, r);
}

void GlobeWidget::drawGlobe(QPainter &p, const QRectF &band) const
{
    const QPointF c = globeCenter();
    const double r = radius();

    p.save();
    QPainterPath clip;
    clip.addEllipse(c, r, r);
    p.setClipPath(clip);

    p.setBrush(Qt::NoBrush);
    QPen gratPen(Theme::current().Graticule, 1.0);
    gratPen.setCosmetic(true);
    p.setPen(gratPen);
    for (const Shape &line : m_scene.graticule)
        if (line.crosses(band, 1))
            p.drawPolyline(line.points);

    p.setPen(Qt::NoPen);
    for (int i = 0; i < m_scene.fills.size(); ++i) {
        if (!m_scene.fills[i].crosses(band, 1))
            continue;
        p.setBrush(m_scene.fillColors[i]);
        p.drawPolygon(m_scene.fills[i].points, Qt::WindingFill);
    }
    QPen borderPen(Theme::current().LandBorder, 1.0);
    borderPen.setCosmetic(true);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    for (const Shape &line : m_scene.borders)
        if (line.crosses(band, 1))
            p.drawPolyline(line.points);
    p.restore();

    // Limb shading, already clipped to the disc: only the rows of the band within its bounds.
    const qreal dpr = m_limbShade.devicePixelRatio();
    const QRect disc = QRectF((c.x() - r) * dpr, (c.y() - r) * dpr, 2 * r * dpr, 2 * r * dpr).toAlignedRect();
    const QRect rows(0, qRound(band.top() * dpr), m_limbShade.width(), qRound(band.height() * dpr));
    if (const QRect src = disc.adjusted(-1, -1, 1, 1) & rows & m_limbShade.rect(); !src.isEmpty())
        p.drawImage(QRectF(QPointF(src.topLeft()) / dpr, QSizeF(src.size()) / dpr), m_limbShade, src);

    // Thin limb outline.
    QColor rim = Theme::current().Halo; rim.setAlpha(140);
    p.setPen(QPen(rim, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r, r);

    // Outside the disc clip: prisms near the limb rise above it.
    drawPrisms(p, band);
}

void GlobeWidget::layoutPrisms()
{
    if (m_prismScale <= 0 || m_heatmap.heights.isEmpty())
        return;
    const QVector3D view(m_rot[0], m_rot[1], m_rot[2]);
    const QVector3D east(m_rot[3], m_rot[4], m_rot[5]);
    const QVector3D north(m_rot[6], m_rot[7], m_rot[8]);
    // Light from the upper left, a little toward the viewer.
    const QVector3D light = (view * 0.35f - east * 0.55f + north * 0.75f).normalized();

    struct Prism {
        const GeoRing *ring;
        const QString *iso;
        float height;
        float depth;
    };
    QVector<Prism> prisms;
    for (const Country &country : std::as_const(m_countries)) {
        const double h = prismHeight(country.iso);
        if (h <= 0)
            continue;
        for (const GeoRing &ring : country.rings) {
            const float sinR = std::sqrt(std::max(0.f, 1.f - ring.cosRadius * ring.cosRadius));
            const float depth = QVector3D::dotProduct(ring.center, view);
            if (ring.cosRadius > 0 && depth < -sinR)
                continue;  // entirely on the far side
            prisms.push_back({&ring, &country.iso, float(h), depth});
        }
    }
    // Painter's algorithm: prisms toward the limb first, the middle of the disc on top
    // (seen at an angle, a prism leans outward, over its neighbours closer to the limb).
    std::sort(prisms.begin(), prisms.end(), [](const Prism &a, const Prism &b) { return a.depth < b.depth; });

    constexpr double MinStep = 1.0;   // px between kept vertices
    constexpr int ShadeLevels = 6;    // walls are merged into strips of one shade
    const QRectF viewport = QRectF(rect()).adjusted(-2, -2, 2, 2);

    QVector<QVector3D> pts;
    QVector<bool> hidden;
    QPolygonF base, cap;
    for (const Prism &prism : std::as_const(prisms)) {
        // Vertices behind the horizon are moved onto it (the flat fills are clipped the
        // same way); the taper brings the height down to zero there.
        pts.clear();
        hidden.clear();
        base.clear();
        cap.clear();
        for (const QVector3D &v : prism.ring->points) {
            QVector3D u = v;
            const float d = QVector3D::dotProduct(u, view);
            const bool behind = d <= 0;
            if (behind) {
                u -= view * d;
                const float len = u.length();
                if (len < 1e-6f)
                    continue;
                u /= len;
            }
            const QPointF b = project(u).pos;
            const QPointF t = behind ? b : project(u * (1 + prism.height * prismTaper(d))).pos;
            if (!cap.isEmpty() && behind == hidden.last() && (cap.last() - t).manhattanLength() < MinStep
                && (base.last() - b).manhattanLength() < MinStep)
                continue;
            pts.push_back(u);
            hidden.push_back(behind);
            base.push_back(b);
            cap.push_back(t);
        }
        const int n = pts.size();
        if (n < 3 || std::all_of(hidden.cbegin(), hidden.cend(), [](bool h) { return h; })
            || !cap.boundingRect().united(base.boundingRect()).intersects(viewport))
            continue;

        // Walls facing the viewer, as strips of consecutive edges with the same shade.
        struct Strip {
            QPolygonF shape;
            int shade;
            float depth;
        };
        QVector<Strip> strips;
        int open = -1;      // index of the strip the previous edge was added to
        int stripLen = 0;
        QPolygonF stripTop;
        float stripDepth = 0;
        auto closeStrip = [&] {
            if (open < 0)
                return;
            for (int k = stripTop.size() - 1; k >= 0; --k)
                strips[open].shape.push_back(stripTop[k]);
            strips[open].depth = stripDepth / stripLen;
            open = -1;
        };
        for (int a = 0; a < n; ++a) {
            const int b = (a + 1) % n;
            const QVector3D normal = QVector3D::crossProduct(pts[b] - pts[a], pts[a] + pts[b]) * prism.ring->winding;
            const float len = normal.length();
            if ((hidden[a] && hidden[b]) || len < 1e-9f || QVector3D::dotProduct(normal, view) <= 0) {
                closeStrip();
                continue;
            }
            const float lit = QVector3D::dotProduct(normal / len, light);
            const int shade = std::clamp(int((lit * 0.5f + 0.5f) * ShadeLevels), 0, ShadeLevels - 1);
            if (open >= 0 && strips[open].shade != shade)
                closeStrip();
            if (open < 0) {
                strips.push_back({QPolygonF() << base[a], shade, 0});
                open = strips.size() - 1;
                stripTop = QPolygonF() << cap[a];
                stripLen = 0;
                stripDepth = 0;
            }
            strips[open].shape.push_back(base[b]);
            stripTop.push_back(cap[b]);
            stripDepth += QVector3D::dotProduct(pts[a] + pts[b], view);
            ++stripLen;
        }
        closeStrip();
        std::sort(strips.begin(), strips.end(), [](const Strip &x, const Strip &y) { return x.depth < y.depth; });

        const QColor top = m_heatmap.colors.value(*prism.iso, Theme::current().Land);
        for (const Strip &strip : std::as_const(strips)) {
            const QColor wall = top.darker(112 + 60 * (ShadeLevels - 1 - strip.shade) / (ShadeLevels - 1));
            m_scene.prismFaces.push_back({makeShape(strip.shape), wall, false});
            m_prismHits.push_back({*prism.iso, strip.shape, strip.shape.boundingRect()});
        }
        m_scene.prismFaces.push_back({makeShape(cap), top, true});
        m_prismHits.push_back({*prism.iso, cap, cap.boundingRect()});
    }
}

void GlobeWidget::drawPrisms(QPainter &p, const QRectF &band) const
{
    QPen capPen(Theme::current().LandBorder, 1.0);
    capPen.setCosmetic(true);
    for (const BaseScene::Face &face : m_scene.prismFaces) {
        if (!face.shape.crosses(band, 1))
            continue;
        if (face.cap) {
            p.setPen(capPen);
        } else {
            // A hairline of the fill color hides antialiasing seams between wall strips.
            QPen seam(face.color, 0.8);
            seam.setCosmetic(true);
            p.setPen(seam);
        }
        p.setBrush(face.color);
        p.drawPolygon(face.shape.points, Qt::WindingFill);
    }
}

void GlobeWidget::layoutFlows()
{
    m_flowPaths.clear();
    if (!m_flowMode)
        return;
    const QVector<FlowArc> &arcs = m_flowMap.arcs;
    m_flowPaths.resize(arcs.size());
    double maxValue = 0;
    for (const FlowArc &arc : arcs)
        maxValue = std::max(maxValue, arc.value);

    const QPointF c = globeCenter();
    const double r = radius();
    constexpr int Samples = 64;
    for (int k = 0; k < arcs.size(); ++k) {
        const FlowArc &arc = arcs[k];
        const GeoRing *ra = mainRing(m_countryIndex.value(arc.from, -1));
        const GeoRing *rb = mainRing(m_countryIndex.value(arc.to, -1));
        if (!ra || !rb || maxValue <= 0)
            continue;
        const QVector3D a = ra->center, b = rb->center;
        const double angle = std::acos(std::clamp(double(QVector3D::dotProduct(a, b)), -1.0, 1.0));
        if (angle < 1e-3)
            continue;
        // Great circle, lifted more for longer routes, from the top of the prisms when they show.
        const double lift = 0.025 + 0.12 * angle / M_PI;
        const double ha = prismHeight(arc.from), hb = prismHeight(arc.to);
        const double sinAngle = std::sin(angle);
        FlowPath &path = m_flowPaths[k];
        path.points.resize(Samples + 1);
        path.visible.resize(Samples + 1);
        path.width = 1.2 + 6.0 * std::sqrt(arc.value / maxValue);
        QPolygonF run;
        for (int i = 0; i <= Samples; ++i) {
            const double t = double(i) / Samples;
            const QVector3D v = (a * float(std::sin((1 - t) * angle)) + b * float(std::sin(t * angle))) / float(sinAngle);
            const Projected pr = project(v * float(1 + ha + (hb - ha) * t + lift * std::sin(M_PI * t)));
            path.points[i] = pr.pos;
            // Hidden only behind the globe: on the far side and inside the disc.
            path.visible[i] = pr.depth > 0 || QLineF(c, pr.pos).length() > r;
            if (path.visible[i]) {
                run << pr.pos;
            } else {
                addLine(path.lines, run);
                run.clear();
            }
        }
        addLine(path.lines, run);
    }
}

void GlobeWidget::drawFlowTrails(QPainter &p, const QRectF &band) const
{
    // Smallest first, so that the big flows stay on top. Bevel joins: the arcs are finely
    // sampled, and round joins cost much more to stroke.
    p.setBrush(Qt::NoBrush);
    for (int k = int(m_flowPaths.size()) - 1; k >= 0; --k) {
        const FlowPath &path = m_flowPaths[k];
        QColor trail = m_flowMap.arcs[k].color;
        trail.setAlpha(100);
        p.setPen(QPen(trail, path.width, Qt::SolidLine, Qt::RoundCap, Qt::BevelJoin));
        for (const Shape &line : path.lines)
            if (line.crosses(band, path.width))
                p.drawPolyline(line.points);
    }
}

void GlobeWidget::drawFlows(QPainter &p)
{
    if (!m_flowMode)
        return;
    const Palette &pal = Theme::current();
    // Particles travel from the exporter to the importer, over the trails of the base image.
    const double seconds = m_clock.elapsed() / 1000.0;
    p.setPen(Qt::NoPen);
    for (int k = int(m_flowPaths.size()) - 1; k >= 0; --k) {
        const FlowPath &path = m_flowPaths[k];
        const int segments = int(path.points.size()) - 1;
        if (segments < 1)
            continue;
        p.setBrush(m_flowMap.arcs[k].color);
        const double dot = path.width * 0.55 + 1.3;
        for (int j = 0; j < 3; ++j) {
            const double t = std::fmod(seconds / FlowTripSeconds + j / 3.0 + k * 0.137, 1.0);
            const double x = t * segments;
            const int i = std::min(int(x), segments - 1);
            if (!path.visible[i] || !path.visible[i + 1])
                continue;
            p.drawEllipse(path.points[i] + (path.points[i + 1] - path.points[i]) * (x - i), dot, dot);
        }
    }

    // Legend, top left.
    if (m_flowMap.title.isEmpty())
        return;
    QFont f = font();
    f.setPointSizeF(11);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(pal.Text);
    p.drawText(QRectF(18, 14, width() - 36, 20), Qt::AlignLeft | Qt::AlignVCenter, m_flowMap.title);
    f.setWeight(QFont::Normal);
    f.setPointSizeF(10.5);
    p.setFont(f);
    double y = 38;
    for (const auto &[color, label] : std::as_const(m_flowMap.keys)) {
        p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(20, y + 8), QPointF(38, y + 8));
        p.setPen(pal.TextDim);
        p.drawText(QRectF(46, y, width() - 64, 16), Qt::AlignLeft | Qt::AlignVCenter, label);
        y += 20;
    }
}

int GlobeWidget::flowArcAt(const QPointF &pos) const
{
    // Largest first: drawn last, so on top.
    for (int k = 0; k < m_flowPaths.size(); ++k) {
        const FlowPath &path = m_flowPaths[k];
        for (const Shape &shape : path.lines) {
            const QPolygonF &line = shape.points;
            for (int i = 0; i + 1 < line.size(); ++i) {
                const QPointF a = line[i], d = line[i + 1] - a;
                const double len2 = QPointF::dotProduct(d, d);
                const double t = len2 > 0 ? std::clamp(QPointF::dotProduct(pos - a, d) / len2, 0.0, 1.0) : 0.0;
                if (QLineF(pos, a + d * t).length() <= std::max(4.0, path.width / 2 + 2))
                    return k;
            }
        }
    }
    return -1;
}

void GlobeWidget::buildClusters()
{
    m_clusters.clear();
    const auto &sites = m_data->sites();
    for (int i = 0; i < sites.size(); ++i) {
        if (!m_data->isCategoryVisible(sites[i].categoryId))
            continue;
        const Projected pr = project(m_siteVectors[i]);
        if (pr.depth < 0.05f)
            continue;

        int best = -1;
        double bestDist = ClusterDistance;
        for (int k = 0; k < m_clusters.size(); ++k) {
            const double dist = QLineF(m_clusters[k].center, pr.pos).length();
            if (dist < bestDist) { bestDist = dist; best = k; }
        }
        if (best < 0) {
            m_clusters.push_back({pr.pos, 0, pr.depth, {i}});
        } else {
            Cluster &cl = m_clusters[best];
            const int n = cl.sites.size();
            cl.center = (cl.center * n + pr.pos) / (n + 1);
            cl.depth = std::max(cl.depth, pr.depth);
            cl.sites.push_back(i);
        }
    }
    auto radiusFor = [](int n) { return n == 1 ? DotRadius : 11.0 + 3.0 * std::log2(double(n)); };
    for (Cluster &cl : m_clusters)
        cl.radius = radiusFor(cl.sites.size());

    // Greedy assignment can leave neighbouring clusters overlapping; merge those.
    for (bool merged = true; merged;) {
        merged = false;
        for (int a = 0; a < m_clusters.size() && !merged; ++a) {
            for (int b = a + 1; b < m_clusters.size(); ++b) {
                Cluster &ca = m_clusters[a];
                const Cluster &cb = m_clusters[b];
                if (QLineF(ca.center, cb.center).length() >= (ca.radius + cb.radius) * 0.85)
                    continue;
                const int na = ca.sites.size(), nb = cb.sites.size();
                ca.center = (ca.center * na + cb.center * nb) / (na + nb);
                ca.depth = std::max(ca.depth, cb.depth);
                ca.sites += cb.sites;
                ca.radius = radiusFor(ca.sites.size());
                m_clusters.removeAt(b);
                merged = true;
                break;
            }
        }
    }
}

void GlobeWidget::drawMarkers(QPainter &p)
{
    // Draw singles first so clusters sit on top.
    for (int pass = 0; pass < 2; ++pass) {
        for (int k = 0; k < m_clusters.size(); ++k) {
            const Cluster &cl = m_clusters[k];
            if ((cl.sites.size() > 1) != (pass == 1))
                continue;
            bool selected = false;
            for (int s : cl.sites)
                if (m_selection.contains(s)) { selected = true; break; }
            drawCluster(p, cl, k == m_hovered, selected);
        }
    }
}

void GlobeWidget::drawCluster(QPainter &p, const Cluster &cl, bool hovered, bool selected)
{
    // Fade markers as they approach the horizon.
    const double opacity = std::clamp((cl.depth - 0.05) / 0.25, 0.15, 1.0);
    p.setOpacity(opacity);
    const double scale = hovered ? 1.25 : 1.0;
    const double r = cl.radius * scale;

    if (selected) {
        const double t = std::fmod(m_clock.elapsed() / 1400.0, 1.0);
        QColor ring = Theme::current().Text;
        ring.setAlphaF(float(0.8 * (1.0 - t)));
        p.setPen(QPen(ring, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(cl.center, r + 4 + t * 14, r + 4 + t * 14);
    }

    if (cl.sites.size() == 1) {
        const Site &s = m_data->sites()[cl.sites.first()];
        QColor outline = Theme::current().ClusterFill;
        outline.setAlpha(200);
        p.setPen(QPen(outline, 1.2));
        p.setBrush(m_data->categoryColor(s.categoryId));
        p.drawEllipse(cl.center, r, r);
        p.setOpacity(1.0);
        return;
    }

    // Count sites per category, in category order, for the donut ring.
    QVector<QPair<QColor, int>> parts;
    for (const Category &cat : m_data->categories()) {
        int n = 0;
        for (int s : cl.sites)
            if (m_data->sites()[s].categoryId == cat.id) ++n;
        if (n) parts.push_back({cat.color, n});
    }

    p.setPen(Qt::NoPen);
    QColor fill = Theme::current().ClusterFill;
    fill.setAlpha(235);
    p.setBrush(fill);
    p.drawEllipse(cl.center, r, r);

    const double ringW = 2.6;
    const QRectF arcRect(cl.center.x() - r + ringW / 2, cl.center.y() - r + ringW / 2,
                         2 * r - ringW, 2 * r - ringW);
    const int total = cl.sites.size();
    const double gap = parts.size() > 1 ? 10.0 : 0.0;   // in 1/16 degree units
    double start = 90 * 16;
    for (const auto &[color, n] : parts) {
        const double span = 360.0 * 16 * n / total;
        p.setPen(QPen(color, ringW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arcRect, int(start - gap / 2), -int(span - gap));
        start -= span;
    }

    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(total >= 10 ? 11.5 : 12.5);
    p.setFont(f);
    p.setPen(Theme::current().Text);
    p.drawText(QRectF(cl.center.x() - r, cl.center.y() - r, 2 * r, 2 * r), Qt::AlignCenter,
               QString::number(total));
    p.setOpacity(1.0);
}

namespace {
constexpr int ShipHeadings = 128;   // sprite headings, 2.8° apart
}

GlobeWidget::ShipSprites &GlobeWidget::shipSprites(double size)
{
    const qreal dpr = devicePixelRatioF();
    QColor outline = Theme::current().ClusterFill;
    outline.setAlpha(170);
    QColor faint = Theme::current().TextFaint;
    faint.setAlpha(140);
    const QRgb tanker = m_data->categoryColor("energy").rgba(), cargo = m_data->categoryColor("industrial").rgba();
    for (int k = 0; k < m_shipSprites.size(); ++k) {
        const ShipSprites &set = m_shipSprites[k];
        if (set.size == size && set.dpr == dpr && set.tanker == tanker && set.cargo == cargo
            && set.outline == outline.rgba() && set.unknown == faint.rgba()) {
            if (k > 0)
                m_shipSprites.move(k, 0);
            return m_shipSprites[0];
        }
    }
    ShipSprites set;
    set.size = size;
    set.dpr = dpr;
    set.tanker = tanker;
    set.cargo = cargo;
    set.outline = outline.rgba();
    set.unknown = faint.rgba();
    // On a standard screen a pixel is coarse: half-pixel phases keep the ships where they belong.
    set.phases = dpr < 1.5 ? 2 : 1;
    set.margin = int(std::ceil((size * 1.5 + 2) * dpr)) + 1;
    set.images.resize(ShipSpriteKinds * ShipHeadings * set.phases * set.phases);
    m_shipSprites.prepend(set);
    if (m_shipSprites.size() > 3)
        m_shipSprites.removeLast();
    return m_shipSprites[0];
}

const QImage &GlobeWidget::shipSprite(ShipSprites &set, int kind, int heading, int phaseX, int phaseY) const
{
    QImage &image = set.images[((kind * ShipHeadings + heading) * set.phases + phaseY) * set.phases + phaseX];
    if (!image.isNull())
        return image;
    const int side = 2 * set.margin + 2;
    image = QImage(side, side, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(set.dpr);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF at((set.margin + double(phaseX) / set.phases) / set.dpr,
                     (set.margin + double(phaseY) / set.phases) / set.dpr);
    const double size = set.size;
    if (kind == UnknownDot) {
        // Type not received yet: a faint dot.
        p.setPen(QPen(QColor::fromRgba(set.unknown), std::max(2.0, size * 0.55), Qt::SolidLine, Qt::RoundCap));
        p.drawPoint(at);
        return image;
    }
    QPainterPath ship;
    if (kind == TankerDot || kind == CargoDot) {
        // Moored or anchored: a small dot.
        ship.addEllipse(at, size * 0.45, size * 0.45);
    } else {
        // A triangle pointing along the heading.
        const double a = 2 * M_PI * heading / ShipHeadings;
        const QPointF d(std::cos(a), std::sin(a)), n(-d.y(), d.x());
        ship.moveTo(at + d * size * 1.5);
        ship.lineTo(at - d * size * 0.9 + n * size * 0.75);
        ship.lineTo(at - d * size * 0.9 - n * size * 0.75);
        ship.closeSubpath();
    }
    p.setOpacity(0.95);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor::fromRgba(kind == TankerShip || kind == TankerDot ? set.tanker : set.cargo));
    p.drawPath(ship);
    // A thin dark outline keeps markers readable over land and dense traffic.
    QPen outlinePen(QColor::fromRgba(set.outline), 0.8);
    outlinePen.setCosmetic(true);
    p.setPen(outlinePen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(ship);
    return image;
}

void GlobeWidget::drawVessels(QPainter &p)
{
    m_vesselHits.clear();
    const QRectF bounds = QRectF(rect()).adjusted(-10, -10, 10, 10);
    const double size = std::round(std::clamp(3.0 + m_zoom * 0.5, 3.6, 7.0) * 4) / 4;
    const bool showTrails = m_zoom >= 2.5;
    ShipSprites &sprites = shipSprites(size);

    struct Placed {
        int kind, heading;
        QPointF pos;
    };
    QVector<Placed> unknown, cargo, tankers;
    QPainterPath tankerTrails, cargoTrails;
    QPointF selectedPos;
    bool selectedVisible = false;

    for (const Vessel &v : m_vessels->vessels()) {
        const Vessel::Class cls = v.vesselClass();
        if (cls == Vessel::Other || !m_vessels->isClassVisible(cls))
            continue;
        const Projected pr = project(v.pos);
        if (pr.depth < 0.02f || !bounds.contains(pr.pos))
            continue;
        m_vesselHits.push_back({pr.pos, v.mmsi});
        if (v.mmsi == m_selectedVessel) {
            selectedPos = pr.pos;
            selectedVisible = true;
        }
        if (cls == Vessel::Unknown) {
            unknown.push_back({UnknownDot, 0, pr.pos});   // drawn under everything else
            continue;
        }
        if (showTrails && v.trail.size() > 1) {
            QPainterPath &trail = cls == Vessel::Tanker ? tankerTrails : cargoTrails;
            bool first = true;
            for (const QVector3D &t : v.trail) {
                const Projected tp = project(t);
                if (tp.depth <= 0) { first = true; continue; }
                if (first) trail.moveTo(tp.pos); else trail.lineTo(tp.pos);
                first = false;
            }
            if (!first)
                trail.lineTo(pr.pos);
        }

        const bool tanker = cls == Vessel::Tanker;
        int kind = tanker ? TankerDot : CargoDot, heading = 0;
        if (v.sog >= 0.5) {
            // The bearing on screen: project the local east/north tangent directions.
            const double lat = qDegreesToRadians(v.lat), lon = qDegreesToRadians(v.lon);
            const double b = qDegreesToRadians(v.bearing());
            const QVector3D east(float(-qSin(lon)), float(qCos(lon)), 0.f);
            const QVector3D north(float(-qSin(lat) * qCos(lon)), float(-qSin(lat) * qSin(lon)), float(qCos(lat)));
            const QVector3D dir = east * float(qSin(b)) + north * float(qCos(b));
            const double dx = m_rot[3] * dir.x() + m_rot[4] * dir.y() + m_rot[5] * dir.z();
            const double dy = -(m_rot[6] * dir.x() + m_rot[7] * dir.y() + m_rot[8] * dir.z());
            if (std::hypot(dx, dy) >= 1e-3) {
                kind = tanker ? TankerShip : CargoShip;
                heading = int(std::lround(std::atan2(dy, dx) / (2 * M_PI) * ShipHeadings)) % ShipHeadings;
                if (heading < 0)
                    heading += ShipHeadings;
            }
        }
        (tanker ? tankers : cargo).push_back({kind, heading, pr.pos});
    }

    // Each sprite goes on whole device pixels, its anchor at the nearest sub-pixel phase.
    const qreal dpr = sprites.dpr;
    const int phases = sprites.phases;
    auto draw = [&](const QVector<Placed> &ships) {
        for (const Placed &s : ships) {
            const qint64 qx = std::llround(s.pos.x() * dpr * phases), qy = std::llround(s.pos.y() * dpr * phases);
            const qint64 ix = qx >= 0 ? qx / phases : -((-qx + phases - 1) / phases);
            const qint64 iy = qy >= 0 ? qy / phases : -((-qy + phases - 1) / phases);
            const QImage &image = shipSprite(sprites, s.kind, s.heading, int(qx - ix * phases), int(qy - iy * phases));
            p.drawImage(QPointF((ix - sprites.margin) / dpr, (iy - sprites.margin) / dpr), image);
        }
    };
    draw(unknown);
    p.setBrush(Qt::NoBrush);
    for (const auto &[trail, color] : {std::pair(&tankerTrails, QColor::fromRgba(sprites.tanker)),
                                       std::pair(&cargoTrails, QColor::fromRgba(sprites.cargo))}) {
        QColor c = color;
        c.setAlpha(110);
        QPen pen(c, 1.0);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.drawPath(*trail);
    }
    draw(cargo);
    draw(tankers);

    if (selectedVisible) {
        const double t = std::fmod(m_clock.elapsed() / 1400.0, 1.0);
        QColor ring = Theme::current().Text;
        ring.setAlphaF(float(0.85 * (1.0 - t)));
        p.setPen(QPen(ring, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(selectedPos, 8 + t * 14, 8 + t * 14);
        p.setPen(QPen(Theme::current().Text, 1.5));
        p.drawEllipse(selectedPos, 7, 7);
    }
}

void GlobeWidget::drawChokepoints(QPainter &p)
{
    m_zoneHits.clear();
    const Palette &pal = Theme::current();
    const auto &zones = m_chokepoints->chokepoints();
    QFont f = font();
    f.setPointSizeF(10.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);

    for (int i = 0; i < zones.size(); ++i) {
        const Chokepoint &z = zones[i];
        // Ring of points at the zone's angular radius around its center.
        const QVector3D c = z.center;
        QVector3D e1 = QVector3D::crossProduct(c, QVector3D(0, 0, 1));
        if (e1.lengthSquared() < 1e-6f)
            e1 = QVector3D(1, 0, 0);
        e1.normalize();
        const QVector3D e2 = QVector3D::crossProduct(c, e1);
        const float sinR = std::sqrt(1 - z.cosRadius * z.cosRadius);
        QPolygonF ring;
        bool visible = true;
        for (int k = 0; k < 48; ++k) {
            const double t = 2 * M_PI * k / 48;
            const QVector3D v = c * z.cosRadius + (e1 * float(qCos(t)) + e2 * float(qSin(t))) * sinR;
            const Projected pr = project(v);
            if (pr.depth <= 0) { visible = false; break; }
            ring << pr.pos;
        }
        const Projected center = project(c);
        if (!visible || center.depth < 0.05f) {
            m_zoneHits.push_back({QPointF(), -1});
            continue;
        }
        double radiusPx = 0;
        for (const QPointF &pt : std::as_const(ring))
            radiusPx = std::max(radiusPx, QLineF(center.pos, pt).length());
        m_zoneHits.push_back({center.pos, std::max(radiusPx, 8.0)});

        const bool selected = i == m_selectedZone;
        QColor fill = pal.Accent;
        fill.setAlpha(selected ? 40 : 16);
        QColor stroke = pal.Accent;
        stroke.setAlpha(selected ? 230 : 120);
        QPen pen(stroke, selected ? 1.6 : 1.0, selected ? Qt::SolidLine : Qt::DashLine);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(fill);
        p.drawPolygon(ring);

        // Label beside the zone, with the number of carriers inside it.
        const int now = m_chokepoints->inZoneNow(i).total();
        const QString label = now > 0 ? QString("%1 · %2").arg(m_chokepoints->name(i)).arg(now)
                                      : m_chokepoints->name(i);
        const QPointF anchor = center.pos + QPointF(radiusPx + 6, -radiusPx * 0.4);
        QColor text = selected ? pal.Text : pal.Accent;
        text.setAlphaF(selected ? 1.0f : 0.85f);
        p.setPen(text);
        p.drawText(QRectF(anchor.x(), anchor.y() - 9, 260, 18), Qt::AlignLeft | Qt::AlignVCenter, label);
    }
}

void GlobeWidget::drawRoute(QPainter &p)
{
    if (m_route.size() < 2)
        return;
    const Palette &pal = Theme::current();
    QPainterPath path;
    bool penDown = false;
    QPointF last;
    for (const QVector3D &v : std::as_const(m_route)) {
        const Projected pr = project(v);
        if (pr.depth <= 0) { penDown = false; continue; }
        if (penDown) path.lineTo(pr.pos); else path.moveTo(pr.pos);
        penDown = true;
        last = pr.pos;
    }
    QPen pen(pal.Accent, 1.8, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setDashPattern({4, 3});
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Destination marker and label.
    const Projected dest = project(m_route.last());
    if (dest.depth > 0) {
        p.setPen(QPen(pal.Accent, 2));
        p.setBrush(pal.ClusterFill);
        p.drawEllipse(dest.pos, 5.5, 5.5);
        QFont f = font();
        f.setPointSizeF(11);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(pal.Text);
        p.drawText(QRectF(dest.pos.x() + 10, dest.pos.y() - 10, 300, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   m_routeLabel);
    }
}

int GlobeWidget::chokepointAt(const QPointF &pos) const
{
    for (int i = 0; i < m_zoneHits.size(); ++i) {
        const auto &[center, radius] = m_zoneHits[i];
        if (radius > 0 && QLineF(center, pos).length() <= radius)
            return i;
    }
    return -1;
}

qint64 GlobeWidget::vesselAt(const QPointF &pos) const
{
    qint64 best = 0;
    double bestDist = 7.0;
    for (const auto &[pt, mmsi] : m_vesselHits) {
        const double d = QLineF(pt, pos).length();
        if (d < bestDist) {
            bestDist = d;
            best = mmsi;
        }
    }
    return best;
}

// ---------------------------------------------------------------- interaction

int GlobeWidget::clusterAt(const QPointF &pos) const
{
    int best = -1;
    double bestDist = 1e9;
    for (int k = 0; k < m_clusters.size(); ++k) {
        const double d = QLineF(m_clusters[k].center, pos).length();
        if (d <= std::max(m_clusters[k].radius, 8.0) + 2 && d < bestDist) {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

void GlobeWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_dragging = false;
    m_pressPos = m_lastPos = e->position();
    m_velocity = {};
    m_viewAnim.stop();
    m_lastMoveTime.start();
}

void GlobeWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    if (m_pressed) {
        if (!m_dragging && (pos - m_pressPos).manhattanLength() > 4) {
            m_dragging = true;
            setCursor(Qt::ClosedHandCursor);
            QToolTip::hideText();
        }
        if (m_dragging) {
            const double degPerPx = qRadiansToDegrees(1.0 / radius());
            // North-south distances are foreshortened by the tilt.
            const QPointF delta((pos.x() - m_lastPos.x()) * degPerPx, (pos.y() - m_lastPos.y()) * degPerPx / m_tiltCos);
            m_lon = wrapLon(m_lon - delta.x());
            m_lat = std::clamp(m_lat + delta.y(), -89.0, 89.0);
            m_velocity = delta;
            m_lastPos = pos;
            m_lastMoveTime.restart();
            updateRotation();
        }
        return;
    }

    const int hovered = clusterAt(pos);
    const qint64 hoveredVessel = hovered < 0 ? vesselAt(pos) : 0;
    if (hovered != m_hovered) {
        m_hovered = hovered;
        update();
    }
    setCursor(hovered >= 0 || hoveredVessel ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (const Vessel *v = hoveredVessel ? m_vessels->vessel(hoveredVessel) : nullptr) {
        const QString kind = v->vesselClass() == Vessel::Tanker ? tr("Tanker")
                             : v->vesselClass() == Vessel::Cargo ? tr("Cargo ship")
                                                                 : tr("Unidentified ship");
        QString text = QString("<b>%1</b> &nbsp;<span style='color:%2'>%3</span><br>%4 kn")
                           .arg((v->name.isEmpty() ? QString::number(v->mmsi) : v->name).toHtmlEscaped(),
                                Theme::current().TextDim.name(), kind,
                                QLocale().toString(v->sog, 'f', 1));
        if (!v->destination.isEmpty())
            text += QString::fromUtf8(" \u2192 ") + v->destination.toHtmlEscaped();
        QToolTip::showText(e->globalPosition().toPoint(), text, this);
        return;
    }
    if (const int point = hovered < 0 ? pointAt(pos) : -1; point >= 0) {
        setCursor(Qt::PointingHandCursor);
        QToolTip::showText(e->globalPosition().toPoint(), m_points[point].tip, this);
        return;
    }
    if (hovered >= 0) {
        const Cluster &cl = m_clusters[hovered];
        QStringList lines;
        for (int i = 0; i < cl.sites.size() && i < 8; ++i) {
            const Site &s = m_data->sites()[cl.sites[i]];
            const Commodity *c = m_data->commodity(s.commodityId);
            lines << QString("<b>%1</b> &nbsp;<span style='color:%2'>%3</span>")
                         .arg(s.name.toHtmlEscaped(), Theme::current().TextDim.name(),
                              c ? c->name.toHtmlEscaped() : QString());
        }
        if (cl.sites.size() > 8)
            lines << tr("… and %1 more").arg(cl.sites.size() - 8);
        QToolTip::showText(e->globalPosition().toPoint(), lines.join("<br>"), this);
    } else if (const int arc = flowArcAt(pos); arc >= 0) {
        QToolTip::showText(e->globalPosition().toPoint(), m_flowMap.arcs[arc].label, this);
    } else if (!m_heatmap.colors.isEmpty()) {
        const QString text = m_heatmap.labels.value(countryAt(pos));
        if (text.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(e->globalPosition().toPoint(), text, this);
    } else {
        QToolTip::hideText();
    }
}

void GlobeWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (m_dragging) {
        // Only keep inertia if the pointer was still moving at release.
        if (m_lastMoveTime.elapsed() > 60)
            m_velocity = {};
        setCursor(Qt::ArrowCursor);
    } else {
        const int k = clusterAt(e->position());
        const qint64 vessel = k < 0 ? vesselAt(e->position()) : 0;
        const int point = k < 0 && !vessel ? pointAt(e->position()) : -1;
        if (point < 0)
            m_selectedPoint.clear();
        const int zone = k < 0 && !vessel && point < 0 ? chokepointAt(e->position()) : -1;
        if (point >= 0) {
            setSelection({});
            setSelectedVessel(0);
            setSelectedChokepoint(-1);
            setSelectedPoint(m_points[point].key);
            emit pointSelected(m_points[point].key);
        } else if (vessel) {
            setSelection({});
            setSelectedChokepoint(-1);
            setSelectedVessel(vessel);
            emit vesselSelected(vessel);
        } else if (zone >= 0) {
            setSelection({});
            setSelectedVessel(0);
            setSelectedChokepoint(zone);
            emit chokepointSelected(zone);
        } else if (const QString iso = k < 0 ? countryAt(e->position()) : QString(); !iso.isEmpty()) {
            setSelection({});
            setSelectedVessel(0);
            setSelectedChokepoint(-1);
            emit countrySelected(iso);
        } else {
            setSelectedChokepoint(-1);
            const QVector<int> sel = k >= 0 ? m_clusters[k].sites : QVector<int>();
            setSelectedVessel(0);
            setSelection(sel);
            emit selectionChanged(sel);
        }
    }
    m_pressed = false;
    m_dragging = false;
    wake();   // inertia
}

void GlobeWidget::wheelEvent(QWheelEvent *e)
{
    const double steps = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->pixelDelta().y() * 2.0;
    m_viewAnim.stop();
    m_zoom = std::clamp(m_zoom * qPow(1.0015, steps), MinZoom, MaxZoom);
    updateRotation();
    e->accept();
}

void GlobeWidget::leaveEvent(QEvent *)
{
    if (m_hovered >= 0) {
        m_hovered = -1;
        update();
    }
}

void GlobeWidget::resizeEvent(QResizeEvent *)
{
    m_baseDirty = true;
    layoutControls();
}

void GlobeWidget::layoutControls()
{
    const int margin = 16, spacing = 10, size = 40;
    int y = height() - margin - size;
    const int x = width() - margin - size;
    for (QToolButton *b : {m_resetButton, m_zoomOutButton, m_zoomInButton, m_spinButton, m_prismButton,
                           m_flowButton, m_pointsButton}) {
        if (b->isHidden())
            continue;
        b->move(x, y);
        y -= size + spacing;
    }
}
