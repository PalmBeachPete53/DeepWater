// Desktop notifications outside macOS (Windows, Linux): a balloon from a system tray icon, which Windows 10/11 shows
// as a native toast. No permission to ask for: the user turns them off in the system settings. On macOS this file is
// empty and Notifications.mm (UserNotifications) is built instead.

#include "Notifications.h"

#ifndef __APPLE__

#include <QApplication>
#include <QIcon>
#include <QSystemTrayIcon>

namespace qtm {

namespace {

QSystemTrayIcon *trayIcon()
{
    static QSystemTrayIcon *icon = [] {
        auto *i = new QSystemTrayIcon(qApp);
        QIcon appIcon = qApp->windowIcon();
        i->setIcon(appIcon.isNull() ? QIcon(QStringLiteral(":/icon/deepwater-512.png")) : appIcon);
        i->setToolTip(QApplication::applicationDisplayName());
        QObject::connect(i, &QSystemTrayIcon::messageClicked, qApp, [] {
            emit Notifications::instance()->activated();
        });
        return i;
    }();
    return icon;
}

} // namespace

Notifications *Notifications::instance()
{
    static Notifications *n = new Notifications(qApp);
    return n;
}

Notifications::Notifications(QObject *parent) : QObject(parent)
{
    m_native = QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages();
    m_permission = m_native ? QStringLiteral("granted") : QStringLiteral("denied");
}

bool Notifications::supported() const
{
    return m_native;
}

QString Notifications::permission() const
{
    return m_permission;
}

void Notifications::setPermission(const QString &p)
{
    if (p == m_permission) return;
    m_permission = p;
    emit permissionChanged(p);
}

void Notifications::refresh()
{
}

void Notifications::requestPermission(std::function<void(const QString &)> done)
{
    if (done) done(m_permission);
}

void Notifications::notify(const QString &title, const QString &body)
{
    if (!m_native) return;
    QSystemTrayIcon *icon = trayIcon();
    icon->show();   // a toast needs a visible tray icon
    icon->showMessage(title, body, QSystemTrayIcon::Information);
}

void Notifications::notifyFallback(const QString &title, const QString &body)
{
    notify(title, body);
}

} // namespace qtm

#endif // !__APPLE__
