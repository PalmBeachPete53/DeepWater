#pragma once

#include "MacroData.h"

// Lines of an SDMX CSV (BIS, OECD): calls `f` with the field getter of each data line, by column name.
void forEachCsvRow(const QByteArray &csv, const std::function<void(const std::function<QByteArray(const char *)> &)> &f);

// Property and debt of each country's domestic market, from four public sources. Every data set keeps its
// series per country (ISO3) as time -> value, time being a fractional year: 2024 for a year, the middle of
// a quarter for a quarter (2024.125 for Q1, 2024.375 for Q2…). They download in a few requests and keep a
// compact text form on disk.
class CountrySeriesSet : public RemoteDataSet
{
    Q_OBJECT
public:
    bool hasData() const { return !m_values.isEmpty(); }
    QStringList countries() const { return m_values.keys(); }
    // Oldest first; empty when the country isn't covered.
    QMap<double, double> series(const QString &iso3, int k) const;
    // The latest value (NaN when none) and its time.
    double latest(const QString &iso3, int k, double *time = nullptr) const;
    // The value at `time` or the latest one before it within `tolerance` years (NaN when none).
    double at(const QString &iso3, int k, double time, double tolerance = 0.3) const;
    // The median of the countries' latest values: a yardstick.
    double median(int k) const;
    // ISO 3166 alpha-2 (as used by the BIS and Eurostat) -> alpha-3; empty for areas that aren't countries.
    static QString iso3For(const QString &iso2);

protected:
    CountrySeriesSet(const QString &cacheName, qint64 staleSecs, int seriesCount, QObject *parent);
    QUrl url() const override { return urls().constFirst(); }
    bool parse(const QByteArray &body) override;

    // The compact form joinParts() builds: a header line, then "ISO3 series time value" lines.
    static QByteArray header();
    static void addLine(QByteArray &out, const QString &iso3, int k, double time, double value);
    // "2024-Q2" -> 2024.375, "2024-05" or "2024-M05" -> 2024.375 (mid-period), "2024" -> 2024; 0 when not a
    // period. A quarter and a month can land on the same time: the caller knows which its series are.
    static double periodTime(const QByteArray &period);

private:
    int m_seriesCount;
    QHash<QString, QVector<QMap<double, double>>> m_values;   // iso3 -> series -> time -> value
};

// BIS, quarterly: real residential property prices (since 1970), credit to households and to non-financial
// companies (% of GDP, loans and debt securities, adjusted for breaks) and debt service ratios (% of income).
class PropertyCredit : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        RealHousePrices,        // index, 2010 = 100, deflated by consumer prices
        HouseholdCredit,        // % of GDP
        CorporateCredit,        // non-financial corporations, % of GDP
        HouseholdDebtService,   // interest and principal, % of disposable income
        PrivateDebtService,     // households and companies together, % of income
        SeriesCount
    };
    explicit PropertyCredit(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// OECD, quarterly: house prices against incomes and against rents, in % of their long-term average.
class HousePriceRatios : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        PriceToIncome,   // 100 = long-term average
        PriceToRent,
        SeriesCount
    };
    explicit HousePriceRatios(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    // The OECD server fails now and then (500) on Qt's default "Accept-Language: en-US,*".
    void prepare(QNetworkRequest &request) const override;
};

// IMF Global Debt Database, annual: debt of households and of non-financial companies (loans and debt
// securities, % of GDP) and of the general government (% of GDP, since the 1950s for some), for about 90
// countries.
class GlobalDebt : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        HouseholdDebt,
        CorporateDebt,
        GovernmentDebt,
        SeriesCount
    };
    explicit GlobalDebt(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// World Bank, annual, nearly every country: bank credit to the private sector (% of GDP) and the share of
// bank loans that are non-performing.
class BankCredit : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        PrivateCredit,   // domestic credit to the private sector, % of GDP
        BadLoans,        // non-performing loans, % of gross loans
        SeriesCount
    };
    explicit BankCredit(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};
