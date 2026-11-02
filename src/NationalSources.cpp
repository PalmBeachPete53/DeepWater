#include "NationalSources.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkRequest>

#include <cmath>

namespace {

const qint64 TwiceADay = 12 * 3600;

// "2026-09-30" / "2026-09-01" -> mid-month time.
double monthTime(const QString &isoDate)
{
    const QDate d = QDate::fromString(isoDate.left(10), Qt::ISODate);
    return d.isValid() ? d.year() + (d.month() - 0.5) / 12.0 : 0;
}

} // namespace

// ---------------------------------------------------------------- BrazilIndicators

BrazilIndicators::BrazilIndicators(QObject *parent)
    : CountrySeriesSet("national_brazil", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> BrazilIndicators::urls() const
{
    return {
        // IPCA, % over twelve months (SGS series 13522), the last 15 months.
        QUrl("https://api.bcb.gov.br/dados/serie/bcdata.sgs.13522/dados/ultimos/15?formato=json"),
        // PNAD Contínua: unemployment rate, rolling quarters (table 6381, variable 4099).
        QUrl("https://apisidra.ibge.gov.br/values/t/6381/n1/all/v/4099/p/last%2015"),
        // PIM-PF: industrial production, % on the same month a year before, whole industry (table 8888).
        QUrl("https://apisidra.ibge.gov.br/values/t/8888/n1/all/v/11602/p/last%2015/c544/129314"),
        // PMC: retail trade volume, % on the same month a year before (table 8880).
        QUrl("https://apisidra.ibge.gov.br/values/t/8880/n1/all/v/11709/p/last%2015/c11046/56734"),
    };
}

QByteArray BrazilIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    // SGS: [{"data":"01/09/2026","valor":"4.58"}].
    for (const QJsonValue &v : QJsonDocument::fromJson(parts.value(0)).array()) {
        const QJsonObject o = v.toObject();
        const QDate d = QDate::fromString(o.value("data").toString(), "dd/MM/yyyy");
        bool ok = false;
        const double value = o.value("valor").toString().toDouble(&ok);
        if (d.isValid() && ok)
            addLine(out, "BRA", CpiYoY, d.year() + (d.month() - 0.5) / 12.0, value);
    }
    // SIDRA: a header object, then rows with the period code D3C ("202609") and the value V.
    for (int k = 1; k < parts.size() && k <= RetailYoY; ++k) {
        const QJsonArray rows = QJsonDocument::fromJson(parts[k]).array();
        for (int i = 1; i < rows.size(); ++i) {
            const QJsonObject o = rows[i].toObject();
            const QString period = o.value("D3C").toString();
            bool ok = false;
            const double value = o.value("V").toString().toDouble(&ok);
            if (period.size() == 6 && ok)
                addLine(out, "BRA", k, period.left(4).toInt() + (period.mid(4).toInt() - 0.5) / 12.0, value);
        }
    }
    return out;
}

// ---------------------------------------------------------------- ArgentinaIndicators

ArgentinaIndicators::ArgentinaIndicators(QObject *parent)
    : CountrySeriesSet("national_argentina", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> ArgentinaIndicators::urls() const
{
    // Index levels over 27 months (the % over a year is computed here: the API's own transformation lags).
    const QString base = "https://apis.datos.gob.ar/series/api/series/?sort=desc&format=json&ids=";
    return {
        QUrl(base + "148.3_INIVELNAL_DICI_M_26&limit=27"),   // CPI, national, index
        QUrl(base + "143.3_NO_PR_2004_A_21&limit=27"),       // EMAE, index
        QUrl(base + "42.3_EPH_PUNTUATAL_0_M_30&limit=8"),    // unemployment rate, quarterly
    };
}

QByteArray ArgentinaIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    // {"data": [["2026-08-01", 0.335], …]}: rates as fractions; quarters dated from their first month.
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k) {
        const QJsonObject root = QJsonDocument::fromJson(parts[k]).object();
        const bool quarterly = root.value("meta").toArray().at(0).toObject().value("frequency").toString() == "quarter";
        QMap<double, double> values;
        for (const QJsonValue &v : root.value("data").toArray()) {
            const QJsonArray row = v.toArray();
            const QDate d = QDate::fromString(row.at(0).toString(), Qt::ISODate);
            if (!d.isValid() || !row.at(1).isDouble())
                continue;
            const double time = quarterly ? d.year() + ((d.month() - 1) / 3 + 0.5) / 4.0 : d.year() + (d.month() - 0.5) / 12.0;
            values.insert(time, row.at(1).toDouble());
        }
        if (k == Unemployment) {
            for (auto it = values.begin(); it != values.end(); ++it)
                addLine(out, "ARG", k, it.key(), 100 * it.value());   // a fraction
        } else {
            // % over a year from the index.
            for (auto it = values.begin(); it != values.end(); ++it)
                for (auto p = values.begin(); p != values.end(); ++p)
                    if (std::abs(p.key() - (it.key() - 1)) < 1e-6 && p.value() > 0) {
                        addLine(out, "ARG", k, it.key(), 100 * (it.value() / p.value() - 1));
                        break;
                    }
        }
    }
    return out;
}

