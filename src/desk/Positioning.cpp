#include "Positioning.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace desk {

namespace {

// Numbers sometimes come as strings.
double number(const QJsonObject &o, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isDouble())
            return v.toDouble();
        if (v.isString()) {
            bool good = false;
            const double d = v.toString().toDouble(&good);
            if (good)
                return d;
        }
    }
    return 0;
}

} // namespace

Outlook parseOutlook(const QByteArray &json)
{
    Outlook out;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (root.isEmpty()) {
        out.message = QStringLiteral("not JSON");
        return out;
    }
    if (root.value("error").toBool()) {
        out.message = root.value("message").toString();
        out.sessionInvalid = out.message.contains("session", Qt::CaseInsensitive);
        return out;
    }
    const QJsonArray list = root.contains("symbols") ? root.value("symbols").toArray() : root.value("outlook").toArray();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        OutlookRow r;
        r.symbol = o.contains("name") ? o.value("name").toString() : o.value("symbol").toString();
        r.longPct = number(o, {"longPercentage", "longPercent"});
        r.shortPct = number(o, {"shortPercentage", "shortPercent"});
        r.longVolume = number(o, {"longVolume", "longVolumeLots"});
        r.shortVolume = number(o, {"shortVolume", "shortVolumeLots"});
        r.longPositions = int(number(o, {"longPositions"}));
        r.shortPositions = int(number(o, {"shortPositions"}));
        r.avgLongPrice = number(o, {"avgLongPrice", "avgLong"});
        r.avgShortPrice = number(o, {"avgShortPrice", "avgShort"});
        if (!r.symbol.isEmpty() && r.longPct + r.shortPct > 0)
            out.rows.push_back(r);
    }
    if (const QJsonObject general = root.value("general").toObject(); general.contains("demoAccountsPercentage"))
        out.demoPct = number(general, {"demoAccountsPercentage"});
    out.ok = !out.rows.isEmpty();
    if (!out.ok)
        out.message = QStringLiteral("no symbol in the answer");
    return out;
}

OutlookSummary summarizeOutlook(const QVector<OutlookRow> &rows, int minPositions)
{
    OutlookSummary s;
    double sum = 0;
    for (const OutlookRow &r : rows) {
        if (r.positions() < minPositions)
            continue;
        if (!s.counted || r.longPct > s.mostLong.longPct)
            s.mostLong = r;
        if (!s.counted || r.longPct < s.mostShort.longPct)
            s.mostShort = r;
        ++s.counted;
        sum += r.longPct;
        s.crowdedLong += r.longPct >= 70;
        s.crowdedShort += r.longPct <= 30;
    }
    if (s.counted)
        s.meanLong = sum / s.counted;
    return s;
}

QString parseLogin(const QByteArray &json, QString *message)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QString session = root.value("session").toString();
    if (root.value("error").toBool() || session.isEmpty()) {
        if (message)
            *message = root.value("message").toString();
        return {};
    }
    return session;
}

double crowdLongGain(const OutlookRow &row, double last)
{
    return row.avgLongPrice > 0 && last > 0 ? (last - row.avgLongPrice) / row.avgLongPrice * 100.0 : 0;
}

double crowdShortGain(const OutlookRow &row, double last)
{
    return row.avgShortPrice > 0 && last > 0 ? (row.avgShortPrice - last) / row.avgShortPrice * 100.0 : 0;
}

} // namespace desk
