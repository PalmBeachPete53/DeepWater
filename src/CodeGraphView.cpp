#include "CodeGraphView.h"

#include "Theme.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QSet>
#include <QScrollArea>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
#include <QtMath>

#include <algorithm>

// ---------------------------------------------------------------- codegraph

namespace codegraph {

QString locate()
{
    QStringList starts{qEnvironmentVariable("DEEPWATER_GRAPH_ROOT"),
                       QSettings().value("devtools/graphRoot").toString(),
                       QCoreApplication::applicationDirPath(), QDir::currentPath()};
    for (const QString &start : starts) {
        if (start.isEmpty())
            continue;
        QDir dir(start);
        for (int i = 0; i < 8; ++i) {
            if (dir.exists("graphify-out/graph.json"))
                return dir.filePath("graphify-out/graph.json");
            if (!dir.cdUp())
                break;
        }
    }
    return {};
}

namespace {

const char *const kTableau10[] = {"#4E79A7", "#F28E2B", "#E15759", "#76B7B2", "#59A14F",
                                  "#EDC948", "#B07AA1", "#FF9DA7", "#9C755F", "#BAB0AC"};

// `const RAW_NODES = [...];` on one line of graph.html.
QJsonArray htmlArray(const QByteArray &html, const char *name)
{
    const QByteArray marker = QByteArray("const ") + name + " = ";
    const int start = html.indexOf(marker);
    if (start < 0)
        return {};
    const int from = start + marker.size();
    const int end = html.indexOf("];\n", from);
    if (end < 0)
        return {};
    return QJsonDocument::fromJson(html.mid(from, end - from + 1)).array();
}

} // namespace

Graph load(const QString &path)
{
    Graph g;
    g.path = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        g.error = QObject::tr("Cannot read %1").arg(path);
        return g;
    }
    g.modified = QFileInfo(f).lastModified();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        g.error = QObject::tr("Invalid graph.json: %1").arg(err.errorString());
        return g;
    }
    const QJsonObject root = doc.object();
    const QJsonArray nodes = root.value("nodes").toArray();
    QHash<QString, int> index;
    g.nodes.reserve(nodes.size());
    for (const QJsonValue &v : nodes) {
        const QJsonObject o = v.toObject();
        Node n;
        n.id = o.value("id").toString();
        n.label = o.value("label").toString(n.id);
        n.file = o.value("source_file").toString();
        n.location = o.value("source_location").toString();
        n.type = o.value("file_type").toString();
        n.community = o.value("community").toInt(-1);
        if (n.community >= 0) {
            if (g.communityNames.size() <= n.community)
                g.communityNames.resize(n.community + 1);
            if (g.communityNames[n.community].isEmpty())
                g.communityNames[n.community] = o.value("community_name").toString();
        }
        index.insert(n.id, g.nodes.size());
        g.nodes.append(n);
    }
    const QJsonArray links = root.value(root.contains("links") ? "links" : "edges").toArray();
    g.edges.reserve(links.size());
    for (const QJsonValue &v : links) {
        const QJsonObject o = v.toObject();
        const int a = index.value(o.value("source").toString(), -1);
        const int b = index.value(o.value("target").toString(), -1);
        if (a >= 0 && b >= 0 && a != b)
            g.edges.append({a, b});
    }
    for (int i = 0; i < g.communityNames.size(); ++i)
        if (g.communityNames[i].isEmpty())
            g.communityNames[i] = QObject::tr("Community %1").arg(i);

    // graph.html's aggregated view: its dots and links as they are.
    QFile html(QFileInfo(path).dir().filePath("graph.html"));
    QHash<int, int> byCid;
    if (html.open(QIODevice::ReadOnly)) {
        const QByteArray text = html.readAll();
        QHash<QString, int> byId;
        for (const QJsonValue &v : htmlArray(text, "RAW_NODES")) {
            const QJsonObject o = v.toObject();
            Community c;
            c.cid = o.value("community").toInt(-1);
            c.label = o.value("label").toString();
            c.color = QColor(o.value("color").toObject().value("background").toString());
            c.size = o.value("size").toDouble(10);
            c.degree = o.value("degree").toInt();
            c.members = o.value("member_count").toInt();
            byId.insert(o.value("id").toString(), g.communities.size());
            byCid.insert(c.cid, g.communities.size());
            g.communities.append(c);
        }
        for (const QJsonValue &v : htmlArray(text, "RAW_EDGES")) {
            const QJsonObject o = v.toObject();
            const int a = byId.value(o.value("from").toString(), -1);
            const int b = byId.value(o.value("to").toString(), -1);
            if (a < 0 || b < 0 || a == b)
                continue;
            CommunityLink l{a, b, 1};
            const QString label = o.value("label").toString();   // "20 cross-community edges"
            l.count = qMax(1, label.section(' ', 0, 0).toInt());
            g.communityLinks.append(l);
        }
    }
    if (g.communities.isEmpty()) {
        // No graph.html: rebuild its view with the same rules (size 10..40 by members, Tableau 10).
        QVector<int> members(g.communityNames.size(), 0);
        for (const Node &n : g.nodes)
            if (n.community >= 0)
                ++members[n.community];
        int maxMembers = 1;
        for (int m : members)
            maxMembers = qMax(maxMembers, m);
        for (int cid = 0; cid < members.size(); ++cid) {
            if (members[cid] == 0)
                continue;
            Community c;
            c.cid = cid;
            c.label = g.communityNames[cid];
            c.color = QColor(kTableau10[cid % 10]);
            c.members = members[cid];
            c.size = 10 + 30.0 * members[cid] / maxMembers;
            byCid.insert(cid, g.communities.size());
            g.communities.append(c);
        }
        QHash<qint64, int> linkIndex;
        for (const Edge &e : g.edges) {
            const int a = byCid.value(g.nodes[e.a].community, -1), b = byCid.value(g.nodes[e.b].community, -1);
            if (a < 0 || b < 0 || a == b)
                continue;
            const qint64 key = qint64(a) * g.communities.size() + b;
            auto it = linkIndex.find(key);
            if (it == linkIndex.end()) {
                linkIndex.insert(key, g.communityLinks.size());
                g.communityLinks.append({a, b, 1});
            } else {
                ++g.communityLinks[it.value()].count;
            }
        }
        for (const CommunityLink &l : g.communityLinks) {
            ++g.communities[l.a].degree;
            ++g.communities[l.b].degree;
        }
    }
    return g;
}

} // namespace codegraph