// ---------------------------------------------------------------- SouthAfricaIndicators

SouthAfricaIndicators::SouthAfricaIndicators(QObject *parent)
    : CountrySeriesSet("national_south_africa", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> SouthAfricaIndicators::urls() const
{
    // The latest value of each headline indicator (no history: the previous value comes from the cache).
    return {QUrl("https://custom.resbank.co.za/SarbWebApi/WebIndicators/HomePageRates")};
}

void SouthAfricaIndicators::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Accept", "application/json");   // XML otherwise
}

QByteArray SouthAfricaIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    // Keep what the previous downloads brought: this answer only carries the latest month.
    for (int k = 0; k < SeriesCount; ++k) {
        const QMap<double, double> s = series("ZAF", k);
        for (auto it = s.begin(); it != s.end(); ++it)
            addLine(out, "ZAF", k, it.key(), it.value());
    }
    for (const QJsonValue &v : QJsonDocument::fromJson(parts.value(0)).array()) {
        const QJsonObject o = v.toObject();
        const QString code = o.value("TimeseriesCode").toString();
        const int k = code == "CPI1000F" ? CpiYoY : code == "PPI1000F" ? PpiYoY : code == "MMRD002A" ? PolicyRate : -1;
        if (k < 0 || !o.value("Value").isDouble())
            continue;
        addLine(out, "ZAF", k, monthTime(o.value("Date").toString()), o.value("Value").toDouble());
    }
    return out;
}

// ---------------------------------------------------------------- SingaporeIndicators

SingaporeIndicators::SingaporeIndicators(QObject *parent)
    : CountrySeriesSet("national_singapore", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> SingaporeIndicators::urls() const
{
    // Percent change in the CPI over the same month a year before, all items (table M213781, row 1).
    return {QUrl("https://tablebuilder.singstat.gov.sg/api/table/tabledata/M213781?seriesNoORrowNo=1&limit=1&sortBy=key%20desc")};
}

void SingaporeIndicators::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Accept", "application/json");
}

QByteArray SingaporeIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (int k = 0; k < SeriesCount; ++k) {
        const QMap<double, double> s = series("SGP", k);
        for (auto it = s.begin(); it != s.end(); ++it)
            addLine(out, "SGP", k, it.key(), it.value());
    }
    const QJsonArray rows = QJsonDocument::fromJson(parts.value(0)).object().value("Data").toObject().value("row").toArray();
    for (const QJsonValue &v : rows) {
        const QJsonObject row = v.toObject();
        if (row.value("rowText").toString() != "All Items")
            continue;
        for (const QJsonValue &c : row.value("columns").toArray()) {
            const QJsonObject col = c.toObject();
            // "2026 Sep" (English month names whatever the locale)
            const QDate d = QLocale(QLocale::English).toDate(col.value("key").toString(), "yyyy MMM");
            bool ok = false;
            const double value = col.value("value").toString().toDouble(&ok);
            if (d.isValid() && ok)
                addLine(out, "SGP", CpiYoY, d.year() + (d.month() - 0.5) / 12.0, value);
        }
    }
    return out;
}
