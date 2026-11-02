#pragma once

#include <QString>
#include <QVector>
#include <QVector3D>

// A single polygon ring, stored as unit vectors on the sphere so that
// re-projecting for a new rotation is just a 3x3 matrix multiply.
struct GeoRing {
    QVector<QVector3D> points;
    QVector3D center;       // normalized mean of the points
    float cosRadius = 1.f;  // cos of the angular radius around center (for culling)
    float winding = 1.f;    // +1 if counterclockwise seen from outside the sphere, -1 otherwise
};

struct Country {
    QString name;
    QString iso;
    QVector<GeoRing> rings;
};

namespace Geo {

// lat/lon in degrees -> unit vector (x toward lon 0 on the equator, z toward north pole)
QVector3D toVector(double latDeg, double lonDeg);

QVector<Country> loadCountries(const QString &path);

} // namespace Geo