// ---------------------------------------------------------------- GraphCanvas

namespace {

constexpr double kCamera = 3.6;    // camera distance, in units of the layout's radius
constexpr double kFlatten = 0.4;   // the disk's thickness relative to its radius

// vis.js forceAtlas2Based options of graph.html.
constexpr double kGravitationalConstant = -60;
constexpr double kCentralGravity = 0.005;
constexpr double kSpringLength = 120;
constexpr double kSpringConstant = 0.08;
constexpr double kDamping = 0.4;
constexpr double kAvoidOverlap = 0.8;
constexpr double kMaxVelocity = 50;
constexpr double kTimestep = 0.5;
constexpr int kMaxSteps = 600;

QColor withAlpha(QColor c, double a)
{
    c.setAlphaF(qBound(0.0, a, 1.0));
    return c;
}

} // namespace

GraphCanvas::GraphCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::OpenHandCursor);
    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &GraphCanvas::tick);
}

void GraphCanvas::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!m_settled)
        m_timer->start();
}

void GraphCanvas::setNetwork(const QVector<Node> &nodes, const QVector<Edge> &edges)
{
    m_nodes = nodes;
    m_edges = edges;
    m_adjacent = QVector<QVector<int>>(m_nodes.size());
    m_degree = QVector<int>(m_nodes.size(), 0);
    for (const Edge &e : m_edges) {
        m_adjacent[e.a].append(e.b);
        m_adjacent[e.b].append(e.a);
        ++m_degree[e.a];
        ++m_degree[e.b];
    }
    m_hover = m_selected = -1;
    m_message.clear();
    seedLayout();
    invalidate();
}

void GraphCanvas::setMessage(const QString &text)
{
    m_message = text;
    invalidate();
}

void GraphCanvas::setHiddenGroups(const QSet<int> &groups)
{
    for (Node &n : m_nodes)
        n.hidden = groups.contains(n.group);
    if (m_selected >= 0 && m_nodes[m_selected].hidden)
        setSelected(-1);
    if (m_hover >= 0 && m_nodes[m_hover].hidden)
        m_hover = -1;
    invalidate();
}

void GraphCanvas::setSelected(int index)
{
    if (index >= m_nodes.size())
        index = -1;
    if (index == m_selected)
        return;
    m_selected = index;
    emit selectionChanged(index);
    invalidate();
}

void GraphCanvas::seedLayout()
{
    // graph.html seeds a Fermat spiral (30·√i at the golden angle); the third axis gets a small,
    // deterministic spread so the forces have something to act on.
    QRandomGenerator rng(7);
    for (int i = 0; i < m_nodes.size(); ++i) {
        Node &n = m_nodes[i];
        const double r = 30 * qSqrt(double(i));
        n.pos = QVector3D(r * qCos(i * 2.4), (rng.generateDouble() - 0.5) * 60, r * qSin(i * 2.4));
        n.vel = {};
    }
    m_steps = 0;
    m_settled = m_nodes.size() < 2;
    if (!m_settled)
        m_timer->start();   // the layout runs until it settles (see tick)
}

bool GraphCanvas::stepLayout()
{
    // vis.js ForceAtlas2Based, in three dimensions: repulsion ∝ degree / distance (shortened by
    // the overlap avoidance), springs toward springLength, central gravity ∝ degree · distance.
    const int n = m_nodes.size();
    QVector<QVector3D> force(n);
    const double overlapFactor = 1 - kAvoidOverlap;
    for (int i = 0; i < n; ++i) {
        const Node &a = m_nodes[i];
        const double degA = m_degree[i] + 1;
        for (int j = 0; j < n; ++j) {
            if (i == j)
                continue;
            const Node &b = m_nodes[j];
            QVector3D d = a.pos - b.pos;
            double dist = d.length();
            if (dist < 1e-3) {
                d = QVector3D(0.1 * (i + 1), 0.05, 0.1 * (j + 1));
                dist = d.length();
            }
            const double eff = qMax(0.1 + overlapFactor * a.size, dist - a.size);
            const double g = kGravitationalConstant * degA / (eff * eff);
            force[i] -= d * g;   // negative constant: away from b
        }
        force[i] -= a.pos * (kCentralGravity * degA);
    }
    for (const Edge &e : m_edges) {
        const QVector3D d = m_nodes[e.b].pos - m_nodes[e.a].pos;
        const double dist = qMax(1e-3, double(d.length()));
        const QVector3D f = d * (kSpringConstant * (dist - kSpringLength) / dist);
        force[e.a] += f;
        force[e.b] -= f;
    }
    double maxSpeed = 0;
    for (int i = 0; i < n; ++i) {
        Node &nd = m_nodes[i];
        nd.vel = nd.vel * (1 - kDamping) + force[i] * kTimestep;
        const double speed = nd.vel.length();
        if (speed > kMaxVelocity)
            nd.vel *= kMaxVelocity / speed;
        nd.pos += nd.vel * kTimestep;
        maxSpeed = qMax(maxSpeed, qMin(speed, kMaxVelocity));
    }
    ++m_steps;
    return maxSpeed > 0.3 && m_steps < kMaxSteps;
}

