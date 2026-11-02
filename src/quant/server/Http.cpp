#include "Http.h"

#include "../core/Error.h"
#include "Json.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QHttp1Configuration>
#include <QMetaObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QThread>
#include <QCoreApplication>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace qtm::http {

namespace {

QMutex mockMutex;
FetchMock mockFn;

// the replies in flight, for abortAll()
std::atomic<bool> aborting { false };
QMutex liveMutex;
QSet<QNetworkReply *> live;

QString envProxy()
{
    for (const char *k : { "HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy" }) {
        const QString v = qEnvironmentVariable(k);
        if (!v.isEmpty()) return v;
    }
    return {};
}

bool bypassProxy(const QUrl &u)
{
    const QString host = u.host().toLower();
    if (host == "localhost" || host == "127.0.0.1" || host == "::1" || host.endsWith(".localhost"))
        return true;
    QString no = qEnvironmentVariable("NO_PROXY");
    if (no.isEmpty()) no = qEnvironmentVariable("no_proxy");
    for (QString entry : no.split(',', Qt::SkipEmptyParts)) {
        entry = entry.trimmed().toLower();
        if (entry == "*") return true;
        entry = entry.section(':', 0, 0);
        if (entry.startsWith('.')) entry = entry.mid(1);
        if (entry.startsWith("*.")) entry = entry.mid(2);
        if (!entry.isEmpty() && (host == entry || host.endsWith('.' + entry))) return true;
    }
    return false;
}

// Like Node's env proxy agent: https requests use HTTPS_PROXY (else HTTP_PROXY), http ones HTTP_PROXY.
QString envProxyFor(const QUrl &u)
{
    QStringList keys { "HTTP_PROXY", "http_proxy" };
    if (u.scheme() == "https") keys = QStringList { "HTTPS_PROXY", "https_proxy" } + keys;
    for (const QString &k : keys) {
        const QString v = qEnvironmentVariable(k.toLatin1().constData());
        if (!v.isEmpty()) return v;
    }
    return {};
}

QNetworkProxy proxyFor(const QUrl &u)
{
    const QString p = envProxyFor(u);
    if (p.isEmpty() || bypassProxy(u))
        return QNetworkProxy(QNetworkProxy::NoProxy);
    const QUrl pu(p.contains("://") ? p : "http://" + p);
    QNetworkProxy proxy(QNetworkProxy::HttpProxy, pu.host(), quint16(pu.port(80)));
    if (!pu.userName().isEmpty()) {
        proxy.setUser(QUrl::fromPercentEncoding(pu.userName(QUrl::FullyEncoded).toUtf8()));
        proxy.setPassword(QUrl::fromPercentEncoding(pu.password(QUrl::FullyEncoded).toUtf8()));
    }
    return proxy;
}

// Node's undici reports the system call's code on e.cause.code.
QString codeFor(QNetworkReply::NetworkError e)
{
    switch (e) {
    case QNetworkReply::HostNotFoundError: return QStringLiteral("ENOTFOUND");
    case QNetworkReply::ConnectionRefusedError: return QStringLiteral("ECONNREFUSED");
    case QNetworkReply::RemoteHostClosedError: return QStringLiteral("UND_ERR_SOCKET");
    case QNetworkReply::TimeoutError: return QStringLiteral("UND_ERR_CONNECT_TIMEOUT");
    case QNetworkReply::SslHandshakeFailedError: return QStringLiteral("UNABLE_TO_VERIFY_LEAF_SIGNATURE");
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ProxyAuthenticationRequiredError: return QStringLiteral("ECONNREFUSED");
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError: return QStringLiteral("ENETUNREACH");
    default: return {};
    }
}

// Like Node's fetch, every request shares one connection pool: a single QNetworkAccessManager on
// a thread of its own, so a keep-alive connection (and its TLS session) serves the next request
// to that host whichever worker thread asks. A manager per worker thread meant a new TLS handshake
// almost every time (a second or more against Akamai-fronted hosts like Nasdaq), which made a
// chart that falls through several sources take twice as long as in Node.
class EnvProxyFactory : public QNetworkProxyFactory
{
public:
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override { return { proxyFor(query.url()) }; }
};

struct Network {
    QThread *thread = nullptr;
    QObject *context = nullptr;          // lives in `thread`: requests are started there
    QNetworkAccessManager *nam = nullptr;
};

Network &network()
{
    static Network *n = [] {
        auto *net = new Network;
        net->thread = new QThread;
        net->thread->setObjectName(QStringLiteral("qtm-network"));
        net->context = new QObject;
        net->context->moveToThread(net->thread);
        net->thread->start();
        QMetaObject::invokeMethod(
            net->context,
            [net] {
                net->nam = new QNetworkAccessManager;
                net->nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
                net->nam->setProxyFactory(new EnvProxyFactory);
            },
            Qt::BlockingQueuedConnection);
        QObject::connect(net->thread, &QThread::finished, net->context, [net] {
            delete net->nam;
            net->nam = nullptr;
            delete net->context;
        }, Qt::DirectConnection);
        // Stop the thread before the application object goes, so nothing runs during teardown.
        qAddPostRoutine([] {
            // End what is in flight first (each waiter gets "fetch failed"), refuse new requests,
            // then stop the thread.
            Network &x = network();
            abortAll(true);
            QMetaObject::invokeMethod(x.context, [] {}, Qt::BlockingQueuedConnection);
            x.thread->quit();
            x.thread->wait();
        });
        return net;
    }();
    return *n;
}

// One request's outcome, filled on the network thread and read by the thread that waits.
struct Pending {
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    bool timedOut = false;
    QVariant status;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QString errorString;
    QByteArray body;
    QString url;
    qint64 ms = 0;
    QEventLoop *loop = nullptr;          // set while the application thread waits in an event loop
};

} // namespace

