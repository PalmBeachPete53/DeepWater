#include "CalendarData.h"

#include <QHash>
#include <QPointer>
#include <QSet>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

namespace {

double jsonNumber(const QJsonValue &v)
{
    return v.isDouble() ? v.toDouble() : qQNaN();
}

// Nasdaq's figures are strings: "$1.89", "($0.12)", "-5.97", "$2,629,595,300,000", "N/A".
double nasdaqNumber(QString s)
{
    s = s.trimmed();
    const bool negative = s.startsWith('(') || s.startsWith('-');
    // Keep the digits and the point (what removing [^0-9.] did; a regex per call was most of the parse).
    s.removeIf([](QChar c) { return !((c >= u'0' && c <= u'9') || c == u'.'); });
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? (negative ? -v : v) : qQNaN();
}

QDate nasdaqDate(const QString &s)
{
    return QDate::fromString(s.trimmed(), "MM/dd/yyyy");
}

QDate addWeekdays(QDate d, int n)
{
    const int step = n < 0 ? -1 : 1;
    while (n != 0) {
        d = d.addDays(step);
        if (d.dayOfWeek() <= 5)
            n -= step;
    }
    return d;
}

} // namespace

// ---------------------------------------------------------------- EconomicCalendar

EconomicCalendar::EconomicCalendar(QObject *parent)
    : RemoteDataSet("tradingview_economic_calendar", 30 * 60, parent)   // 800 KB for all of it; ReleaseWatcher asks about what's due
{
    loadCached();
}

QStringList EconomicCalendar::countries()
{
    return {"US", "EU", "GB", "DE", "FR", "IT", "ES", "CH", "SE", "NO", "JP", "CN", "IN", "KR",
            "AU", "NZ", "CA", "MX", "BR", "ZA", "TR", "RU"};
}

void EconomicCalendar::window(QDate *from, QDate *to)
{
    // Monday of last week to the Monday after next week: last week, yesterday, today, tomorrow, this week and
    // next week, whatever the day.
    const QDate today = QDate::currentDate();
    *from = today.addDays(1 - today.dayOfWeek() - 7);
    *to = from->addDays(21);
}

QUrl EconomicCalendar::rangeUrl(const QDate &from, const QDate &to)
{
    auto iso = [](const QDate &d) {
        return d.startOfDay().toUTC().toString("yyyy-MM-ddTHH:mm:ss.000Z");
    };
    // A fake server for the tests, on this computer only (as DEEPWATER_METACOPIER_URL).
    const QUrl test(qEnvironmentVariable("DEEPWATER_CALENDAR_URL"));
    QUrl url = test.isValid() && (test.host() == "127.0.0.1" || test.host() == "localhost")
                   ? test
                   : QUrl("https://economic-calendar.tradingview.com/events");
    QUrlQuery q;
    q.addQueryItem("from", iso(from));
    q.addQueryItem("to", iso(to));
    q.addQueryItem("countries", countries().join(','));
    url.setQuery(q);
    return url;
}

QUrl EconomicCalendar::url() const
{
    QDate from, to;
    window(&from, &to);
    return rangeUrl(from, to);
}

void EconomicCalendar::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Origin", "https://www.tradingview.com");
}

QVector<EconomicCalendar::Event> EconomicCalendar::parseEvents(const QJsonArray &result)
{
    QVector<Event> out;
    out.reserve(result.size());
    for (const QJsonValue &v : result) {
        const QJsonObject o = v.toObject();
        Event e;
        e.time = QDateTime::fromString(o.value("date").toString(), Qt::ISODateWithMs);
        if (!e.time.isValid())
            continue;
        e.time = e.time.toUTC();
        e.country = o.value("country").toString();
        e.title = o.value("title").toString().trimmed();
        e.period = o.value("period").toString().trimmed();
        e.impact = std::clamp(o.value("importance").toInt() + 1, 0, 2);
        e.actual = jsonNumber(o.value("actual"));
        e.forecast = jsonNumber(o.value("forecast"));
        e.previous = jsonNumber(o.value("previous"));
        e.unit = o.value("unit").toString();
        e.scale = o.value("scale").toString();
        e.source = o.value("source").toString();
        e.sourceUrl = o.value("source_url").toString();
        out.push_back(e);
    }
    std::stable_sort(out.begin(), out.end(), [](const Event &a, const Event &b) { return a.time < b.time; });
    return out;
}