void GraphCanvas::tick()
{
    // The timer only runs while the layout moves and the canvas is shown (shown again: see showEvent).
    if (!isVisible() || m_settled) {
        m_timer->stop();
        return;
    }
    for (int s = 0; s < 2 && !m_settled; ++s)
        m_settled = !stepLayout();
    invalidate();
}

void GraphCanvas::project()
{
    // Fit: the 90th-percentile radius fills the view (isolated nodes drift far out under
    // ForceAtlas2, as in graph.html), then the usual orbit and perspective.
    QVector<double> radii;
    radii.reserve(m_nodes.size());
    for (const Node &n : m_nodes)
        radii.append(QVector3D(n.pos.x(), n.pos.y() * kFlatten, n.pos.z()).length() + n.size);
    double radius = 1;
    if (!radii.isEmpty()) {
        const int k = qMin(radii.size() - 1, radii.size() * 9 / 10);
        std::nth_element(radii.begin(), radii.begin() + k, radii.end());
        radius = qMax(1.0, radii[k]);
    }
    const double cy = qCos(m_yaw), sy = qSin(m_yaw), cp = qCos(m_pitch), sp = qSin(m_pitch);
    const QPointF center(width() / 2.0, height() / 2.0);
    const double fit = qMin(width() * 0.47, height() * 0.47) * m_zoom / radius;
    for (Node &n : m_nodes) {
        const double y = n.pos.y() * kFlatten;
        const double x1 = n.pos.x() * cy + n.pos.z() * sy;
        const double z1 = -n.pos.x() * sy + n.pos.z() * cy;
        const double y2 = y * cp - z1 * sp;
        const double z2 = (y * sp + z1 * cp) / radius;   // toward the viewer, -1..1
        n.scale = kCamera / (kCamera - z2) * fit;
        n.screen = center + QPointF(x1, -y2) * n.scale;
        n.depth = qBound(0.0, (z2 + 1.2) / 2.4, 1.0);
    }
}

int GraphCanvas::hitTest(const QPointF &p) const
{
    int best = -1;
    double bestDepth = -1;
    for (int i = 0; i < m_nodes.size(); ++i) {
        const Node &n = m_nodes[i];
        if (!n.hidden && QLineF(p, n.screen).length() <= n.size * n.scale + 2 && n.depth > bestDepth) {
            best = i;
            bestDepth = n.depth;
        }
    }
    return best;
}

void GraphCanvas::drawNode(QPainter &p, int i, bool highlighted, double alpha)
{
    const Node &n = m_nodes[i];
    const double r = n.size * n.scale;
    // vis "dot": filled, 1.5 px border of the same colour; highlighted: white fill. A touch of
    // shading so the dots read as spheres once the view is tilted.
    QRadialGradient grad(n.screen + QPointF(-r * 0.35, -r * 0.35), r * 1.5);
    const QColor base = highlighted ? QColor(255, 255, 255) : n.color;
    grad.setColorAt(0.0, withAlpha(base.lighter(highlighted ? 100 : 125), alpha));
    grad.setColorAt(0.55, withAlpha(base, alpha));
    grad.setColorAt(1.0, withAlpha(base.darker(highlighted ? 110 : 140), alpha));
    p.setPen(QPen(withAlpha(n.color, alpha), 1.5));
    p.setBrush(grad);
    p.drawEllipse(n.screen, r, r);
}

void GraphCanvas::drawLabel(QPainter &p, int i, double alpha, QVector<QRectF> *taken)
{
    const Node &n = m_nodes[i];
    const double r = n.size * n.scale;
    QRectF box = p.fontMetrics().boundingRect(n.label).adjusted(-3, -1, 3, 1);
    box.moveCenter(n.screen + QPointF(0, r + box.height() / 2 + 2));   // vis: label under the dot
    if (taken) {
        for (const QRectF &other : *taken)
            if (other.intersects(box))
                return;
        taken->append(box);
    }
    const Palette &t = Theme::current();
    p.setPen(withAlpha(Theme::isLight(t) ? QColor(20, 20, 30) : QColor(255, 255, 255), alpha));
    p.drawText(box, Qt::AlignCenter, n.label);
}

