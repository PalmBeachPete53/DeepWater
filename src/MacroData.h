#pragma once

#include "fresh/Fresh.h"

#include <QDate>
#include <QDateTime>
#include <QFuture>
#include <QHash>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <functional>

// Shared by the data sets: a quote-aware CSV line split; one file of a zip archive whose name starts with
// `prefix` (empty if missing); and the values of a World Bank bulk download (a zip of CSV files, one line
// per country or aggregate, one column per year).
QList<QByteArray> splitCsvLine(const QByteArray &line);
QByteArray unzipFile(const QByteArray &zip, const QByteArray &prefix);
// Every line (without its '\n') of the first file of a zip archive whose name starts with `prefix`, inflated a
// megabyte at a time: for files too large to hold unpacked (unzipFile stops at 64 MB). False if none or damaged.
bool forEachZipLine(const QByteArray &zip, const QByteArray &prefix, const std::function<void(const QByteArray &)> &f);
void forEachWorldBankValue(const QByteArray &zip, const std::function<void(const QByteArray &iso3, int year, double value)> &f);

// Base for daily-refreshed public data sets: fetch one URL, parse, cache the
// raw body on disk, retry after failures.
class RemoteDataSet : public QObject
{
    Q_OBJECT
public:
    explicit RemoteDataSet(const QString &cacheName, qint64 staleSecs, QObject *parent = nullptr);
    ~RemoteDataSet() override;

    // The cached copy is read and parsed on the thread pool, all data sets at once (see loadCached):
    // call this before reading the subclass's data the first time. The base class's own calls wait.
    void waitCached() const;

    void refreshIfStale();
    // The last time the source answered, with something new or not.
    QDateTime fetchedAt() const { waitCached(); return m_fetchedAt; }
    // The last time it had something new.
    QDateTime changedAt() const { waitCached(); return m_changedAt.isValid() ? m_changedAt : m_fetchedAt; }
    // How often it's worth asking: see fresh::checkEverySecs.
    qint64 checkEverySecs() const;
    QString errorString() const { waitCached(); return m_error; }

    // Sources page: a source turned off makes no requests (its last cached copy stays in use);
    // refreshNow() downloads at once whatever the age of the data.
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    void refreshNow() { m_manual = true; fetch(); }
    bool isFetching() const { return m_fetching; }
    QUrl sourceUrl() const { return url(); }

signals:
    // New data, a new error, or the answer to refreshNow(). A check that finds nothing new says nothing.
    void updated();

protected:
    // Its period between two checks, when that depends on the moment (FuturesQuotes).
    virtual qint64 staleSecsNow() const { return m_staleSecs; }
    // Call once at the end of the subclass constructor, after its members exist: starts reading and
    // parsing the cached copy on the thread pool. parse() then runs off the UI thread, so it must only
    // touch the data set's own members until waitCached() returns.
    void loadCached();
    virtual QUrl url() const = 0;
    // Data sets downloaded in several requests, one after the other (all must succeed): their URLs, and
    // the one body parse() reads made from their bodies, in the same order.
    virtual QList<QUrl> urls() const { return {url()}; }
    virtual QByteArray joinParts(const QList<QByteArray> &parts) const { return parts.value(0); }
    // Last touches to a request before it's sent (headers a server needs).
    virtual void prepare(QNetworkRequest &) const {}
    // Whether the data set can ask its source at all (a key it needs is missing): false with the reason in `why`
    // makes refreshIfStale() set the error and skip the download, until the next call.
    virtual bool ready(QString *why) const { Q_UNUSED(why); return true; }
    // Parses a downloaded body, or a cached one (as written by cacheBody). Sets m_error on failure.
    virtual bool parse(const QByteArray &body) = 0;
    // What goes to the disk cache after a successful parse: the body itself unless the subclass
    // keeps a smaller form (parse() must then read both).
    virtual QByteArray cacheBody(const QByteArray &body) const { return body; }
    // Data sets with large downloads parse them on the thread pool: parser() returns a function that
    // joins the parts (as joinParts) and parses them, using only what it captured by value; it returns
    // what publishes the result on the UI thread (true on success, m_error set otherwise) and the body to
    // cache (as cacheBody). Empty, the default: joinParts(), parse() and cacheBody() run on the UI thread.
    struct Parsed {
        std::function<bool()> publish;
        QByteArray cacheBody;
    };
    using Parser = std::function<Parsed(const QList<QByteArray> &parts)>;
    virtual Parser parser() { return {}; }

    QString m_error;
    int m_transferTimeoutMs = 60 * 1000;   // without data received

private:
    void fetch();
    void fetchPart();
    void finish(const QByteArray &body);
    void finished(bool ok, const QByteArray &cacheBody);
    void unchanged();
    void readFreshness();
    void saveFreshness() const;