bool EconomicCalendar::parse(const QByteArray &body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    if (root.contains("compact")) {
        // Our own cached form: the window, then one array per event.
        QVector<Event> events;
        for (const QJsonValue &v : root.value("events").toArray()) {
            const QJsonArray a = v.toArray();
            Event e;
            e.time = QDateTime::fromSecsSinceEpoch(a.at(0).toInteger(), QTimeZone::UTC);
            e.country = a.at(1).toString();
            e.title = a.at(2).toString();
            e.period = a.at(3).toString();
            e.impact = a.at(4).toInt();
            e.actual = jsonNumber(a.at(5));
            e.forecast = jsonNumber(a.at(6));
            e.previous = jsonNumber(a.at(7));
            e.unit = a.at(8).toString();
            e.scale = a.at(9).toString();
            e.source = a.at(10).toString();
            events.push_back(e);
        }
        m_from = QDate::fromString(root.value("from").toString(), Qt::ISODate);
        m_to = QDate::fromString(root.value("to").toString(), Qt::ISODate);
        m_events = events;
        return m_from.isValid();
    }
    if (root.value("status").toString() != "ok") {
        m_error = root.value("errmsg").toString(tr("unexpected answer from TradingView"));
        return false;
    }
    m_events = parseEvents(root.value("result").toArray());
    window(&m_from, &m_to);
    return true;
}

QByteArray EconomicCalendar::cacheBody(const QByteArray &) const
{
    return compact(m_events, m_from, m_to);
}

QByteArray EconomicCalendar::compact(const QVector<Event> &list, const QDate &from, const QDate &to)
{
    auto num = [](double v) { return std::isnan(v) ? QJsonValue() : QJsonValue(v); };
    QJsonArray events;
    for (const Event &e : list)
        events.append(QJsonArray {e.time.toSecsSinceEpoch(), e.country, e.title, e.period, e.impact,
                                  num(e.actual), num(e.forecast), num(e.previous), e.unit, e.scale, e.source});
    return QJsonDocument(QJsonObject {{"compact", 1},
                                      {"from", from.toString(Qt::ISODate)},
                                      {"to", to.toString(Qt::ISODate)},
                                      {"events", events}})
        .toJson(QJsonDocument::Compact);
}

RemoteDataSet::Parser EconomicCalendar::parser()
{
    return [this](const QList<QByteArray> &parts) {
        const QJsonObject root = QJsonDocument::fromJson(parts.value(0)).object();
        if (root.value("status").toString() != "ok") {
            const QString error = root.value("errmsg").toString(tr("unexpected answer from TradingView"));
            return Parsed {[this, error] {
                               m_error = error;
                               return false;
                           },
                           {}};
        }
        const QVector<Event> events = parseEvents(root.value("result").toArray());
        QDate from, to;
        window(&from, &to);
        return Parsed {[this, events, from, to] {
                           m_events = events;
                           m_from = from;
                           m_to = to;
                           return true;
                       },
                       compact(events, from, to)};
    };
}

void EconomicCalendar::fetchDay(const QDate &day)
{
    QNetworkRequest request(rangeUrl(day, day.addDays(1)));
    request.setTransferTimeout(m_transferTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    prepare(request);
    QNetworkReply *reply = m_dayNet.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, day] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit dayFetched(day, {}, reply->errorString());
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (root.value("status").toString() != "ok") {
            emit dayFetched(day, {}, tr("unexpected answer from TradingView"));
            return;
        }
        emit dayFetched(day, parseEvents(root.value("result").toArray()), {});
    });
}

