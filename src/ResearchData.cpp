#include "ResearchData.h"

#include "HttpCache.h"
#include "KeyedSources.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QThreadPool>
#include <QTimeZone>
#include <QUrlQuery>

#include <QTimer>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>

namespace research {

// ---------------------------------------------------------------- helpers

double raw(const QJsonObject &o, const char *key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble())
        return v.toDouble();
    if (v.isObject()) {
        const QJsonValue r = v.toObject().value("raw");
        if (r.isDouble())
            return r.toDouble();
    }
    return NaN;
}

QString text(const QJsonObject &o, const char *key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isString())
        return v.toString();
    if (v.isObject())
        return v.toObject().value("fmt").toString();
    return {};
}

namespace {

QDate fromEpoch(qint64 secs)
{
    return QDateTime::fromSecsSinceEpoch(secs, QTimeZone::UTC).date();
}

// "1,426,283,914", "$491,540,225", "(690,085)", "1.919%": the number, NaN if none.
double nasdaqNumber(QString s)
{
    s = s.trimmed();
    if (s.isEmpty() || s == "N/A" || s == "--")
        return NaN;
    bool negative = s.startsWith('(') || s.startsWith('-');
    // Keep the digits and the point (what removing [^0-9.] did, without compiling a regex per call).
    s.removeIf([](QChar c) { return !((c >= u'0' && c <= u'9') || c == u'.'); });
    bool okay = false;
    const double v = s.toDouble(&okay);
    return okay ? (negative ? -v : v) : NaN;
}

// " Common Stock", " Ordinary Shares"… and what follows, at the end of a Nasdaq listing's name.
const QRegularExpression &shareClassSuffix()
{
    static const QRegularExpression re("\\s+(Common Stock|Ordinary Shares|Class [A-C] Common Stock|American Depositary Shares).*$");
    return re;
}

QDate usDate(const QString &s)
{
    return QDate::fromString(s.trimmed(), "M/d/yyyy");
}

QString safeKey(const QString &key)
{
    QString k = key;
    static const QRegularExpression unsafe("[^A-Za-z0-9_.-]");
    k.replace(unsafe, "_");
    if (k.size() > 80)
        k = k.left(40) + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex());
    return "research_" + k;
}

QString hostGroup(const QUrl &url)
{
    const QString h = url.host();
    if (h.contains("yahoo"))
        return "yahoo";
    if (h.contains("nasdaq"))
        return "nasdaq";
    if (h.contains("sec.gov"))
        return "sec";
    if (h.contains("wikidata"))
        return "wikidata";
    if (h.contains("wikipedia"))
        return "wikipedia";
    if (h.contains("treasury.gov"))
        return "treasury";
    if (h.contains("stlouisfed.org"))
        return "fred";
    if (h.contains("oecd.org"))
        return "oecd";
    if (h.contains("coinmarketcap"))
        return "coinmarketcap";
    if (h.contains("worldbank.org"))
        return "worldbank";
    if (h.contains("imf.org"))
        return "imf";
    if (h.contains("ecb.europa.eu"))
        return "ecb";
    if (h.contains("europa.eu"))
        return "eurostat";
    if (h.contains("ilo.org"))
        return "ilo";
    if (h.contains("ons.gov.uk"))
        return "ons";
    if (h.contains("bankofengland.co.uk"))
        return "boe";
    if (h.contains("bcb.gov.br"))
        return "bcb";
    if (h.contains("statcan.gc.ca"))
        return "statcan";
    if (h.contains("bankofcanada.ca"))
        return "boc";
    if (h.contains("boj.or.jp"))
        return "boj";
    if (h.contains("snb.ch"))
        return "snb";
    if (h.contains("abs.gov.au"))
        return "abs";
    if (h.contains("bundesbank.de"))
        return "bbk";
    if (h.contains("insee.fr"))
        return "insee";
    if (h.contains("ine.es"))
        return "ine";
    if (h.contains("ssb.no") || h.contains("scb.se"))
        return "pxweb";
    if (h.contains("bok.or.kr"))
        return "ecos";
    if (h.contains("banxico.org.mx"))
        return "banxico";
    if (h.contains("inegi.org.mx"))
        return "inegi";
    if (h.contains("tcmb.gov.tr"))
        return "evds";
    if (h.contains("bps.go.id"))
        return "bps";
    if (h.contains("e-stat.go.jp"))
        return "estat";
    if (h.contains("bis.org"))
        return "bis";
    if (h.contains("eia.gov"))
        return "eia";
    if (h.contains("alternative.me"))
        return "alternative";
    return h;
}

// The shortest time between two requests to a host with a rate limit: FRED allows 120 a minute per key.
qint64 minSpacingMs(const QString &group)
{
    return group == "fred" ? 550 : 0;
}

int maxInFlight(const QString &group)
{
    if (group == "yahoo")
        return 4;
    return 2;
}

// ---------------------------------------------------------------- SEC company facts

struct MetricDef {
    const char *id;
    QStringList concepts;   // us-gaap or ifrs-full names, by priority
    bool instant;
};

const QVector<MetricDef> &secMetrics()
{
    static const QVector<MetricDef> defs {
        {"revenue", {"Revenues", "RevenueFromContractWithCustomerExcludingAssessedTax",
                     "RevenueFromContractWithCustomerIncludingAssessedTax", "SalesRevenueNet", "SalesRevenueGoodsNet",
                     "SalesRevenueServicesNet", "Revenue", "RevenueFromContractsWithCustomers"}, false},
        {"costOfRevenue", {"CostOfRevenue", "CostOfGoodsAndServicesSold", "CostOfGoodsSold", "CostOfSales"}, false},
        {"grossProfit", {"GrossProfit"}, false},
        {"operatingIncome", {"OperatingIncomeLoss", "ProfitLossFromOperatingActivities"}, false},
        {"netIncome", {"NetIncomeLoss", "ProfitLossAttributableToOwnersOfParent", "ProfitLoss"}, false},
        {"eps", {"EarningsPerShareBasic", "BasicEarningsLossPerShare"}, false},
        {"epsDiluted", {"EarningsPerShareDiluted", "DilutedEarningsLossPerShare"}, false},
        {"da", {"DepreciationDepletionAndAmortization", "DepreciationAmortizationAndAccretionNet",
                "DepreciationAndAmortization", "DepreciationAndAmortisationExpense", "Depreciation"}, false},
        {"ocf", {"NetCashProvidedByUsedInOperatingActivities",
                 "NetCashProvidedByUsedInOperatingActivitiesContinuingOperations",
                 "CashFlowsFromUsedInOperatingActivities"}, false},
        {"capex", {"PaymentsToAcquirePropertyPlantAndEquipment", "PaymentsToAcquireProductiveAssets",
                   "PurchaseOfPropertyPlantAndEquipmentClassifiedAsInvestingActivities",
                   "PurchaseOfPropertyPlantAndEquipment"}, false},
        {"dividendsPerShare", {"CommonStockDividendsPerShareDeclared", "CommonStockDividendsPerShareCashPaid"}, false},
        {"assets", {"Assets"}, true},
        {"liabilities", {"Liabilities"}, true},
        {"liabilitiesAndEquity", {"LiabilitiesAndStockholdersEquity", "EquityAndLiabilities"}, true},
        {"equity", {"StockholdersEquity", "EquityAttributableToOwnersOfParent",
                    "StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest", "Equity"}, true},
        {"equityTotal", {"StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest", "Equity",
                         "StockholdersEquity"}, true},
        {"cash", {"CashAndCashEquivalentsAtCarryingValue",
                  "CashCashEquivalentsRestrictedCashAndRestrictedCashEquivalents", "CashAndCashEquivalents"}, true},
        {"debtLong", {"LongTermDebt", "LongTermDebtAndCapitalLeaseObligationsIncludingCurrentMaturities"}, true},
        {"debtLongNoncurrent", {"LongTermDebtNoncurrent", "LongTermDebtAndCapitalLeaseObligations",
                                "NoncurrentPortionOfNoncurrentBorrowings", "LongtermBorrowings"}, true},
        {"debtLongCurrent", {"LongTermDebtCurrent", "LongTermDebtAndCapitalLeaseObligationsCurrent",
                             "CurrentPortionOfLongtermBorrowings"}, true},
        {"debtShort", {"CommercialPaper", "ShortTermBorrowings", "ShorttermBorrowings"}, true},
    };
    return defs;
}

struct Fact {
    QDate start, end, filed;
    double value;
};

// Stock splits: date and ratio (new shares per old one).
using Splits = QVector<std::pair<QDate, double>>;

// Every fact of a concept in its first fitting unit, deduplicated by period (the latest filing wins).
// Per-share facts are put on today's share count: a filing restates its figures for the splits before it,
// so a value is divided by the splits that happened after it was filed.
QVector<Fact> conceptFacts(const QJsonObject &conceptObj, bool perShare, QString *unit, const Splits &splits = {})
{
    const QJsonObject units = conceptObj.value("units").toObject();
    QString chosen;
    for (auto it = units.begin(); it != units.end(); ++it) {
        const bool isPerShare = it.key().contains("/shares");
        if (isPerShare != perShare || it.key() == "shares" || it.key() == "pure")
            continue;
        if (chosen.isEmpty() || it.key() == "USD" || it.key() == "USD/shares")
            chosen = it.key();
    }
    if (chosen.isEmpty())
        return {};
    if (unit)
        *unit = chosen;
    QHash<QString, Fact> byPeriod;
    for (const QJsonValue &v : units.value(chosen).toArray()) {
        const QJsonObject f = v.toObject();
        Fact fact {QDate::fromString(f.value("start").toString(), Qt::ISODate),
                   QDate::fromString(f.value("end").toString(), Qt::ISODate),
                   QDate::fromString(f.value("filed").toString(), Qt::ISODate), f.value("val").toDouble()};
        if (!fact.end.isValid())
            continue;
        const QString key = f.value("start").toString() + f.value("end").toString();
        auto it = byPeriod.find(key);
        if (it == byPeriod.end() || it->filed < fact.filed)
            byPeriod.insert(key, fact);
    }
    QVector<Fact> out = byPeriod.values().toVector();
    if (perShare)
        for (Fact &f : out)
            for (const auto &[date, ratio] : splits)
                if (date > f.filed && ratio > 0)
                    f.value /= ratio;
    return out;
}

QJsonArray toJson(const std::map<QDate, double> &points)
{
    QJsonArray out;
    for (const auto &[date, value] : points)
        out.append(QJsonArray {date.toString(Qt::ISODate), value});
    return out;
}

// Company facts (several MB) -> {"currency", "annual": {metric: [[end, value]…]}, "quarterly": {…}}.
QByteArray compactSecFacts(const QByteArray &body, const Splits &splits)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonObject facts = root.value("facts").toObject();
    if (facts.isEmpty())
        return {};
    const QJsonObject gaap = facts.value("us-gaap").toObject();
    const QJsonObject ifrs = facts.value("ifrs-full").toObject();

    QHash<QString, std::map<QDate, double>> annual, quarterly;
    QString currency;
    std::map<QDate, int> annualEnds, quarterEnds;   // period ends seen on duration metrics
    QVector<std::pair<const MetricDef *, QVector<Fact>>> instants;

    for (const MetricDef &m : secMetrics()) {
        const bool perShare = QByteArray(m.id).startsWith("eps") || QByteArray(m.id) == "dividendsPerShare";
        std::map<QDate, double> &a = annual[m.id];
        std::map<QDate, double> &q = quarterly[m.id];
        QVector<Fact> allInstants;
        for (const QString &name : m.concepts) {
            QJsonObject conceptObj = gaap.value(name).toObject();
            if (conceptObj.isEmpty())
                conceptObj = ifrs.value(name).toObject();
            if (conceptObj.isEmpty())
                continue;
            QString unit;
            const QVector<Fact> list = conceptFacts(conceptObj, perShare, &unit, splits);
            if (list.isEmpty())
                continue;
            if (m.instant) {
                for (const Fact &f : list)
                    if (!f.start.isValid())
                        allInstants.push_back(f);
                continue;
            }
            if (QByteArray(m.id) == "revenue" && currency.isEmpty())
                currency = unit;
            // Durations: about a year, a quarter, or nine months (for the fourth quarter).
            std::map<QDate, Fact> years, quarters;
            QVector<Fact> nineMonths;
            for (const Fact &f : list) {
                if (!f.start.isValid())
                    continue;
                const qint64 days = f.start.daysTo(f.end);
                if (days >= 340 && days <= 390)
                    years[f.end] = f;
                else if (days >= 80 && days <= 100)
                    quarters[f.end] = f;
                else if (days >= 260 && days <= 290)
                    nineMonths.push_back(f);
            }
            for (const auto &[end, f] : years) {
                if (!a.count(end))
                    a[end] = f.value;
                annualEnds[end]++;
            }
            for (const auto &[end, f] : quarters) {
                if (!q.count(end))
                    q[end] = f.value;
                quarterEnds[end]++;
            }
            // The fourth quarter is rarely reported on its own: the year less the first nine months.
            for (const auto &[end, y] : years) {
                if (q.count(end))
                    continue;
                double rest = NaN;
                for (const Fact &n : nineMonths)
                    if (n.start == y.start && n.end < end && n.end.daysTo(end) < 110)
                        rest = n.value;
                if (!ok(rest)) {
                    double sum = 0;
                    int count = 0;
                    for (const auto &[qend, qf] : quarters)
                        if (qf.start >= y.start.addDays(-5) && qend < end.addDays(-20)) {
                            sum += qf.value;
                            ++count;
                        }
                    if (count == 3)
                        rest = sum;
                }
                if (ok(rest)) {
                    q[end] = y.value - rest;
                    quarterEnds[end]++;
                }
            }
        }
        if (m.instant)
            instants.push_back({&m, allInstants});
    }

    // Balance sheet lines: their value at the period ends of the income statement.
    for (const auto &[m, list] : instants) {
        std::map<QDate, Fact> byEnd;
        for (const Fact &f : list)
            if (!byEnd.count(f.end) || byEnd[f.end].filed < f.filed)
                byEnd[f.end] = f;
        std::map<QDate, double> &a = annual[m->id];
        std::map<QDate, double> &q = quarterly[m->id];
        for (const auto &[end, f] : byEnd) {
            if (annualEnds.count(end))
                a[end] = f.value;
            if (quarterEnds.count(end) || annualEnds.count(end))
                q[end] = f.value;
        }
    }

    // Derived lines.
    for (auto *set : {&annual, &quarterly}) {
        auto &s = *set;
        auto value = [&s](const char *id, const QDate &d) {
            auto it = s[id].find(d);
            return it == s[id].end() ? NaN : it->second;
        };
        QSet<QDate> dates;
        for (auto it = s.begin(); it != s.end(); ++it)
            for (const auto &[d, v] : it.value())
                dates.insert(d);
        for (const QDate &d : dates) {
            const double rev = value("revenue", d);
            double gp = value("grossProfit", d);
            if (!ok(gp) && ok(rev) && ok(value("costOfRevenue", d)))
                s["grossProfit"][d] = gp = rev - value("costOfRevenue", d);
            const double op = value("operatingIncome", d), ni = value("netIncome", d);
            if (ok(rev) && rev != 0) {
                if (ok(gp)) s["grossMargin"][d] = gp / rev;
                if (ok(op)) s["operatingMargin"][d] = op / rev;
                if (ok(ni)) s["netMargin"][d] = ni / rev;
            }
            const double ocf = value("ocf", d), capex = value("capex", d);
            if (ok(ocf))
                s["fcf"][d] = ocf - (ok(capex) ? std::abs(capex) : 0);
            if (ok(op) && ok(value("da", d)))
                s["ebitda"][d] = op + value("da", d);
            if (!ok(value("liabilities", d)) && ok(value("liabilitiesAndEquity", d)) && ok(value("equityTotal", d)))
                s["liabilities"][d] = value("liabilitiesAndEquity", d) - value("equityTotal", d);
            double debt = value("debtLong", d);
            if (!ok(debt) && (ok(value("debtLongNoncurrent", d)) || ok(value("debtLongCurrent", d))))
                debt = (ok(value("debtLongNoncurrent", d)) ? value("debtLongNoncurrent", d) : 0)
                     + (ok(value("debtLongCurrent", d)) ? value("debtLongCurrent", d) : 0);
            if (ok(debt) || ok(value("debtShort", d)))
                s["totalDebt"][d] = (ok(debt) ? debt : 0) + (ok(value("debtShort", d)) ? value("debtShort", d) : 0);
        }
    }

    QJsonObject outAnnual, outQuarterly;
    for (const Metric &m : fundamentalMetrics()) {
        if (!annual.value(m.id).empty())
            outAnnual.insert(m.id, toJson(annual.value(m.id)));
        if (!quarterly.value(m.id).empty())
            outQuarterly.insert(m.id, toJson(quarterly.value(m.id)));
    }
    if (outAnnual.isEmpty() && outQuarterly.isEmpty())
        return {};
    return QJsonDocument(QJsonObject {{"source", "SEC EDGAR"}, {"currency", currency.isEmpty() ? "USD" : currency},
                                      {"annual", outAnnual}, {"quarterly", outQuarterly}})
        .toJson(QJsonDocument::Compact);
}

