#pragma once

#include "MacroData.h"

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <array>

// Map › supply chains: the data sets behind the "Supply chains" group of heatmap indicators (crop yields,
// road freight, crude oil stocks) and the globe's supply chain layer (ports, disruptions, oil storage sites),
// plus the headline gauges of the overview (the New York Fed's supply chain pressure index, US freight
// indices). See README › Supply chains.
namespace supply {

// ISO 3166 alpha-2 to alpha-3, from the bundled country names (Eurostat's EL and UK included); empty when
// unknown. Safe from any thread.
QString iso3FromIso2(const QString &iso2);

enum class Crop { Wheat, Corn, Rice, Soybeans };
constexpr int CropCount = 4;

// Yields of the four main field crops by country and year, in tonnes per hectare: the USDA's Production,
// Supply and Distribution database (estimates and forecasts of the current marketing year, revised every
// month with the WASDE report; the European Union as a whole only) and Eurostat's crop statistics for each
// EU country (harvest year, EU standard humidity).
class CropYields : public RemoteDataSet
{
    Q_OBJECT
public:
    enum class Source { Usda, Eurostat };

    explicit CropYields(QObject *parent = nullptr);

    bool hasData() const { waitCached(); return !m_values.isEmpty(); }
    double yield(const QString &iso3, Crop crop, int year) const;    // t/ha, NaN when not published
    double change(const QString &iso3, Crop crop, int year) const;   // % against the year before
    QMap<int, double> series(const QString &iso3, Crop crop) const;
    Source source(const QString &iso3, Crop crop) const;
    int lastYear(Crop crop) const;
    // The latest year whose change covers at least 90% of the countries of the best recent year (the current
    // marketing year has USDA forecasts before Eurostat's harvest figures).
    int defaultYear(Crop crop) const;

protected:
    QUrl url() const override;
    QList<QUrl> urls() const override;
    bool parse(const QByteArray &body) override;
    Parser parser() override;

private:
    struct Series {
        QMap<int, double> years;
        Source source = Source::Usda;
    };
    using Values = QHash<QString, std::array<Series, CropCount>>;
    // The downloads (two zips of CSV, one JSON-stat) to the compact form the cache keeps.
    static QByteArray build(const QList<QByteArray> &parts, QString *error);
    static bool read(const QByteArray &compact, Values *out, QString *error);

    Values m_values;
};

// Road freight by country and year, in million tonne-kilometres: the International Transport Forum's annual
// transport trends (OECD), about 45 countries.
class RoadFreight : public RemoteDataSet
{
    Q_OBJECT
public:
    explicit RoadFreight(QObject *parent = nullptr);

    bool hasData() const { waitCached(); return !m_values.isEmpty(); }
    double tonneKm(const QString &iso3, int year) const;
    double change(const QString &iso3, int year) const;   // % against the year before
    QMap<int, double> series(const QString &iso3) const { waitCached(); return m_values.value(iso3); }
    int lastYear() const { waitCached(); return m_lastYear; }
    int defaultYear() const;

protected:
    QUrl url() const override;
    void prepare(QNetworkRequest &request) const override;
    bool parse(const QByteArray &body) override;

private:
    QHash<QString, QMap<int, double>> m_values;
    int m_lastYear = 0;
};

// Crude oil stocks at the end of each month by country, in thousand barrels: the Joint Organisations Data
// Initiative (JODI-Oil world database, primary products), about 90 countries. A 23 MB zip, read as it is
// inflated and kept from 2015 on.
class OilStocks : public RemoteDataSet
{
    Q_OBJECT
public:
    explicit OilStocks(QObject *parent = nullptr);

    bool hasData() const { waitCached(); return !m_values.isEmpty(); }
    struct Latest {
        QDate month;
        double value = 0;      // thousand barrels
        double yearAgo = 0;    // the same month a year before, 0 when missing
        double change = 0;     // %, NaN without yearAgo
    };
    // The latest month published, if it is less than a year older than the newest month of any country.
    bool latest(const QString &iso3, Latest *out) const;
    QMap<QDate, double> series(const QString &iso3) const { waitCached(); return m_values.value(iso3); }
    QHash<QString, double> changes() const;   // iso3 -> % over a year at the latest month

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;
    QByteArray cacheBody(const QByteArray &body) const override;
    Parser parser() override;

private:
    using Values = QHash<QString, QMap<QDate, double>>;
    static QByteArray build(const QByteArray &zip, QString *error);
    static bool read(const QByteArray &compact, Values *out, QDate *newest, QString *error);

