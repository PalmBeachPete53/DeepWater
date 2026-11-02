#pragma once

#include "MacroData.h"

#include <QDateTime>

// Real commodity prices from two public sources, no key:
// - FuturesQuotes: futures traded in the US (NYMEX, COMEX, CBOT, ICE US) from Yahoo Finance, delayed by about
//   ten minutes: the contract Yahoo follows (usually the nearest, sometimes the next), intraday over five days (daily closes over three months for the few quoted
//   only as daily settlements).
// - CommodityBenchmarks: the IMF's monthly average benchmark prices (Primary Commodity Price System), for every
//   commodity; the only source for those without a free futures quote (coal, uranium, nickel, zinc…).
class FuturesQuotes : public RemoteDataSet
{
    Q_OBJECT
public:
    // A contract to quote: a continuous symbol ("CL=F"), or a root rolled by hand when Yahoo's continuous symbol
    // is unreliable (cotton: "CT" + the contract month code + "26" + ".NYB").
    struct Contract {
        QString symbol;
        QString roll;      // contract months as codes (cotton "HKNVZ": March, May, July, October, December)
        QString suffix;
        bool dailyOnly = false;   // daily settlements only, no intraday trades on Yahoo
    };
    struct Point {
        QDateTime time;
        double value;
    };
    struct Quote {
        double price = qQNaN();
        QDateTime time;                    // of the price
        bool settlement = false;           // the price is a daily close (time is then that day's midnight)
        double previousClose = qQNaN();    // the same contract's close on the trading day before the price's
        QDate previousDate;
        QVector<Point> intraday;           // 30-minute closes over five days, oldest first
        QVector<Point> daily;              // daily closes over three months, oldest first
        QString name;                      // Yahoo's: "Crude Oil Dec 26"
    };

    explicit FuturesQuotes(const QVector<Contract> &contracts, QObject *parent = nullptr);

    // The symbol to ask for today: the contract itself, or for a rolled root the first contract month at least
    // two months ahead (Dec 26 in October, Mar 27 in November), before most trading has moved on.
    static QString symbolFor(const Contract &contract, const QDate &today = QDate::currentDate());

    bool hasData() const { return !m_quotes.isEmpty(); }
    const Quote *quote(const QString &symbol) const;

protected:
    QUrl url() const override { return urls().constFirst(); }
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    // Yahoo answers 429 to most user agents, a full browser one included, but not to a bare "Mozilla/5.0".
    void prepare(QNetworkRequest &request) const override;
    bool parse(const QByteArray &body) override;

private:
    QVector<Contract> m_contracts;
    QHash<QString, Quote> m_quotes;   // symbol -> quote
};

class CommodityBenchmarks : public RemoteDataSet
{
    Q_OBJECT
public:
    // `codes`: PCPS indicators, "POILWTI", "PURAN"…
    explicit CommodityBenchmarks(const QStringList &codes, QObject *parent = nullptr);

    bool hasData() const { return !m_series.isEmpty(); }
    // Monthly averages in US dollars (first day of the month -> value), oldest first; empty when not covered.
    QMap<QDate, double> series(const QString &code) const { return m_series.value(code); }

protected:
    QUrl url() const override;
    void prepare(QNetworkRequest &request) const override;   // asks for CSV
    bool parse(const QByteArray &body) override;

private:
    QStringList m_codes;
    QHash<QString, QMap<QDate, double>> m_series;
};