Fundamentals readFundamentals(const QByteArray &compact)
{
    Fundamentals f;
    const QJsonObject o = QJsonDocument::fromJson(compact).object();
    f.source = o.value("source").toString();
    f.currency = o.value("currency").toString();
    for (auto [key, target] : {std::pair {"annual", &f.annual}, std::pair {"quarterly", &f.quarterly}}) {
        const QJsonObject set = o.value(key).toObject();
        for (auto it = set.begin(); it != set.end(); ++it) {
            Series s;
            for (const QJsonValue &p : it.value().toArray())
                s.push_back({QDate::fromString(p[0].toString(), Qt::ISODate), p[1].toDouble()});
            target->insert(it.key(), s);
        }
    }
    return f;
}

// Yahoo's fundamentals-timeseries lines used when the SEC has nothing (foreign listings).
const QVector<std::pair<const char *, const char *>> &yahooFundamentalLines()
{
    static const QVector<std::pair<const char *, const char *>> lines {
        {"revenue", "TotalRevenue"}, {"costOfRevenue", "CostOfRevenue"}, {"grossProfit", "GrossProfit"},
        {"operatingIncome", "OperatingIncome"}, {"netIncome", "NetIncome"}, {"eps", "BasicEPS"},
        {"epsDiluted", "DilutedEPS"}, {"ebitda", "EBITDA"}, {"ocf", "OperatingCashFlow"},
        {"capex", "CapitalExpenditure"}, {"fcf", "FreeCashFlow"}, {"assets", "TotalAssets"},
        {"liabilities", "TotalLiabilitiesNetMinorityInterest"}, {"equity", "StockholdersEquity"},
        {"cash", "CashAndCashEquivalents"}, {"totalDebt", "TotalDebt"},
    };
    return lines;
}

// A fundamentals-timeseries body -> line name (no prefix) -> period end -> value, and the currency.
void readTimeseries(const QByteArray &body, QHash<QString, std::map<QDate, double>> *annual,
                    QHash<QString, std::map<QDate, double>> *quarterly, QString *currency)
{
    const QJsonArray result = QJsonDocument::fromJson(body).object().value("timeseries").toObject()
                                  .value("result").toArray();
    for (const QJsonValue &rv : result) {
        const QJsonObject r = rv.toObject();
        const QString type = r.value("meta").toObject().value("type").toArray().first().toString();
        const bool isAnnual = type.startsWith("annual");
        const bool isQuarterly = type.startsWith("quarterly");
        if (!isAnnual && !isQuarterly)
            continue;
        const QString name = type.mid(isAnnual ? 6 : 9);
        auto &target = (isAnnual ? *annual : *quarterly)[name];
        for (const QJsonValue &pv : r.value(type).toArray()) {
            const QJsonObject p = pv.toObject();
            if (p.isEmpty())
                continue;
            const QDate d = QDate::fromString(p.value("asOfDate").toString(), Qt::ISODate);
            const double v = p.value("reportedValue").toObject().value("raw").toDouble(NaN);
            if (d.isValid() && ok(v))
                target[d] = v;
            if (currency && currency->isEmpty())
                *currency = p.value("currencyCode").toString();
        }
    }
}

QByteArray yahooFundamentalsToCompact(const QByteArray &body)
{
    QHash<QString, std::map<QDate, double>> a, q;
    QString currency;
    readTimeseries(body, &a, &q, &currency);
    QJsonObject outA, outQ;
    for (const auto &[id, line] : yahooFundamentalLines()) {
        auto fix = [&](std::map<QDate, double> m) {
            if (QByteArray(id) == "capex")
                for (auto &[d, v] : m) v = std::abs(v);
            return m;
        };
        if (!a.value(line).empty()) outA.insert(id, toJson(fix(a.value(line))));
        if (!q.value(line).empty()) outQ.insert(id, toJson(fix(q.value(line))));
    }
    for (auto *pair : {&outA, &outQ}) {
        // Margins from the lines above.
        QJsonObject &o = *pair;
        auto series = [&o](const char *id) {
            std::map<QDate, double> m;
            for (const QJsonValue &p : o.value(id).toArray())
                m[QDate::fromString(p[0].toString(), Qt::ISODate)] = p[1].toDouble();
            return m;
        };
        const auto rev = series("revenue");
        for (auto [id, src] : {std::pair {"grossMargin", "grossProfit"}, std::pair {"operatingMargin", "operatingIncome"},
                               std::pair {"netMargin", "netIncome"}}) {
            std::map<QDate, double> m;
            for (const auto &[d, v] : series(src))
                if (rev.count(d) && rev.at(d) != 0)
                    m[d] = v / rev.at(d);
            if (!m.empty())
                o.insert(id, toJson(m));
        }
    }
    if (outA.isEmpty() && outQ.isEmpty())
        return {};
    return QJsonDocument(QJsonObject {{"source", "Yahoo Finance"}, {"currency", currency.isEmpty() ? "USD" : currency},
                                      {"annual", outA}, {"quarterly", outQ}})
        .toJson(QJsonDocument::Compact);
}

QUrl timeseriesUrl(const QString &symbol, const QStringList &lines, bool annual, bool quarterly)
{
    QStringList types;
    for (const QString &l : lines) {
        if (annual) types << "annual" + l;
        if (quarterly) types << "quarterly" + l;
    }
    QUrl url("https://query1.finance.yahoo.com/ws/fundamentals-timeseries/v1/finance/timeseries/"
             + QUrl::toPercentEncoding(symbol));
    QUrlQuery q;
    q.addQueryItem("symbol", symbol);
    q.addQueryItem("type", types.join(','));
    q.addQueryItem("period1", "493590046");
    q.addQueryItem("period2", QString::number(QDateTime::currentSecsSinceEpoch() + 86400));
    url.setQuery(q);
    return url;
}

// ---------------------------------------------------------------- Wikipedia tables

// "|| {{NyseSymbol|MMM}}" rows of the S&P 500 list: symbol, link target, name, GICS sector.
struct WikiRow {
    QString symbol, target, name, sector;
};

QVector<WikiRow> parseSp500(const QString &wikitext)
{
    QVector<WikiRow> out;
    static const QRegularExpression symbolRe(R"(\{\{\s*(?:NyseSymbol|NasdaqSymbol|CboeSymbol|BZX link|Nyse symbol|Nasdaq symbol)\s*\|\s*([A-Za-z.\-]+))",
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression linkRe(R"(\[\[([^\]|]+)(?:\|([^\]]+))?\]\])");
    const int start = wikitext.indexOf("id=\"constituents\"");
    const QString table = wikitext.mid(start < 0 ? 0 : start);
    static const QRegularExpression rowBreak("\\n\\|-");
    const QStringList rows = table.split(rowBreak);
    for (const QString &row : rows) {
        const auto sm = symbolRe.match(row);
        if (!sm.hasMatch())
            continue;
        QStringList cells;
        for (QString line : row.split('\n')) {
            line = line.trimmed();
            if (!line.startsWith('|') || line.startsWith("|}") || line.startsWith("|-"))
                continue;
            while (line.startsWith('|'))
                line.remove(0, 1);
            for (const QString &c : line.split("||"))
                cells << c.trimmed();
        }
        if (cells.size() < 3)
            continue;
        WikiRow r;
        r.symbol = sm.captured(1).toUpper().replace('.', '-');
        const auto lm = linkRe.match(cells[1]);
        r.target = lm.hasMatch() ? lm.captured(1).trimmed() : cells[1];
        r.name = lm.hasMatch() ? (lm.captured(2).isEmpty() ? lm.captured(1) : lm.captured(2)).trimmed() : cells[1];
        r.sector = cells[2];
        static const QRegularExpression brackets("\\[\\[|\\]\\]");
        r.sector.remove(brackets);
        out.push_back(r);
    }
    return out;
}

QStringList navboxTargets(const QString &wikitext)
{
    QStringList out;
    static const QRegularExpression itemRe(R"(\n\*\s*\[\[([^\]|]+)(?:\|[^\]]+)?\]\])");
    for (auto it = itemRe.globalMatch(wikitext); it.hasNext();)
        out << it.next().captured(1).trimmed();
    return out;
}

} // namespace

// ---------------------------------------------------------------- metrics

const QVector<Metric> &fundamentalMetrics()
{
    static const QVector<Metric> metrics {
        {"revenue", QObject::tr("Revenue"), Metric::Money},
        {"grossProfit", QObject::tr("Gross Profit"), Metric::Money},
        {"operatingIncome", QObject::tr("Operating Income"), Metric::Money},
        {"netIncome", QObject::tr("Net Income"), Metric::Money},
        {"ebitda", QObject::tr("EBITDA"), Metric::Money},
        {"eps", QObject::tr("EPS"), Metric::PerShare},
        {"epsDiluted", QObject::tr("EPS Diluted"), Metric::PerShare},
        {"grossMargin", QObject::tr("Gross Margin"), Metric::Percent},
        {"operatingMargin", QObject::tr("Operating Margin"), Metric::Percent},
        {"netMargin", QObject::tr("Net Margin"), Metric::Percent},
        {"assets", QObject::tr("Total Assets"), Metric::Money},
        {"liabilities", QObject::tr("Total Liabilities"), Metric::Money},
        {"equity", QObject::tr("Total Equity"), Metric::Money},
        {"totalDebt", QObject::tr("Total Debt"), Metric::Money},
        {"cash", QObject::tr("Cash & Equivalents"), Metric::Money},
        {"ocf", QObject::tr("Operating Cash Flow"), Metric::Money},
        {"fcf", QObject::tr("Free Cash Flow"), Metric::Money},
        {"capex", QObject::tr("CapEx"), Metric::Money},
        {"dividendsPerShare", QObject::tr("Dividends per Share"), Metric::PerShare},
        {"costOfRevenue", QObject::tr("Cost of Revenue"), Metric::Money},
        {"da", QObject::tr("Depreciation & Amortization"), Metric::Money},
    };
    return metrics;
}

const Metric *findMetric(const QString &id)
{
    for (const Metric &m : fundamentalMetrics())
        if (m.id == id)
            return &m;
    return nullptr;
}

QStringList Indexes::indexesOf(const QString &symbol) const
{
    QStringList out;
    auto has = [&symbol](const QVector<IndexMember> &list) {
        return std::any_of(list.begin(), list.end(), [&](const IndexMember &m) { return m.symbol == symbol; });
    };
    if (has(sp500)) out << "S&P 500";
    if (has(dow)) out << "DJIA";
    if (has(nasdaq100)) out << "NASDAQ-100";
    return out;
}

// ---------------------------------------------------------------- Client

Client *Client::instance()
{
    static Client *client = new Client;
    return client;
}

Client::Client(QObject *parent)
    : QObject(parent)
{
}

void Client::get(const QString &key, const QUrl &url, qint64 maxAgeSecs, int flags, QObject *ctx, Raw done,
                 std::function<QByteArray(const QByteArray &)> transform)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto it = m_memory.find(key);
    if (it == m_memory.end()) {
        Entry e;
        if (HttpCache::load(safeKey(key), &e.body, &e.at))
            it = m_memory.insert(key, e);
    }
    const bool enabled = !m_disabled.contains(key.section(':', 0, 0));
    const bool fresh = it != m_memory.end() && it->at.secsTo(now) < maxAgeSecs;
    if (fresh || (!enabled && it != m_memory.end())) {
        const QByteArray body = it->body;
        QPointer<QObject> guard(ctx);
        QMetaObject::invokeMethod(this, [guard, done, body] { if (guard) done(body, {}); }, Qt::QueuedConnection);
        return;
    }
    if (!enabled) {
        QPointer<QObject> guard(ctx);
        QMetaObject::invokeMethod(this, [guard, done] {
            if (guard) done({}, tr("This source is turned off in Sources"));
        }, Qt::QueuedConnection);
        return;
    }
    const bool already = m_waiting.contains(key);
    m_waiting[key].push_back({ctx, done});
    if (!already)
        enqueue({key, url, flags, transform, 0, m_pendingHeaders.take(key)});
}