void GraphCanvas::renderScene()
{
    QElapsedTimer clock;
    clock.start();
    const Palette &t = Theme::current();
    const bool light = Theme::isLight(t);
    const qreal dpr = devicePixelRatioF();
    if (m_cache.size() != size() * dpr) {
        m_cache = QPixmap(size() * dpr);
        m_cache.setDevicePixelRatio(dpr);
    }
    m_cache.fill(t.Background);
    QPainter p(&m_cache);
    p.setRenderHint(QPainter::Antialiasing);
    project();
    const int sel = m_selected;
    auto nodeAlpha = [&](int i) {
        const Node &n = m_nodes[i];
        double a = 0.55 + 0.45 * n.depth;
        if (sel >= 0 && i != sel && !m_adjacent[sel].contains(i))
            a *= 0.25;
        return a;
    };

    // Links: vis default grey, dashed when aggregated, an arrow at the target. Far ones fade.
    const QColor edgeColor(0x84, 0x84, 0x84);   // vis default
    Q_UNUSED(light);
    for (const Edge &e : m_edges) {
        const Node &a = m_nodes[e.a];
        const Node &b = m_nodes[e.b];
        if (a.hidden || b.hidden)
            continue;
        const double scale = (a.scale + b.scale) / 2;
        double alpha = e.opacity * (0.6 + 0.4 * (a.depth + b.depth) / 2);
        if (sel >= 0 && e.a != sel && e.b != sel)
            alpha *= 0.2;
        QLineF line(a.screen, b.screen);
        if (line.length() < 1e-3)
            continue;
        // Stop at the dots' borders, like vis.
        const QPointF dir = (line.p2() - line.p1()) / line.length();
        line.setP1(a.screen + dir * (a.size * a.scale));
        line.setP2(b.screen - dir * (b.size * b.scale));
        if (QLineF(line.p1(), b.screen).length() < b.size * b.scale)
            continue;   // overlapping dots
        QPen pen(withAlpha(edgeColor, alpha), qMax(1.0, e.width * scale));
        if (e.dashed) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern({5, 5});
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(line);
        // Arrow head (vis scaleFactor 0.5: about 8 px).
        const double len = 8 * scale, half = 3.2 * scale;
        const QPointF tip = line.p2(), back = tip - dir * len;
        const QPointF ortho(-dir.y(), dir.x());
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(edgeColor, alpha));
        p.drawPolygon(QPolygonF({tip, back + ortho * half, back - ortho * half}));
    }

    // Dots, far to near. Labels only for the selection and its neighbours: at this zoom graph.html
    // keeps them under vis's draw threshold too.
    QVector<int> order(m_nodes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [this](int a, int b) { return m_nodes[a].depth < m_nodes[b].depth; });
    for (int i : order)
        if (!m_nodes[i].hidden)
            drawNode(p, i, i == sel, nodeAlpha(i));
    if (sel >= 0) {
        QFont f = font();
        f.setPixelSize(12);
        p.setFont(f);
        QVector<QRectF> taken;
        drawLabel(p, sel, 1.0, &taken);
        for (int j : m_adjacent[sel])
            if (!m_nodes[j].hidden)
                drawLabel(p, j, 0.95, &taken);
    }
    m_renderMs = int(clock.elapsed());
    m_sceneDirty = false;
    emit rendered();
}

void GraphCanvas::paintEvent(QPaintEvent *)
{
    const Palette &t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!m_message.isEmpty() || m_nodes.isEmpty()) {
        p.fillRect(rect(), t.Background);
        p.setPen(t.TextDim);
        p.drawText(rect().adjusted(24, 0, -24, 0), Qt::AlignCenter | Qt::TextWordWrap,
                   m_message.isEmpty() ? tr("No graph loaded") : m_message);
        return;
    }
    if (m_sceneDirty)
        renderScene();
    p.drawPixmap(0, 0, m_cache);
    if (m_hover < 0)
        return;

    // Hover, as vis does it: the dot turns white, its links thicken, the neighbours are named.
    const Node &n = m_nodes[m_hover];
    QVector<QLineF> lines;
    for (int j : m_adjacent[m_hover])
        if (!m_nodes[j].hidden)
            lines.append(QLineF(n.screen, m_nodes[j].screen));
    p.setPen(QPen(withAlpha(t.Text, 0.75), 2));
    p.drawLines(lines);
    for (int j : m_adjacent[m_hover])
        if (!m_nodes[j].hidden)
            drawNode(p, j, false, 1.0);
    drawNode(p, m_hover, true, 1.0);
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);
    QVector<QRectF> taken;
    drawLabel(p, m_hover, 1.0, &taken);
    for (int j : m_adjacent[m_hover])
        if (!m_nodes[j].hidden)
            drawLabel(p, j, 1.0, &taken);

    // Tooltip: the label, the title and the link count.
    QFont bold = font();
    bold.setBold(true);
    const QFontMetrics fmB(bold), fm(font());
    const QString l1 = n.label;
    const QString l2 = n.title;
    const QString l3 = tr("%n links", nullptr, m_adjacent[m_hover].size());
    const int w = qMax(fmB.horizontalAdvance(l1), qMax(fm.horizontalAdvance(l2), fm.horizontalAdvance(l3))) + 20;
    const int h = fmB.height() + (l2.isEmpty() ? 1 : 2) * fm.height() + 18;
    QPointF at = m_cursor + QPointF(14, 14);
    if (at.x() + w > width())
        at.setX(width() - w - 4);
    if (at.y() + h > height())
        at.setY(m_cursor.y() - h - 10);
    const QRectF box(at, QSizeF(w, h));
    p.setPen(QPen(t.TooltipBorder, 1));
    p.setBrush(t.Tooltip);
    p.drawRoundedRect(box, 4, 4);
    p.setFont(bold);
    p.setPen(t.Text);
    p.drawText(box.adjusted(10, 7, -10, 0), Qt::AlignLeft | Qt::AlignTop, l1);
    p.setFont(font());
    p.setPen(t.TextDim);
    int y = 9 + fmB.height();
    if (!l2.isEmpty()) {
        p.drawText(box.adjusted(10, y, -10, 0), Qt::AlignLeft | Qt::AlignTop, l2);
        y += fm.height() + 2;
    }
    p.drawText(box.adjusted(10, y, -10, 0), Qt::AlignLeft | Qt::AlignTop, l3);
}

void GraphCanvas::resizeEvent(QResizeEvent *)
{
    invalidate();
}

void GraphCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_moved = false;
    m_pressPos = m_lastPos = e->position();
    setCursor(Qt::ClosedHandCursor);
}

