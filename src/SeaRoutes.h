#pragma once

#include <QHash>
#include <QString>
#include <QVector>
#include <QVector3D>

// Maritime route network (Eurostat "marnet" via searoute-py) with shortest-path
// routing, so routes follow shipping lanes instead of crossing land.
class SeaRoutes
{
public:
    bool load(const QString &path);

    struct Route {
        QVector<QVector3D> points;   // unit vectors, from origin to destination
        double lengthKm = 0;
        bool isValid() const { return points.size() >= 2; }
    };

    // Snaps both ends to the nearest network nodes and runs Dijkstra.
    Route route(double fromLat, double fromLon, double toLat, double toLon) const;

private:
    int nearestNode(const QVector3D &v) const;

    QVector<QVector3D> m_nodes;
    // Adjacency in CSR form: neighbours of node i are m_adj[m_offsets[i] .. m_offsets[i+1]).
    QVector<int> m_offsets;
    QVector<int> m_adj;
    QVector<float> m_weights;   // km
};

struct Port {
    QString locode;   // UN/LOCODE, e.g. NLRTM
    QString name;
    double lat = 0;
    double lon = 0;
    bool major = false;
};

// Resolves the free-text AIS destination field ("NL RTM", "ROTTERDAM",
// "SGSIN>CNSHA", "GB-RAM"...) to a port.
class PortIndex
{
public:
    bool load(const QString &path);
    const Port *resolve(const QString &destination) const;

private:
    const Port *byCode(const QString &code) const;

    QVector<Port> m_ports;
    QHash<QString, int> m_byCode;
    QHash<QString, QVector<int>> m_byName;   // normalized name -> ports
};