void Client::setEnabled(const QString &group, bool on)
{
    if (on)
        m_disabled.remove(group);
    else
        m_disabled.insert(group);
}

void Client::forget(const QString &group)
{
    // Stale in memory, so the next request downloads again (the disk copy is only read on a memory miss).
    for (auto it = m_memory.begin(); it != m_memory.end(); ++it)
        if (it.key().section(':', 0, 0) == group)
            it->at = QDateTime::fromSecsSinceEpoch(0, QTimeZone::UTC);
}

void Client::enqueue(const Job &job)
{
    m_queue.enqueue(job);
    m_hosts[hostGroup(job.url)].pending++;
    pump();
}

void Client::pump()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 wait = 0;
    for (int i = 0; i < m_queue.size();) {
        const QString group = hostGroup(m_queue[i].url);
        if (m_inFlight.value(group) >= maxInFlight(group)) {
            ++i;
            continue;
        }
        if (const qint64 spacing = minSpacingMs(group); spacing > 0) {
            const qint64 next = m_lastStart.value(group) + spacing;
            if (now < next) {
                wait = wait == 0 ? next - now : std::min(wait, next - now);
                ++i;
                continue;
            }
            m_lastStart[group] = now;
        }
        Job job = m_queue.takeAt(i);
        if ((job.flags & YahooCrumb) && m_crumb.isEmpty()) {
            m_needCrumb.push_back(job);
            ensureCrumb();
            continue;
        }
        send(job);
    }
    if (wait > 0 && !m_pumpScheduled) {
        m_pumpScheduled = true;
        QTimer::singleShot(int(wait), this, [this] {
            m_pumpScheduled = false;
            pump();
        });
    }
}

void Client::ensureCrumb()
{
    if (m_crumbPending)
        return;
    m_crumbPending = true;
    // A Yahoo session: the consent cookie from fc.yahoo.com (it answers 404, with the cookie), then the crumb.
    QNetworkRequest cookieReq(QUrl("https://fc.yahoo.com/"));
    cookieReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
    cookieReq.setTransferTimeout(20000);
    QNetworkReply *r1 = m_net.get(cookieReq);
    connect(r1, &QNetworkReply::finished, this, [this, r1] {
        r1->deleteLater();
        QNetworkRequest crumbReq(QUrl("https://query1.finance.yahoo.com/v1/test/getcrumb"));
        crumbReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
        crumbReq.setTransferTimeout(20000);
        QNetworkReply *r2 = m_net.get(crumbReq);
        connect(r2, &QNetworkReply::finished, this, [this, r2] {
            r2->deleteLater();
            m_crumbPending = false;
            const QByteArray crumb = r2->readAll().trimmed();
            const bool good = r2->error() == QNetworkReply::NoError && !crumb.isEmpty() && crumb.size() < 40
                              && !crumb.contains('<') && !crumb.contains('{');
            const QVector<Job> jobs = std::exchange(m_needCrumb, {});
            if (!good) {
                const QString error = tr("Yahoo session refused: %1")
                                          .arg(r2->error() != QNetworkReply::NoError ? r2->errorString()
                                                                                     : QString::fromUtf8(crumb.left(60)));
                for (const Job &j : jobs) {
                    m_hosts["yahoo"].pending--;
                    finish(j.key, {}, error);
                }
                return;
            }
            m_crumb = QString::fromUtf8(crumb);
            for (const Job &j : jobs)
                m_queue.prepend(j);
            pump();
        });
    });
}

void Client::send(Job job)
{
    const QString group = hostGroup(job.url);
    m_inFlight[group]++;
    QUrl url = job.url;
    if (job.flags & YahooCrumb) {
        QUrlQuery q(url);
        q.removeAllQueryItems("crumb");
        q.addQueryItem("crumb", m_crumb);
        url.setQuery(q);
    }
    QNetworkRequest req(url);
    req.setTransferTimeout(45000);
    if (job.flags & (Nasdaq | Browser | YahooCrumb))
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
    else
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1 (desktop app; research page)"));
    if (job.flags & Nasdaq) {
        req.setRawHeader("Accept", "application/json");
        req.setRawHeader("Origin", "https://www.nasdaq.com");
        req.setRawHeader("Referer", "https://www.nasdaq.com/");
    }
    if (job.flags & Wiki)
        req.setRawHeader("Accept", "application/sparql-results+json");
    if (job.flags & Json)
        req.setRawHeader("Accept", "application/json");
    for (const auto &[name, value] : job.headers)
        req.setRawHeader(name, value);
    if (group == "oecd")   // the OECD server answers HTTP 500 to Qt's default Accept-Language now and then
        req.setRawHeader("Accept-Language", "en");
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job, group]() mutable {
        reply->deleteLater();
        m_inFlight[group]--;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // An expired Yahoo session: a new crumb, once.
        if ((job.flags & YahooCrumb) && (status == 401 || status == 403) && job.attempt == 0) {
            m_crumb.clear();
            job.attempt++;
            m_needCrumb.push_back(job);
            ensureCrumb();
            pump();
            return;
        }
        // Too many requests: wait a little and try again (three times).
        if (status == 429 && job.attempt < 3) {
            job.attempt++;
            // FRED counts a minute: wait it out.
            const int delay = group == "fred" ? 20000 * job.attempt : 2000 * job.attempt;
            QTimer::singleShot(delay, this, [this, job] {
                m_queue.enqueue(job);
                pump();
            });
            pump();
            return;
        }
        m_hosts[group].pending--;
        QByteArray body = reply->readAll();
        QString error;
        if (reply->error() != QNetworkReply::NoError)
            error = status ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
        if (!error.isEmpty() || !job.transform) {
            finish(job.key, error.isEmpty() ? body : QByteArray(), error);
            pump();
            return;
        }
        // Large bodies (SEC company facts) are reduced on a worker thread.
        QPointer<Client> self(this);
        auto transform = job.transform;
        const QString key = job.key;
        QThreadPool::globalInstance()->start([self, transform, body, key] {
            const QByteArray out = transform(body);
            QMetaObject::invokeMethod(self, [self, key, out] {
                if (self)
                    self->finish(key, out, out.isEmpty() ? tr("No usable data in the answer") : QString());
            }, Qt::QueuedConnection);
        });
        pump();
    });
}

void Client::finish(const QString &key, const QByteArray &body, const QString &error)
{
    const QString group = [&] {
        // The host of a finished key isn't kept; the key's prefix names it.
        const QString p = key.section(':', 0, 0);
        return p;
    }();
    HostState &h = m_hosts[group];
    QByteArray result = body;
    QString err = error;
    if (err.isEmpty() && !body.isEmpty()) {
        const Entry e {body, QDateTime::currentDateTimeUtc()};
        m_memory.insert(key, e);
        HttpCache::save(safeKey(key), body, e.at);
        h.lastOk = e.at;
    } else {
        if (err.isEmpty())
            err = tr("Empty answer");
        h.lastError = QDateTime::currentDateTimeUtc();
        h.error = err;
        // A stale copy is better than nothing.
        if (m_memory.contains(key))
            result = m_memory.value(key).body;
    }
    const QVector<Waiter> waiters = m_waiting.take(key);
    for (const Waiter &w : waiters)
        if (w.ctx)
            w.done(result, result.isEmpty() ? err : QString());
}

// ---------------------------------------------------------------- typed requests

void Client::summary(const QString &symbol, QObject *ctx, Done<QJsonObject> done)
{
    const QString modules = "assetProfile,summaryDetail,defaultKeyStatistics,financialData,price,"
                            "majorHoldersBreakdown,recommendationTrend,upgradeDowngradeHistory,earnings,calendarEvents,"
                            "earningsTrend,netSharePurchaseActivity,insiderTransactions,institutionOwnership,quoteType";
    QUrl url("https://query1.finance.yahoo.com/v10/finance/quoteSummary/" + QUrl::toPercentEncoding(symbol));
    url.setQuery("modules=" + modules);
    QPointer<QObject> guard(ctx);
    get("yahoo:summary:" + symbol, url, 15 * 60, YahooCrumb, ctx, [this, guard, done, symbol](const QByteArray &body, const QString &err) {
        QJsonObject root;
        if (const auto known = m_summaryParsed.constFind(symbol); known != m_summaryParsed.constEnd() && sameBody(body, known->body)) {
            root = known->value;
        } else {
            root = QJsonDocument::fromJson(body).object().value("quoteSummary").toObject();
            if (m_summaryParsed.size() >= 8)
                m_summaryParsed.clear();
            if (!body.isEmpty())
                m_summaryParsed.insert(symbol, {body, root});
        }
        QJsonObject result = root.value("result").toArray().first().toObject();
        if (result.isEmpty()) {
            const QString e = root.value("error").toObject().value("description").toString();
            done({}, !err.isEmpty() ? err : !e.isEmpty() ? e : tr("Unknown symbol"));
            return;
        }
        // Statements in another currency than the quote (ADRs, some foreign listings): the exchange rate, from
        // Yahoo's currency pair, goes with the summary as "fxToQuote". Pence-style quotes (GBp, ZAc, ILA)
        // are a hundredth of their currency.
        QString fin = text(result.value("financialData").toObject(), "financialCurrency");
        QString quote = text(result.value("price").toObject(), "currency");
        if (fin.isEmpty() || quote.isEmpty() || fin == quote) {
            done(result, err);
            return;
        }
        double factor = 1;
        static const QHash<QString, QString> minor {{"GBp", "GBP"}, {"ZAc", "ZAR"}, {"ILA", "ILS"}};
        if (minor.contains(quote)) {
            quote = minor.value(quote);
            factor = 100;
        }
        if (fin == quote) {
            result.insert("fxToQuote", factor);
            done(result, err);
            return;
        }
        prices(fin + quote + "=X", "5d", "1d", guard, [result, factor, done, err](const Series &s, const QString &) mutable {
            if (!s.isEmpty() && s.last().value > 0)
                result.insert("fxToQuote", s.last().value * factor);
            done(result, err);
        });
    });
}

void Client::quotes(const QStringList &symbols, QObject *ctx, Done<QHash<QString, QJsonObject>> done)
{
    quotes(symbols, 5 * 60, ctx, std::move(done));
}

void Client::quotes(const QStringList &symbols, qint64 maxAgeSecs, QObject *ctx, Done<QHash<QString, QJsonObject>> done)
{
    QStringList sorted = symbols;
    sorted.sort();
    QUrl url("https://query1.finance.yahoo.com/v7/finance/quote");
    url.setQuery("symbols=" + QUrl::toPercentEncoding(sorted.join(','), ","));
    get("yahoo:quote:" + sorted.join(','), url, maxAgeSecs, YahooCrumb, ctx, [done](const QByteArray &body, const QString &err) {
        QHash<QString, QJsonObject> out;
        for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("quoteResponse").toObject()
                                       .value("result").toArray())
            out.insert(v.toObject().value("symbol").toString(), v.toObject());
        done(out, out.isEmpty() && err.isEmpty() ? tr("No quotes") : err);
    });
}

void Client::screener(const QString &id, int count, QObject *ctx, Done<QVector<QJsonObject>> done)
{
    QUrl url("https://query1.finance.yahoo.com/v1/finance/screener/predefined/saved");
    url.setQuery(QString("scrIds=%1&count=%2").arg(id).arg(count));
    get(QString("yahoo:screener:%1:%2").arg(id).arg(count), url, 2 * 60, Browser, ctx,
        [done](const QByteArray &body, const QString &err) {
            QVector<QJsonObject> out;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("finance").toObject().value("result")
                                           .toArray().first().toObject().value("quotes").toArray())
                out.push_back(v.toObject());
            done(out, out.isEmpty() && err.isEmpty() ? tr("Empty screener") : err);
        });
}

namespace {

// The Treasury's CSV: "Date,"1 Mo",…" then one line per day, newest first ("10/30/2026,4.06,…").
QVector<YieldCurveDay> readYieldCurve(const QByteArray &body)
{
    QVector<YieldCurveDay> out;
    const QList<QByteArray> lines = body.split('\n');
    if (lines.isEmpty())
        return out;
    QStringList head;
    for (QByteArray h : lines.first().trimmed().split(','))
        head << QString::fromUtf8(h.replace('"', "")).trimmed();
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> cells = lines[i].trimmed().split(',');
        if (cells.size() < 2)
            continue;
        YieldCurveDay d;
        d.date = QDate::fromString(QString::fromUtf8(cells[0]).remove('"'), "MM/dd/yyyy");
        if (!d.date.isValid())
            continue;
        for (int c = 1; c < cells.size() && c < head.size(); ++c) {
            bool good = false;
            const double v = cells[c].toDouble(&good);
            if (good)
                d.yields.insert(head[c], v);
        }
        out.push_back(d);
    }
    std::sort(out.begin(), out.end(), [](const YieldCurveDay &a, const YieldCurveDay &b) { return a.date > b.date; });
    return out;
}

QUrl yieldCurveUrl(int year)
{
    return QUrl(QString("https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/"
                        "%1/all?type=daily_treasury_yield_curve&field_tdr_date_value=%1&page&_format=csv").arg(year));
}

} // namespace

void Client::treasuryYields(QObject *ctx, Done<QVector<YieldCurveDay>> done)
{
    const int year = QDate::currentDate().year();
    QPointer<QObject> guard(ctx);
    // Published once a day, after the close.
    get(QString("treasury:yields:%1").arg(year), yieldCurveUrl(year), 3600, Browser, ctx,
        [this, year, guard, done](const QByteArray &body, const QString &err) {
            QVector<YieldCurveDay> days = readYieldCurve(body);
            if (days.size() >= 2) {
                done(days, {});
                return;
            }
            // Early January: the change needs last year's last day.
            get(QString("treasury:yields:%1").arg(year - 1), yieldCurveUrl(year - 1), 24 * 3600, Browser, guard,
                [days, err, done](const QByteArray &prevBody, const QString &prevErr) mutable {
                    days += readYieldCurve(prevBody);
                    done(days, days.isEmpty() ? (err.isEmpty() ? prevErr : err) : QString());
                });
        });
}

