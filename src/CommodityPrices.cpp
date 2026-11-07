#include "CommodityPrices.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTimeZone>

namespace {

// Yahoo's spark endpoint takes at most 20 symbols a request.
constexpr int SymbolsPerRequest = 20;

QDate utcDate(qint64 secs)
{
    return QDateTime::fromSecsSinceEpoch(secs, QTimeZone::UTC).date();
}

} // namespace

// ---------------------------------------------------------------- FuturesQuotes

FuturesQuotes::FuturesQuotes(const QVector<Contract> &contracts, QObject *parent)
    : RemoteDataSet("yahoo_commodity_futures", 5 * 60, parent)
    , m_contracts(contracts)
{
    m_transferTimeoutMs = 30 * 1000;
    loadCached();
}

QString FuturesQuotes::symbolFor(const Contract &contract, const QDate &today)
{
    if (contract.roll.isEmpty())
        return contract.symbol;
    static const QString Codes = "FGHJKMNQUVXZ";   // January to December
    const QDate from = QDate(today.year(), today.month(), 1).addMonths(2);
    for (int i = 0; i < 12; ++i) {
        const QDate month = from.addMonths(i);
        const QChar code = Codes.at(month.month() - 1);
        if (contract.roll.contains(code))
            return QString("%1%2%3%4").arg(contract.symbol, code, QString::number(month.year() % 100).rightJustified(2, '0'),
                                           contract.suffix);
    }
    return contract.symbol;
}

const FuturesQuotes::Quote *FuturesQuotes::quote(const QString &symbol) const
{
    auto it = m_quotes.constFind(symbol);
    return it == m_quotes.constEnd() ? nullptr : &*it;
}

qint64 FuturesQuotes::staleSecsNow() const
{
    return fresh::futuresEverySecs(QDateTime::currentDateTimeUtc());
}

QList<QUrl> FuturesQuotes::urls() const
{
    // Intraday over five days for the contracts that trade on Yahoo's feed (the chart), and over one day for their
    // previous close; daily closes for those quoted only as daily settlements. Yahoo's daily bars of a continuous
    // symbol are spliced from other contract months (rice's January contract at 17.90 next to daily closes of 17.43):
    // they can't give the previous close of the contract quoted.
    QStringList intraday, daily;
    for (const Contract &c : m_contracts)
        (c.dailyOnly ? daily : intraday) << symbolFor(c);
    QList<QUrl> out;
    auto add = [&out](const QStringList &symbols, const char *range, const char *interval) {
        for (int i = 0; i < symbols.size(); i += SymbolsPerRequest)
            out << QUrl(QString("https://query1.finance.yahoo.com/v7/finance/spark?symbols=%1&range=%2&interval=%3")
                            .arg(symbols.mid(i, SymbolsPerRequest).join(','), range, interval));
    };
    add(intraday, "5d", "30m");
    add(intraday, "1d", "30m");
    add(daily, "3mo", "1d");
    return out;
}

QByteArray FuturesQuotes::joinParts(const QList<QByteArray> &parts) const
{
    // A JSON array of the responses, as they came: each says its own interval.
    QByteArray out = "[";
    for (int i = 0; i < parts.size(); ++i)
        out += (i ? "," : "") + parts[i];
    return out + "]";
}

void FuturesQuotes::prepare(QNetworkRequest &request) const
{
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
}