void EconomicCalendar::fetchLive(const QStringList &countries, QObject *ctx, std::function<void()> done)
{
    const QDate today = QDate::currentDate();
    QUrl url = rangeUrl(today.addDays(-1), today.addDays(2));
    QUrlQuery q(url);
    q.removeAllQueryItems("countries");
    q.addQueryItem("countries", countries.join(','));
    url.setQuery(q);
    QNetworkRequest request(url);
    request.setTransferTimeout(8000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    prepare(request);
    QNetworkReply *reply = m_dayNet.get(request);
    QPointer<QObject> guard(ctx);
    connect(reply, &QNetworkReply::finished, this, [this, reply, guard, done] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            if (root.value("status").toString() == "ok") {
                const auto keyOf = [](const Event &e) { return QString::number(e.time.toSecsSinceEpoch()) + '|' + e.country + '|' + e.title; };
                QHash<QString, int> index;
                for (int i = 0; i < m_events.size(); ++i)
                    index.insert(keyOf(m_events[i]), i);
                bool changed = false;
                const auto same = [](double a, double b) { return (std::isnan(a) && std::isnan(b)) || a == b; };
                for (const Event &e : parseEvents(root.value("result").toArray())) {
                    const auto it = index.constFind(keyOf(e));
                    if (it == index.constEnd())
                        continue;
                    Event &mine = m_events[*it];
                    if (same(mine.actual, e.actual) && same(mine.forecast, e.forecast))
                        continue;
                    mine.actual = e.actual;
                    mine.forecast = e.forecast;
                    mine.previous = e.previous;
                    changed = true;
                }
                if (changed)
                    emit updated();
            }
        }
        if (guard)
            done();
    });
}

void EconomicCalendar::fetchRange(const QDate &from, const QDate &to, QObject *ctx,
                                  std::function<void(const QVector<EconomicCalendar::Event> &, const QString &)> done)
{
    fetchRange(QString(), from, to, ctx, std::move(done));
}

void EconomicCalendar::fetchRange(const QString &country, const QDate &from, const QDate &to, QObject *ctx,
                                  std::function<void(const QVector<EconomicCalendar::Event> &, const QString &)> done)
{
    fetchPage(country, from, to, ctx, std::move(done), {}, 0);
}