// ---------------------------------------------------------------- Economy

namespace {

// FRED's graph CSV: "observation_date,VIXCLS" then "2026-10-29,17.20" (empty or "." when missing).
Series readFredCsv(const QByteArray &body)
{
    Series s;
    const QList<QByteArray> lines = body.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        const int comma = line.indexOf(',');
        if (comma < 0)
            continue;
        bool ok = false;
        const double v = line.mid(comma + 1).toDouble(&ok);
        const QDate d = QDate::fromString(QString::fromLatin1(line.left(comma)), Qt::ISODate);
        if (ok && d.isValid())
            s.push_back({d, v});
    }
    return s;
}

// FRED's API: {"observations": [{"date": "2026-10-29", "value": "17.20"}…]}.
Series readFredJson(const QByteArray &body)
{
    Series s;
    for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("observations").toArray()) {
        const QJsonObject o = v.toObject();
        bool ok = false;
        const double value = o.value("value").toString().toDouble(&ok);
        const QDate d = QDate::fromString(o.value("date").toString(), Qt::ISODate);
        if (ok && d.isValid())
            s.push_back({d, value});
    }
    return s;
}

void sortByDate(Series &s)
{
    std::sort(s.begin(), s.end(), [](const Point &a, const Point &b) { return a.date < b.date; });
    // One value per date (the last one read).
    Series out;
    for (const Point &p : s) {
        if (!out.isEmpty() && out.back().date == p.date)
            out.back() = p;
        else
            out.push_back(p);
    }
    s = out;
}

} // namespace

void Client::fredSeries(const QString &id, const QDate &since, QObject *ctx, Done<Series> done)
{
    const QString key = sourceKey("fred");
    const QString start = since.isValid() ? since.toString(Qt::ISODate) : QString();
    QUrl url;
    if (!key.isEmpty()) {
        url = QUrl("https://api.stlouisfed.org/fred/series/observations");
        QUrlQuery q;
        q.addQueryItem("series_id", id);
        q.addQueryItem("api_key", key);
        q.addQueryItem("file_type", "json");
        if (!start.isEmpty())
            q.addQueryItem("observation_start", start);
        url.setQuery(q);
    } else {
        url = QUrl("https://fred.stlouisfed.org/graph/fredgraph.csv");
        QUrlQuery q;
        q.addQueryItem("id", id);
        if (!start.isEmpty())
            q.addQueryItem("cosd", start);
        url.setQuery(q);
    }
    const bool api = !key.isEmpty();
    get(QString("fred:%1:%2:%3").arg(id, start, api ? "api" : "csv"), url, 6 * 3600, 0, ctx,
        [done, api](const QByteArray &body, const QString &err) {
            Series s = api ? readFredJson(body) : readFredCsv(body);
            sortByDate(s);
            QString error = s.isEmpty() && err.isEmpty() ? tr("No observations") : err;
            // FRED's public CSV turns away requests that don't come from a browser; its API takes a free key.
            if (s.isEmpty() && !api)
                error = tr("FRED refused the download (%1). A free FRED key, saved in Sources › Map · databases with "
                           "a free key › FRED, makes it use FRED's API.").arg(error);
            done(s, error);
        });
}

void Client::oecdSeries(const QString &flowKey, const QDate &since, QObject *ctx, Done<Series> done)
{
    QUrl url("https://sdmx.oecd.org/public/rest/data/OECD.SDD.STES,DSD_KEI@DF_KEI,4.0/" + flowKey);
    QUrlQuery q;
    if (since.isValid())
        q.addQueryItem("startPeriod", since.toString("yyyy-MM"));
    q.addQueryItem("dimensionAtObservation", "AllDimensions");
    q.addQueryItem("format", "csv");
    url.setQuery(q);
    get(QString("oecd:%1:%2").arg(flowKey, since.toString("yyyy-MM")), url, 24 * 3600, 0, ctx,
        [done](const QByteArray &body, const QString &err) {
            // A CSV with a header naming the columns; rows aren't in date order.
            Series s;
            const QList<QByteArray> lines = body.split('\n');
            if (!lines.isEmpty()) {
                const QList<QByteArray> head = lines[0].trimmed().split(',');
                const int period = head.indexOf("TIME_PERIOD"), value = head.indexOf("OBS_VALUE");
                for (int i = 1; i < lines.size() && period >= 0 && value >= 0; ++i) {
                    const QList<QByteArray> cells = lines[i].trimmed().split(',');
                    if (cells.size() <= std::max(period, value))
                        continue;
                    bool ok = false;
                    const double v = cells[value].toDouble(&ok);
                    const QDate d = QDate::fromString(QString::fromLatin1(cells[period]) + "-01", Qt::ISODate);
                    if (ok && d.isValid())
                        s.push_back({d, v});
                }
            }
            sortByDate(s);
            done(s, s.isEmpty() && err.isEmpty() ? tr("No observations") : err);
        });
}

void Client::raw(const QString &key, const QUrl &url, qint64 maxAgeSecs, bool json, QObject *ctx, Done<QByteArray> done)
{
    get(key, url, maxAgeSecs, json ? Json : 0, ctx, [done](const QByteArray &body, const QString &err) { done(body, err); });
}

void Client::raw(const QString &key, const QUrl &url, qint64 maxAgeSecs, bool json,
                 const QList<std::pair<QByteArray, QByteArray>> &headers, QObject *ctx, Done<QByteArray> done)
{
    m_pendingHeaders.insert(key, headers);
    get(key, url, maxAgeSecs, json ? Json : 0, ctx, [done](const QByteArray &body, const QString &err) { done(body, err); });
    m_pendingHeaders.remove(key);   // served from the cache, or already queued with them
}

void Client::cryptoFearGreed(QObject *ctx, Done<Series> done)
{
    get("alternative:fng", QUrl("https://api.alternative.me/fng/?limit=0&format=json"), 3600, 0, ctx,
        [done](const QByteArray &body, const QString &err) {
            Series s;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("data").toArray()) {
                const QJsonObject o = v.toObject();
                bool ok = false;
                const double value = o.value("value").toString().toDouble(&ok);
                const qint64 ts = o.value("timestamp").toString().toLongLong();
                if (ok && ts > 0)
                    s.push_back({fromEpoch(ts), value});
            }
            sortByDate(s);
            done(s, s.isEmpty() && err.isEmpty() ? tr("No observations") : err);
        });
}

void Client::altcoinSeason(QObject *ctx, Done<Series> done)
{
    // Asking from 2020 returns what there is (it starts in March 2024).
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QUrl url(QString("https://api.coinmarketcap.com/data-api/v3/altcoin-season/chart?start=1577836800&end=%1")
                       .arg(now - now % 3600));
    get("coinmarketcap:altseason", url, 3600, 0, ctx, [done](const QByteArray &body, const QString &err) {
        Series s;
        for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("data").toObject().value("points").toArray()) {
            const QJsonObject o = v.toObject();
            bool ok = false;
            const double value = o.value("altcoinIndex").toString().toDouble(&ok);
            const qint64 ts = o.value("timestamp").toString().toLongLong();
            if (ok && ts > 0)
                s.push_back({fromEpoch(ts), value});
        }
        sortByDate(s);
        done(s, s.isEmpty() && err.isEmpty() ? tr("No observations") : err);
    });
}

namespace {

void readCryptoMarket(const QByteArray &body, std::map<QDate, std::pair<double, double>> *out)
{
    for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("data").toObject().value("points").toArray()) {
        const QJsonObject o = v.toObject();
        const double cap = o.value("marketCap").toDouble(NaN), btc = o.value("btcValue").toDouble(NaN);
        const qint64 ts = o.value("timestamp").toVariant().toLongLong();
        if (ts > 0 && std::isfinite(cap) && cap > 0)
            (*out)[fromEpoch(ts)] = {cap, std::isfinite(btc) ? 100 * btc / cap : NaN};
    }
}

} // namespace

void Client::cryptoMarket(QObject *ctx, Done<CryptoMarket> done)
{
    // The last year day by day, and everything since 2022 week by week for the longer ranges.
    const QString base = "https://api.coinmarketcap.com/data-api/v4/global-metrics/quotes/historical?convertId=2781&range=";
    QPointer<QObject> guard(ctx);
    get("coinmarketcap:global:1y", QUrl(base + "1y"), 3600, 0, ctx,
        [this, base, guard, done](const QByteArray &daily, const QString &err) {
            get("coinmarketcap:global:all", QUrl(base + "all"), 6 * 3600, 0, guard,
                [daily, err, done](const QByteArray &weekly, const QString &weeklyErr) {
                    std::map<QDate, std::pair<double, double>> recent, older;
                    readCryptoMarket(daily, &recent);
                    readCryptoMarket(weekly, &older);
                    const QDate first = recent.empty() ? QDate() : recent.begin()->first;
                    for (const auto &[d, v] : older)
                        if (!first.isValid() || d < first)
                            recent[d] = v;
                    CryptoMarket m;
                    for (const auto &[d, v] : recent) {
                        m.cap.push_back({d, v.first});
                        if (std::isfinite(v.second))
                            m.btcDominance.push_back({d, v.second});
                    }
                    done(m, m.cap.isEmpty() ? (err.isEmpty() ? weeklyErr : err) : QString());
                });
        });
}

void Client::statement(const QString &symbol, const QString &kind, bool quarterly, QObject *ctx, Done<Statement> done)
{
    QStringList lines;
    for (const StatementLine &l : statementLayout(kind))
        lines << l.key;
    const QUrl url = timeseriesUrl(symbol, lines, !quarterly, quarterly);
    get(QString("yahoo:statement:%1:%2:%3").arg(symbol, kind, quarterly ? "q" : "a"), url, 12 * 3600, Browser, ctx,
        [done, quarterly](const QByteArray &body, const QString &err) {
            QHash<QString, std::map<QDate, double>> a, q;
            Statement s;
            readTimeseries(body, &a, &q, &s.currency);
            const auto &src = quarterly ? q : a;
            std::set<QDate> ends;
            for (auto it = src.begin(); it != src.end(); ++it)
                for (const auto &[d, v] : it.value())
                    ends.insert(d);
            for (auto it = ends.rbegin(); it != ends.rend(); ++it)
                s.ends.push_back(*it);
            for (auto it = src.begin(); it != src.end(); ++it) {
                QVector<double> values(s.ends.size(), NaN);
                for (int i = 0; i < s.ends.size(); ++i) {
                    auto f = it.value().find(s.ends[i]);
                    if (f != it.value().end())
                        values[i] = f->second;
                }
                s.lines.insert(it.key(), values);
            }
            done(s, s.ends.isEmpty() && err.isEmpty() ? tr("No statements for this symbol") : err);
        });
}

void Client::secCik(const QString &symbol, QObject *ctx, Done<QString> done)
{
    QUrl url("https://efts.sec.gov/LATEST/search-index");
    url.setQuery("keysTyped=" + QUrl::toPercentEncoding(symbol));
    get("sec:cik:" + symbol, url, 30 * 86400, Sec, ctx, [done, symbol](const QByteArray &body, const QString &err) {
        const QJsonArray hits = QJsonDocument::fromJson(body).object().value("hits").toObject().value("hits").toArray();
        QString wanted = symbol.toUpper();
        wanted.replace('-', '.');   // BRK-B is filed as BRK.B… and sometimes BRK-B
        for (const QJsonValue &h : hits) {
            const QJsonObject o = h.toObject();
            static const QRegularExpression tickerBreak("[, ]+");
            const QStringList tickers = o.value("_source").toObject().value("tickers").toString().split(tickerBreak);
            for (const QString &t : tickers)
                if (t.compare(wanted, Qt::CaseInsensitive) == 0 || t.compare(symbol, Qt::CaseInsensitive) == 0) {
                    done(o.value("_id").toString(), {});
                    return;
                }
        }
        done({}, err.isEmpty() ? tr("Not an SEC filer") : err);
    });
}

void Client::fundamentals(const QString &symbol, QObject *ctx, Done<Fundamentals> done)
{
    QPointer<QObject> guard(ctx);
    auto fromYahoo = [this, symbol, guard, done](const QString &why) {
        if (!guard)
            return;
        QStringList lines;
        for (const auto &[id, line] : yahooFundamentalLines())
            lines << line;
        get("yahoo:fundamentals:" + symbol, timeseriesUrl(symbol, lines, true, true), 12 * 3600, Browser, guard,
            [done, why](const QByteArray &body, const QString &err) {
                Fundamentals f = readFundamentals(body);
                done(f, f.isEmpty() ? (err.isEmpty() ? why : err) : QString());
            }, yahooFundamentalsToCompact);
    };
    // Only plain US-style tickers can be SEC filers.
    if (symbol.contains('.') || symbol.contains('^') || symbol.contains('=')) {
        fromYahoo(tr("No SEC filings"));
        return;
    }
    secCik(symbol, ctx, [this, symbol, guard, done, fromYahoo](const QString &cik, const QString &) {
        if (!guard)
            return;
        if (cik.isEmpty()) {
            fromYahoo(tr("No SEC filings"));
            return;
        }
        // The splits first: per-share figures filed before one are divided by it.
        QUrl splitsUrl("https://query1.finance.yahoo.com/v8/finance/chart/" + QUrl::toPercentEncoding(symbol));
        splitsUrl.setQuery("range=max&interval=3mo&events=split");
        get("yahoo:splits:" + symbol, splitsUrl, 24 * 3600, Browser, guard,
            [this, symbol, cik, guard, done, fromYahoo](const QByteArray &splitsBody, const QString &) {
            if (!guard)
                return;
            Splits splits;
            const QJsonObject events = QJsonDocument::fromJson(splitsBody).object().value("chart").toObject().value("result")
                                           .toArray().first().toObject().value("events").toObject().value("splits").toObject();
            for (const QJsonValue &v : events) {
                const QJsonObject o = v.toObject();
                const double num = o.value("numerator").toDouble(), den = o.value("denominator").toDouble();
                if (num > 0 && den > 0)
                    splits.push_back({fromEpoch(o.value("date").toInteger()), num / den});
            }
            const QUrl url(QString("https://data.sec.gov/api/xbrl/companyfacts/CIK%1.json").arg(cik.toLongLong(), 10, 10, QChar('0')));
            get("sec:facts2:" + symbol, url, 24 * 3600, Sec, guard, [done, fromYahoo](const QByteArray &body, const QString &err) {
                Fundamentals f = readFundamentals(body);
                if (f.isEmpty())
                    fromYahoo(err);
                else
                    done(f, {});
            }, [splits](const QByteArray &body) { return compactSecFacts(body, splits); });
        });
    });
}

