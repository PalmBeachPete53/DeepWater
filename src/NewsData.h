#pragma once

#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>

// The News tab's data: the latest headlines of a category (general, stocks, crypto, forex), optionally for one
// ticker. Two providers: Financial Modeling Prep when its key is entered in Sources (its news endpoints, with the
// ticker filter done by the server); otherwise free RSS feeds (CNBC, MarketWatch, BBC, Nasdaq, Seeking Alpha,
// Investing.com, Cointelegraph, Decrypt, CoinDesk, FXStreet), merged and sorted, the ticker of an item read from
// the feed when it gives one (Nasdaq, Seeking Alpha) or guessed from its text ("(NASDAQ: NFLX)", "Bitcoin",
// "EUR/USD"). Answers are kept five minutes.
namespace news {

enum class Category { General, Stocks, Crypto, Forex };

struct Item {
    QString title;
    QString summary;
    QString url;
    QString site;        // the host of the article ("zacks.com")
    QString symbol;      // "NFLX", "BTCUSD", "EURUSD"; empty when unknown
    QString publisher;
    QDateTime published; // UTC; invalid when the feed gives none
};

// What the Sources page shows per provider ("rss", "fmp").
struct State {
    QDateTime lastOk;
    QDateTime lastError;
    QString error;
    int pending = 0;
};

class Wire : public QObject
{
    Q_OBJECT
public:
    static Wire *instance();

    // `provider`: "fmp" or "rss", the one that answered; `error`: why nothing (or less) came.
    using Done = std::function<void(const QVector<Item> &items, const QString &provider, const QString &error)>;
    // `ctx` guards the callback. `force` ignores the cached answer.
    void fetch(Category category, const QString &ticker, bool force, QObject *ctx, Done done);

    // A Financial Modeling Prep key is saved and the source is on.
    bool usesFmp() const;
    State state(const QString &provider) const { return m_states.value(provider); }
    void setEnabled(const QString &provider, bool on);
    bool isEnabled(const QString &provider) const { return !m_disabled.contains(provider); }
    // Drops the cached answers (Sources › Refresh).
    void forget();

    // The feeds of a category (the Sources page names and probes them).
    static QStringList feedUrls(Category category);

    // The items of one RSS / Atom document; `category` drives the symbol guess; `prefer` is the ticker the feed
    // is about (kept as the badge when the feed lists it among several, or gives none).
    static QVector<Item> parseFeed(const QByteArray &xml, Category category, const QString &prefer = QString());
    static QString guessSymbol(const QString &text, Category category);
    static QString symbolFromFmp(const QString &symbol);

private:
    explicit Wire(QObject *parent = nullptr);
    void fetchFmp(Category category, const QString &ticker, const QString &key, Done done);
    void fetchRss(Category category, const QString &ticker, const QString &note, Done done);
    void deliver(const QString &key, const QVector<Item> &items, const QString &provider, const QString &error);

    struct Waiter {
        QPointer<QObject> ctx;
        Done done;
    };
    struct Cached {
        QVector<Item> items;
        QString provider;
        QString error;
        QDateTime at;
    };
    QNetworkAccessManager m_net;
    QHash<QString, Cached> m_cache;
    QHash<QString, QVector<Waiter>> m_waiting;   // requests in flight, by cache key
    QHash<QString, State> m_states;
    QStringList m_disabled;
};

// The starred articles: kept in the app data folder (news-favorites.json) as they were when starred, newest star
// first. Shown by the Favorites tab; the star of a card or of the reader toggles them.
class Favorites : public QObject
{
    Q_OBJECT
public:
    static Favorites *instance();
    bool contains(const QString &url) const;
    void toggle(const Item &item);
    QVector<Item> items() const;
    int count() const { return m_entries.size(); }

signals:
    void changed();

private:
    explicit Favorites(QObject *parent = nullptr);
    void load();
    void save() const;

    struct Entry {
        Item item;
        QDateTime savedAt;
    };
    QVector<Entry> m_entries;   // newest first
};

// "22m ago", "1h ago", "3d ago"; empty when the time is unknown.
QString ago(const QDateTime &t, const QDateTime &now = QDateTime::currentDateTimeUtc());

} // namespace news