void GraphCanvas::mouseMoveEvent(QMouseEvent *e)
{
    m_cursor = e->position();
    if (m_dragging) {
        const QPointF d = e->position() - m_lastPos;
        m_lastPos = e->position();
        if ((e->position() - m_pressPos).manhattanLength() > 3)
            m_moved = true;
        m_yaw += d.x() * 0.006;
        m_pitch = qBound(-1.5, m_pitch + d.y() * 0.006, 1.5);
        m_hover = -1;
        invalidate();
        return;
    }
    const int hit = hitTest(m_cursor);
    if (hit != m_hover || hit >= 0) {
        m_hover = hit;
        update();   // overlay only, the cached scene is untouched
    }
}

void GraphCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
    if (!m_moved) {
        const int hit = hitTest(e->position());
        setSelected(hit);   // graph.html: a click on the background clears the info panel
        invalidate();
    }
}

void GraphCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    const int hit = hitTest(e->position());
    if (hit >= 0)
        emit activated(m_nodes[hit].ref);
}

void GraphCanvas::wheelEvent(QWheelEvent *e)
{
    const double steps = e->angleDelta().y() / 120.0;
    m_zoom = qBound(0.3, m_zoom * qPow(1.15, steps), 8.0);
    invalidate();
}

void GraphCanvas::leaveEvent(QEvent *)
{
    if (m_hover >= 0) {
        m_hover = -1;
        update();
    }
}

void GraphCanvas::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && m_selected >= 0) {
        setSelected(-1);
        return;
    }
    QWidget::keyPressEvent(e);
}

// ---------------------------------------------------------------- CodeGraphView

namespace {

QPixmap arrowPixmap(const QColor &color, bool back)
{
    const int size = 14;
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (back) {
        p.drawPolyline(QPolygonF({QPointF(9, 3), QPointF(5, 7), QPointF(9, 11)}));
    } else {
        // Circular arrow (reload / spin).
        QPainterPath arc;
        arc.arcMoveTo(QRectF(2.5, 2.5, 9, 9), 40);
        arc.arcTo(QRectF(2.5, 2.5, 9, 9), 40, 280);
        p.drawPath(arc);
        p.drawPolyline(QPolygonF({QPointF(11.2, 2.2), QPointF(11.4, 5.6), QPointF(8.1, 5.2)}));
    }
    return pm;
}

QToolButton *iconButton(const QString &tip, bool back)
{
    auto *b = new QToolButton;
    b->setObjectName("devIcon");
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedSize(26, 26);
    QIcon icon;
    icon.addPixmap(arrowPixmap(Theme::current().TextDim, back), QIcon::Normal, QIcon::Off);
    icon.addPixmap(arrowPixmap(Theme::current().Text, back), QIcon::Active, QIcon::Off);
    icon.addPixmap(arrowPixmap(Theme::current().Accent, back), QIcon::Normal, QIcon::On);
    icon.addPixmap(arrowPixmap(Theme::current().Accent, back), QIcon::Active, QIcon::On);
    b->setIcon(icon);
    b->setIconSize(QSize(14, 14));
    return b;
}

} // namespace

namespace {

// graph.html's legend row: checkbox, colour dot, label, count; dimmed when unchecked.
class LegendDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {200, 24}; }
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const Palette &t = Theme::current();
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (opt.state & QStyle::State_MouseOver)
            p->fillRect(opt.rect, t.Hover);
        p->setOpacity(checked ? 1.0 : 0.35);
        const QRectF r = opt.rect;
        // Checkbox.
        const QRectF box(r.left() + 6, r.center().y() - 6, 12, 12);
        p->setPen(QPen(checked ? t.Accent : t.ControlBorder, 1));
        p->setBrush(checked ? t.Accent : Qt::NoBrush);
        p->drawRoundedRect(box, 2, 2);
        if (checked) {
            p->setPen(QPen(t.Background, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawPolyline(QPolygonF({box.topLeft() + QPointF(2.5, 6), box.topLeft() + QPointF(5, 8.5), box.topLeft() + QPointF(9.5, 3.5)}));
        }
        // Dot.
        p->setPen(Qt::NoPen);
        p->setBrush(index.data(Qt::UserRole + 1).value<QColor>());
        p->drawEllipse(QPointF(r.left() + 32, r.center().y()), 5, 5);
        // Count, then the label in what is left.
        QFont f = opt.font;
        f.setPixelSize(11);
        p->setFont(f);
        const QString count = QString::number(index.data(Qt::UserRole + 2).toInt());
        const int countW = QFontMetrics(f).horizontalAdvance(count);
        p->setPen(t.TextFaint);
        p->drawText(QRectF(r.right() - countW - 8, r.top(), countW, r.height()), Qt::AlignVCenter | Qt::AlignRight, count);
        f.setPixelSize(12);
        p->setFont(f);
        p->setPen(t.Text);
        const QRectF labelRect(r.left() + 44, r.top(), r.width() - 44 - countW - 16, r.height());
        p->drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetrics(f).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(labelRect.width())));
        p->restore();
    }
};

QLabel *sideTitle(const QString &text)
{
    auto *l = new QLabel(text);
    l->setObjectName("devSideTitle");
    return l;
}

} // namespace