QJsonValue FetchResponse::json() const
{
    bool ok = false;
    const QJsonValue v = jsv::parse(body, &ok);
    if (ok) return v;
    // V8's SyntaxError text for the usual case, a body that isn't JSON at all (an HTML page, "Too
    // Many Requests"): the bad character and up to 10 characters either side of it.
    QString src = QString::fromUtf8(body);
    if (src.startsWith(QChar(0xFEFF))) src.remove(0, 1);   // res.json() decodes UTF-8 without the BOM
    qsizetype pos = 0;
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) ++pos;
    if (pos == src.size())
        throw Error(QStringLiteral("Unexpected end of JSON input"));
    // true / false / null: the first character that leaves the word is the bad one.
    for (const QString word : { QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null") }) {
        if (src[pos] != word[0]) continue;
        qsizetype k = 0;
        while (k < word.size() && pos + k < src.size() && src[pos + k] == word[k]) ++k;
        if (k == word.size()) {
            // The whole word, then something else.
            qsizetype at = pos + k;
            while (at < src.size() && (src[at] == ' ' || src[at] == '\t' || src[at] == '\n' || src[at] == '\r')) ++at;
            const qsizetype line = src.left(at).count('\n') + 1;
            const qsizetype col = at - (src.lastIndexOf('\n', at - 1) + 1) + 1;
            throw Error(QStringLiteral("Unexpected non-whitespace character after JSON at position %1 (line %2 column %3)")
                            .arg(at).arg(line).arg(col));
        }
        if (pos + k == src.size()) throw Error(QStringLiteral("Unexpected end of JSON input"));
        pos += k;
        break;
    }
    const QChar c = src[pos];
    // A character that can start a JSON value fails further in (a cut-off body): V8 then names
    // the position; that detail isn't reproduced, the token is reported where parsing stopped.
    if (QStringLiteral("{[\"-0123456789").contains(c)) {
        const QString t = src.mid(pos);
        throw Error(QStringLiteral("Unexpected token '%1', \"%2\"... is not valid JSON").arg(t.left(1), t.left(10)));
    }
    constexpr qsizetype ctx = 10;
    const qsizetype len = src.size();
    QString msg;
    if (len < 2 * ctx + 1)
        msg = QStringLiteral("\"%1\"").arg(src);
    else if (pos < ctx)
        msg = QStringLiteral("\"%1\"...").arg(src.left(pos + ctx));
    else if (pos < len - ctx)
        msg = QStringLiteral("...\"%1\"...").arg(src.mid(pos - ctx, 2 * ctx));
    else
        msg = QStringLiteral("...\"%1\"").arg(src.mid(pos - ctx));
    throw Error(QStringLiteral("Unexpected token '%1', %2 is not valid JSON").arg(QString(c), msg));
}

void setFetchMock(FetchMock mock)
{
    QMutexLocker lock(&mockMutex);
    mockFn = std::move(mock);
}

void abortAll(bool on)
{
    QMutexLocker lock(&liveMutex);
    aborting = on;
    if (!on)
        return;
    // the replies belong to the network thread: abort them there
    for (QNetworkReply *r : std::as_const(live))
        QMetaObject::invokeMethod(r, [r] { r->abort(); }, Qt::QueuedConnection);
}

QString proxyUrl()
{
    return envProxy();
}