void Client::prices(const QString &symbol, const QString &range, const QString &interval, QObject *ctx, Done<Series> done)
{
    QUrl url("https://query1.finance.yahoo.com/v8/finance/chart/" + QUrl::toPercentEncoding(symbol));
    url.setQuery(QString("range=%1&interval=%2&includePrePost=false").arg(range, interval));
    get(QString("yahoo:chart:%1:%2:%3").arg(symbol, range, interval), url, interval.endsWith('m') ? 5 * 60 : 3600,
        Browser, ctx, [done](const QByteArray &body, const QString &err) {
            const QJsonObject r = QJsonDocument::fromJson(body).object().value("chart").toObject().value("result")
                                      .toArray().first().toObject();
            const QJsonArray ts = r.value("timestamp").toArray();
            const QJsonArray close = r.value("indicators").toObject().value("quote").toArray().first().toObject()
                                         .value("close").toArray();
            Series s;
            for (int i = 0; i < ts.size() && i < close.size(); ++i)
                if (close[i].isDouble())
                    s.push_back({fromEpoch(ts[i].toInteger()), close[i].toDouble()});
            done(s, s.isEmpty() && err.isEmpty() ? tr("No prices") : err);
        });
}

void Client::candles(const QString &symbol, const QString &range, const QString &interval, QObject *ctx, Done<QVector<Bar>> done)
{
    QUrl url("https://query1.finance.yahoo.com/v8/finance/chart/" + QUrl::toPercentEncoding(symbol));
    url.setQuery(QString("range=%1&interval=%2&includePrePost=false").arg(range, interval));
    get(QString("yahoo:candles:%1:%2:%3").arg(symbol, range, interval), url, interval.endsWith('m') ? 5 * 60 : 3600,
        Browser, ctx, [done](const QByteArray &body, const QString &err) {
            const QJsonObject r = QJsonDocument::fromJson(body).object().value("chart").toObject().value("result")
                                      .toArray().first().toObject();
            const QJsonArray ts = r.value("timestamp").toArray();
            const QJsonObject q = r.value("indicators").toObject().value("quote").toArray().first().toObject();
            const QJsonArray open = q.value("open").toArray(), high = q.value("high").toArray(), low = q.value("low").toArray(),
                             close = q.value("close").toArray(), volume = q.value("volume").toArray();
            QVector<Bar> bars;
            for (int i = 0; i < ts.size() && i < close.size(); ++i) {
                if (!close[i].isDouble())
                    continue;
                Bar b;
                b.time = QDateTime::fromSecsSinceEpoch(ts[i].toInteger(), QTimeZone::UTC);
                b.open = open.at(i).toDouble(NaN);
                b.high = high.at(i).toDouble(NaN);
                b.low = low.at(i).toDouble(NaN);
                b.close = close[i].toDouble();
                b.volume = volume.at(i).toDouble(NaN);
                bars.push_back(b);
            }
            done(bars, bars.isEmpty() && err.isEmpty() ? tr("No prices") : err);
        });
}

void Client::spark(const QStringList &symbols, const QString &range, QObject *ctx, Done<QHash<QString, Series>> done)
{
    // 20 symbols per request at most.
    struct State {
        QHash<QString, Series> out;
        int left = 0;
        QString error;
    };
    auto state = std::make_shared<State>();
    QStringList sorted = symbols;
    sorted.sort();
    sorted.removeDuplicates();
    QList<QStringList> chunks;
    for (int i = 0; i < sorted.size(); i += 20)
        chunks << sorted.mid(i, 20);
    state->left = chunks.size();
    if (chunks.isEmpty()) {
        done({}, {});
        return;
    }
    for (const QStringList &chunk : chunks) {
        QUrl url("https://query1.finance.yahoo.com/v7/finance/spark");
        url.setQuery(QString("symbols=%1&range=%2&interval=1d").arg(QString::fromUtf8(QUrl::toPercentEncoding(chunk.join(','), ",^")), range));
        get(QString("yahoo:spark:%1:%2").arg(range, chunk.join(',')), url, range == "5d" ? 5 * 60 : 3 * 3600, Browser, ctx,
            [state, done](const QByteArray &body, const QString &err) {
                for (const QJsonValue &rv : QJsonDocument::fromJson(body).object().value("spark").toObject()
                                                .value("result").toArray()) {
                    const QJsonObject r = rv.toObject();
                    const QJsonObject resp = r.value("response").toArray().first().toObject();
                    const QJsonArray ts = resp.value("timestamp").toArray();
                    const QJsonArray close = resp.value("indicators").toObject().value("quote").toArray().first()
                                                 .toObject().value("close").toArray();
                    Series s;
                    for (int i = 0; i < ts.size() && i < close.size(); ++i)
                        if (close[i].isDouble())
                            s.push_back({fromEpoch(ts[i].toInteger()), close[i].toDouble()});
                    state->out.insert(r.value("symbol").toString(), s);
                }
                if (!err.isEmpty())
                    state->error = err;
                if (--state->left == 0)
                    done(state->out, state->error);
            });
    }
}

void Client::holders(const QString &symbol, QObject *ctx, Done<Holders> done)
{
    QUrl url("https://api.nasdaq.com/api/company/" + QUrl::toPercentEncoding(symbol.toUpper().replace('-', '.'))
             + "/institutional-holdings");
    url.setQuery("limit=25&type=TOTAL&sortColumn=marketValue&sortOrder=DESC");
    get("nasdaq:holders:" + symbol, url, 24 * 3600, Nasdaq, ctx, [done](const QByteArray &body, const QString &err) {
        const QJsonObject d = QJsonDocument::fromJson(body).object().value("data").toObject();
        Holders h;
        h.institutionalPct = nasdaqNumber(d.value("ownershipSummary").toObject().value("SharesOutstandingPCT").toObject()
                                              .value("value").toString()) / 100.0;
        const QJsonObject tx = d.value("holdingsTransactions").toObject();
        h.count = int(nasdaqNumber(tx.value("totalRecords").toString()));
        for (const QJsonValue &rv : tx.value("table").toObject().value("rows").toArray()) {
            const QJsonObject r = rv.toObject();
            Holder x;
            x.name = r.value("ownerName").toString();
            x.date = usDate(r.value("date").toString());
            x.shares = nasdaqNumber(r.value("sharesHeld").toString());
            x.change = nasdaqNumber(r.value("sharesChange").toString());
            x.changePct = nasdaqNumber(r.value("sharesChangePCT").toString()) / 100.0;
            x.value = nasdaqNumber(r.value("marketValue").toString()) * 1000.0;   // in thousands
            h.top.push_back(x);
        }
        done(h, h.top.isEmpty() && err.isEmpty() ? tr("No institutional holders reported") : err);
    });
}

void Client::insiders(const QString &symbol, QObject *ctx, Done<QVector<InsiderTrade>> done)
{
    QUrl url("https://api.nasdaq.com/api/company/" + QUrl::toPercentEncoding(symbol.toUpper().replace('-', '.'))
             + "/insider-trades");
    url.setQuery("limit=200&type=ALL&sortColumn=lastDate&sortOrder=DESC");
    get("nasdaq:insiders:" + symbol, url, 12 * 3600, Nasdaq, ctx, [done](const QByteArray &body, const QString &err) {
        QVector<InsiderTrade> out;
        const QJsonObject d = QJsonDocument::fromJson(body).object().value("data").toObject();
        for (const QJsonValue &rv : d.value("transactionTable").toObject().value("table").toObject().value("rows").toArray()) {
            const QJsonObject r = rv.toObject();
            InsiderTrade t;
            t.date = usDate(r.value("lastDate").toString());
            t.name = r.value("insider").toString();
            t.relation = r.value("relation").toString();
            t.type = r.value("transactionType").toString();
            t.shares = nasdaqNumber(r.value("sharesTraded").toString());
            t.price = nasdaqNumber(r.value("lastPrice").toString());
            t.held = nasdaqNumber(r.value("sharesHeld").toString());
            t.buy = t.type.contains("Buy", Qt::CaseInsensitive) || t.type.contains("Acquisition", Qt::CaseInsensitive);
            t.sell = t.type.contains("Sell", Qt::CaseInsensitive) || t.type.contains("Disposition", Qt::CaseInsensitive);
            // "Sell", "Automatic Sell", "Buy", "Automatic Buy" happen on the market; "Disposition (Non Open
            // Market)", "Acquisition (Non Open Market)" and "Option Execute" don't.
            t.openMarket = !t.type.contains("Non Open Market", Qt::CaseInsensitive)
                           && (t.type.contains("Sell", Qt::CaseInsensitive) || t.type.contains("Buy", Qt::CaseInsensitive));
            out.push_back(t);
        }
        done(out, out.isEmpty() && err.isEmpty() ? tr("No insider trades reported") : err);
    });
}

void Client::peers(const QString &symbol, QObject *ctx, Done<QStringList> done)
{
    const QUrl url("https://query2.finance.yahoo.com/v6/finance/recommendationsbysymbol/" + QUrl::toPercentEncoding(symbol));
    get("yahoo:peers:" + symbol, url, 7 * 86400, Browser, ctx, [done](const QByteArray &body, const QString &err) {
        QStringList out;
        for (const QJsonValue &r : QJsonDocument::fromJson(body).object().value("finance").toObject().value("result").toArray())
            for (const QJsonValue &s : r.toObject().value("recommendedSymbols").toArray())
                out << s.toObject().value("symbol").toString();
        done(out, err);
    });
}

void Client::employees(const QString &symbol, QObject *ctx, Done<Series> done)
{
    // Head counts with their date, on the Wikidata item listed under this ticker.
    const QString query = QString(
        "SELECT ?n ?date WHERE { ?item p:P414 ?st . ?st pq:P249 \"%1\" . ?item p:P1128 ?e . ?e ps:P1128 ?n . "
        "?e pq:P585 ?date } ORDER BY ?date").arg(QString(symbol).replace('"', ""));
    QUrl url("https://query.wikidata.org/sparql");
    url.setQuery("query=" + QString::fromLatin1(QUrl::toPercentEncoding(query)), QUrl::StrictMode);
    get("wikidata:employees:" + symbol, url, 30 * 86400, Wiki, ctx, [done](const QByteArray &body, const QString &err) {
        std::map<int, double> byYear;   // one point a year: the last
        for (const QJsonValue &b : QJsonDocument::fromJson(body).object().value("results").toObject().value("bindings").toArray()) {
            const QJsonObject o = b.toObject();
            const QDate d = QDateTime::fromString(o.value("date").toObject().value("value").toString(), Qt::ISODate).date();
            const double n = o.value("n").toObject().value("value").toString().toDouble();
            if (d.isValid() && n > 0)
                byYear[d.year()] = n;
        }
        Series s;
        for (const auto &[y, n] : byYear)
            s.push_back({QDate(y, 12, 31), n});
        done(s, err);
    });
}

namespace {

// Wikidata's ticker for a Yahoo symbol: "MC.PA" -> "MC", "BRK-B" -> "BRK.B".
QString wikidataTicker(const QString &symbol)
{
    QString t = symbol.toUpper();
    static const QRegularExpression suffix("\\.[A-Z]{1,3}$");
    if (t.contains('.'))
        t.remove(suffix);
    return t.replace('-', '.');
}

// A listing on Wikidata ("Taiwan Stock Exchange", "2317") as a Yahoo symbol ("2317.TW"); empty for exchanges Yahoo
// doesn't carry. `rank` orders the choices: US listings first, then home exchanges, then over the counter.
QString yahooSymbol(const QString &exchange, QString ticker, int *rank)
{
    static const QVector<std::pair<const char *, const char *>> suffixes {
        {"Taiwan Stock Exchange", ".TW"}, {"Taipei Exchange", ".TWO"}, {"Tokyo Stock Exchange", ".T"},
        {"Korea Exchange", ".KS"}, {"Korean Stock Exchange", ".KS"}, {"KOSDAQ", ".KQ"}, {"Hong Kong Stock Exchange", ".HK"},
        {"Shanghai Stock Exchange", ".SS"}, {"Shenzhen Stock Exchange", ".SZ"}, {"London Stock Exchange", ".L"},
        {"Euronext Paris", ".PA"}, {"Euronext Amsterdam", ".AS"}, {"Euronext Brussels", ".BR"}, {"Xetra", ".DE"},
        {"Frankfurt Stock Exchange", ".F"}, {"SIX Swiss Exchange", ".SW"}, {"Swiss Stock Exchange", ".SW"},
        {"Toronto Stock Exchange", ".TO"}, {"Italian Stock Exchange", ".MI"}, {"Borsa Italiana", ".MI"},
        {"Madrid Stock Exchange", ".MC"}, {"Bolsa de Madrid", ".MC"}, {"National Stock Exchange of India", ".NS"},
        {"Bombay Stock Exchange", ".BO"}, {"Australian Securities Exchange", ".AX"}, {"Nasdaq Stockholm", ".ST"},
        {"Oslo Stock Exchange", ".OL"}, {"Nasdaq Copenhagen", ".CO"}, {"Nasdaq Helsinki", ".HE"},
        {"São Paulo Stock Exchange", ".SA"}, {"B3", ".SA"},
    };
    ticker = ticker.trimmed();
    if (ticker.isEmpty())
        return {};
    if (exchange == "Nasdaq" || exchange == "New York Stock Exchange" || exchange == "NYSE American") {
        *rank = 0;
        return ticker.replace('.', '-');
    }
    for (int i = 0; i < suffixes.size(); ++i)
        if (exchange == QLatin1String(suffixes[i].first)) {
            *rank = 1 + i;
            if (QByteArray(suffixes[i].second) == ".HK")
                ticker = ticker.rightJustified(4, '0');
            return ticker + suffixes[i].second;
        }
    if (exchange.startsWith("OTC")) {
        *rank = 100;
        return ticker;
    }
    return {};
}

} // namespace

