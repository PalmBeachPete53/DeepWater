#include "VolumeProfile.h"

#include "ChartPane.h"
#include "Text.h"
#include "Theme.h"

#include <QFontMetricsF>
#include <QPainter>

#include <cmath>
#include <limits>

namespace qtm {

namespace {
// JS `x | 0`.
int toInt32(double v)
{
    if (!std::isfinite(v)) return 0;
    return int(qint64(std::trunc(v)));
}
} // namespace

// Volume profile for the visible range (TradingView's VPVR). Volume per price row for the bars on
// screen, split into up and down volume, with the point of control (busiest row) and the value
// area (the rows around it holding 70% of the volume). Redraws as you scroll or zoom.
VolumeProfile::VolumeProfile(ChartPane *pane, const QJsonObject &params) : m_pane(pane)
{
    m_p = QJsonObject{{"rows", 24}, {"width", 30}, {"valueArea", 70}};
    for (auto it = params.begin(); it != params.end(); ++it)
        if (!it.value().isUndefined()) m_p[it.key()] = it.value();
}

double VolumeProfile::num(const QString &key) const
{
    const QJsonValue v = m_p.value(key);
    if (v.isDouble()) return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        return ok ? d : NaN;
    }
    return NaN;
}

std::optional<VolumeProfile::Profile> VolumeProfile::profile(const Bars &bars, int rows, double valueAreaPct)
{
    double hi = -INFINITY, lo = INFINITY;
    for (const Bar &b : bars) {
        hi = std::max(hi, b.high);
        lo = std::min(lo, b.low);
    }
    if (!std::isfinite(hi) || hi <= lo) return std::nullopt;
    Profile p;
    p.lo = lo;
    p.step = (hi - lo) / rows;
    p.up = QVector<double>(rows, 0);
    p.down = QVector<double>(rows, 0);
    for (const Bar &b : bars) {
        const double v = b.volume;
        if (!v || std::isnan(v)) continue;
        const int a = std::min(rows - 1, int(std::floor((b.low - lo) / p.step)));
        const int z = std::min(rows - 1, int(std::floor((b.high - lo) / p.step)));
        const double share = v / (z - a + 1);
        for (int k = a; k <= z; ++k) (b.close >= b.open ? p.up : p.down)[k] += share;
    }
    p.total.resize(rows);
    double sum = 0;
    for (int k = 0; k < rows; ++k) {
        p.total[k] = p.up[k] + p.down[k];
        sum += p.total[k];
    }
    if (!sum) return std::nullopt;
    p.poc = 0;
    for (int k = 0; k < rows; ++k)
        if (p.total[k] > p.total[p.poc]) p.poc = k;
    double inside = p.total[p.poc];
    int a = p.poc, z = p.poc;
    while (inside < (sum * valueAreaPct) / 100 && (a > 0 || z < rows - 1)) {
        const double below = a > 0 ? p.total[a - 1] : -1, above = z < rows - 1 ? p.total[z + 1] : -1;
        if (above >= below) inside += p.total[++z];
        else inside += p.total[--a];
    }
    p.vaLow = a;
    p.vaHigh = z;
    p.max = -INFINITY;
    for (double t : p.total) p.max = std::max(p.max, t);
    return p;
}

void VolumeProfile::draw(QPainter &ctx, const QSizeF &size)
{
    const Bars &bars = m_pane->shown;
    const auto range = m_pane->chart->timeScale()->getVisibleLogicalRange();
    if (bars.isEmpty() || !range || !m_series) return;
    const int from = std::max(0, int(std::floor(range->from)));
    const int to = std::min(int(bars.size()) - 1, int(std::ceil(range->to)));
    if (to <= from) return;
    const auto prof = profile(bars.mid(from, to - from + 1), std::max(4, std::min(200, toInt32(num("rows")))),
                              num("valueArea"));
    if (!prof) return;
    const double W = (size.width() * std::max(5.0, std::min(80.0, num("width")))) / 100;
    const double x0 = size.width() - W;
    for (int k = 0; k < prof->total.size(); ++k) {
        const auto yTop = m_series->priceToCoordinate(prof->lo + (k + 1) * prof->step);
        const auto yBot = m_series->priceToCoordinate(prof->lo + k * prof->step);
        if (!yTop || !yBot) continue;
        const double h = std::max(1.0, *yBot - *yTop - 1);
        const bool inVa = k >= prof->vaLow && k <= prof->vaHigh;
        const double uw = (prof->up[k] / prof->max) * W, dw = (prof->down[k] / prof->max) * W;
        ctx.fillRect(QRectF(x0 + W - uw - dw, *yTop, uw, h).normalized(),
                     theme::color("--up", inVa ? 0.45 : 0.2));
        ctx.fillRect(QRectF(x0 + W - dw, *yTop, dw, h).normalized(),
                     theme::color("--down", inVa ? 0.45 : 0.2));
    }
    const auto yPoc = m_series->priceToCoordinate(prof->lo + (prof->poc + 0.5) * prof->step);
    if (yPoc) {
        QPen pen(QColor("#ff9800"));
        pen.setWidthF(1);
        pen.setCapStyle(Qt::FlatCap);
        pen.setDashPattern({4, 3});
        ctx.setPen(pen);
        ctx.drawLine(QPointF(0, *yPoc), QPointF(size.width(), *yPoc));
        const QFont f = theme::font(10);
        const double w = text::width(f, "POC");
        text::draw(ctx, QPointF(size.width() - 4 - w, *yPoc - 4), f, "POC", QColor("#ff9800"));
    }
}

