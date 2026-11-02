#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QReadWriteLock>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QWebEngineUrlRequestInterceptor>

#include <atomic>

struct AdblockEngine;

// The article reader's ad blocker: Brave's engine (adblock-rust, third_party/adblock-ffi) fed with the filter
// lists Brave turns on by default (uBlock Origin's, EasyList, EasyPrivacy, Brave's own, the cookie-notice and
// app-promo lists; `list_catalog.json` of brave/adblock-resources). The lists are downloaded once a day (HttpCache)
// and the engine is built on the thread pool; until then nothing is blocked. Network requests are checked from
// Chromium's IO thread (the engine is thread-safe); cosmetic rules (element hiding) are asked by the reader on the
// UI thread. Turned off in Sources, it makes no request and blocks nothing.
namespace adblock {

struct FilterList {
    const char *title;
    const char *url;
};
const QVector<FilterList> &filterLists();

struct Cosmetic {
    QStringList hide;         // selectors to hide on this page
    QStringList exceptions;   // generic selectors that must not be hidden here
    bool generichide = false; // the page is excepted from the generic rules
};

class Blocker : public QObject
{
    Q_OBJECT
public:
    static Blocker *instance();

    // Compiled with the engine (a Rust toolchain was there at build time).
    static bool available();
    bool ready() const { return m_engine.load() != nullptr; }
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on);
    // Downloads the lists (those older than a day) and builds the engine, once; later calls do nothing while the
    // copy is fresh. `force` downloads them again.
    void start(bool force = false);

    // Thread-safe: block this request? `type` is adblock-rust's request type ("script", "image"…).
    bool shouldBlock(const QUrl &url, const QUrl &firstParty, const char *type) const;
    Cosmetic cosmeticFor(const QUrl &url) const;
    // The generic hiding selectors for a page whose elements carry these classes and ids.
    QStringList hiddenSelectors(const QStringList &classes, const QStringList &ids, const QStringList &exceptions) const;

    // The lists as WebKit content-blocker rules (the macOS reader): a JSON array, and an id (the lists' hash) for
    // WKContentRuleListStore's cache. Empty until converted (seconds, on the thread pool; the result is kept in
    // HttpCache under the same hash).
    QByteArray contentRules() const { return m_contentRules; }
    QString contentRulesId() const { return m_contentRulesId; }
    int contentRuleCount() const { return m_contentRuleCount; }

    // For the Sources page.
    QDateTime fetchedAt() const { return m_fetchedAt; }
    QString errorString() const { return m_error; }
    bool isFetching() const { return m_pending > 0 || m_building; }
    int ruleBytes() const { return m_ruleBytes; }
    qint64 blockedTotal() const { return m_blocked.load(); }
    void countBlocked() { m_blocked.fetch_add(1); }

signals:
    void readyChanged();
    void stateChanged();
    void contentRulesChanged();

private:
    explicit Blocker(QObject *parent = nullptr);
    void build();
    void convertForWebKit(const QByteArray &rules);

    QNetworkAccessManager m_net;
    std::atomic<AdblockEngine *> m_engine {nullptr};
    QVector<QByteArray> m_bodies;        // one per list, loaded or downloaded
    QVector<QDateTime> m_fetched;
    int m_pending = 0;
    bool m_building = false;
    bool m_started = false;
    bool m_enabled = true;
    QDateTime m_fetchedAt;               // the oldest list's time
    QString m_error;
    int m_ruleBytes = 0;
    std::atomic<qint64> m_blocked {0};
    QByteArray m_contentRules;
    QString m_contentRulesId;
    int m_contentRuleCount = 0;
};

// Installed on the reader's web profile: asks the engine about every sub-resource. Runs on the IO thread.
class Interceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit Interceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

signals:
    void blocked(const QUrl &url);   // queued to the UI thread
};

} // namespace adblock
