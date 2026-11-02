#pragma once

#include "DomesticFinance.h"

// National statistical offices and central banks read directly, without a key, for the large economies the
// international compilations cover late or not at all. One data set per country (a failing office doesn't take
// the others down), each a CountrySeriesSet with the country's own series; the KeyIndicators façade tries them
// before Eurostat, the OECD and the IMF.

// Brazil: the central bank's SGS (IPCA over twelve months) and IBGE's SIDRA (PNAD unemployment, PIM-PF industrial
// production, PMC retail trade), JSON, no key.
class BrazilIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, Unemployment, IndustryYoY, RetailYoY, SeriesCount };
    explicit BrazilIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// Argentina: INDEC's series through the national open data API (datos.gob.ar): consumer prices and the monthly
// activity estimator (EMAE) over a year, the quarterly unemployment rate.
class ArgentinaIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, ActivityYoY, Unemployment, SeriesCount };
    explicit ArgentinaIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// South Africa: the Reserve Bank's web indicators (consumer and producer prices over a year, the repo rate).
class SouthAfricaIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, PpiYoY, PolicyRate, SeriesCount };
    explicit SouthAfricaIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    void prepare(QNetworkRequest &request) const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// Singapore: the Department of Statistics' table builder (consumer prices over a year).
class SingaporeIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, SeriesCount };
    explicit SingaporeIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    void prepare(QNetworkRequest &request) const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};