CodeGraphView::CodeGraphView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("devCodebase");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *bar = new QWidget;
    bar->setObjectName("devToolRow");
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(6);
    m_back = iconButton(tr("Back to the communities"), true);
    m_back->hide();
    row->addWidget(m_back);
    m_title = new QLabel;
    m_title->setObjectName("devTitle");
    row->addWidget(m_title, 1);
    m_reload = iconButton(tr("Reload graph.json"), false);
    row->addWidget(m_reload);
    layout->addWidget(bar);

    // Canvas on the left, graph.html's sidebar on the right.
    auto *body = new QWidget;
    auto *split = new QHBoxLayout(body);
    split->setContentsMargins(0, 0, 0, 0);
    split->setSpacing(0);
    m_canvas = new GraphCanvas;
    split->addWidget(m_canvas, 1);

    auto *side = new QFrame;
    side->setObjectName("devSidebar");
    side->setFixedWidth(280);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    auto *searchWrap = new QWidget;
    searchWrap->setObjectName("devSideSection");
    auto *searchLayout = new QVBoxLayout(searchWrap);
    searchLayout->setContentsMargins(12, 12, 12, 12);
    searchLayout->setSpacing(4);
    m_search = new QLineEdit;
    m_search->setObjectName("devSearch");
    m_search->setPlaceholderText(tr("Search nodes..."));
    m_search->setClearButtonEnabled(true);
    searchLayout->addWidget(m_search);
    m_results = new QListWidget;
    m_results->setObjectName("devResults");
    m_results->setMaximumHeight(140);
    m_results->setFocusPolicy(Qt::NoFocus);
    m_results->hide();
    searchLayout->addWidget(m_results);
    sideLayout->addWidget(searchWrap);

    auto *infoWrap = new QWidget;
    infoWrap->setObjectName("devSideSection");
    auto *infoLayout = new QVBoxLayout(infoWrap);
    infoLayout->setContentsMargins(14, 14, 14, 14);
    infoLayout->setSpacing(8);
    infoLayout->addWidget(sideTitle(tr("Node info")));
    m_info = new QLabel;
    m_info->setObjectName("devInfo");
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::RichText);
    m_info->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    m_info->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_info->setMinimumHeight(110);
    auto *infoScroll = new QScrollArea;
    infoScroll->setObjectName("devInfoScroll");
    infoScroll->setWidgetResizable(true);
    infoScroll->setFrameShape(QFrame::NoFrame);
    infoScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    infoScroll->setWidget(m_info);
    infoScroll->setMaximumHeight(260);
    infoLayout->addWidget(infoScroll);
    sideLayout->addWidget(infoWrap);

    auto *legendWrap = new QWidget;
    legendWrap->setObjectName("devSideSection");
    auto *legendLayout = new QVBoxLayout(legendWrap);
    legendLayout->setContentsMargins(14, 14, 14, 8);
    legendLayout->setSpacing(6);
    legendLayout->addWidget(sideTitle(tr("Communities")));
    m_selectAll = new QCheckBox(tr("Select all"));
    m_selectAll->setObjectName("devSelectAll");
    m_selectAll->setTristate(true);
    m_selectAll->setChecked(true);
    legendLayout->addWidget(m_selectAll);
    m_legend = new QListWidget;
    m_legend->setObjectName("devLegend");
    m_legend->setItemDelegate(new LegendDelegate(m_legend));
    m_legend->setFocusPolicy(Qt::NoFocus);
    m_legend->setMouseTracking(true);
    m_legend->setSelectionMode(QAbstractItemView::NoSelection);
    m_legend->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    legendLayout->addWidget(m_legend, 1);
    sideLayout->addWidget(legendWrap, 1);

    m_stats = new QLabel;
    m_stats->setObjectName("devSideStats");
    sideLayout->addWidget(m_stats);
    split->addWidget(side);
    layout->addWidget(body, 1);

    m_status = new QLabel;
    m_status->setObjectName("devStatus");
    layout->addWidget(m_status);

    connect(m_back, &QToolButton::clicked, this, &CodeGraphView::showCommunities);
    connect(m_reload, &QToolButton::clicked, this, &CodeGraphView::reload);
    connect(m_canvas, &GraphCanvas::rendered, this, [this] { updateStatus(); });
    connect(m_canvas, &GraphCanvas::activated, this, [this](int ref) {
        if (m_community < 0)
            showCommunity(ref);
    });
    connect(m_canvas, &GraphCanvas::selectionChanged, this, &CodeGraphView::showInfo);
    connect(m_info, &QLabel::linkActivated, this, [this](const QString &link) {
        m_canvas->setSelected(link.toInt());
    });
    connect(m_search, &QLineEdit::textChanged, this, &CodeGraphView::runSearch);
    connect(m_results, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        m_canvas->setSelected(item->data(Qt::UserRole).toInt());
    });
    connect(m_legend, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    });
    connect(m_legend, &QListWidget::itemChanged, this, [this] { legendChanged(); });
    connect(m_selectAll, &QCheckBox::clicked, this, [this](bool checked) {
        m_selectAll->setTristate(false);
        m_selectAll->setChecked(checked);
        m_legendUpdating = true;
        for (int i = 0; i < m_legend->count(); ++i)
            m_legend->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        m_legendUpdating = false;
        legendChanged();
    });

    // graph.json is rewritten whole by `graphify update`; watch its folder and reload shortly after.
    m_watcher = new QFileSystemWatcher(this);
    m_reloadDelay = new QTimer(this);
    m_reloadDelay->setSingleShot(true);
    m_reloadDelay->setInterval(1500);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_reloadDelay, qOverload<>(&QTimer::start));
    connect(m_reloadDelay, &QTimer::timeout, this, [this] {
        const QDateTime modified = QFileInfo(m_path).lastModified();
        if (m_graph && modified.isValid() && modified != m_graph->modified)
            reload();
    });

    showInfo(-1);
    setGraphPath(codegraph::locate());
}

