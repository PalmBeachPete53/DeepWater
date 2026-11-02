#include "SeaRoutes.h"

#include "GeoData.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtMath>

#include <queue>

namespace {

constexpr double EarthRadiusKm = 6371.0;

double arcKm(const QVector3D &a, const QVector3D &b)
{
    const double d = std::clamp(double(QVector3D::dotProduct(a, b)), -1.0, 1.0);
    return std::acos(d) * EarthRadiusKm;
}

QString normalize(const QString &s)
{
    static const QRegularExpression nonAlnum("[^A-Z0-9]+");
    return s.toUpper().replace(nonAlnum, " ").simplified();
}

} // namespace

// ---------------------------------------------------------------- SeaRoutes

bool SeaRoutes::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray nodes = root.value("nodes").toArray();
    const QJsonArray edges = root.value("edges").toArray();

    m_nodes.reserve(nodes.size() / 2);
    for (int i = 0; i + 1 < nodes.size(); i += 2)
        m_nodes.push_back(Geo::toVector(nodes[i + 1].toDouble(), nodes[i].toDouble()));

    // Build CSR adjacency (edges are undirected).
    const int n = m_nodes.size();
    QVector<int> degree(n, 0);
    for (int i = 0; i + 1 < edges.size(); i += 2) {
        ++degree[edges[i].toInt()];
        ++degree[edges[i + 1].toInt()];
    }
    m_offsets.resize(n + 1);
    m_offsets[0] = 0;
    for (int i = 0; i < n; ++i)
        m_offsets[i + 1] = m_offsets[i] + degree[i];
    m_adj.resize(m_offsets[n]);
    m_weights.resize(m_offsets[n]);
    QVector<int> fill = m_offsets;
    for (int i = 0; i + 1 < edges.size(); i += 2) {
        const int a = edges[i].toInt(), b = edges[i + 1].toInt();
        const float w = float(arcKm(m_nodes[a], m_nodes[b]));
        m_adj[fill[a]] = b; m_weights[fill[a]++] = w;
        m_adj[fill[b]] = a; m_weights[fill[b]++] = w;
    }
    return n > 0;
}

int SeaRoutes::nearestNode(const QVector3D &v) const
{
    int best = -1;
    float bestDot = -2;
    for (int i = 0; i < m_nodes.size(); ++i) {
        const float d = QVector3D::dotProduct(m_nodes[i], v);
        if (d > bestDot) {
            bestDot = d;
            best = i;
        }
    }
    return best;
}

SeaRoutes::Route SeaRoutes::route(double fromLat, double fromLon, double toLat, double toLon) const
{
    Route r;
    if (m_nodes.isEmpty())
        return r;
    const QVector3D from = Geo::toVector(fromLat, fromLon);
    const QVector3D to = Geo::toVector(toLat, toLon);
    const int src = nearestNode(from), dst = nearestNode(to);

    const int n = m_nodes.size();
    QVector<double> dist(n, std::numeric_limits<double>::infinity());
    QVector<int> prev(n, -1);
    using Item = std::pair<double, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
    dist[src] = 0;
    queue.push({0, src});
    while (!queue.empty()) {
        const auto [d, u] = queue.top();
        queue.pop();
        if (u == dst)
            break;
        if (d > dist[u])
            continue;
        for (int k = m_offsets[u]; k < m_offsets[u + 1]; ++k) {
            const int v = m_adj[k];
            const double nd = d + m_weights[k];
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
                queue.push({nd, v});
            }
        }
    }
    if (src != dst && prev[dst] < 0)
        return r;

    QVector<QVector3D> path;
    for (int v = dst; v >= 0; v = prev[v])
        path.push_back(m_nodes[v]);
    std::reverse(path.begin(), path.end());

    r.points.push_back(from);
    r.points += path;
    r.points.push_back(to);
    for (int i = 1; i < r.points.size(); ++i)
        r.lengthKm += arcKm(r.points[i - 1], r.points[i]);
    return r;
}

// ---------------------------------------------------------------- PortIndex

bool PortIndex::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonArray rows = QJsonDocument::fromJson(file.readAll()).array();
    m_ports.reserve(rows.size());
    for (const QJsonValue &rv : rows) {
        const QJsonArray r = rv.toArray();
        Port p;
        p.locode = r[0].toString();
        p.name = r[1].toString();
        p.lat = r[2].toDouble();
        p.lon = r[3].toDouble();
        p.major = r[4].toInt() == 1;
        const int idx = m_ports.size();
        m_ports.push_back(p);
        m_byCode.insert(p.locode, idx);
        m_byName[normalize(p.name)].push_back(idx);
    }
    return !m_ports.isEmpty();
}

const Port *PortIndex::byCode(const QString &code) const
{
    auto it = m_byCode.constFind(code);
    return it == m_byCode.constEnd() ? nullptr : &m_ports[*it];
}

const Port *PortIndex::resolve(const QString &destination) const
{
    // "FROM>TO" is a common convention; the destination is the last part.
    QString raw = destination.section('>', -1).trimmed();
    const QString norm = normalize(raw);
    if (norm.isEmpty())
        return nullptr;

    // UN/LOCODE, written "NLRTM", "NL RTM" or "NL-RTM".
    const QString compact = QString(norm).remove(' ');
    if (compact.size() == 5 && compact.at(0).isLetter() && compact.at(1).isLetter()) {
        if (const Port *p = byCode(compact))
            return p;
    }
    const QStringList tokens = norm.split(' ');
    if (tokens.size() >= 2 && tokens[0].size() == 2 && tokens[1].size() == 3) {
        if (const Port *p = byCode(tokens[0] + tokens[1]))
            return p;
    }
    if (tokens.size() >= 2 && tokens[0].size() == 5) {   // "NLRTM ANCH"
        if (const Port *p = byCode(tokens[0]))
            return p;
    }

    // Port name, with or without a leading country code / trailing noise.
    auto byName = [this](const QString &name) -> const Port * {
        const auto it = m_byName.constFind(name);
        if (it == m_byName.constEnd())
            return nullptr;
        // Prefer a single major port; give up on ambiguous names.
        const Port *major = nullptr;
        int majors = 0;
        for (int idx : *it)
            if (m_ports[idx].major) { major = &m_ports[idx]; ++majors; }
        if (majors == 1)
            return major;
        return it->size() == 1 ? &m_ports[it->first()] : nullptr;
    };
    if (const Port *p = byName(norm))
        return p;
    if (tokens.size() >= 2 && tokens[0].size() == 2) {
        if (const Port *p = byName(tokens.mid(1).join(' ')))
            return p;
    }
    static const QStringList noise {"PORT", "ANCH", "ANCHORAGE", "OPL", "PILOT", "ROADS", "RDS"};
    QStringList cleaned;
    for (const QString &t : tokens)
        if (!noise.contains(t)) cleaned << t;
    if (cleaned.size() != tokens.size() && !cleaned.isEmpty()) {
        if (const Port *p = byName(cleaned.join(' ')))
            return p;
    }
    // "ITAGUIA, BRAZIL", "SANTOS BR": drop trailing words one at a time.
    for (int n = int(tokens.size()) - 1; n >= 1; --n) {
        if (const Port *p = byName(tokens.mid(0, n).join(' ')))
            return p;
    }
    return nullptr;
}
