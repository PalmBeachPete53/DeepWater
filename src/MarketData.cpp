#include "MarketData.h"

#include "CommodityPrices.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTimeZone>
#include <QtMath>

namespace {
// Older than this, a futures quote is no longer current (Yahoo keeps the last trade of a contract it stopped
// getting): the IMF's monthly average is shown instead.
constexpr qint64 StaleQuoteSecs = 10 * 24 * 3600;
}

MarketData::MarketData(QObject *parent)
    : QObject(parent)
{
    // Layers hidden in the previous session.
    const QStringList hidden = QSettings().value("hiddenLayers").toStringList();
    m_hidden = QSet<QString>(hidden.begin(), hidden.end());
}

bool MarketData::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    for (const QJsonValue &v : root.value("categories").toArray()) {
        const QJsonObject o = v.toObject();
        m_categoryIndex.insert(o.value("id").toString(), m_categories.size());
        Category cat;
        cat.id = o.value("id").toString();
        cat.rawName = o.value("name").toString();
        cat.baseColor = QColor(o.value("color").toString());
        cat.color = cat.baseColor;
        m_categories.push_back(cat);
    }

    for (const QJsonValue &v : root.value("commodities").toArray()) {
        const QJsonObject o = v.toObject();
        Commodity c;
        c.id = o.value("id").toString();
        c.rawName = o.value("name").toString();
        c.categoryId = o.value("cat").toString();
        const QJsonObject future = o.value("future").toObject();
        c.future.symbol = future.value("symbol").toString();
        c.future.roll = future.value("roll").toString();
        c.future.suffix = future.value("suffix").toString();
        c.future.market = future.value("market").toString();
        c.future.unit = future.value("unit").toString();
        c.future.rawWhat = future.value("what").toString();
        c.future.dailyOnly = future.value("daily").toBool();
        const QJsonObject imf = o.value("imf").toObject();
        c.imf.code = imf.value("code").toString();
        c.imf.unit = imf.value("unit").toString();
        c.imf.rawWhat = imf.value("what").toString();
        c.unit = c.future.symbol.isEmpty() ? c.imf.unit : c.future.unit;
        m_commodityIndex.insert(c.id, m_commodities.size());
        m_commodities.push_back(c);
    }

    for (const QJsonValue &v : root.value("sites").toArray()) {
        const QJsonObject o = v.toObject();
        Site s;
        s.index = m_sites.size();
        s.name = o.value("name").toString();
        s.rawCountry = o.value("country").toString();
        s.lat = o.value("lat").toDouble();
        s.lon = o.value("lon").toDouble();
        s.commodityId = o.value("commodity").toString();
        s.rawType = o.value("type").toString();
        const Commodity *c = commodity(s.commodityId);
        s.categoryId = o.value("hub").toBool() ? QStringLiteral("hub")
                                               : (c ? c->categoryId : QString());
        m_sites.push_back(s);
    }

    retranslate();
    return !m_sites.isEmpty();
}

void MarketData::setCategoryColors(const QHash<QString, QColor> &overrides)
{
    for (Category &c : m_categories)
        c.color = overrides.value(c.id, c.baseColor);
}

void MarketData::retranslate()
{
    auto tr = [](const QString &s) {
        return QCoreApplication::translate("MarketData", s.toUtf8().constData());
    };
    for (Category &c : m_categories)
        c.name = tr(c.rawName);
    for (Commodity &c : m_commodities) {
        c.name = tr(c.rawName);
        c.future.what = c.future.rawWhat.isEmpty() ? QString() : tr(c.future.rawWhat);
        c.imf.what = c.imf.rawWhat.isEmpty() ? QString() : tr(c.imf.rawWhat);
    }
    for (Site &s : m_sites) {
        s.type = tr(s.rawType);
        s.country = tr(s.rawCountry);
    }
}

const Category *MarketData::category(const QString &id) const
{
    auto it = m_categoryIndex.constFind(id);
    return it == m_categoryIndex.constEnd() ? nullptr : &m_categories[*it];
}

