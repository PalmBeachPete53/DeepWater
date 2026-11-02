#pragma once

// Desktop notifications for fired alerts: the browser Notification API of the web app
// (app.js listenAlerts, panels.js renderAlerts / openAlertDialog), on macOS.
//
// Native notifications go through UserNotifications (UNUserNotificationCenter), which needs the
// app bundle (a bundle identifier) and the user's permission. When the framework can't be used
// (a bare binary outside the bundle, or the system refuses the app), we fall back to
// `osascript -e 'display notification ...'`, which needs no permission of ours; in that case the
// permission reads "granted", as nothing is left to ask for.

#include <QObject>
#include <QString>

#include <functional>

namespace qtm {

class Notifications : public QObject
{
    Q_OBJECT

public:
    static Notifications *instance();

    // Like `"Notification" in window`.
    bool supported() const;
    // Like Notification.permission: "granted", "denied" or "default". It is cached (the system
    // answers asynchronously) and refreshed whenever the app becomes active.
    QString permission() const;
    // Like Notification.requestPermission(): asks once, `done` runs with the new permission.
    void requestPermission(std::function<void(const QString &)> done = {});
    // Like new Notification(title, { body }). Shown in the foreground too.
    void notify(const QString &title, const QString &body);

signals:
    void permissionChanged(const QString &permission);
    // The user clicked one of our notifications (the app is brought to the front already).
    void activated();

private:
    explicit Notifications(QObject *parent = nullptr);
    void refresh();
    void setPermission(const QString &p);
    void notifyFallback(const QString &title, const QString &body);

    bool m_native = false;     // UNUserNotificationCenter usable
    bool m_fallback = false;   // osascript instead
    QString m_permission = "default";
};

} // namespace qtm
