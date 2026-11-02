#pragma once

// A blocking fetch() for the backend's worker threads: one QNetworkAccessManager per thread and a
// local QEventLoop. Same knobs the JS used: method, headers, body, AbortSignal.timeout(ms).
// HTTPS_PROXY / HTTP_PROXY (and NO_PROXY) are honoured, like proxy.js makes Node do.
//
// A network failure (no answer at all) throws FetchError, with the same fields the JS code reads
// off fetch's TypeError / DOMException: name "TimeoutError" for a timeout, cause.code ("ENOTFOUND",
// "ECONNREFUSED" ...) and message ("fetch failed"). Any HTTP status, 404 or 500 included, is a
// FetchResponse (like fetch, which only rejects when there is no response).

#include <QByteArray>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>

#include <exception>
#include <functional>
#include <optional>
#include <string>

namespace qtm::http {

struct FetchOptions {
    QByteArray method = "GET";
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    bool hasBody = false;
    int timeoutMs = 0;       // 0 = no timeout
};

struct FetchResponse {
    int status = 0;
    QByteArray body;
    QString url;
    bool ok() const { return status >= 200 && status < 300; }
    QString text() const { return QString::fromUtf8(body); }
    // res.json(): throws qtm::Error with a JSON.parse-like message when the body isn't JSON.
    QJsonValue json() const;
};

class FetchError : public std::exception
{
public:
    FetchError(bool timeout, QString code, QString message)
        : m_timeout(timeout), m_code(std::move(code)), m_message(std::move(message)),
          m_what(m_message.toStdString())
    {
    }
    bool timeout() const { return m_timeout; }        // e.name === "TimeoutError"
    QString code() const { return m_code; }           // e.cause?.code ("" when none)
    QString message() const { return m_message; }     // e.message
    const char *what() const noexcept override { return m_what.c_str(); }

private:
    bool m_timeout;
    QString m_code;
    QString m_message;
    std::string m_what;
};

FetchResponse fetch(const QString &url, const FetchOptions &options = {});

// Tests replace the network, like the JS tests replace globalThis.fetch. The mock may throw
// FetchError, or return nothing to let that request through to the real network (the tests' own
// calls to the local API). Pass an empty function to restore the real network.
using FetchMock = std::function<std::optional<FetchResponse>(const QString &url, const FetchOptions &options)>;
void setFetchMock(FetchMock mock);

// Quitting: abortAll(true) ends every fetch in flight at once and makes new ones fail ("fetch
// failed"), so the threads waiting on the network finish; abortAll(false) lets fetches through again.
void abortAll(bool on);

// The proxy URL in use ("" when none), for the start-up note.
QString proxyUrl();

} // namespace qtm::http