void Client::supplyChain(const QString &symbol, QObject *ctx, Done<SupplyChain> done)
{
    // Two queries return one row per product and linked company, with what's needed to sort them out here
    // (the aggregated query with these tests runs past Wikidata's time limit):
    //   suppliers: a manufacturer of the company's products, unless it's also the product's brand or developer
    //              (Nintendo for the Switch), the product belongs to another brand (Apple's chips made by TSMC
    //              aren't TSMC's products), or three or more companies make it (shared process nodes);
    //   customers: the brand or developer of a product the company makes.
    // A brand that isn't a company counts for its listed owner; the company's own subsidiaries are left out.
    const QString ticker = wikidataTicker(symbol).replace('"', "");
    const QString common = QString(
        "  BIND(EXISTS { ?b wdt:P452 [] } || EXISTS { ?b wdt:P414 [] } AS ?bCompany)\n"
        "  BIND(EXISTS { ?b wdt:P749|wdt:P127 ?co } AS ?bSub)\n"
        "  OPTIONAL { FILTER(!?bCompany) ?b wdt:P749|wdt:P127 ?owner . ?owner wdt:P414 [] . }\n"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en,mul,zh,ja,ko,fr,de\". }\n"
        "} LIMIT 4000");
    const QString supplierQuery = QString(
        "SELECT ?co ?p ?pLabel ?b ?bLabel ?isBrand ?coBrand ?hasBrand ?bCompany ?bSub ?owner ?ownerLabel WHERE {\n"
        "  ?co p:P414/pq:P249 \"%1\" .\n"
        "  ?p wdt:P1716|wdt:P178|wdt:P176 ?co . ?p wdt:P176 ?b . FILTER(?b != ?co)\n"
        "  BIND(EXISTS { ?p wdt:P1716|wdt:P178 ?b } AS ?isBrand)\n"
        "  BIND(EXISTS { ?p wdt:P1716|wdt:P178 ?co } || EXISTS { ?p wdt:P1716|wdt:P178 ?x . ?x wdt:P127|wdt:P749 ?co } AS ?coBrand)\n"
        "  BIND(EXISTS { ?p wdt:P1716|wdt:P178 [] } AS ?hasBrand)\n").arg(ticker) + common;
    const QString customerQuery = QString(
        "SELECT ?co ?p ?pLabel ?b ?bLabel ?bCompany ?bSub ?owner ?ownerLabel WHERE {\n"
        "  ?co p:P414/pq:P249 \"%1\" .\n"
        "  ?p wdt:P176 ?co . ?p wdt:P1716|wdt:P178 ?b . FILTER(?b != ?co)\n").arg(ticker) + common;
    auto sparqlUrl = [](const QString &q) {
        QUrl url("https://query.wikidata.org/sparql");
        url.setQuery("query=" + QString::fromLatin1(QUrl::toPercentEncoding(q)), QUrl::StrictMode);
        return url;
    };
    // Rows -> partners (company id -> name, products).
    auto aggregate = [](const QByteArray &body, bool suppliers, QString *coItem) {
        const QJsonArray rows = QJsonDocument::fromJson(body).object().value("results").toObject().value("bindings").toArray();
        auto val = [](const QJsonObject &r, const char *k) { return r.value(k).toObject().value("value").toString(); };
        QHash<QString, QSet<QString>> makers;   // product -> its other manufacturers
        for (const QJsonValue &v : rows)
            makers[val(v.toObject(), "p")].insert(val(v.toObject(), "b"));
        QHash<QString, Partner> byItem;
        for (const QJsonValue &v : rows) {
            const QJsonObject r = v.toObject();
            const QString co = val(r, "co").section('/', -1);
            if (coItem && coItem->isEmpty())
                *coItem = co;
            if (val(r, "bSub") == "true")
                continue;
            if (suppliers) {
                if (val(r, "isBrand") == "true")
                    continue;
                if (val(r, "coBrand") != "true" && val(r, "hasBrand") == "true")
                    continue;
                if (makers.value(val(r, "p")).size() + 1 >= 3)
                    continue;
            }
            QString item, name;
            if (val(r, "bCompany") == "true") {
                item = val(r, "b");
                name = val(r, "bLabel");
            } else if (!val(r, "owner").isEmpty()) {
                item = val(r, "owner");
                name = val(r, "ownerLabel");
            } else {
                continue;
            }
            item = item.section('/', -1);
            if (item == co)
                continue;
            Partner &p = byItem[item];
            p.item = item;
            p.name = name;
            const QString product = val(r, "pLabel");
            if (!p.products.contains(product))
                p.products << product;
        }
        QVector<Partner> out = byItem.values().toVector();
        std::sort(out.begin(), out.end(), [](const Partner &a, const Partner &b) {
            return a.products.size() != b.products.size() ? a.products.size() > b.products.size() : a.name < b.name;
        });
        if (out.size() > 12)
            out.resize(12);
        return out;
    };
    struct State {
        SupplyChain chain;
        int left = 2;
        QString error;
    };
    auto state = std::make_shared<State>();
    QPointer<QObject> guard(ctx);
    auto withTickers = [this, state, guard, done, sparqlUrl] {
        if (--state->left > 0 || !guard)
            return;
        // A company on both sides (a few odd product records) stays on the side with more products.
        auto &sup = state->chain.suppliers, &cus = state->chain.customers;
        for (int i = sup.size() - 1; i >= 0; --i)
            for (int j = cus.size() - 1; j >= 0; --j)
                if (i < sup.size() && sup[i].item == cus[j].item) {
                    if (sup[i].products.size() >= cus[j].products.size())
                        cus.removeAt(j);
                    else {
                        sup.removeAt(i);
                        break;
                    }
                }
        QStringList items;
        for (const auto *list : {&state->chain.suppliers, &state->chain.customers})
            for (const Partner &p : *list)
                items << "wd:" + p.item;
        items.removeDuplicates();
        if (items.isEmpty()) {
            done(state->chain, state->error);
            return;
        }
        const QString q = QString("SELECT ?c ?exLabel ?tk WHERE { VALUES ?c { %1 } ?c p:P414 ?st . ?st ps:P414 ?ex ; pq:P249 ?tk . "
                                  "?ex rdfs:label ?exLabel . FILTER(lang(?exLabel) = \"en\") }").arg(items.join(' '));
        items.sort();
        get("wikidata:listings:" + QString::fromLatin1(QCryptographicHash::hash(items.join(',').toUtf8(), QCryptographicHash::Md5).toHex()),
            sparqlUrl(q), 7 * 86400, Wiki, guard, [state, done](const QByteArray &body, const QString &err) {
            struct Best { QString symbol, listing; int rank = 1000; };
            QHash<QString, Best> best;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("results").toObject().value("bindings").toArray()) {
                const QJsonObject r = v.toObject();
                const QString item = r.value("c").toObject().value("value").toString().section('/', -1);
                const QString ex = r.value("exLabel").toObject().value("value").toString();
                const QString tk = r.value("tk").toObject().value("value").toString();
                int rank = 1000;
                const QString sym = yahooSymbol(ex, tk, &rank);
                Best &b = best[item];
                if (b.listing.isEmpty() || (!sym.isEmpty() && (rank < b.rank || (rank == b.rank && sym < b.symbol)))) {
                    if (!sym.isEmpty()) {
                        b.symbol = sym;
                        b.rank = rank;
                    }
                    if (!sym.isEmpty() || b.listing.isEmpty())
                        b.listing = ex + ": " + tk;
                }
            }
            for (auto *list : {&state->chain.suppliers, &state->chain.customers})
                for (Partner &p : *list) {
                    p.symbol = best.value(p.item).symbol;
                    p.listing = best.value(p.item).listing;
                }
            done(state->chain, state->error.isEmpty() ? err : state->error);
        });
    };
    get("wikidata:suppliers:" + ticker, sparqlUrl(supplierQuery), 7 * 86400, Wiki, ctx,
        [state, aggregate, withTickers](const QByteArray &body, const QString &err) {
            state->chain.suppliers = aggregate(body, true, &state->chain.item);
            if (!err.isEmpty()) state->error = err;
            withTickers();
        });
    get("wikidata:customers:" + ticker, sparqlUrl(customerQuery), 7 * 86400, Wiki, ctx,
        [state, aggregate, withTickers](const QByteArray &body, const QString &err) {
            state->chain.customers = aggregate(body, false, &state->chain.item);
            if (!err.isEmpty()) state->error = err;
            withTickers();
        });
}

void Client::search(const QString &query, QObject *ctx, Done<QVector<SearchHit>> done)
{
    QUrl url("https://query1.finance.yahoo.com/v1/finance/search");
    QUrlQuery q;
    q.addQueryItem("q", query);
    q.addQueryItem("quotesCount", "10");
    q.addQueryItem("newsCount", "0");
    q.addQueryItem("listsCount", "0");
    url.setQuery(q);
    get("yahoo:search:" + query.toLower(), url, 86400, Browser, ctx, [done](const QByteArray &body, const QString &err) {
        QVector<SearchHit> out;
        for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("quotes").toArray()) {
            const QJsonObject o = v.toObject();
            const QString type = o.value("quoteType").toString();
            if (type != "EQUITY" && type != "ETF")
                continue;
            out.push_back({o.value("symbol").toString(),
                           o.value("longname").toString().isEmpty() ? o.value("shortname").toString() : o.value("longname").toString(),
                           o.value("exchDisp").toString(), o.value("typeDisp").toString()});
        }
        done(out, err);
    });
}

void Client::listings(QObject *ctx, Done<QVector<Listing>> done)
{
    const QUrl url("https://api.nasdaq.com/api/screener/stocks?tableonly=true&limit=10000&download=true");
    // About 2 MB: kept as a compact JSON array.
    auto compactBody = [](const QByteArray &body) {
        QJsonArray out;
        for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("data").toObject().value("rows").toArray()) {
            const QJsonObject r = v.toObject();
            QString symbol = r.value("symbol").toString().trimmed();
            static const QRegularExpression notStock("\\b(Notes?|Debentures?|Preferred|Warrants?|Units?|Rights?|Depositary Shares, each)\\b|%",
                                                     QRegularExpression::CaseInsensitiveOption);
            if (symbol.isEmpty() || symbol.contains('^') || notStock.match(r.value("name").toString()).hasMatch())
                continue;
            symbol.replace('/', '-').replace('^', "-P");
            out.append(QJsonArray {symbol, r.value("name").toString(), r.value("sector").toString(),
                                   r.value("industry").toString(), r.value("country").toString(),
                                   nasdaqNumber(r.value("lastsale").toString()), nasdaqNumber(r.value("pctchange").toString()),
                                   nasdaqNumber(r.value("marketCap").toString()), nasdaqNumber(r.value("volume").toString())});
        }
        return out.isEmpty() ? QByteArray() : QJsonDocument(out).toJson(QJsonDocument::Compact);
    };
    get("nasdaq:listings", url, 30 * 60, Nasdaq, ctx, [this, done](const QByteArray &body, const QString &err) {
        if (sameBody(body, m_listingsParsed.body)) {
            const QVector<Listing> out = m_listingsParsed.value;   // shared, not copied
            done(out, out.isEmpty() && err.isEmpty() ? tr("Empty screener") : err);
            return;
        }
        QVector<Listing> out;
        const QJsonArray rows = QJsonDocument::fromJson(body).array();
        QSet<QString> symbols;
        for (const QJsonValue &v : rows)
            symbols.insert(v[0].toString());
        static const QRegularExpression notStock("\\b(Notes?|Debentures?|Preferred|Warrants?|Units?|Rights?)\\b|%",
                                                 QRegularExpression::CaseInsensitiveOption);
        for (const QJsonValue &v : rows) {
            const QJsonArray a = v.toArray();
            Listing l;
            l.symbol = a[0].toString();
            l.name = a[1].toString();
            // Notes, preferreds, warrants, units and rights aren't companies; nor is "ABCDP" next to "ABCD".
            if (notStock.match(l.name).hasMatch()
                || (l.symbol.size() == 5 && QString("PWUR").contains(l.symbol.back()) && symbols.contains(l.symbol.left(4))))
                continue;
            l.name.remove(shareClassSuffix());
            l.sector = a[2].toString();
            l.industry = a[3].toString();
            l.country = a[4].toString();
            l.price = a[5].isDouble() ? a[5].toDouble() : NaN;
            l.changePct = a[6].isDouble() ? a[6].toDouble() / 100.0 : NaN;
            l.marketCap = a[7].isDouble() ? a[7].toDouble() : NaN;
            l.volume = a[8].isDouble() ? a[8].toDouble() : NaN;
            out.push_back(l);
        }
        m_listingsParsed = {body, out};
        done(out, out.isEmpty() && err.isEmpty() ? tr("Empty screener") : err);
    }, compactBody);
}

void Client::indexes(QObject *ctx, Done<Indexes> done)
{
    struct State {
        QByteArray sp, dow, ndx;
        int left = 3;
        QString error;
    };
    auto state = std::make_shared<State>();
    auto wikitext = [](const QByteArray &body) {
        return QJsonDocument::fromJson(body).object().value("parse").toObject().value("wikitext").toString();
    };
    auto finishAll = [this, state, done, wikitext] {
        if (--state->left > 0)
            return;
        ParsedIndexes &known = m_indexesParsed;
        if (sameBody(state->sp, known.sp) && sameBody(state->dow, known.dow) && sameBody(state->ndx, known.ndx)) {
            const Indexes idx = known.value;   // shared, not copied
            done(idx, idx.sp500.isEmpty() ? (state->error.isEmpty() ? tr("No index members") : state->error) : QString());
            return;
        }
        Indexes idx;
        const QVector<WikiRow> rows = parseSp500(wikitext(state->sp));
        QHash<QString, const WikiRow *> byTarget, bySymbol;
        for (const WikiRow &r : rows) {
            idx.sp500.push_back({r.symbol, r.name, r.sector});
            if (!byTarget.contains(r.target))
                byTarget.insert(r.target, &r);
            bySymbol.insert(r.symbol, &r);
        }
        for (const QString &t : navboxTargets(wikitext(state->dow)))
            if (const WikiRow *r = byTarget.value(t))
                idx.dow.push_back({r->symbol, r->name, r->sector});
        const QJsonObject d = QJsonDocument::fromJson(state->ndx).object().value("data").toObject();
        for (const QJsonValue &v : d.value("data").toObject().value("rows").toArray()) {
            const QJsonObject o = v.toObject();
            const QString symbol = o.value("symbol").toString().replace('/', '-');
            const WikiRow *r = bySymbol.value(symbol);
            QString name = o.value("companyName").toString();
            name.remove(shareClassSuffix());
            idx.nasdaq100.push_back({symbol, r ? r->name : name, r ? r->sector : QString()});
        }
        known = {state->sp, state->dow, state->ndx, idx};
        done(idx, idx.sp500.isEmpty() ? (state->error.isEmpty() ? tr("No index members") : state->error) : QString());
    };
    get("wikipedia:sp500", QUrl("https://en.wikipedia.org/w/api.php?action=parse&page=List_of_S%26P_500_companies"
                                "&prop=wikitext&format=json&formatversion=2&section=1"),
        86400, 0, ctx, [state, finishAll](const QByteArray &body, const QString &err) {
            state->sp = body;
            if (!err.isEmpty()) state->error = err;
            finishAll();
        });
    get("wikipedia:dow", QUrl("https://en.wikipedia.org/w/api.php?action=parse"
                              "&page=Template:Dow_Jones_Industrial_Average_companies&prop=wikitext&format=json&formatversion=2"),
        86400, 0, ctx, [state, finishAll](const QByteArray &body, const QString &err) {
            state->dow = body;
            if (!err.isEmpty()) state->error = err;
            finishAll();
        });
    get("nasdaq:ndx", QUrl("https://api.nasdaq.com/api/quote/list-type/nasdaq100"), 86400, Nasdaq, ctx,
        [state, finishAll](const QByteArray &body, const QString &err) {
            state->ndx = body;
            if (!err.isEmpty()) state->error = err;
            finishAll();
        });
}

