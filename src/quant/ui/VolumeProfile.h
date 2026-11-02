#pragma once

// Port of quant-terminal/public/js/volume-profile.js: the visible-range volume profile (VPVR) and
// the session volume profile (SVP), drawn as series primitives under the candles.

#include "../chart/Chart.h"
#include "../core/Bar.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace qtm {

class ChartPane;

class VolumeProfile : public lw::Primitive
{
public:
    VolumeProfile(ChartPane *pane, const QJsonObject &params);

    struct Profile {
        double lo = 0, step = 0;
        QVector<double> up, down, total;
        int poc = 0, vaLow = 0, vaHigh = 0;
        double max = 0;
    };
    // Rows of { lo, hi, up, down } plus poc and value-area indexes (JS VolumeProfile.profile).
    static std::optional<Profile> profile(const Bars &bars, int rows, double valueAreaPct);

    ZOrder zOrder() const override { return ZOrder::Bottom; }
    void draw(QPainter &p, const QSizeF &size) override;

protected:
    ChartPane *m_pane;
    QJsonObject m_p;   // { rows, width, valueArea, period }
    double num(const QString &key) const;
};

class SessionVolumeProfile : public VolumeProfile
{
public:
    SessionVolumeProfile(ChartPane *pane, const QJsonObject &params);

    static QString periodFor(const QString &choice, double tfSeconds);
    struct Session { int from, to; };
    static QVector<Session> sessions(const Bars &bars, const QString &period,
                                     const std::function<QString(double, const QString &)> &keyOf);

    std::function<QString(double, const QString &)> keyOf;
    double tfSeconds = 0;

    void draw(QPainter &p, const QSizeF &size) override;

private:
    // Drawn at every frame (the crosshair moving): the sessions are kept while the bars' times are the same,
    // and the profiles of the closed ones (only the last session's bars change with ticks).
    struct {
        qsizetype count = -1;
        double first = 0, last = 0;
        QString period;
        QVector<Session> list;
    } m_sessions;
    QHash<int, std::optional<Profile>> m_closed;   // by first bar
    int m_closedRows = 0;
    double m_closedValueArea = 0;
};

} // namespace qtm