void CodeGraphView::setLegend(const QVector<LegendEntry> &entries)
{
    m_legendUpdating = true;
    m_legend->clear();
    for (const LegendEntry &e : entries) {
        auto *item = new QListWidgetItem(e.label);
        item->setData(Qt::UserRole, e.group);
        item->setData(Qt::UserRole + 1, e.color);
        item->setData(Qt::UserRole + 2, e.count);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setToolTip(e.label);
        m_legend->addItem(item);
    }
    m_legendUpdating = false;
    m_selectAll->setTristate(false);
    m_selectAll->setChecked(true);
}

void CodeGraphView::legendChanged()
{
    if (m_legendUpdating)
        return;
    QSet<int> hidden;
    for (int i = 0; i < m_legend->count(); ++i)
        if (m_legend->item(i)->checkState() != Qt::Checked)
            hidden.insert(m_legend->item(i)->data(Qt::UserRole).toInt());
    m_canvas->setHiddenGroups(hidden);
    const int total = m_legend->count();
    if (hidden.isEmpty() || hidden.size() == total) {
        m_selectAll->setTristate(false);
        m_selectAll->setChecked(hidden.isEmpty());
    } else {
        m_selectAll->setTristate(true);
        m_selectAll->setCheckState(Qt::PartiallyChecked);
    }
}

void CodeGraphView::showInfo(int index)
{
    const Palette &t = Theme::current();
    if (index < 0 || index >= m_canvas->nodes().size()) {
        m_info->setText(QStringLiteral("<i style=\"color:%1\">%2</i>").arg(t.TextFaint.name(), tr("Click a node to inspect it")));
        return;
    }
    const GraphCanvas::Node &n = m_canvas->nodes()[index];
    const bool meta = n.type.isEmpty() && n.file.isEmpty();   // a community
    auto field = [](const QString &label, const QString &value) {
        return QStringLiteral("<div>%1: %2</div>").arg(label, value.toHtmlEscaped());
    };
    QString html = QStringLiteral("<div><b>%1</b></div>").arg(n.label.toHtmlEscaped());
    if (!meta)
        html += field(tr("Type"), n.type.isEmpty() ? tr("unknown") : n.type);
    html += field(tr("Community"), n.community);
    if (!meta)
        html += field(tr("Source"), n.file.isEmpty() ? QStringLiteral("-") : n.file);
    else if (n.members >= 0)
        html += field(tr("Members"), QString::number(n.members));
    const QVector<int> &nb = m_canvas->neighbours(index);
    html += field(tr("Degree"), QString::number(nb.size()));
    if (!nb.isEmpty()) {
        html += QStringLiteral("<div style=\"margin-top:8px;color:%1;font-size:11px\">%2 (%3)</div>")
                    .arg(t.TextDim.name(), tr("Neighbors"), QString::number(nb.size()));
        for (int j : nb) {
            const GraphCanvas::Node &o = m_canvas->nodes()[j];
            html += QStringLiteral("<div style=\"border-left:3px solid %1;padding-left:6px;margin:2px 0\">"
                                   "<a href=\"%2\" style=\"color:%3;text-decoration:none\">%4</a></div>")
                        .arg(o.color.name(), QString::number(j), t.Text.name(), o.label.toHtmlEscaped());
        }
    }
    m_info->setText(html);
}