// ---------------------------------------------------------------- statements layout

const QVector<StatementLine> &statementLayout(const QString &kind)
{
    using L = StatementLine;
    static const QVector<L> income {
        {"TotalRevenue", QObject::tr("Total Revenue"), 0, true}, {"OperatingRevenue", QObject::tr("Operating Revenue"), 1, false},
        {"CostOfRevenue", QObject::tr("Cost of Revenue"), 0, false}, {"GrossProfit", QObject::tr("Gross Profit"), 0, false},
        {"OperatingExpense", QObject::tr("Operating Expense"), 0, true},
        {"SellingGeneralAndAdministration", QObject::tr("Selling General and Administrative"), 1, false},
        {"ResearchAndDevelopment", QObject::tr("Research & Development"), 1, false},
        {"OtherOperatingExpenses", QObject::tr("Other Operating Expenses"), 1, false},
        {"OperatingIncome", QObject::tr("Operating Income"), 0, false},
        {"NetNonOperatingInterestIncomeExpense", QObject::tr("Net Non Operating Interest Income"), 0, true},
        {"InterestIncomeNonOperating", QObject::tr("Interest Income Non Operating"), 1, false},
        {"InterestExpenseNonOperating", QObject::tr("Interest Expense Non Operating"), 1, false},
        {"OtherIncomeExpense", QObject::tr("Other Income Expense"), 0, true},
        {"OtherNonOperatingIncomeExpenses", QObject::tr("Other Non Operating Income"), 1, false},
        {"SpecialIncomeCharges", QObject::tr("Special Income Charges"), 1, false},
        {"PretaxIncome", QObject::tr("Pretax Income"), 0, false}, {"TaxProvision", QObject::tr("Tax Provision"), 0, false},
        {"NetIncomeCommonStockholders", QObject::tr("Net Income Common Stockholders"), 0, true},
        {"NetIncome", QObject::tr("Net Income"), 1, false},
        {"DilutedNIAvailtoComStockholders", QObject::tr("Diluted NI Available to Com Stockholders"), 0, false},
        {"BasicEPS", QObject::tr("Basic EPS"), 0, false}, {"DilutedEPS", QObject::tr("Diluted EPS"), 0, false},
        {"BasicAverageShares", QObject::tr("Basic Average Shares"), 0, false},
        {"DilutedAverageShares", QObject::tr("Diluted Average Shares"), 0, false},
        {"TotalOperatingIncomeAsReported", QObject::tr("Total Operating Income as Reported"), 0, false},
        {"TotalExpenses", QObject::tr("Total Expenses"), 0, false},
        {"NetIncomeFromContinuingAndDiscontinuedOperation", QObject::tr("Net Income from Continuing & Discontinued Operation"), 0, false},
        {"NormalizedIncome", QObject::tr("Normalized Income"), 0, false},
        {"InterestIncome", QObject::tr("Interest Income"), 0, false}, {"InterestExpense", QObject::tr("Interest Expense"), 0, false},
        {"NetInterestIncome", QObject::tr("Net Interest Income"), 0, false}, {"EBIT", "EBIT", 0, false},
        {"EBITDA", "EBITDA", 0, false}, {"ReconciledCostOfRevenue", QObject::tr("Reconciled Cost of Revenue"), 0, false},
        {"ReconciledDepreciation", QObject::tr("Reconciled Depreciation"), 0, false},
        {"NetIncomeFromContinuingOperationNetMinorityInterest", QObject::tr("Net Income from Continuing Operation Net Minority Interest"), 0, false},
        {"NormalizedEBITDA", QObject::tr("Normalized EBITDA"), 0, false},
        {"TaxRateForCalcs", QObject::tr("Tax Rate for Calcs"), 0, false},
        {"TaxEffectOfUnusualItems", QObject::tr("Tax Effect of Unusual Items"), 0, false},
    };
    static const QVector<L> balance {
        {"TotalAssets", QObject::tr("Total Assets"), 0, true}, {"CurrentAssets", QObject::tr("Current Assets"), 1, false},
        {"CashCashEquivalentsAndShortTermInvestments", QObject::tr("Cash, Cash Equivalents & Short Term Investments"), 2, false},
        {"CashAndCashEquivalents", QObject::tr("Cash And Cash Equivalents"), 3, false},
        {"CashFinancial", QObject::tr("Cash"), 4, false}, {"CashEquivalents", QObject::tr("Cash Equivalents"), 4, false},
        {"OtherShortTermInvestments", QObject::tr("Other Short Term Investments"), 3, false},
        {"Receivables", QObject::tr("Receivables"), 2, false}, {"AccountsReceivable", QObject::tr("Accounts receivable"), 3, false},
        {"OtherReceivables", QObject::tr("Other Receivables"), 3, false}, {"Inventory", QObject::tr("Inventory"), 2, false},
        {"OtherCurrentAssets", QObject::tr("Other Current Assets"), 2, false},
        {"TotalNonCurrentAssets", QObject::tr("Total non-current assets"), 1, false}, {"NetPPE", QObject::tr("Net PPE"), 2, false},
        {"GrossPPE", QObject::tr("Gross PPE"), 3, false}, {"AccumulatedDepreciation", QObject::tr("Accumulated Depreciation"), 3, false},
        {"GoodwillAndOtherIntangibleAssets", QObject::tr("Goodwill And Other Intangible Assets"), 2, false},
        {"Goodwill", QObject::tr("Goodwill"), 3, false}, {"OtherIntangibleAssets", QObject::tr("Other Intangible Assets"), 3, false},
        {"InvestmentsAndAdvances", QObject::tr("Investments And Advances"), 2, false},
        {"NonCurrentDeferredAssets", QObject::tr("Non Current Deferred Assets"), 2, false},
        {"OtherNonCurrentAssets", QObject::tr("Other Non Current Assets"), 2, false},
        {"TotalLiabilitiesNetMinorityInterest", QObject::tr("Total Liabilities Net Minority Interest"), 0, true},
        {"CurrentLiabilities", QObject::tr("Current Liabilities"), 1, false},
        {"PayablesAndAccruedExpenses", QObject::tr("Payables And Accrued Expenses"), 2, false},
        {"Payables", QObject::tr("Payables"), 3, false}, {"CurrentAccruedExpenses", QObject::tr("Accrued Expenses"), 3, false},
        {"CurrentDebtAndCapitalLeaseObligation", QObject::tr("Current Debt And Capital Lease Obligation"), 2, false},
        {"CurrentDebt", QObject::tr("Current Debt"), 3, false},
        {"CurrentDeferredLiabilities", QObject::tr("Current Deferred Liabilities"), 2, false},
        {"CurrentDeferredRevenue", QObject::tr("Current Deferred Revenue"), 3, false},
        {"OtherCurrentLiabilities", QObject::tr("Other Current Liabilities"), 2, false},
        {"TotalNonCurrentLiabilitiesNetMinorityInterest", QObject::tr("Total Non Current Liabilities Net Minority Interest"), 1, false},
        {"LongTermDebtAndCapitalLeaseObligation", QObject::tr("Long Term Debt And Capital Lease Obligation"), 2, false},
        {"LongTermDebt", QObject::tr("Long Term Debt"), 3, false},
        {"TradeandOtherPayablesNonCurrent", QObject::tr("Trade and Other Payables Non Current"), 2, false},
        {"OtherNonCurrentLiabilities", QObject::tr("Other Non Current Liabilities"), 2, false},
        {"TotalEquityGrossMinorityInterest", QObject::tr("Total Equity Gross Minority Interest"), 0, true},
        {"StockholdersEquity", QObject::tr("Stockholders' Equity"), 1, false}, {"CapitalStock", QObject::tr("Capital Stock"), 2, false},
        {"CommonStock", QObject::tr("Common Stock"), 3, false}, {"RetainedEarnings", QObject::tr("Retained Earnings"), 2, false},
        {"GainsLossesNotAffectingRetainedEarnings", QObject::tr("Gains Losses Not Affecting Retained Earnings"), 2, false},
        {"MinorityInterest", QObject::tr("Minority Interest"), 1, false},
        {"TotalCapitalization", QObject::tr("Total Capitalization"), 0, false},
        {"CommonStockEquity", QObject::tr("Common Stock Equity"), 0, false},
        {"NetTangibleAssets", QObject::tr("Net Tangible Assets"), 0, false},
        {"WorkingCapital", QObject::tr("Working Capital"), 0, false}, {"InvestedCapital", QObject::tr("Invested Capital"), 0, false},
        {"TangibleBookValue", QObject::tr("Tangible Book Value"), 0, false}, {"TotalDebt", QObject::tr("Total Debt"), 0, false},
        {"NetDebt", QObject::tr("Net Debt"), 0, false}, {"ShareIssued", QObject::tr("Share Issued"), 0, false},
        {"OrdinarySharesNumber", QObject::tr("Ordinary Shares Number"), 0, false},
        {"TreasurySharesNumber", QObject::tr("Treasury Shares Number"), 0, false},
    };
    static const QVector<L> cash {
        {"OperatingCashFlow", QObject::tr("Operating Cash Flow"), 0, true},
        {"CashFlowFromContinuingOperatingActivities", QObject::tr("Cash Flow from Continuing Operating Activities"), 1, false},
        {"NetIncomeFromContinuingOperations", QObject::tr("Net Income from Continuing Operations"), 2, false},
        {"DepreciationAmortizationDepletion", QObject::tr("Depreciation Amortization Depletion"), 2, false},
        {"DepreciationAndAmortization", QObject::tr("Depreciation & amortization"), 3, false},
        {"DeferredTax", QObject::tr("Deferred Tax"), 2, false}, {"DeferredIncomeTax", QObject::tr("Deferred Income Tax"), 3, false},
        {"StockBasedCompensation", QObject::tr("Stock based compensation"), 2, false},
        {"OtherNonCashItems", QObject::tr("Other non-cash items"), 2, false},
        {"ChangeInWorkingCapital", QObject::tr("Change in working capital"), 2, false},
        {"ChangeInReceivables", QObject::tr("Change in Receivables"), 3, false},
        {"ChangeInInventory", QObject::tr("Change in Inventory"), 3, false},
        {"ChangeInPayablesAndAccruedExpense", QObject::tr("Change in Payables And Accrued Expense"), 3, false},
        {"ChangeInOtherCurrentAssets", QObject::tr("Change in Other Current Assets"), 3, false},
        {"ChangeInOtherCurrentLiabilities", QObject::tr("Change in Other Current Liabilities"), 3, false},
        {"ChangeInOtherWorkingCapital", QObject::tr("Change in Other Working Capital"), 3, false},
        {"InvestingCashFlow", QObject::tr("Investing Cash Flow"), 0, true},
        {"CashFlowFromContinuingInvestingActivities", QObject::tr("Cash Flow from Continuing Investing Activities"), 1, false},
        {"NetPPEPurchaseAndSale", QObject::tr("Net PPE Purchase And Sale"), 2, false},
        {"PurchaseOfPPE", QObject::tr("Purchase of PPE"), 3, false},
        {"NetBusinessPurchaseAndSale", QObject::tr("Net Business Purchase and Sale"), 2, false},
        {"PurchaseOfBusiness", QObject::tr("Purchase of Business"), 3, false},
        {"NetInvestmentPurchaseAndSale", QObject::tr("Net Investment Purchase And Sale"), 2, false},
        {"PurchaseOfInvestment", QObject::tr("Purchase of Investment"), 3, false},
        {"SaleOfInvestment", QObject::tr("Sale of Investment"), 3, false},
        {"NetOtherInvestingChanges", QObject::tr("Net Other Investing Changes"), 2, false},
        {"FinancingCashFlow", QObject::tr("Financing Cash Flow"), 0, true},
        {"CashFlowFromContinuingFinancingActivities", QObject::tr("Cash Flow from Continuing Financing Activities"), 1, false},
        {"NetIssuancePaymentsOfDebt", QObject::tr("Net Issuance Payments of Debt"), 2, false},
        {"NetLongTermDebtIssuance", QObject::tr("Net Long Term Debt Issuance"), 3, false},
        {"LongTermDebtIssuance", QObject::tr("Long Term Debt Issuance"), 4, false},
        {"LongTermDebtPayments", QObject::tr("Long Term Debt Payments"), 4, false},
        {"NetShortTermDebtIssuance", QObject::tr("Net Short Term Debt Issuance"), 3, false},
        {"NetCommonStockIssuance", QObject::tr("Net Common Stock Issuance"), 2, false},
        {"CommonStockIssuance", QObject::tr("Common Stock Issuance"), 3, false},
        {"CommonStockPayments", QObject::tr("Common Stock Payments"), 3, false},
        {"CashDividendsPaid", QObject::tr("Cash Dividends Paid"), 2, false},
        {"CommonStockDividendPaid", QObject::tr("Common Stock Dividend Paid"), 3, false},
        {"NetOtherFinancingCharges", QObject::tr("Net Other Financing Charges"), 2, false},
        {"EndCashPosition", QObject::tr("End Cash Position"), 0, true},
        {"ChangesInCash", QObject::tr("Changes in Cash"), 1, false},
        {"BeginningCashPosition", QObject::tr("Beginning Cash Position"), 1, false},
        {"IncomeTaxPaidSupplementalData", QObject::tr("Income Tax Paid Supplemental Data"), 0, false},
        {"InterestPaidSupplementalData", QObject::tr("Interest Paid Supplemental Data"), 0, false},
        {"CapitalExpenditure", QObject::tr("Capital Expenditure"), 0, false},
        {"IssuanceOfCapitalStock", QObject::tr("Issuance of Capital Stock"), 0, false},
        {"IssuanceOfDebt", QObject::tr("Issuance of Debt"), 0, false},
        {"RepaymentOfDebt", QObject::tr("Repayment of Debt"), 0, false},
        {"RepurchaseOfCapitalStock", QObject::tr("Repurchase of Capital Stock"), 0, false},
        {"FreeCashFlow", QObject::tr("Free Cash Flow"), 0, true},
    };
    if (kind == "balance")
        return balance;
    if (kind == "cash")
        return cash;
    return income;
}

