#include "AdBlock.h"

#include "HttpCache.h"

#ifdef DEEPWATER_ADBLOCK
#include "adblock_ffi.h"
#endif

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <QWebEngineUrlRequestInfo>
#include <QtDebug>

namespace adblock {

namespace {

constexpr qint64 KeepSecs = 24 * 3600;

QString cacheName(int index)
{
    return QStringLiteral("adblock-list-%1").arg(index);
}

// Chromium's resource type as adblock-rust's request type.
const char *requestType(QWebEngineUrlRequestInfo::ResourceType type)
{
    using T = QWebEngineUrlRequestInfo;
    switch (type) {
    case T::ResourceTypeMainFrame: return "document";
    case T::ResourceTypeSubFrame:
    case T::ResourceTypeNavigationPreloadSubFrame: return "subdocument";
    case T::ResourceTypeStylesheet: return "stylesheet";
    case T::ResourceTypeScript:
    case T::ResourceTypeJson: return "script";
    case T::ResourceTypeImage:
    case T::ResourceTypeFavicon: return "image";
    case T::ResourceTypeFontResource: return "font";
    case T::ResourceTypeObject:
    case T::ResourceTypePluginResource: return "object";
    case T::ResourceTypeMedia: return "media";
    case T::ResourceTypeXhr: return "xmlhttprequest";
    case T::ResourceTypePing:
    case T::ResourceTypeCspReport: return "ping";
    case T::ResourceTypeWebSocket: return "websocket";
    default: return "other";
    }
}

} // namespace

// Brave's default set on 2026-11-01 (brave/adblock-resources, filter_lists/list_catalog.json: the entries that are
// `default_enabled`, less the Android and iOS ones).
const QVector<FilterList> &filterLists()
{
    static const QVector<FilterList> v {
        {"uBlock Origin filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters.txt"},
        {"uBlock Origin 2020 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2020.txt"},
        {"uBlock Origin 2021 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2021.txt"},
        {"uBlock Origin 2022 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2022.txt"},
        {"uBlock Origin 2023 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2023.txt"},
        {"uBlock Origin 2024 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2024.txt"},
        {"uBlock Origin 2025 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2025.txt"},
        {"uBlock Origin 2026 filters", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-2026.txt"},
        {"uBlock Origin filters – General", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/filters-general.txt"},
        {"uBlock Origin filters – Badware risks", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/badware.txt"},
        {"uBlock Origin filters – Resource abuse", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/resource-abuse.txt"},
        {"uBlock Origin filters – Unbreak", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/unbreak.txt"},
        {"uBlock Origin filters – Quick fixes", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/quick-fixes.txt"},
        {"uBlock Origin filters – Link shorteners", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/ubo-link-shorteners.txt"},
        {"EasyList", "https://easylist.to/easylist/easylist.txt"},
        {"URLhaus malicious URL blocklist", "https://malware-filter.gitlab.io/malware-filter/urlhaus-filter-agh-online.txt"},
        {"Brave Unbreak", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-unbreak.txt"},
        {"Brave Specific", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-specific.txt"},
        {"Brave Social", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-social.txt"},
        {"SugarCoat rules", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-sugarcoat.txt"},
        {"EasyPrivacy", "https://easylist.to/easylist/easyprivacy.txt"},
        {"uBlock Origin filters – Privacy", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/privacy.txt"},
        {"Brave First-Party", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-firstparty.txt"},
        {"Brave First-Party Regional", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-firstparty-regional.txt"},
        {"EasyList Cookie", "https://secure.fanboy.co.nz/fanboy-cookiemonster_ubo.txt"},
        {"uBlock Origin filters – Cookie notices", "https://raw.githubusercontent.com/uBlockOrigin/uAssets/master/filters/annoyances-cookies.txt"},
        {"Brave cookie additions", "https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists/brave-cookie-specific.txt"},
        {"Fanboy mobile notifications", "https://secure.fanboy.co.nz/fanboy-mobile-notifications.txt"},
    };
    return v;
}

Blocker *Blocker::instance()
{
    static Blocker *b = new Blocker;
    return b;
}

Blocker::Blocker(QObject *parent)
    : QObject(parent)
{
    m_net.setTransferTimeout(60000);
}

bool Blocker::available()
{
#ifdef DEEPWATER_ADBLOCK
    return true;
#else
    return false;
#endif
}

void Blocker::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    emit stateChanged();
    if (on)
        start();
}

void Blocker::start(bool force)
{
    if (!available() || !m_enabled || m_building || m_pending > 0)
        return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!force && m_started && m_fetchedAt.isValid() && m_fetchedAt.secsTo(now) < KeepSecs)
        return;
    m_started = true;
    const QVector<FilterList> &lists = filterLists();
    m_bodies.resize(lists.size());
    m_fetched.resize(lists.size());
    m_error.clear();
    for (int i = 0; i < lists.size(); ++i) {
        QByteArray body;
        QDateTime at;
        if (HttpCache::load(cacheName(i), &body, &at)) {
            m_bodies[i] = body;
            m_fetched[i] = at;
        }
        if (!force && at.isValid() && at.secsTo(now) < KeepSecs)
            continue;
        QNetworkRequest req{QUrl(QString::fromLatin1(lists[i].url))};
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1 (desktop app; ad blocker lists)"));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        m_pending++;
        QNetworkReply *reply = m_net.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, i] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() == QNetworkReply::NoError) {
                const QDateTime at = QDateTime::currentDateTimeUtc();
                m_bodies[i] = reply->readAll();
                m_fetched[i] = at;
                HttpCache::save(cacheName(i), m_bodies[i], at);
            } else {
                m_error = QStringLiteral("%1: %2").arg(reply->url().host(),
                                                       status ? QStringLiteral("HTTP %1").arg(status) : reply->errorString());
            }
            if (--m_pending == 0)
                build();
            else
                emit stateChanged();
        });
    }
    if (m_pending == 0)
        build();
    else
        emit stateChanged();
}

void Blocker::build()
{
#ifdef DEEPWATER_ADBLOCK
    QByteArray rules;
    QDateTime oldest;
    int present = 0;
    for (int i = 0; i < m_bodies.size(); ++i) {
        if (m_bodies[i].isEmpty())
            continue;
        present++;
        rules += m_bodies[i];
        rules += '\n';
        if (!oldest.isValid() || m_fetched[i] < oldest)
            oldest = m_fetched[i];
    }
    m_fetchedAt = oldest;
    m_ruleBytes = rules.size();
    if (present == 0) {
        if (m_error.isEmpty())
            m_error = tr("No filter list");
        emit stateChanged();
        return;
    }
    m_building = true;
    emit stateChanged();
#ifdef Q_OS_MACOS
    convertForWebKit(rules);
#endif
    QPointer<Blocker> self(this);
    QThreadPool::globalInstance()->start([self, rules] {
        QElapsedTimer t;
        t.start();
        AdblockEngine *engine = adblock_engine_new(rules.constData(), size_t(rules.size()));
        const qint64 ms = t.elapsed();
        QMetaObject::invokeMethod(self, [self, engine, ms] {
            if (!self) {
                adblock_engine_free(engine);
                return;
            }
            self->m_building = false;
            if (!engine) {
                self->m_error = tr("The filter lists couldn't be compiled");
            } else {
                qInfo().noquote() << QStringLiteral("adblock: engine built in %1 ms from %2 KB of rules").arg(ms).arg(self->m_ruleBytes / 1024);
                // The IO thread may still be inside the old engine: free it a little later.
                if (AdblockEngine *old = self->m_engine.exchange(engine))
                    QTimer::singleShot(5000, [old] { adblock_engine_free(old); });
                emit self->readyChanged();
            }
            emit self->stateChanged();
        }, Qt::QueuedConnection);
    });
#else
    m_error = tr("Built without the ad blocker (no Rust toolchain)");
    emit stateChanged();
#endif
}

void Blocker::convertForWebKit(const QByteArray &rules)
{
#ifdef DEEPWATER_ADBLOCK
    const QString id = QString::fromLatin1(QCryptographicHash::hash(rules, QCryptographicHash::Sha1).toHex().left(16));
    if (id == m_contentRulesId && !m_contentRules.isEmpty())
        return;
    // The same lists converted earlier: the cached result.
    QByteArray cached;
    QDateTime at;
    if (HttpCache::load("adblock-webkit", &cached, &at) && cached.startsWith((id + '\n').toLatin1())) {
        m_contentRules = cached.mid(id.size() + 1);
        m_contentRulesId = id;
        m_contentRuleCount = -1;
        emit contentRulesChanged();
        return;
    }
    QPointer<Blocker> self(this);
    QThreadPool::globalInstance()->start([self, rules, id] {
        QElapsedTimer t;
        t.start();
        size_t count = 0;
        char *json = adblock_content_blocking_json(rules.constData(), size_t(rules.size()), &count);
        const QByteArray out = json ? QByteArray(json) : QByteArray();
        if (json) adblock_string_free(json);
        const qint64 ms = t.elapsed();
        QMetaObject::invokeMethod(self, [self, out, id, count, ms] {
            if (!self) return;
            if (out.isEmpty()) {
                qWarning() << "adblock: the lists couldn't be converted to WebKit content rules";
                return;
            }
            qInfo().noquote() << QStringLiteral("adblock: %1 WebKit content rules in %2 ms").arg(count).arg(ms);
            self->m_contentRules = out;
            self->m_contentRulesId = id;
            self->m_contentRuleCount = int(count);
            HttpCache::save("adblock-webkit", (id + '\n').toLatin1() + out, QDateTime::currentDateTimeUtc());
            emit self->contentRulesChanged();
        }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(rules);
#endif
}

bool Blocker::shouldBlock(const QUrl &url, const QUrl &firstParty, const char *type) const
{
#ifdef DEEPWATER_ADBLOCK
    AdblockEngine *engine = m_engine.load();
    if (!engine || !m_enabled)
        return false;
    return adblock_engine_check(engine, url.toEncoded().constData(), firstParty.toEncoded().constData(), type) == 1;
#else
    Q_UNUSED(url);
    Q_UNUSED(firstParty);
    Q_UNUSED(type);
    return false;
#endif
}

Cosmetic Blocker::cosmeticFor(const QUrl &url) const
{
    Cosmetic out;
#ifdef DEEPWATER_ADBLOCK
    AdblockEngine *engine = m_engine.load();
    if (!engine || !m_enabled)
        return out;
    char *json = adblock_engine_cosmetic(engine, url.toEncoded().constData());
    if (!json)
        return out;
    const QJsonObject o = QJsonDocument::fromJson(QByteArray(json)).object();
    adblock_string_free(json);
    for (const QJsonValue &v : o.value("hide").toArray()) out.hide << v.toString();
    for (const QJsonValue &v : o.value("exceptions").toArray()) out.exceptions << v.toString();
    out.generichide = o.value("generichide").toBool();
#else
    Q_UNUSED(url);
#endif
    return out;
}

QStringList Blocker::hiddenSelectors(const QStringList &classes, const QStringList &ids, const QStringList &exceptions) const
{
    QStringList out;
#ifdef DEEPWATER_ADBLOCK
    AdblockEngine *engine = m_engine.load();
    if (!engine || !m_enabled)
        return out;
    auto json = [](const QStringList &l) { return QJsonDocument(QJsonArray::fromStringList(l)).toJson(QJsonDocument::Compact); };
    char *result = adblock_engine_hidden_selectors(engine, json(classes).constData(), json(ids).constData(), json(exceptions).constData());
    if (!result)
        return out;
    for (const QJsonValue &v : QJsonDocument::fromJson(QByteArray(result)).array()) out << v.toString();
    adblock_string_free(result);
#else
    Q_UNUSED(classes);
    Q_UNUSED(ids);
    Q_UNUSED(exceptions);
#endif
    return out;
}

// ---------------------------------------------------------------- Interceptor

Interceptor::Interceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame
        || info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadMainFrame)
        return;   // the article itself is never blocked
    Blocker *blocker = Blocker::instance();
    if (blocker->shouldBlock(info.requestUrl(), info.firstPartyUrl(), requestType(info.resourceType()))) {
        info.block(true);
        blocker->countBlocked();
        emit blocked(info.requestUrl());
    }
}

} // namespace adblock