void CodeGraphView::runSearch(const QString &text)
{
    const QString q = text.trimmed();
    m_results->clear();
    if (q.isEmpty()) {
        m_results->hide();
        return;
    }
    const QVector<GraphCanvas::Node> &nodes = m_canvas->nodes();
    for (int i = 0; i < nodes.size() && m_results->count() < 20; ++i) {
        if (!nodes[i].label.contains(q, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(nodes[i].label);
        item->setData(Qt::UserRole, i);
        QPixmap bar(3, 16);
        bar.fill(nodes[i].color);
        item->setIcon(QIcon(bar));
        m_results->addItem(item);
    }
    m_results->setVisible(m_results->count() > 0);
}

void CodeGraphView::setGraphPath(const QString &path)
{
    m_path = path;
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    if (path.isEmpty()) {
        m_canvas->setMessage(tr("graphify-out/graph.json not found.\nRun /graphify in the project to build the code graph."));
        m_title->setText(tr("CODEBASE"));
        m_status->setText({});
        return;
    }
    m_watcher->addPath(QFileInfo(path).absolutePath());
    reload();
}

void CodeGraphView::reload()
{
    if (m_path.isEmpty() || m_loading)
        return;
    m_loading = true;
    updateStatus(tr("loading…"));
    m_loader = new QFutureWatcher<codegraph::Graph>(this);
    connect(m_loader, &QFutureWatcher<codegraph::Graph>::finished, this, [this] {
        m_graph = std::make_shared<codegraph::Graph>(m_loader->result());
        m_loader->deleteLater();
        m_loader = nullptr;
        m_loading = false;
        applyGraph();
    });
    const QString path = m_path;
    m_loader->setFuture(QtConcurrent::run([path] { return codegraph::load(path); }));
}

void CodeGraphView::applyGraph()
{
    if (!m_graph->error.isEmpty()) {
        m_canvas->setMessage(m_graph->error);
        updateStatus();
        return;
    }
    if (m_community >= 0)
        showCommunity(m_community);
    else
        showCommunities();
}

void CodeGraphView::showCommunities()
{
    m_community = -1;
    m_back->hide();
    if (!m_graph || !m_graph->error.isEmpty())
        return;
    const codegraph::Graph &g = *m_graph;
    QVector<GraphCanvas::Node> nodes;
    nodes.reserve(g.communities.size());
    QVector<LegendEntry> legend;
    for (const codegraph::Community &c : g.communities) {
        GraphCanvas::Node n;
        n.label = c.label;
        n.title = tr("%n nodes", nullptr, c.members);
        n.color = c.color;
        n.size = c.size;
        n.ref = c.cid;
        n.group = c.cid;
        n.community = c.label;
        n.members = c.members;
        nodes.append(n);
        legend.append({c.cid, c.label, c.color, c.members});
    }
    QVector<GraphCanvas::Edge> edges;
    edges.reserve(g.communityLinks.size());
    for (const codegraph::CommunityLink &l : g.communityLinks) {
        GraphCanvas::Edge e;
        e.a = l.a;
        e.b = l.b;
        e.dashed = true;
        e.title = tr("%n cross-community edges", nullptr, l.count);
        edges.append(e);
    }
    m_canvas->setNetwork(nodes, edges);
    setLegend(legend);
    m_stats->setText(tr("%n nodes", nullptr, nodes.size()) + QStringLiteral(" · ") + tr("%n edges", nullptr, edges.size())
                     + QStringLiteral(" · ") + tr("%n communities", nullptr, legend.size()));
    m_title->setText(tr("CODEBASE · %n communities", nullptr, nodes.size()));
    showInfo(-1);
    updateStatus();
}

void CodeGraphView::showCommunity(int cid)
{
    if (!m_graph || cid < 0)
        return;
    const codegraph::Graph &g = *m_graph;
    QColor color(0x4E, 0x79, 0xA7);
    QString name = cid < g.communityNames.size() ? g.communityNames[cid] : QString();
    for (const codegraph::Community &c : g.communities)
        if (c.cid == cid) {
            color = c.color;
            name = c.label;
        }
    m_community = cid;
    m_back->show();

    // The community's own graph: its nodes in its colour, sized by degree, its internal edges.
    QVector<GraphCanvas::Node> nodes;
    QHash<int, int> nodeOf;
    QHash<QString, int> fileGroup;
    QVector<LegendEntry> legend;   // the community's files
    for (int i = 0; i < g.nodes.size(); ++i) {
        const codegraph::Node &n = g.nodes[i];
        if (n.community != cid)
            continue;
        GraphCanvas::Node v;
        v.label = n.label;
        v.title = n.file.isEmpty() ? n.type : n.file + (n.location.isEmpty() ? QString() : ":" + n.location.mid(1));
        v.color = color;
        v.ref = i;
        v.type = n.type;
        v.file = n.file;
        v.community = name;
        auto it = fileGroup.find(n.file);
        if (it == fileGroup.end()) {
            it = fileGroup.insert(n.file, legend.size());
            legend.append({int(legend.size()), n.file.isEmpty() ? tr("(no file)") : n.file, color, 0});
        }
        v.group = it.value();
        ++legend[it.value()].count;
        nodeOf.insert(i, nodes.size());
        nodes.append(v);
    }
    QVector<GraphCanvas::Edge> edges;
    QVector<int> degree(nodes.size(), 0), external(nodes.size(), 0);
    for (const codegraph::Edge &e : g.edges) {
        const int a = nodeOf.value(e.a, -1), b = nodeOf.value(e.b, -1);
        if (a < 0 && b < 0)
            continue;
        if (a < 0 || b < 0) {
            ++external[a < 0 ? b : a];
            continue;
        }
        edges.append({a, b, 1, 0.35, false, {}});
        ++degree[a];
        ++degree[b];
    }
    int maxDegree = 1;
    for (int d : degree)
        maxDegree = qMax(maxDegree, d);
    for (int i = 0; i < nodes.size(); ++i) {
        nodes[i].size = 6 + 14 * qSqrt(double(degree[i]) / maxDegree);
        if (external[i] > 0)
            nodes[i].title += QStringLiteral(" · ") + tr("%n outside links", nullptr, external[i]);
    }
    m_canvas->setNetwork(nodes, edges);
    setLegend(legend);
    m_stats->setText(tr("%n nodes", nullptr, nodes.size()) + QStringLiteral(" · ") + tr("%n edges", nullptr, edges.size())
                     + QStringLiteral(" · ") + tr("%n files", nullptr, legend.size()));
    m_title->setText(name.toUpper() + QStringLiteral(" · ") + tr("%n nodes", nullptr, nodes.size()));
    showInfo(-1);
    updateStatus();
}

void CodeGraphView::updateStatus(const QString &extra)
{
    QStringList parts;
    if (m_graph && m_graph->error.isEmpty()) {
        // Counted once per graph: the status is updated at every frame of the layout.
        if (m_summaryOf != m_graph.get()) {
            int populated = 0;
            for (const codegraph::Node &n : m_graph->nodes)
                populated = qMax(populated, n.community + 1);
            QVector<bool> seen(populated, false);
            for (const codegraph::Node &n : m_graph->nodes)
                if (n.community >= 0)
                    seen[n.community] = true;
            m_summary = QStringList {tr("%n communities", nullptr, int(std::count(seen.cbegin(), seen.cend(), true))),
                                     tr("%n nodes", nullptr, m_graph->nodes.size()),
                                     tr("%n links", nullptr, m_graph->edges.size()),
                                     QLocale().toString(m_graph->modified, QLocale::ShortFormat)};
            m_summaryOf = m_graph.get();
        }
        parts << m_summary;
    }
    if (!extra.isEmpty())
        parts << extra;
    else if (m_canvas->lastRenderMs() > 0)
        parts << tr("render %1 ms").arg(m_canvas->lastRenderMs());
    m_status->setText(parts.join(QStringLiteral("  ·  ")));
    m_status->setToolTip(m_path);
}
