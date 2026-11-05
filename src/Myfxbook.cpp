#include "Myfxbook.h"

#include "HttpCache.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrlQuery>

namespace {

constexpr qint64 MinInterval = 30 * 60;   // 48 requests a day at most, of the 100 allowed
const char *const SessionKey = "desk/myfxbook/session";
const char *const CacheName = "myfxbook_outlook";

// By hand: QUrlQuery leaves characters of a password, like + and &, as they are.
QByteArray encoded(const QString &value)
{
    return QUrl::toPercentEncoding(value);
}

QNetworkRequest requestFor(const QByteArray &url)
{
    QNetworkRequest request {QUrl::fromEncoded(url)};
    request.setTransferTimeout(30 * 1000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/1.0"));
    return request;
}

} // namespace

Myfxbook *Myfxbook::instance()
{
    static auto *m = new Myfxbook;
    return m;
}

Myfxbook::Myfxbook(QObject *parent)
    : QObject(parent)
{
    m_session = QSettings().value(SessionKey).toString();
    QByteArray body;
    QDateTime at;
    if (HttpCache::load(CacheName, &body, &at)) {
        m_outlook = desk::parseOutlook(body);
        if (m_outlook.ok)
            m_fetchedAt = at;
    }
}

void Myfxbook::logIn(const QString &email, const QString &password)
{
    const QByteArray url = "https://www.myfxbook.com/api/login.json?email=" + encoded(email) + "&password=" + encoded(password);
    QNetworkReply *reply = m_net.get(requestFor(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        // errorString() would hold the URL, and so the password: only the status is shown.
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            emit loginFailed(tr("Myfxbook can't be reached."));
            return;
        }
        QString message;
        const QString session = desk::parseLogin(reply->readAll(), &message);
        if (session.isEmpty()) {
            emit loginFailed(message.isEmpty() ? tr("Myfxbook refused the login (HTTP %1).").arg(status) : message);
            return;
        }
        m_session = session;
        if (!m_memoryOnly)
            QSettings().setValue(SessionKey, m_session);
        m_error.clear();
        emit changed();
        refreshIfStale();
    });
}

void Myfxbook::logOut()
{
    if (!m_session.isEmpty() && !m_memoryOnly) {
        QNetworkReply *reply = m_net.get(requestFor("https://www.myfxbook.com/api/logout.json?session=" + encoded(m_session)));
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    forgetSession();
    m_error.clear();
    emit changed();
}

void Myfxbook::forgetSession()
{
    m_session.clear();
    if (!m_memoryOnly)
        QSettings().remove(SessionKey);
}

void Myfxbook::setEnabled(bool on)
{
    m_enabled = on;
}

void Myfxbook::refreshIfStale()
{
    if (!m_enabled || !isConnected() || m_fetching || m_memoryOnly)
        return;
    if (!m_fetchedAt.isValid() || m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) >= MinInterval)
        fetch();
}

void Myfxbook::refreshNow()
{
    refreshIfStale();
}

void Myfxbook::fetch()
{
    m_fetching = true;
    QNetworkReply *reply = m_net.get(requestFor("https://www.myfxbook.com/api/get-community-outlook.json?session=" + encoded(m_session)));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_fetching = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const desk::Outlook answer = desk::parseOutlook(body);
        if (answer.ok) {
            m_outlook = answer;
            m_fetchedAt = QDateTime::currentDateTimeUtc();
            m_error.clear();
            HttpCache::save(CacheName, body, m_fetchedAt);
        } else if (answer.sessionInvalid) {
            forgetSession();
            m_error = tr("The Myfxbook session has ended: log in again.");
        } else if (!answer.message.isEmpty() && status == 200) {
            m_error = tr("Myfxbook: %1").arg(answer.message);
        } else {
            // Not errorString(): it would hold the URL, and so the session.
            m_error = status ? tr("Myfxbook answered HTTP %1.").arg(status) : tr("Myfxbook can't be reached.");
        }
        emit changed();
    });
}

void Myfxbook::loadSampleForThisRun(const QString &path)
{
    m_memoryOnly = true;
    m_session = QStringLiteral("sample");
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        m_outlook = desk::parseOutlook(f.readAll());
        m_fetchedAt = QDateTime::currentDateTimeUtc();
    }
    emit changed();
}
