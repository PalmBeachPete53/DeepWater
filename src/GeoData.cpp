#include "GeoData.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

namespace Geo {

QVector3D toVector(double latDeg, double lonDeg)
{
    const double lat = qDegreesToRadians(latDeg);
    const double lon = qDegreesToRadians(lonDeg);
    return QVector3D(float(qCos(lat) * qCos(lon)), float(qCos(lat) * qSin(lon)), float(qSin(lat)));
}

QVector<Country> loadCountries(const QString &path)
{
    QVector<Country> countries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return countries;

    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    countries.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Country country;
        country.name = o.value("n").toString();
        country.iso = o.value("iso").toString();
        for (const QJsonValue &rv : o.value("r").toArray()) {
            const QJsonArray flat = rv.toArray();
            GeoRing ring;
            ring.points.reserve(flat.size() / 2);
            QVector3D sum;
            for (int i = 0; i + 1 < flat.size(); i += 2) {
                const QVector3D p = toVector(flat[i + 1].toDouble(), flat[i].toDouble());
                ring.points.push_back(p);
                sum += p;
            }
            if (ring.points.size() < 3)
                continue;
            ring.center = sum.length() > 1e-6f ? sum.normalized() : ring.points.first();
            for (const QVector3D &p : std::as_const(ring.points))
                ring.cosRadius = std::min(ring.cosRadius, QVector3D::dotProduct(p, ring.center));
            float turn = 0;
            for (int i = 0; i < ring.points.size(); ++i)
                turn += QVector3D::dotProduct(
                    QVector3D::crossProduct(ring.points[i], ring.points[(i + 1) % ring.points.size()]), ring.center);
            ring.winding = turn >= 0 ? 1.f : -1.f;
            country.rings.push_back(std::move(ring));
        }
        countries.push_back(std::move(country));
    }
    return countries;
}

} // namespace Geo