bool FuturesQuotes::parse(const QByteArray &body)
{
    struct Raw {
        double metaPrice = 0;
        qint64 metaTime = 0;
        qint64 offset = 0;   // the exchange's offset from UTC, seconds
        double previousClose = 0;   // the contract's own, from the one-day request
        QString name;
        QVector<QPair<qint64, double>> intraday, daily;
    };
    QHash<QString, Raw> raw;
    const QJsonArray responses = QJsonDocument::fromJson(body).array();
    for (const QJsonValue &response : responses) {
        for (const QJsonValue &v : response.toObject().value("spark").toObject().value("result").toArray()) {
            const QJsonObject result = v.toObject();
            const QJsonObject series = result.value("response").toArray().at(0).toObject();
            const QJsonObject meta = series.value("meta").toObject();
            Raw &r = raw[result.value("symbol").toString()];
            if (meta.value("regularMarketTime").toInteger() > r.metaTime) {
                r.metaTime = meta.value("regularMarketTime").toInteger();
                r.metaPrice = meta.value("regularMarketPrice").toDouble();
            }
            r.offset = meta.value("gmtoffset").toInteger();
            if (r.name.isEmpty())
                r.name = meta.value("shortName").toString();
            const QJsonArray times = series.value("timestamp").toArray();
            const QJsonArray closes = series.value("indicators").toObject().value("quote").toArray().at(0)
                                          .toObject().value("close").toArray();
            if (meta.value("range").toString() == "1d") {
                r.previousClose = meta.value("chartPreviousClose").toDouble();
                continue;
            }
            auto &points = meta.value("dataGranularity").toString() == "1d" ? r.daily : r.intraday;
            for (int i = 0; i < times.size() && i < closes.size(); ++i)
                if (closes[i].isDouble() && closes[i].toDouble() > 0)
                    points.push_back({times[i].toInteger(), closes[i].toDouble()});
        }
    }

    QHash<QString, Quote> quotes;
    for (auto it = raw.cbegin(); it != raw.cend(); ++it) {
        const Raw &r = *it;
        // A futures trading day starts the evening before (CME at 18:00 New York): a trade belongs to the day
        // six hours later. Daily bars are stamped at midnight, exchange time, of their day.
        auto tradeDay = [&](qint64 t) { return utcDate(t + r.offset + 6 * 3600); };
        auto barDay = [&](qint64 t) { return utcDate(t + r.offset); };

        Quote q;
        q.name = r.name;
        qint64 time = 0;
        if (r.metaTime > 0 && r.metaPrice > 0) {
            time = r.metaTime;
            q.price = r.metaPrice;
        }
        if (!r.intraday.isEmpty() && r.intraday.last().first > time) {
            time = r.intraday.last().first;
            q.price = r.intraday.last().second;
        }
        // Yahoo's latest trade can be months old for contracts it only gets settlements of.
        int previous = int(r.daily.size()) - 1;
        if (!r.daily.isEmpty() && (time == 0 || barDay(r.daily.last().first) > tradeDay(time))) {
            time = r.daily.last().first;
            q.price = r.daily.last().second;
            q.settlement = true;
            previous = int(r.daily.size()) - 2;
        } else {
            while (previous >= 0 && barDay(r.daily[previous].first) >= tradeDay(time))
                --previous;
        }
        if (time == 0)
            continue;
        q.time = QDateTime::fromSecsSinceEpoch(time, QTimeZone::UTC);
        if (r.previousClose > 0) {
            // Its day: the last trading day before the price's, among the intraday bars.
            q.previousClose = r.previousClose;
            for (const auto &p : r.intraday)
                if (tradeDay(p.first) < tradeDay(time))
                    q.previousDate = tradeDay(p.first);
        } else if (previous >= 0) {
            q.previousClose = r.daily[previous].second;
            q.previousDate = barDay(r.daily[previous].first);
        }
        for (const auto &p : r.intraday)
            q.intraday.push_back({QDateTime::fromSecsSinceEpoch(p.first, QTimeZone::UTC), p.second});
        for (const auto &p : r.daily)
            q.daily.push_back({QDateTime::fromSecsSinceEpoch(p.first, QTimeZone::UTC), p.second});
        quotes.insert(it.key(), q);
    }
    if (quotes.isEmpty()) {
        m_error = tr("No futures quotes in Yahoo's answer");
        return false;
    }
    m_quotes = quotes;
    return true;
}

// ---------------------------------------------------------------- CommodityBenchmarks

CommodityBenchmarks::CommodityBenchmarks(const QStringList &codes, QObject *parent)
    : RemoteDataSet("imf_pcps", 24 * 3600, parent)
    , m_codes(codes)
{
    loadCached();
}

QUrl CommodityBenchmarks::url() const
{
    // Three years of monthly averages in US dollars, without the attributes (16 times smaller).
    const QDate start = QDate::currentDate().addYears(-3);
    return QUrl(QString("https://api.imf.org/external/sdmx/2.1/data/IMF.RES,PCPS/G001.%1.USD.M"
                        "?startPeriod=%2-%3&detail=dataonly")
                    .arg(m_codes.join('+')).arg(start.year()).arg(start.month(), 2, 10, QChar('0')));
}

void CommodityBenchmarks::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Accept", "application/vnd.sdmx.data+csv;version=1.0.0");
}

bool CommodityBenchmarks::parse(const QByteArray &body)
{
    const QList<QByteArray> lines = body.split('\n');
    const QList<QByteArray> header = splitCsvLine(lines.value(0).trimmed());
    const int codeCol = int(header.indexOf("INDICATOR"));
    const int periodCol = int(header.indexOf("TIME_PERIOD"));
    const int valueCol = int(header.indexOf("OBS_VALUE"));
    if (codeCol < 0 || periodCol < 0 || valueCol < 0) {
        m_error = tr("Unexpected IMF commodity price format");
        return false;
    }
    QHash<QString, QMap<QDate, double>> series;
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> f = splitCsvLine(lines[i].trimmed());
        if (f.size() <= std::max({codeCol, periodCol, valueCol}))
            continue;
        // "2026-M09"
        const QDate month(f[periodCol].left(4).toInt(), f[periodCol].mid(6).toInt(), 1);
        bool ok = false;
        const double value = f[valueCol].toDouble(&ok);
        if (month.isValid() && ok && value > 0)
            series[QString::fromLatin1(f[codeCol])].insert(month, value);
    }
    if (series.isEmpty()) {
        m_error = tr("No IMF commodity prices");
        return false;
    }
    m_series = series;
    return true;
}
