#pragma once

#include "MacroData.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QVector>

#include <functional>

// The Calendar page's data: economic releases (TradingView), US corporate events (Nasdaq) and the
// sessions exchanges really traded (Yahoo, to catch holidays and early closes).

// Economic releases of 21 large economies and the euro area, from TradingView's public economic calendar
// (no key; it wants a tradingview.com Origin). One request covers last week to the end of next week, ~1 MB
// of JSON (each event carries a long description), cached in a compact form; a day outside that window
// (the date picker) is downloaded on its own with fetchDay().
class EconomicCalendar : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Event {
        QDateTime time;     // UTC
        QString country;    // ISO 3166 alpha-2, or "EU" for the euro area
        QString title;
        QString period;     // the period the figure covers: "Sep", "Q3", "Oct/24"
        int impact = 0;     // 0 low, 1 medium, 2 high
        double actual = qQNaN(), forecast = qQNaN(), previous = qQNaN();
        QString unit;       // "%", "$", "€", "cf"…
        QString scale;      // "K", "M", "B", "T"
        QString source;
        QString sourceUrl;  // the publisher's page for this figure
    };

    explicit EconomicCalendar(QObject *parent = nullptr);

    static QStringList countries();
    bool hasData() const { return m_from.isValid(); }
    // The days loaded (local dates, `to` excluded).
    QDate from() const { return m_from; }
    QDate to() const { return m_to; }
    const QVector<Event> &events() const { return m_events; }

    void fetchDay(const QDate &day);
    // Today's and its neighbours' figures of a few countries, merged into events(): updated() if one has changed.
    // Small, for asking often while a figure is due (ReleaseWatcher). `done` runs whatever the answer.
    void fetchLive(const QStringList &countries, QObject *ctx, std::function<void()> done);
    // A longer range on its own (the Desk's fundamentals read the last months' releases), for `ctx`'s lifetime.
    // `country` empty: every country of the calendar; set, only that one (a few years of a country stay small).
    void fetchRange(const QString &country, const QDate &from, const QDate &to, QObject *ctx,
                    std::function<void(const QVector<EconomicCalendar::Event> &, const QString &)> done);
    void fetchRange(const QDate &from, const QDate &to, QObject *ctx,
                    std::function<void(const QVector<EconomicCalendar::Event> &events, const QString &error)> done);
    // The request for a range of days and the events of an answer: shared with the survey headlines.
    static QUrl rangeUrl(const QDate &from, const QDate &to);
    static QVector<Event> parseEvents(const QJsonArray &result);

signals:
    void dayFetched(const QDate &day, const QVector<EconomicCalendar::Event> &events, const QString &error);

protected:
    QUrl url() const override;
    void prepare(QNetworkRequest &request) const override;
    bool parse(const QByteArray &body) override;
    QByteArray cacheBody(const QByteArray &body) const override;
    Parser parser() override;   // the ~1 MB answer is parsed and compacted on the thread pool

private:
    void fetchPage(const QString &country, const QDate &from, const QDate &to, QObject *ctx,
                   std::function<void(const QVector<EconomicCalendar::Event> &, const QString &)> done,
                   QVector<EconomicCalendar::Event> soFar, int pages);
    static QByteArray compact(const QVector<Event> &events, const QDate &from, const QDate &to);
    static void window(QDate *from, QDate *to);

    QDate m_from, m_to;
    QVector<Event> m_events;   // by time
    QNetworkAccessManager m_dayNet;
};

// US corporate events from Nasdaq's public calendars (api.nasdaq.com, no key): earnings over five weekdays
// each side of today, dividends going ex over the next seven weekdays, upcoming splits, and IPOs of last month,
// this month and next. One request per day or month, one after the other (~22, about 25 s), kept as they came.
class CorporateCalendar : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Earnings {
        QDate date;
        QString symbol, name;
        QString time;       // "pre", "after" or empty
        double eps = qQNaN(), estimate = qQNaN(), surprisePct = qQNaN(), lastYearEps = qQNaN();
        double marketCap = 0;
        int estimates = 0;
    };
    struct Dividend {
        QString symbol, name;
        QDate exDate, payDate;
        double amount = qQNaN();
    };
    struct Split {
        QString symbol, name;
        QDate date;
        QString ratio;      // "3:1": new shares for old ones
    };
    struct Ipo {
        QString symbol, name, exchange, price;
        QDate date;
        bool priced = false;   // else expected on that date
        double amount = qQNaN();   // dollar value of the shares offered
    };

    explicit CorporateCalendar(QObject *parent = nullptr);

    bool hasData() const { return m_fetchedAtLeastOnce; }
    const QVector<Earnings> &earnings() const { return m_earnings; }   // by date, then market cap
    const QVector<Dividend> &dividends() const { return m_dividends; }
    const QVector<Split> &splits() const { return m_splits; }
    const QVector<Ipo> &ipos() const { return m_ipos; }

protected:
    QUrl url() const override { return urls().constFirst(); }
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    void prepare(QNetworkRequest &request) const override;
    bool parse(const QByteArray &body) override;
    Parser parser() override;   // about 25 answers: joined and parsed on the thread pool

private:
    struct Data {
        QVector<Earnings> earnings;
        QVector<Dividend> dividends;
        QVector<Split> splits;
        QVector<Ipo> ipos;
    };
    // The answers with the URL each came from ({"url", "body"} objects): what the cache keeps.
    static bool read(const QJsonArray &parts, Data *data);
    static QJsonArray join(const QList<QByteArray> &parts, const QList<QUrl> &from);

    QVector<Earnings> m_earnings;
    QVector<Dividend> m_dividends;
    QVector<Split> m_splits;
    QVector<Ipo> m_ipos;
    bool m_fetchedAtLeastOnce = false;
};

// The last regular session of an index on each exchange the Market hours tab shows, from Yahoo's spark endpoint
// (meta.currentTradingPeriod, no key). The tab computes opening hours itself; this tells it when an exchange
// that should be open today didn't open (a holiday) or closed early.
class MarketSessions : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Session {
        QDateTime start, end;   // UTC
    };

    MarketSessions(const QStringList &symbols, QObject *parent = nullptr);

    const Session *session(const QString &symbol) const;

protected:
    QUrl url() const override { return urls().constFirst(); }
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    void prepare(QNetworkRequest &request) const override;
    bool parse(const QByteArray &body) override;

private:
    QStringList m_symbols;
    QHash<QString, Session> m_sessions;
};