FetchResponse fetch(const QString &url, const FetchOptions &options)
{
    {
        FetchMock mock;
        {
            QMutexLocker lock(&mockMutex);
            mock = mockFn;
        }
        if (mock) {
            if (std::optional<FetchResponse> r = mock(url, options)) {
                if (r->url.isEmpty()) r->url = url;
                return *r;
            }
        }
    }

    const QUrl u(url);
    if (!u.isValid() || (u.scheme() != "http" && u.scheme() != "https") || aborting)
        throw FetchError(false, {}, QStringLiteral("fetch failed"));
    QNetworkRequest req(u);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    // Node's fetch opens as many connections to a host as there are requests in flight; Qt's
    // default of 6 queued the rest (the screener's ten Nasdaq quote batches went 6, then 4).
    static const QHttp1Configuration http1 = [] {
        QHttp1Configuration c;
        c.setNumberOfConnectionsPerHost(64);
        return c;
    }();
    req.setHttp1Configuration(http1);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    // Binance's exchangeInfo is tens of MB once unzipped; Node has no such cap.
    req.setDecompressedSafetyCheckThreshold(-1);
    bool haveUa = false, haveAccept = false, haveType = false;
    for (const auto &h : options.headers) {
        const QByteArray k = h.first.toLower();
        if (k == "user-agent") haveUa = true;
        if (k == "accept") haveAccept = true;
        if (k == "content-type") haveType = true;
        req.setRawHeader(h.first, h.second);
    }
    // What Node's fetch sends when the caller doesn't say.
    if (!haveUa) req.setRawHeader("User-Agent", "node");
    if (!haveAccept) req.setRawHeader("Accept", "*/*");
    if (!haveType && (options.hasBody || !options.body.isEmpty())) req.setRawHeader("Content-Type", "text/plain;charset=UTF-8");

    Network &net = network();
    auto st = std::make_shared<Pending>();
    const QByteArray method = options.method, body = options.body;
    const bool withBody = options.hasBody || !options.body.isEmpty();
    const int timeoutMs = options.timeoutMs;
    QMetaObject::invokeMethod(net.context, [&net, st, req, method, body, withBody, timeoutMs] {
        auto finish = [st](QNetworkReply *reply, bool timedOut, qint64 ms) {
            {
                std::lock_guard lock(st->m);
                st->timedOut = timedOut;
                st->status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                st->error = reply->error();
                st->errorString = reply->errorString();
                st->url = reply->url().toString();
                if (!timedOut) st->body = reply->readAll();
                st->ms = ms;
                st->done = true;
                if (st->loop) QMetaObject::invokeMethod(st->loop, &QEventLoop::quit, Qt::QueuedConnection);
            }
            st->cv.notify_all();
        };
        QNetworkReply *reply = nullptr;
        if (!net.nam || aborting) {
            std::lock_guard lock(st->m);
            st->error = QNetworkReply::OperationCanceledError;
            st->done = true;
            if (st->loop) QMetaObject::invokeMethod(st->loop, &QEventLoop::quit, Qt::QueuedConnection);
            st->cv.notify_all();
            return;
        }
        if (withBody)
            reply = net.nam->sendCustomRequest(req, method, body);
        else if (method == "GET")
            reply = net.nam->get(req);
        else
            reply = net.nam->sendCustomRequest(req, method);
        {
            QMutexLocker lock(&liveMutex);
            live.insert(reply);
            if (aborting) QMetaObject::invokeMethod(reply, [reply] { reply->abort(); }, Qt::QueuedConnection);
        }
        auto took = std::make_shared<QElapsedTimer>();
        took->start();
        auto timedOut = std::make_shared<bool>(false);
        QTimer *timer = nullptr;
        if (timeoutMs > 0) {
            // AbortSignal.timeout(ms)
            timer = new QTimer(reply);
            timer->setSingleShot(true);
            QObject::connect(timer, &QTimer::timeout, reply, [reply, timedOut] {
                *timedOut = true;
                reply->abort();
            });
            timer->start(timeoutMs);
        }
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, timer, timedOut, took, finish] {
            if (timer) timer->stop();
            {
                QMutexLocker lock(&liveMutex);
                live.remove(reply);
            }
            finish(reply, *timedOut, took->elapsed());
            reply->deleteLater();
        });
    }, Qt::QueuedConnection);

    if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
        // The application thread keeps serving its events while it waits (the local API lives
        // there, and a caller on that thread may be asking it).
        QEventLoop loop;
        {
            std::lock_guard lock(st->m);
            if (!st->done) st->loop = &loop;
        }
        if (st->loop) loop.exec(QEventLoop::ExcludeUserInputEvents);
        std::lock_guard lock(st->m);
        st->loop = nullptr;
    } else {
        std::unique_lock lock(st->m);
        st->cv.wait(lock, [&] { return st->done; });
    }

    static const bool debug = qEnvironmentVariableIsSet("QTM_HTTP_DEBUG");
    if (debug)
        qInfo("fetch %s %s -> %s%s (%s) %lld ms", options.method.constData(), qPrintable(url),
              st->status.isValid() ? qPrintable(st->status.toString()) : "no answer", st->timedOut ? " timeout" : "",
              qPrintable(st->errorString), st->ms);
    FetchResponse out;
    out.url = st->url.isEmpty() ? url : st->url;
    if (st->timedOut)
        throw FetchError(true, {}, QStringLiteral("The operation was aborted due to timeout"));
    // An HTTP error status is still an answer. A connection error or a broken body (a reset
    // mid-body, a gzip stream that doesn't inflate) means it didn't arrive: fetch rejects then too.
    const QNetworkReply::NetworkError err = st->error;
    const bool broken = (err >= 1 && err <= 199) || err == QNetworkReply::ProtocolFailure;
    if (!st->status.isValid() || broken)
        throw FetchError(false, codeFor(err), QStringLiteral("fetch failed"));
    out.status = st->status.toInt();
    out.body = std::move(st->body);
    return out;
}

} // namespace qtm::http