    Values m_values;
    QDate m_newest;
};

// A few series of monthly or weekly values (time as a date), shared by the EIA stocks, the pressure index and
// the freight indices.
using DatedSeries = QMap<QDate, double>;

// Weekly US crude stocks from the EIA (free key, "keys/eia"): Cushing, the Gulf Coast, the Strategic Petroleum
// Reserve and the commercial total, over three years.
class EiaOilStocks : public RemoteDataSet
{
    Q_OBJECT
public:
    explicit EiaOilStocks(QObject *parent = nullptr);
    static QStringList seriesIds();

    bool hasData() const { waitCached(); return !m_values.isEmpty(); }
    DatedSeries series(const QString &id) const { waitCached(); return m_values.value(id); }

protected:
    QUrl url() const override;
    bool ready(QString *why) const override;
    bool parse(const QByteArray &body) override;

private:
    QHash<QString, DatedSeries> m_values;
};

// The Global Supply Chain Pressure Index of the Federal Reserve Bank of New York (monthly, standard deviations
// from its average since 1997; the latest vintage of the interactive chart's CSV).
class PressureIndex : public RemoteDataSet
{
    Q_OBJECT
public:
    explicit PressureIndex(QObject *parent = nullptr);

    bool hasData() const { waitCached(); return !m_values.isEmpty(); }
    DatedSeries values() const { waitCached(); return m_values; }

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;

private:
    DatedSeries m_values;
};

// US freight activity and prices from FRED (free key, "keys/fred"): ATA truck tonnage, Cass freight shipments,
// the freight transportation services index, the producer price of deep sea freight.
class FreightIndices : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Info {
        QString id;
        QString name;
        QString note;
    };
    explicit FreightIndices(QObject *parent = nullptr);
    static QVector<Info> infos();

    bool hasData() const { waitCached(); return !m_values.isEmpty(); }
    DatedSeries series(const QString &id) const { waitCached(); return m_values.value(id); }

protected:
    QUrl url() const override;
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    bool ready(QString *why) const override;
    bool parse(const QByteArray &body) override;

private:
    QHash<QString, DatedSeries> m_values;
};

// Ports and maritime disruptions from IMF PortWatch (public ArcGIS layers, no key): the ~2,000 ports of its
// database with their profile, their port calls and cargo over the latest seven days published against the
// same days a year before (52 weeks, the same weekdays), and the disruption events (cyclones, earthquakes,
// floods, conflicts) with the ports they hit. The service allows 3,000 request units a minute and its
// statistics queries time out on the daily table, so each day is one plain query, a few seconds apart.
class PortActivity : public QObject
{
    Q_OBJECT
public:
    struct Port {
        QString id;          // "port1114"
        QString name;
        QString country;
        QString iso3;
        QString locode;
        double lat = 0;
        double lon = 0;
        int vessels = 0;                // the port profile: vessels PortWatch counted there, all types
        int container = 0, dryBulk = 0, tanker = 0;
        QStringList industries;         // its main industries, by trade value
        double shareImports = 0;        // % of the country's seaborne imports, exports
        double shareExports = 0;
        // Activity over the window (-1 when the port reported nothing on any day).
        int calls = -1;
        int callsYearAgo = -1;
        double imports = 0, exports = 0;   // tonnes
        double importsYearAgo = 0, exportsYearAgo = 0;
    };
    struct Disruption {
        int id = 0;
        QString type;        // EQ, TC, FL, WF, VO, DR, OT…
        QString name;
        QString alert;       // RED, ORANGE, GREEN
        QString countries;
        QString severity;
        QStringList ports;   // affected ports, by name
        QDate from;
        QDate to;            // invalid while ongoing
        double lat = 0;
        double lon = 0;
    };

    explicit PortActivity(QObject *parent = nullptr);