    QString m_cacheName;
    QList<QUrl> m_partUrls;        // the fetch in progress: URLs left, bodies received
    QList<QByteArray> m_parts;
    qint64 m_staleSecs;
    QNetworkAccessManager m_net;
    QDateTime m_fetchedAt;
    QDateTime m_changedAt;
    // What recognises the last answer. The body's hash also covers servers that give no validators: nothing is parsed
    // again, and no page redrawn, for an answer already known.
    fresh::Validators m_validators, m_received, m_sent;
    QString m_validatorsUrl, m_receivedUrl;
    QByteArray m_hash, m_receivedHash;
    bool m_conditionalWorks = false;
    qint64 m_lastBytes = -1;   // the size of the last whole answer
    bool m_manual = false;
    QTimer m_retryTimer;
    bool m_fetching = false;
    bool m_enabled = true;
    mutable QFuture<void> m_cacheLoad;
};

// Central bank policy rates from the BIS (daily series, harmonised across banks).
class PolicyRates : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Bank {
        QString currency;   // EUR
        QString area;       // BIS reference area, XM
        QString name;       // ECB
    };
    struct Point {
        QDate date;
        double rate;
    };

    explicit PolicyRates(QObject *parent = nullptr);

    const QVector<Bank> &banks() const { return m_banks; }
    QVector<Point> history(const QString &currency) const { return m_history.value(currency); }
    bool hasRate(const QString &currency) const { return !m_history.value(currency).isEmpty(); }
    double current(const QString &currency) const;
    // Last change of the rate: its date and size in percentage points.
    bool lastChange(const QString &currency, QDate *date, double *delta) const;
    double changeOver(const QString &currency, int days) const;

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;
    Parser parser() override;

private:
    static bool read(const QByteArray &body, const QVector<Bank> &banks, QHash<QString, QVector<Point>> *history,
                     QString *error);

    QVector<Bank> m_banks;
    QHash<QString, QVector<Point>> m_history;   // currency -> points, oldest first
};

// Weekly positioning in CME currency futures (CFTC Traders in Financial Futures):
// asset managers and leveraged funds. Week-on-week changes show speculative
// capital moving in or out of each currency against the dollar.
class CotData : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Contract {
        QString currency;   // EUR
        QString code;       // CFTC contract market code
    };
    struct Week {
        QDate date;
        qint64 assetMgrNet = 0;
        qint64 levFundsNet = 0;
        qint64 openInterest = 0;
        double assetMgrPct() const { return openInterest ? 100.0 * assetMgrNet / openInterest : 0; }
        double levFundsPct() const { return openInterest ? 100.0 * levFundsNet / openInterest : 0; }
    };

    explicit CotData(QObject *parent = nullptr);

    const QVector<Contract> &contracts() const { return m_contracts; }
    QVector<Week> weeks(const QString &currency) const { return m_weeks.value(currency); }

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;

private:
    QVector<Contract> m_contracts;
    QHash<QString, QVector<Week>> m_weeks;   // currency -> weeks, oldest first
};

// Annual indicators for every country from the IMF World Economic Outlook
// (DataMapper API): history, current-year estimates and projections.
class WorldEconomy : public RemoteDataSet
{
    Q_OBJECT
public:
    enum Indicator {
        GdpGrowth,        // real GDP, annual % change
        Inflation,        // average consumer prices, annual % change
        Unemployment,     // % of labour force
        CurrentAccount,   // % of GDP
        GovBalance,       // general government net lending/borrowing, % of GDP
        GovDebt,          // general government gross debt, % of GDP
        GdpUsd,           // current prices, billions of US dollars
        GdpPerCapitaPpp,  // current international dollars
        ExportVolume,     // exports of goods and services, volume, annual % change
        IndicatorCount
    };
    struct Country {
        QString iso3;
        QString iso2;
        QString name;   // English; translate in context "Country"
        QString region; // continent: AF, AS, EU, NA, OC, SA
    };

    explicit WorldEconomy(QObject *parent = nullptr);

    const QVector<Country> &countries() const { return m_countries; }
    const Country *country(const QString &iso3) const;
    QString displayName(const QString &iso3) const;
    // ISO3 code for an English country name as written in markets.json.
    QString isoForName(const QString &name) const;
    bool hasData() const { return !m_values.isEmpty(); }
    // NaN when the IMF has no figure.
    double value(const QString &iso3, Indicator indicator, int year) const;
    QMap<int, double> series(const QString &iso3, Indicator indicator) const;

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;
    Parser parser() override;

private:
    using Values = QHash<QString, QVector<QMap<int, double>>>;
    static bool read(const QByteArray &body, const QHash<QString, int> &index, Values *values, QString *error);