// ---------------------------------------------------------------- valuation and grades

Valuation intrinsicValue(const QJsonObject &summary)
{
    // Two-stage discounted free cash flow, from Yahoo's figures:
    //   cash flow: the mean of the trailing twelve months' free cash flow and net income (just the free cash
    //              flow when net income isn't positive);
    //   growth: the mean of the revenue growth and the analysts' growth estimate for next year's earnings,
    //           kept within -5 %…+25 %, for five years, then fading in a straight line to 2.5 % by year ten;
    //   discount rate: 4.25 % (about the 10-year Treasury yield) + beta × 5 % equity risk premium, within 6…14 %;
    //   terminal value: Gordon growth at 2.5 % after year ten;
    //   equity: enterprise value + cash − debt, divided by the shares outstanding.
    Valuation v;
    const QJsonObject fd = summary.value("financialData").toObject();
    const QJsonObject ks = summary.value("defaultKeyStatistics").toObject();
    const QJsonObject sd = summary.value("summaryDetail").toObject();
    v.price = raw(fd, "currentPrice");
    if (!ok(v.price))
        v.price = raw(summary.value("price").toObject(), "regularMarketPrice");
    // Free cash flow swings with investment cycles (a capex boom can take it near zero): the base is the mean
    // of the trailing free cash flow and net income when both are positive.
    const double rawFcf = raw(fd, "freeCashflow");
    const double netIncome = raw(ks, "netIncomeToCommon");
    const double fcf = ok(rawFcf) && ok(netIncome) && rawFcf > 0 && netIncome > 0 ? (rawFcf + netIncome) / 2 : rawFcf;
    double shares = raw(ks, "sharesOutstanding");
    if (!ok(shares))
        shares = raw(ks, "impliedSharesOutstanding");
    double beta = raw(sd, "beta");
    if (!ok(beta))
        beta = raw(ks, "beta");
    if (!ok(beta))
        beta = 1.0;
    beta = std::clamp(beta, 0.6, 2.0);
    v.discountRate = std::clamp(0.0425 + beta * 0.05, 0.06, 0.14);

    double nextYear = NaN;
    for (const QJsonValue &t : summary.value("earningsTrend").toObject().value("trend").toArray())
        if (t.toObject().value("period").toString() == "+1y")
            nextYear = raw(t.toObject(), "growth");
    const double revGrowth = raw(fd, "revenueGrowth");
    double g = ok(nextYear) && ok(revGrowth) ? (nextYear + revGrowth) / 2 : ok(nextYear) ? nextYear : ok(revGrowth) ? revGrowth : 0.03;
    g = std::clamp(g, -0.05, 0.25);
    v.growth = g;
    v.method = QObject::tr("Two-stage discounted cash flow: the mean of trailing free cash flow and net income, growing %1 a year for five years, "
                           "fading to 2.5% by year ten, discounted at %2 (4.25% + beta × 5%), plus cash less debt, "
                           "per share.")
                   .arg(percent(g, 1, false), percent(v.discountRate, 1, false));
    if (!ok(fcf) || fcf <= 0 || !ok(shares) || shares <= 0) {
        v.method = QObject::tr("Not meaningful: no positive free cash flow to discount.");
        return v;
    }
    const double terminal = 0.025, r = v.discountRate;
    double cf = fcf, pv = 0;
    for (int year = 1; year <= 10; ++year) {
        const double growth = year <= 5 ? g : g + (terminal - g) * (year - 5) / 5.0;
        cf *= 1 + growth;
        pv += cf / std::pow(1 + r, year);
    }
    pv += cf * (1 + terminal) / (r - terminal) / std::pow(1 + r, 10);
    const double cash = raw(fd, "totalCash"), debt = raw(fd, "totalDebt");
    const double equity = pv + (ok(cash) ? cash : 0) - (ok(debt) ? debt : 0);
    if (equity <= 0) {
        v.method = QObject::tr("Not meaningful: debt exceeds the discounted cash flows.");
        return v;
    }
    // Per share through the market cap, which also takes care of ADRs (several shares each, other currency).
    const double fx = financialToQuote(summary);
    const double mcap = raw(sd, "marketCap");
    if (ok(fx) && ok(mcap) && mcap > 0 && ok(v.price))
        v.intrinsic = v.price * equity * fx / mcap;
    else if (text(fd, "financialCurrency").isEmpty() || text(fd, "financialCurrency") == text(summary.value("price").toObject(), "currency"))
        v.intrinsic = equity / shares;
    else {
        v.method = QObject::tr("Not meaningful: the statements' currency can't be converted.");
        return v;
    }
    if (ok(v.price) && v.price > 0 && v.intrinsic > 0)
        v.upside = v.intrinsic / v.price - 1;
    return v;
}

double financialToQuote(const QJsonObject &summary)
{
    const QJsonObject fd = summary.value("financialData").toObject();
    const QJsonObject sd = summary.value("summaryDetail").toObject();
    const QString fin = text(fd, "financialCurrency"), quote = text(summary.value("price").toObject(), "currency");
    if (fin.isEmpty() || quote.isEmpty() || fin == quote)
        return 1;
    Q_UNUSED(sd);
    return summary.value("fxToQuote").toDouble(NaN);   // set by Client::summary from Yahoo's currency pair
}

namespace {

// Linear score between a bad and a good value (either order), within 0..1.
double scale(double v, double bad, double good)
{
    if (!ok(v))
        return NaN;
    return std::clamp((v - bad) / (good - bad), 0.0, 1.0);
}

double mean(std::initializer_list<double> values)
{
    double sum = 0;
    int n = 0;
    for (double v : values)
        if (ok(v)) {
            sum += v;
            ++n;
        }
    return n ? sum / n : NaN;
}

QString letter(double s)
{
    if (!ok(s))
        return "–";
    static const std::pair<double, const char *> grades[] {
        {0.90, "A+"}, {0.83, "A"}, {0.77, "A-"}, {0.70, "B+"}, {0.63, "B"}, {0.57, "B-"}, {0.50, "C+"},
        {0.43, "C"}, {0.37, "C-"}, {0.30, "D+"}, {0.23, "D"}, {0.17, "D-"}};
    for (const auto &[min, name] : grades)
        if (s >= min)
            return name;
    return "F";
}

} // namespace

Grades factorGrades(const QJsonObject &summary, const Valuation &valuation)
{
    // Scored on fixed thresholds (no sector adjustment), each 0..1:
    //   valuation: upside to the intrinsic value (−50 %…+50 %), forward P/E (40…10), PEG (3…1), EV/EBITDA (30…8);
    //   profitability: gross (20…60 %), operating (0…30 %) and net (0…25 %) margins, ROE (0…30 %), ROA (0…15 %);
    //   financial health: current ratio (0.8…2), quick ratio (0.5…1.5), debt/equity (200…30 %), net cash in % of
    //   the market cap (−30…+10 %), free cash flow / debt (0…50 %).
    // The rating weighs valuation and profitability 30 % each, health 20 %, and 20 % for the price's lead over the
    // S&P 500 over a year (−30…+30 points) and the change in analysts' earnings estimate for this year over 90 days
    // (−5…+5 %).
    Grades g;
    const QJsonObject fd = summary.value("financialData").toObject();
    const QJsonObject ks = summary.value("defaultKeyStatistics").toObject();
    const QJsonObject sd = summary.value("summaryDetail").toObject();
    const double mcap = raw(sd, "marketCap");
    g.valuationScore = mean({scale(valuation.upside, -0.5, 0.5), scale(raw(ks, "forwardPE"), 40, 10),
                             scale(raw(ks, "pegRatio"), 3, 1), scale(raw(ks, "enterpriseToEbitda"), 30, 8)});
    g.profitabilityScore = mean({scale(raw(fd, "grossMargins"), 0.2, 0.6), scale(raw(fd, "operatingMargins"), 0, 0.3),
                                 scale(raw(fd, "profitMargins"), 0, 0.25), scale(raw(fd, "returnOnEquity"), 0, 0.3),
                                 scale(raw(fd, "returnOnAssets"), 0, 0.15)});
    const double cash = raw(fd, "totalCash"), debt = raw(fd, "totalDebt"), fcf = raw(fd, "freeCashflow");
    g.healthScore = mean({scale(raw(fd, "currentRatio"), 0.8, 2), scale(raw(fd, "quickRatio"), 0.5, 1.5),
                          scale(raw(fd, "debtToEquity"), 200, 30),
                          ok(cash) && ok(debt) && ok(mcap) && mcap > 0 ? scale((cash - debt) * financialToQuote(summary) / mcap, -0.3, 0.1) : NaN,
                          ok(fcf) && ok(debt) && debt > 0 ? scale(fcf / debt, 0, 0.5) : NaN});
    g.valuation = letter(g.valuationScore);
    g.profitability = letter(g.profitabilityScore);
    g.health = letter(g.healthScore);

    const double lead = raw(ks, "52WeekChange") - raw(ks, "SandP52WeekChange");
    double revision = NaN;
    for (const QJsonValue &t : summary.value("earningsTrend").toObject().value("trend").toArray()) {
        const QJsonObject o = t.toObject();
        if (o.value("period").toString() != "0y")
            continue;
        const QJsonObject trend = o.value("epsTrend").toObject();
        const double now = raw(trend, "current"), before = raw(trend, "90daysAgo");
        if (ok(now) && ok(before) && before != 0)
            revision = now / std::abs(before) - 1;
    }
    const double momentum = mean({scale(lead, -0.3, 0.3), scale(revision, -0.05, 0.05)});
    double sum = 0, weight = 0;
    for (auto [s, w] : {std::pair {g.valuationScore, 0.3}, std::pair {g.profitabilityScore, 0.3},
                        std::pair {g.healthScore, 0.2}, std::pair {momentum, 0.2}})
        if (ok(s)) {
            sum += s * w;
            weight += w;
        }
    if (weight > 0) {
        g.score = 1 + 4 * sum / weight;
        g.rating = ratingName(g.score);
    }
    return g;
}

QString ratingName(double score)
{
    if (!ok(score))
        return "–";
    if (score >= 4.5) return QObject::tr("Strong Buy");
    if (score >= 3.5) return QObject::tr("Buy");
    if (score >= 2.5) return QObject::tr("Hold");
    if (score >= 1.5) return QObject::tr("Sell");
    return QObject::tr("Strong Sell");
}

int gradeBucket(const QString &grade)
{
    const QString g = grade.trimmed().toLower();
    if (g.isEmpty())
        return -1;
    if (g.contains("strong buy") || g.contains("top pick") || g.contains("conviction buy"))
        return 4;
    if (g.contains("strong sell"))
        return 0;
    if (g.contains("underperform") || g.contains("underweight") || g.contains("reduce") || g == "sell"
        || g.contains("negative") || g.contains("moderate sell") || g.contains("sector underperform"))
        return g == "sell" ? 0 : 1;
    if (g.contains("buy") || g.contains("outperform") || g.contains("overweight") || g.contains("accumulate")
        || g.contains("positive") || g == "add")
        return 3;
    if (g.contains("hold") || g.contains("neutral") || g.contains("perform") || g.contains("equal") || g.contains("weight")
        || g.contains("in-line") || g.contains("in line") || g.contains("mixed") || g.contains("fair value"))
        return 2;
    return -1;
}

// ---------------------------------------------------------------- formatting

QString currencySymbol(const QString &currency)
{
    if (currency.isEmpty() || currency == "USD") return "$";
    if (currency == "EUR") return "€";
    if (currency == "GBP" || currency == "GBp") return "£";
    if (currency == "JPY" || currency == "CNY") return "¥";
    if (currency == "INR") return "₹";
    if (currency == "KRW") return "₩";
    return currency + " ";
}

QString compact(double v, int decimals)
{
    if (!ok(v))
        return "N/A";
    const double a = std::abs(v);
    const QLocale loc;
    if (a >= 1e12) return loc.toString(v / 1e12, 'f', decimals) + "T";
    if (a >= 1e9) return loc.toString(v / 1e9, 'f', decimals) + "B";
    if (a >= 1e6) return loc.toString(v / 1e6, 'f', decimals) + "M";
    if (a >= 1e3) return loc.toString(v / 1e3, 'f', decimals) + "K";
    return loc.toString(v, 'f', v == std::round(v) ? 0 : decimals);   // share counts: "500", not "500.00"
}

QString money(double v, const QString &currency, int decimals)
{
    if (!ok(v))
        return "N/A";
    const QString sym = currencySymbol(currency);
    const QString body = std::abs(v) >= 1e5 ? compact(std::abs(v), decimals) : QLocale().toString(std::abs(v), 'f', decimals);
    return (v < 0 ? "-" : "") + sym + body;
}

QString percent(double v, int decimals, bool sign)
{
    if (!ok(v))
        return "N/A";
    const QString s = QLocale().toString(v * 100, 'f', decimals) + "%";
    return sign && v > 0 ? "+" + s : s;
}

QString plain(double v, int decimals)
{
    return ok(v) ? QLocale().toString(v, 'f', decimals) : "N/A";
}

QString full(double v, const QString &currency)
{
    if (!ok(v))
        return "–";
    return (v < 0 ? "-" : "") + currencySymbol(currency) + QLocale().toString(std::abs(v), 'f', 2);
}

} // namespace research