    const QVector<Port> &ports() const { return m_ports; }
    const Port *port(const QString &id) const;
    const QVector<Disruption> &disruptions() const { return m_disruptions; }
    // Events still running, or that ended less than `days` ago, latest first.
    QVector<Disruption> recentDisruptions(int days = 30) const;
    QDate windowEnd() const { return m_windowEnd; }
    static constexpr int WindowDays = 7;
    // Days of the window compared with a year before (both days downloaded), out of WindowDays.
    int windowPairs() const { return m_pairs; }

    void refreshIfStale();
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    void refreshNow();
    bool isFetching() const { return m_fetching; }
    QDateTime fetchedAt() const { return m_fetchedAt; }
    QString errorString() const { return m_error; }
    static QUrl probeUrl();

signals:
    void updated();

private:
    struct Step {
        enum Kind { PortsPage, Disruptions, LatestDate, Day } kind;
        int offset = 0;     // PortsPage
        QDate day;          // Day
        bool yearAgo = false;
        int tries = 0;
    };
    void next();
    // A failed step goes back to the front of the queue after `delayMs`; a day that failed 8 times is left for the
    // next fetch, any other step stops this one.
    void retry(Step step, const QString &error, int delayMs);
    // The ports' activity from the days downloaded (each day d of the window with d - 364).
    void applyWindow();
    void request(const Step &step);
    bool handle(const Step &step, const QByteArray &body);
    void fail(const QString &error);
    void finish();
    void loadCache();
    void saveCache() const;

    QNetworkAccessManager m_net;
    QVector<Port> m_ports;
    QHash<QString, int> m_index;   // port id -> index in m_ports
    QVector<Disruption> m_disruptions;
    QDate m_windowEnd;
    QDateTime m_fetchedAt;
    QDateTime m_portsFetchedAt;    // the port database changes rarely: downloaded again after 30 days
    QString m_error;
    bool m_enabled = true;
    bool m_fetching = false;
    // The fetch in progress: steps not sent yet, the new ports and days.
    QVector<Step> m_steps;
    QVector<Port> m_newPorts;
    using DayTotals = QHash<QString, std::array<double, 3>>;   // port id -> calls, tonnes unloaded, loaded
    QMap<QDate, DayTotals> m_days;      // complete days, kept in the cache: a fetch asks only for the missing ones
    QHash<QDate, DayTotals> m_partial;  // days with pages still to come
    int m_pairs = 0;
    int m_inFlight = 0;   // requests sent and not answered yet (up to 3 days at once)
    QTimer m_retryTimer;
};

// A place on the map where crude oil is stored, traded or shipped (bundled list, resources/supply_chain.json).
struct OilSite {
    QString id;
    QString name;
    QString iso3;
    double lat = 0;
    double lon = 0;
    QString kind;      // hub, reserve, export, import
    QString eia;       // EIA weekly series of its stocks, if the EIA publishes one
    QString about;
};

// The supply chain data sets together, as the map page and the Sources page see them.
class SupplyChain : public QObject
{
    Q_OBJECT
public:
    explicit SupplyChain(QObject *parent = nullptr);

    CropYields *crops() const { return m_crops; }
    RoadFreight *road() const { return m_road; }
    OilStocks *oil() const { return m_oil; }
    EiaOilStocks *eia() const { return m_eia; }
    PressureIndex *pressure() const { return m_pressure; }
    FreightIndices *freight() const { return m_freight; }
    PortActivity *ports() const { return m_ports; }
    const QVector<OilSite> &oilSites() const { return m_sites; }
    const OilSite *oilSite(const QString &id) const;
    // Translated names of a crop, a site kind, a disruption type and an alert level.
    static QString cropName(Crop crop);
    static QString siteKindName(const QString &kind);
    static QString disruptionTypeName(const QString &type);
    // Colors in the current theme: a port by its port calls against a year before (grey without figures), an
    // alert level (red, orange, green).
    static QColor portColor(const PortActivity::Port &port);
    static QColor alertColor(const QString &alert);
    // Change in % between two totals, NaN when the earlier one is missing.
    static double changePct(double now, double before);

    void refreshIfStale();

signals:
    void updated();

private:
    CropYields *m_crops;
    RoadFreight *m_road;
    OilStocks *m_oil;
    EiaOilStocks *m_eia;
    PressureIndex *m_pressure;
    FreightIndices *m_freight;
    PortActivity *m_ports;
    QVector<OilSite> m_sites;
};

} // namespace supply