// TradingView answers at most 2000 events, the earliest of the range: a longer range takes several requests, each
// from the day the last answer ended on (its events come twice and are dropped).
void EconomicCalendar::fetchPage(const QString &country, const QDate &from, const QDate &to, QObject *ctx,
                                 std::function<void(const QVector<EconomicCalendar::Event> &, const QString &)> done,
                                 QVector<EconomicCalendar::Event> soFar, int pages)
{
    static constexpr int PageLimit = 2000, MaxPages = 15;
    QUrl url = rangeUrl(from, to);
    if (!country.isEmpty()) {
        QUrlQuery q(url);
        q.removeQueryItem("countries");
        q.addQueryItem("countries", country);
        url.setQuery(q);
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(60 * 1000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    prepare(request);
    QNetworkReply *reply = m_dayNet.get(request);
    connect(reply, &QNetworkReply::finished, ctx, [this, reply, country, from, to, ctx, done = std::move(done), soFar = std::move(soFar), pages]() mutable {
        reply->deleteLater();
        // A later page that fails leaves what came before: most of the range beats nothing.
        if (reply->error() != QNetworkReply::NoError) {
            done(soFar, reply->errorString());
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (root.value("status").toString() != "ok") {
            done(soFar, tr("unexpected answer from TradingView"));
            return;
        }
        const QVector<Event> page = parseEvents(root.value("result").toArray());
        QSet<QString> seen;
        for (const Event &e : std::as_const(soFar))
            seen.insert(e.country + e.title + e.period + QString::number(e.time.toSecsSinceEpoch()));
        for (const Event &e : page)
            if (!seen.contains(e.country + e.title + e.period + QString::number(e.time.toSecsSinceEpoch())))
                soFar.push_back(e);
        QDate last;
        for (const Event &e : page)
            if (!last.isValid() || e.time.toLocalTime().date() > last)
                last = e.time.toLocalTime().date();
        if (page.size() < PageLimit || pages + 1 >= MaxPages || !last.isValid() || last >= to) {
            done(soFar, {});
            return;
        }
        fetchPage(country, last > from ? last : from.addDays(1), to, ctx, std::move(done), std::move(soFar), pages + 1);
    });
}

// ---------------------------------------------------------------- CorporateCalendar

CorporateCalendar::CorporateCalendar(QObject *parent)
    : RemoteDataSet("nasdaq_corporate_calendar", 3 * 3600, parent)
{
    m_transferTimeoutMs = 30 * 1000;
    loadCached();
}

QList<QUrl> CorporateCalendar::urls() const
{
    const QString api = "https://api.nasdaq.com/api/";
    QList<QUrl> out;
    QDate today = QDate::currentDate();
    // Earnings: five weekdays back (the recent results) to five ahead; today counts as a weekday on weekends.
    for (QDate d = addWeekdays(today, -5); d <= addWeekdays(today, 5); d = d.addDays(1))
        if (d.dayOfWeek() <= 5 || d == today)
            out << QUrl(api + "calendar/earnings?date=" + d.toString(Qt::ISODate));
    for (QDate d = today, end = addWeekdays(today, 7); d <= end; d = d.addDays(1))
        if (d.dayOfWeek() <= 5)
            out << QUrl(api + "calendar/dividends?date=" + d.toString(Qt::ISODate));
    out << QUrl(api + "calendar/splits");
    for (int m = -1; m <= 1; ++m)
        out << QUrl(api + "ipo/calendar?date=" + today.addMonths(m).toString("yyyy-MM"));
    return out;
}

QByteArray CorporateCalendar::joinParts(const QList<QByteArray> &parts) const
{
    return QJsonDocument(join(parts, urls())).toJson(QJsonDocument::Compact);
}

QJsonArray CorporateCalendar::join(const QList<QByteArray> &parts, const QList<QUrl> &from)
{
    // Each answer with the URL it came from (the kind and the day of the data).
    QJsonArray out;
    for (int i = 0; i < parts.size(); ++i)
        out.append(QJsonObject {{"url", from.value(i).toString()},
                                {"body", QJsonDocument::fromJson(parts[i]).object()}});
    return out;
}

RemoteDataSet::Parser CorporateCalendar::parser()
{
    // Joined, parsed and serialized for the cache once, on the thread pool (the parts used to be parsed,
    // written out joined, then parsed again, on the UI thread).
    return [this, from = urls()](const QList<QByteArray> &parts) {
        const QJsonArray joined = join(parts, from);
        Data data;
        if (!read(joined, &data))
            return Parsed {[this] {
                               m_error = tr("unexpected answer from Nasdaq");
                               return false;
                           },
                           {}};
        return Parsed {[this, data] {
                           m_earnings = data.earnings;
                           m_dividends = data.dividends;
                           m_splits = data.splits;
                           m_ipos = data.ipos;
                           m_fetchedAtLeastOnce = true;
                           return true;
                       },
                       QJsonDocument(joined).toJson(QJsonDocument::Compact)};
    };
}

void CorporateCalendar::prepare(QNetworkRequest &request) const
{
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Origin", "https://www.nasdaq.com");
    request.setRawHeader("Referer", "https://www.nasdaq.com/");
}

bool CorporateCalendar::read(const QJsonArray &parts, Data *data)
{
    if (parts.isEmpty())
        return false;
    QVector<Earnings> earnings;
    QVector<Dividend> dividends;
    QVector<Split> splits;
    QVector<Ipo> ipos;
    for (const QJsonValue &pv : parts) {
        const QUrl url(pv.toObject().value("url").toString());
        const QJsonObject data = pv.toObject().value("body").toObject().value("data").toObject();
        const QString path = url.path();
        if (path.endsWith("/calendar/earnings")) {
            const QDate day = QDate::fromString(QUrlQuery(url).queryItemValue("date"), Qt::ISODate);
            for (const QJsonValue &rv : data.value("rows").toArray()) {
                const QJsonObject r = rv.toObject();
                Earnings e;
                e.date = day;
                e.symbol = r.value("symbol").toString().trimmed();
                e.name = r.value("name").toString().trimmed();
                const QString time = r.value("time").toString();
                e.time = time == "time-pre-market" ? "pre" : time == "time-after-hours" ? "after" : QString();
                e.eps = nasdaqNumber(r.value("eps").toString());
                e.estimate = nasdaqNumber(r.value("epsForecast").toString());
                e.surprisePct = nasdaqNumber(r.value("surprise").toString());
                e.lastYearEps = nasdaqNumber(r.value("lastYearEPS").toString());
                e.marketCap = nasdaqNumber(r.value("marketCap").toString());
                e.estimates = r.value("noOfEsts").toString().toInt();
                if (!e.symbol.isEmpty())
                    earnings.push_back(e);
            }
        } else if (path.endsWith("/calendar/dividends")) {
            for (const QJsonValue &rv : data.value("calendar").toObject().value("rows").toArray()) {
                const QJsonObject r = rv.toObject();
                Dividend d;
                d.symbol = r.value("symbol").toString().trimmed();
                d.name = r.value("companyName").toString().trimmed();
                d.exDate = nasdaqDate(r.value("dividend_Ex_Date").toString());
                d.payDate = nasdaqDate(r.value("payment_Date").toString());
                d.amount = jsonNumber(r.value("dividend_Rate"));
                if (!d.symbol.isEmpty() && d.exDate.isValid())
                    dividends.push_back(d);
            }
        } else if (path.endsWith("/calendar/splits")) {
            for (const QJsonValue &rv : data.value("rows").toArray()) {
                const QJsonObject r = rv.toObject();
                Split s;
                s.symbol = r.value("symbol").toString().trimmed();
                s.name = r.value("name").toString().trimmed();
                s.date = nasdaqDate(r.value("executionDate").toString());
                s.ratio = r.value("ratio").toString().remove(' ');   // "1 : 4" and "3:1" alike
                if (!s.symbol.isEmpty())
                    splits.push_back(s);
            }
        } else if (path.endsWith("/ipo/calendar")) {
            auto add = [&](const QJsonArray &rows, bool priced) {
                for (const QJsonValue &rv : rows) {
                    const QJsonObject r = rv.toObject();
                    Ipo i;
                    i.symbol = r.value("proposedTickerSymbol").toString().trimmed();
                    i.name = r.value("companyName").toString().trimmed();
                    i.exchange = r.value("proposedExchange").toString().trimmed();
                    i.price = r.value("proposedSharePrice").toString().trimmed();
                    i.date = nasdaqDate(r.value(priced ? "pricedDate" : "expectedPriceDate").toString());
                    i.priced = priced;
                    i.amount = nasdaqNumber(r.value("dollarValueOfSharesOffered").toString());
                    if (!i.symbol.isEmpty() && i.date.isValid())
                        ipos.push_back(i);
                }
            };
            add(data.value("priced").toObject().value("rows").toArray(), true);
            add(data.value("upcoming").toObject().value("upcomingTable").toObject().value("rows").toArray(), false);
        }
    }
    // Largest companies first within a day: the ones people look for.
    std::sort(earnings.begin(), earnings.end(), [](const Earnings &a, const Earnings &b) {
        return a.date != b.date ? a.date < b.date : a.marketCap > b.marketCap;
    });
    std::sort(dividends.begin(), dividends.end(), [](const Dividend &a, const Dividend &b) {
        return a.exDate != b.exDate ? a.exDate < b.exDate : a.symbol < b.symbol;
    });
    std::sort(splits.begin(), splits.end(), [](const Split &a, const Split &b) {
        return a.date != b.date ? a.date < b.date : a.symbol < b.symbol;
    });
    // An IPO can be both upcoming in one month's answer and priced in the next one's: keep the priced one.
    std::sort(ipos.begin(), ipos.end(), [](const Ipo &a, const Ipo &b) {
        return a.symbol != b.symbol ? a.symbol < b.symbol : a.priced > b.priced;
    });
    ipos.erase(std::unique(ipos.begin(), ipos.end(), [](const Ipo &a, const Ipo &b) { return a.symbol == b.symbol; }),
               ipos.end());
    std::sort(ipos.begin(), ipos.end(), [](const Ipo &a, const Ipo &b) {
        return a.date != b.date ? a.date < b.date : a.symbol < b.symbol;
    });
    *data = {earnings, dividends, splits, ipos};
    return true;
}

bool CorporateCalendar::parse(const QByteArray &body)
{
    Data data;
    if (!read(QJsonDocument::fromJson(body).array(), &data)) {
        m_error = tr("unexpected answer from Nasdaq");
        return false;
    }
    m_earnings = data.earnings;
    m_dividends = data.dividends;
    m_splits = data.splits;
    m_ipos = data.ipos;
    m_fetchedAtLeastOnce = true;
    return true;
}

// ---------------------------------------------------------------- MarketSessions

namespace {
constexpr int SessionSymbolsPerRequest = 20;   // the spark endpoint's limit
}

MarketSessions::MarketSessions(const QStringList &symbols, QObject *parent)
    : RemoteDataSet("yahoo_market_sessions", 15 * 60, parent)
    , m_symbols(symbols)
{
    loadCached();
}

QList<QUrl> MarketSessions::urls() const
{
    QList<QUrl> out;
    for (int i = 0; i < m_symbols.size(); i += SessionSymbolsPerRequest)
        out << QUrl(QString("https://query1.finance.yahoo.com/v7/finance/spark?symbols=%1&range=1d&interval=1h")
                        .arg(m_symbols.mid(i, SessionSymbolsPerRequest).join(',')));
    return out;
}

QByteArray MarketSessions::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = "[";
    for (int i = 0; i < parts.size(); ++i)
        out += (i ? "," : "") + parts[i];
    return out + "]";
}

void MarketSessions::prepare(QNetworkRequest &request) const
{
    // Yahoo answers 429 to most user agents, but not to a bare "Mozilla/5.0" (see FuturesQuotes).
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
}

bool MarketSessions::parse(const QByteArray &body)
{
    QHash<QString, Session> sessions;
    for (const QJsonValue &response : QJsonDocument::fromJson(body).array()) {
        for (const QJsonValue &v : response.toObject().value("spark").toObject().value("result").toArray()) {
            const QJsonObject result = v.toObject();
            const QJsonObject meta = result.value("response").toArray().at(0).toObject().value("meta").toObject();
            const QJsonObject regular = meta.value("currentTradingPeriod").toObject().value("regular").toObject();
            const qint64 start = regular.value("start").toInteger(), end = regular.value("end").toInteger();
            if (start > 0 && end > start)
                sessions.insert(result.value("symbol").toString(),
                                {QDateTime::fromSecsSinceEpoch(start, QTimeZone::UTC),
                                 QDateTime::fromSecsSinceEpoch(end, QTimeZone::UTC)});
        }
    }
    if (sessions.isEmpty()) {
        m_error = tr("no trading sessions in Yahoo's answer");
        return false;
    }
    m_sessions = sessions;
    return true;
}

const MarketSessions::Session *MarketSessions::session(const QString &symbol) const
{
    const auto it = m_sessions.constFind(symbol);
    return it == m_sessions.cend() ? nullptr : &*it;
}
