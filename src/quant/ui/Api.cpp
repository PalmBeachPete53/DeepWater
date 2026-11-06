#include "Api.h"

#include "../core/Error.h"
#include "../server/Async.h"
#include "../server/Server.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <QUrl>
#include <QUrlQuery>
#include <QWidget>

namespace qtm {

namespace {

// the backend waits for this pool when it goes (Async.h)
QThreadPool *pool()
{
    return async::requestPool();
}

// Runs `work` on the pool and hands its result to `done` on `context`'s thread.
template <typename T>
void runAsync(QObject *context, std::function<T()> work, std::function<void(const T &)> done,
              api::Fail fail)
{
    QPointer<QObject> guard(context);
    pool()->start([guard, work = std::move(work), done = std::move(done), fail = std::move(fail)] {
        T result {};
        QString error;
        bool ok = true;
        try {
            result = work();
        } catch (const Error &e) {
            ok = false;
            error = e.message();
        } catch (const std::exception &e) {
            ok = false;
            error = QString::fromUtf8(e.what());
        }
        // back on the UI thread, where `context` lives and may be deleted meanwhile: the guard
        // is checked there (reading it here would race with the deletion)
        QObject *app = QCoreApplication::instance();
        if (!app)
            return;
        QMetaObject::invokeMethod(app, [guard, ok, result, error, done, fail] {
            if (!guard)
                return;
            if (ok) {
                if (done) done(result);
            } else if (fail) {
                fail(error);
            }
        }, Qt::QueuedConnection);
    });
}

// JS req(): the error is data.error, else the body, else "HTTP <status>".
QString errorText(const Response &r)
{
    if (r.isText)
        return r.text.isEmpty() ? QString("HTTP %1").arg(r.status) : QString::fromUtf8(r.text);
    if (r.json.isObject() && r.json.toObject().value("error").isString()
        && !r.json.toObject().value("error").toString().isEmpty())
        return r.json.toObject().value("error").toString();
    if (r.json.isString() && !r.json.toString().isEmpty())
        return r.json.toString();
    return QString("HTTP %1").arg(r.status);
}

void request(QObject *context, const QString &method, const QString &url, const QByteArray &body,
             api::Ok ok, api::Fail fail)
{
    const QUrl u(QStringLiteral("http://localhost") + url);
    const QString path = u.path(QUrl::FullyDecoded);
    QHash<QString, QString> query;
    for (const auto &item : QUrlQuery(u).queryItems(QUrl::FullyDecoded))
        query.insert(item.first, item.second);
    runAsync<Response>(
        context,
        [method, path, query, body]() -> Response {
            Backend *b = Backend::instance();
            if (!b)
                throw Error("The local server is not responding. Is it still running?");
            return b->handle(method, path, query, body);
        },
        [ok, fail](const Response &r) {
            if (r.status < 200 || r.status >= 300) {
                if (fail) fail(errorText(r));
                return;
            }
            if (ok) ok(r.isText ? QJsonValue(QString::fromUtf8(r.text)) : r.json);
        },
        fail);
}

QByteArray jsonBody(const QJsonValue &v)
{
    if (v.isObject()) return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
    if (v.isArray()) return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    return QByteArray("{}");
}

QString enc(const QString &s)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

} // namespace

namespace api {

void get(QObject *context, const QString &url, Ok ok, Fail fail)
{
    request(context, "GET", url, {}, std::move(ok), std::move(fail));
}
void post(QObject *context, const QString &url, const QJsonValue &body, Ok ok, Fail fail)
{
    request(context, "POST", url, jsonBody(body), std::move(ok), std::move(fail));
}
void put(QObject *context, const QString &url, const QJsonValue &body, Ok ok, Fail fail)
{
    request(context, "PUT", url, jsonBody(body), std::move(ok), std::move(fail));
}
void patch(QObject *context, const QString &url, const QJsonValue &body, Ok ok, Fail fail)
{
    request(context, "PATCH", url, jsonBody(body), std::move(ok), std::move(fail));
}
void del(QObject *context, const QString &url, Ok ok, Fail fail)
{
    request(context, "DELETE", url, {}, std::move(ok), std::move(fail));
}
void postText(QObject *context, const QString &url, const QByteArray &body, Ok ok, Fail fail)
{
    request(context, "POST", url, body, std::move(ok), std::move(fail));
}

void candles(QObject *context, const QString &symbol, const QString &tf, int limit,
             std::function<void(const CandleSet &)> ok, Fail fail)
{
    runAsync<CandleSet>(
        context,
        [symbol, tf, limit]() -> CandleSet {
            Backend *b = Backend::instance();
            if (!b)
                throw Error("The local server is not responding. Is it still running?");
            return b->candles(symbol, tf, limit);
        },
        std::move(ok), std::move(fail));
}

void storedCandles(QObject *context, const QString &symbol, const QString &tf, std::function<void(const StoredCandles &)> ok)
{
    runAsync<StoredCandles>(
        context,
        [symbol, tf]() -> StoredCandles {
            Backend *b = Backend::instance();
            return b ? b->candleStore().read(symbol, tf).value_or(StoredCandles{}) : StoredCandles{};
        },
        std::move(ok), [](const QString &) {});
}

void saveCandles(QObject *context, const QString &symbol, const QString &tf, const StoredCandles &stored)
{
    runAsync<int>(
        context,
        [symbol, tf, stored]() -> int {
            if (Backend *b = Backend::instance()) b->candleStore().write(symbol, tf, stored);
            return 0;
        },
        [](const int &) {}, [](const QString &) {});
}

void quotes(QObject *context, const QStringList &symbols, Ok ok, Fail fail)
{
    get(context, "/api/quotes?symbols=" + enc(symbols.join(',')), std::move(ok), std::move(fail));
}

void search(QObject *context, const QString &q, Ok ok, Fail fail)
{
    get(context, "/api/search?q=" + enc(q), std::move(ok), std::move(fail));
}

void run(QObject *context, std::function<QJsonObject()> work, std::function<void(const QJsonObject &)> done,
         Fail fail)
{
    runAsync<QJsonObject>(context, std::move(work), std::move(done), std::move(fail));
}

} // namespace api

void download(QWidget *parent, const QString &name, const QByteArray &content)
{
    const QString path = QFileDialog::getSaveFileName(parent, QString(), name);
    if (path.isEmpty())
        return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content);
}

} // namespace qtm