    QVector<Country> m_countries;
    QHash<QString, int> m_index;
    QHash<QString, QVector<QMap<int, double>>> m_values;   // iso3 -> indicator -> year -> value
};

// Annual balance of payments of every country from the IMF (BPM6): capital flows
// of the financial account and trade in goods and services. Reported figures only,
// so the latest year lags by a year or so and there are no projections.
class BalanceOfPayments : public RemoteDataSet
{
    Q_OBJECT
public:
    enum Series {
        FdiInflows,         // direct investment, net incurrence of liabilities
        PortfolioInflows,   // portfolio investment, net incurrence of liabilities
        FinancialAccount,   // financial account balance: net acquisition of assets less liabilities
        Exports,            // goods and services, credit
        Imports,            // goods and services, debit
        SeriesCount
    };

    explicit BalanceOfPayments(QObject *parent = nullptr);

    bool hasData() const { return !m_values.isEmpty(); }
    // Billions of US dollars, NaN when not reported.
    double value(const QString &iso3, Series series, int year) const;
    // Countries report with different lags: the latest year most of them have
    // reported, and the latest year any has.
    int latestYear() const { return m_latestYear; }
    int lastYear() const { return m_lastYear; }

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;
    Parser parser() override;

private:
    using Values = QHash<QString, QVector<QMap<int, double>>>;
    static bool read(const QByteArray &body, Values *values, int *lastYear, int *latestYear, QString *error);

    QHash<QString, QVector<QMap<int, double>>> m_values;   // iso3 -> series -> year -> value
    int m_latestYear = 0;
    int m_lastYear = 0;
};

// Exports of goods between every pair of countries from the IMF (International Trade
// in Goods by partner country), for the latest year nearly all countries have reported.
// The download is large (~27 MB, every year since 1948), so only that year is cached.
class TradeFlows : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Flow {
        QString from;   // exporter, ISO3
        QString to;     // importer, ISO3
        double value;   // billions of US dollars, as reported by the exporter (FOB)
    };

    explicit TradeFlows(QObject *parent = nullptr);

    bool hasData() const { return !m_flows.isEmpty(); }
    int year() const { return m_year; }
    const QVector<Flow> &flows() const { return m_flows; }   // largest first

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;
    QByteArray cacheBody(const QByteArray &body) const override;
    Parser parser() override;

private:
    static bool read(const QByteArray &body, QVector<Flow> *flows, int *year, QString *error);
    static QByteArray compact(int year, const QVector<Flow> &flows);

    QVector<Flow> m_flows;
    int m_year = 0;
};

// Structure of every country's economy from the World Bank (World Development Indicators): the
// share of GDP and of jobs of the primary sector (agriculture, forestry, fishing), the secondary
// (industry: mining, manufacturing, energy, construction) and the tertiary (services). Value added
// comes from national accounts with a year or two of lag, since 1960 for most countries; jobs are
// ILO modelled estimates, since 1991.
class EconomySectors : public RemoteDataSet
{
    Q_OBJECT
public:
    enum Series {
        PrimaryGdp,         // value added, % of GDP
        SecondaryGdp,
        ManufacturingGdp,   // part of the secondary sector
        TertiaryGdp,
        PrimaryJobs,        // employment, % of total
        SecondaryJobs,
        TertiaryJobs,
        SeriesCount
    };

    explicit EconomySectors(QObject *parent = nullptr);

    bool hasData() const { return !m_values.isEmpty(); }
    // NaN when not reported.
    double value(const QString &iso3, Series series, int year) const;
    // Latest year with a figure for the country, 0 when none.
    int latestYear(const QString &iso3, Series series) const;
    // Every year reported, oldest first.
    QMap<int, double> series(const QString &iso3, Series series) const;

protected:
    QUrl url() const override;
    // One bulk download per series (a zip of CSV files, ~100 KB each, every year since 1960), turned
    // into the compact text form parse() reads and the cache keeps.
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    bool parse(const QByteArray &body) override;
    Parser parser() override;

private:
    using Values = QHash<QString, QVector<QMap<int, double>>>;
    static QByteArray join(const QList<QByteArray> &parts);
    static bool read(const QByteArray &body, Values *values, QString *error);

    QHash<QString, QVector<QMap<int, double>>> m_values;   // iso3 -> series -> year -> value
};

// Month-end policy rates of every central bank in the BIS data set (about 40;
// euro area members get the ECB rate).
class WorldPolicyRates : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Point {
        QDate month;
        double rate;
    };

    explicit WorldPolicyRates(QObject *parent = nullptr);

    // Keyed by ISO 3166 alpha-2 code. Oldest first, empty when not covered.
    QVector<Point> history(const QString &iso2) const;
    bool isEuroArea(const QString &iso2) const;

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;

private:
    QHash<QString, QVector<Point>> m_history;   // BIS area -> points
};