// Session volume profile (TradingView's SVP): one profile per session, drawn from the session's first bar, using only
// that session's bars. Sessions are days on intraday charts; on daily and slower charts "Auto" uses months, then years,
// so each profile still has enough bars to mean something.
SessionVolumeProfile::SessionVolumeProfile(ChartPane *pane, const QJsonObject &params)
    : VolumeProfile(pane, QJsonObject())
{
    m_p = QJsonObject{{"period", "Auto"}, {"rows", 24}, {"width", 70}, {"valueArea", 70}};
    for (auto it = params.begin(); it != params.end(); ++it)
        if (!it.value().isUndefined()) m_p[it.key()] = it.value();
}

QString SessionVolumeProfile::periodFor(const QString &choice, double tfSeconds)
{
    if (!choice.isEmpty() && choice != "Auto") return choice.toLower();
    if (tfSeconds < 86400) return "day";
    if (tfSeconds < 7 * 86400) return "month";
    return "year";
}

QVector<SessionVolumeProfile::Session> SessionVolumeProfile::sessions(
    const Bars &bars, const QString &period, const std::function<QString(double, const QString &)> &keyOf)
{
    QVector<Session> out;
    QString key;
    bool first = true;
    for (int i = 0; i < bars.size(); ++i) {
        const QString k = keyOf(bars[i].time, period);
        if (first || k != key) {
            out.push_back({i, i});
            key = k;
            first = false;
        } else {
            out.last().to = i;
        }
    }
    return out;
}

void SessionVolumeProfile::draw(QPainter &ctx, const QSizeF &)
{
    const Bars &bars = m_pane->shown;
    lw::TimeScale *ts = m_pane->chart->timeScale();
    const auto range = ts->getVisibleLogicalRange();
    if (bars.isEmpty() || !range || !m_series || !keyOf) return;
    const QString period = periodFor(m_p.value("period").toString(), tfSeconds ? tfSeconds : 86400);
    const int rows = std::max(4, std::min(200, toInt32(num("rows"))));
    double spacing = m_pane->chart->options().barSpacing;
    if (!spacing) spacing = 6;
    if (m_sessions.count != bars.size() || m_sessions.first != bars.first().time || m_sessions.last != bars.last().time
        || m_sessions.period != period) {
        m_sessions = {bars.size(), bars.first().time, bars.last().time, period, sessions(bars, period, keyOf)};
        m_closed.clear();
    }
    const double valueArea = num("valueArea");
    if (rows != m_closedRows || valueArea != m_closedValueArea) {
        m_closed.clear();
        m_closedRows = rows;
        m_closedValueArea = valueArea;
    }
    const QColor upIn = theme::color("--up", 0.4), upOut = theme::color("--up", 0.16);
    const QColor downIn = theme::color("--down", 0.4), downOut = theme::color("--down", 0.16);
    for (const Session &s : std::as_const(m_sessions.list)) {
        if (s.to < range->from - 1 || s.from > range->to + 1) continue;
        std::optional<Profile> prof;
        const bool closed = s.to < bars.size() - 1;
        if (const auto known = m_closed.constFind(s.from); closed && known != m_closed.constEnd()) {
            prof = *known;
        } else {
            prof = profile(bars.mid(s.from, s.to - s.from + 1), rows, valueArea);
            if (closed)
                m_closed.insert(s.from, prof);
        }
        const auto xa = ts->logicalToCoordinate(s.from), xz = ts->logicalToCoordinate(s.to);
        if (!prof || !xa || !xz) continue;
        const double x0 = *xa - spacing / 2, span = std::max(spacing, *xz - *xa + spacing);
        const double W = (span * std::max(5.0, std::min(100.0, num("width")))) / 100;
        for (int k = 0; k < prof->total.size(); ++k) {
            const auto yTop = m_series->priceToCoordinate(prof->lo + (k + 1) * prof->step);
            const auto yBot = m_series->priceToCoordinate(prof->lo + k * prof->step);
            if (!yTop || !yBot) continue;
            const double h = std::max(1.0, *yBot - *yTop - 1);
            const bool inVa = k >= prof->vaLow && k <= prof->vaHigh;
            const double uw = (prof->up[k] / prof->max) * W, dw = (prof->down[k] / prof->max) * W;
            ctx.fillRect(QRectF(x0, *yTop, uw, h).normalized(), inVa ? upIn : upOut);
            ctx.fillRect(QRectF(x0 + uw, *yTop, dw, h).normalized(), inVa ? downIn : downOut);
        }
        const auto yPoc = m_series->priceToCoordinate(prof->lo + (prof->poc + 0.5) * prof->step);
        if (yPoc) {
            QPen pen(QColor("#ff9800"));
            pen.setWidthF(1.5);
            pen.setCapStyle(Qt::FlatCap);
            ctx.setPen(pen);
            ctx.drawLine(QPointF(x0, *yPoc), QPointF(x0 + span, *yPoc));
        }
    }
}

} // namespace qtm
