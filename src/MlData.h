#pragma once

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "KeyIndicators.h"

class CotReport;
class FxData;
class WorldEconomy;
class WorldPolicyRates;
class PropertyCredit;
class HousePriceRatios;
class GlobalDebt;
class BankCredit;
class EconomySectors;

// Writes the data Deepwater holds as CSV files the machine-learning scripts read (<app data>/ml/data): the
// country series of every data set (long form: source, iso3, series, time, value), the IMF's annual indicators,
// the policy rates, the exchange rates, the COT report, and daily prices of chosen symbols from Yahoo (through the
// Research client). Every file has a header; times are ISO dates or fractional years as the sets keep them.
class MlDataExporter : public QObject
{
    Q_OBJECT
public:
    struct Sources {
        KeyIndicators *key = nullptr;   // its sets: OECD, Eurostat, IMF CPI, BLS, national, keyed, surveys
        WorldEconomy *economy = nullptr;
        WorldPolicyRates *rates = nullptr;
        FxData *fx = nullptr;
        CotReport *cot = nullptr;
        PropertyCredit *property = nullptr;
        HousePriceRatios *housePrices = nullptr;
        GlobalDebt *globalDebt = nullptr;
        BankCredit *bankCredit = nullptr;
        EconomySectors *sectors = nullptr;
    };
    // The country series sets and their series' names, in the order of their indices (shared with the local
    // models' tools, LocalTools); sets not given are left out.
    struct CountrySet {
        const class CountrySeriesSet *set = nullptr;
        QString source;        // "oecd", "eurostat", "bis_property"…
        QStringList series;    // "cpi_yoy", "unemployment"…
    };
    static QVector<CountrySet> countrySets(const Sources &s);
    // The names of the IMF World Economic Outlook indicators, in WorldEconomy::Indicator order.
    static QStringList weoIndicators();
    struct File {
        QString name;
        qint64 bytes = 0;
        int rows = 0;
        QDateTime written;
    };

    explicit MlDataExporter(const Sources &sources, QObject *parent = nullptr);

    static QString dir();
    // The symbols whose daily prices are exported (settings ml/symbols).
    static QStringList symbols();
    static void setSymbols(const QStringList &symbols);
    // Writes everything that doesn't need the network at once, then asks Yahoo for the prices (ten years, daily),
    // one symbol after the other; log() tells, finished() comes at the end.
    void exportAll();
    bool isBusy() const { return m_pendingSymbols > 0; }
    static QVector<File> files();
    // The header of a CSV, to pick a target and features.
    static QStringList columns(const QString &file);

signals:
    void log(const QString &line);
    void finished();

private:
    int writeCountrySeries();
    int writeWeo();
    int writeRates();
    int writeFx();
    int writeCot();
    int writeGolden();
    void nextSymbol();

    Sources m_s;
    QStringList m_symbolsLeft;
    int m_pendingSymbols = 0;
};