const Commodity *MarketData::commodity(const QString &id) const
{
    auto it = m_commodityIndex.constFind(id);
    return it == m_commodityIndex.constEnd() ? nullptr : &m_commodities[*it];
}

QColor MarketData::categoryColor(const QString &id) const
{
    const Category *c = category(id);
    return c ? c->color : QColor(Qt::gray);
}

int MarketData::siteCount(const QString &categoryId) const
{
    return std::count_if(m_sites.begin(), m_sites.end(),
                         [&](const Site &s) { return s.categoryId == categoryId; });
}

QVector<const Site *> MarketData::sitesForCommodity(const QString &commodityId) const
{
    QVector<const Site *> out;
    for (const Site &s : m_sites)
        if (s.commodityId == commodityId)
            out.push_back(&s);
    return out;
}

void MarketData::setCategoryVisible(const QString &id, bool visible)
{
    if (visible == isCategoryVisible(id))
        return;
    if (visible)
        m_hidden.remove(id);
    else
        m_hidden.insert(id);
    QSettings().setValue("hiddenLayers", QStringList(m_hidden.begin(), m_hidden.end()));
    emit filterChanged();
}

void MarketData::applyPrices(const FuturesQuotes *futures, const CommodityBenchmarks *benchmarks)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (Commodity &c : m_commodities) {
        const QMap<QDate, double> monthly = benchmarks && !c.imf.code.isEmpty() ? benchmarks->series(c.imf.code)
                                                                                 : QMap<QDate, double>();
        c.benchmark = monthly.isEmpty() ? qQNaN() : monthly.last();
        c.benchmarkMonth = monthly.isEmpty() ? QDate() : monthly.lastKey();

        const QString symbol = c.future.symbol.isEmpty()
            ? QString()
            : FuturesQuotes::symbolFor({c.future.symbol, c.future.roll, c.future.suffix, c.future.dailyOnly});
        const FuturesQuotes::Quote *q = futures && !symbol.isEmpty() ? futures->quote(symbol) : nullptr;
        if (q && q->time.secsTo(now) < StaleQuoteSecs) {
            c.source = Commodity::Source::Futures;
            c.unit = c.future.unit;
            c.price = q->price;
            c.time = q->time;
            c.settlement = q->settlement;
            c.previousClose = q->previousClose;
            c.previousDate = q->previousDate;
            c.contract = symbol;
            c.contractName = q->name;
            c.history.clear();
            const bool intraday = !c.future.dailyOnly && q->intraday.size() >= 2;
            for (const FuturesQuotes::Point &p : intraday ? q->intraday : q->daily)
                c.history.push_back(p.value);
            // The latest trade, after the last bar.
            if (intraday && c.history.last() != c.price)
                c.history.push_back(c.price);
            c.span = intraday ? Commodity::Span::FiveDays : Commodity::Span::ThreeMonths;
        } else if (!monthly.isEmpty()) {
            c.source = Commodity::Source::Monthly;
            c.unit = c.imf.unit;
            c.price = monthly.last();
            c.time = QDateTime(monthly.lastKey(), QTime(0, 0), QTimeZone::UTC);
            c.settlement = false;
            const auto previous = std::prev(monthly.cend(), std::min<qsizetype>(2, monthly.size()));
            c.previousClose = monthly.size() >= 2 ? *previous : qQNaN();
            c.previousDate = monthly.size() >= 2 ? previous.key() : QDate();
            c.contract.clear();
            c.contractName.clear();
            c.history.clear();
            for (auto it = monthly.lowerBound(monthly.lastKey().addMonths(-23)); it != monthly.cend(); ++it)
                c.history.push_back(*it);
            c.span = Commodity::Span::TwoYears;
        } else {
            c.source = Commodity::Source::None;
            c.price = c.previousClose = qQNaN();
            c.time = QDateTime();
            c.history.clear();
        }
    }
    emit pricesUpdated();
}
