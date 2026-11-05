#pragma once

#include "desk/Positioning.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>

// Myfxbook's community outlook, through its official API and the user's free account. The password goes once to
// Myfxbook, over HTTPS, to open a session, and is never kept: only the session is, which Myfxbook ends after a month or
// when the address changes, and then the Desk asks again. The session travels in the URL, so nothing here goes through
// the Research client, whose errors would show the URL. A free account has 100 requests a day: answers are kept on disk
// and asked at most every half hour.
class Myfxbook : public QObject
{
    Q_OBJECT
public:
    static Myfxbook *instance();

    bool isConnected() const { return !m_session.isEmpty(); }
    void logIn(const QString &email, const QString &password);
    void logOut();

    const desk::Outlook &outlook() const { return m_outlook; }
    QDateTime fetchedAt() const { return m_fetchedAt; }
    QString errorString() const { return m_error; }
    bool isFetching() const { return m_fetching; }
    void refreshIfStale();
    void refreshNow();     // the Refresh of Sources: still not more often than Myfxbook allows
    void setEnabled(bool on);

    // Dev flag --myfxbook-sample <file>: an answer in memory for this run; no request, nothing read or written.
    void loadSampleForThisRun(const QString &path);

signals:
    void changed();
    void loginFailed(const QString &message);

private:
    explicit Myfxbook(QObject *parent = nullptr);
    void fetch();
    void forgetSession();

    QNetworkAccessManager m_net;
    QString m_session;
    desk::Outlook m_outlook;
    QDateTime m_fetchedAt;
    QString m_error;
    bool m_fetching = false;
    bool m_enabled = true;
    bool m_memoryOnly = false;
};
