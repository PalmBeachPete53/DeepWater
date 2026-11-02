#include "Notifications.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointer>
#include <QProcess>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

using qtm::Notifications;

// Shows our notifications while the app is in front too (macOS hides them by default), and
// brings the app forward when one is clicked.
@interface QtmNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation QtmNotificationDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    Q_UNUSED(center);
    Q_UNUSED(notification);
    completionHandler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList
                      | UNNotificationPresentationOptionSound);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completionHandler
{
    Q_UNUSED(center);
    Q_UNUSED(response);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (@available(macOS 14, *))
            [NSApp activate];
        else
            [NSApp activateIgnoringOtherApps:YES];
        emit Notifications::instance()->activated();
    });
    completionHandler();
}

@end

namespace qtm {

namespace {

QString fromStatus(UNAuthorizationStatus s)
{
    switch (s) {
    case UNAuthorizationStatusAuthorized:
    case UNAuthorizationStatusProvisional:
        return "granted";
    case UNAuthorizationStatusDenied:
        return "denied";
    default:
        return "default";
    }
}

} // namespace

Notifications *Notifications::instance()
{
    static Notifications *n = new Notifications(qApp);
    return n;
}

Notifications::Notifications(QObject *parent) : QObject(parent)
{
    // UNUserNotificationCenter throws without a bundle identifier (the binary run on its own).
    m_native = NSBundle.mainBundle.bundleIdentifier.length > 0;
    if (!m_native) {
        m_fallback = true;
        m_permission = "granted";
        return;
    }
    static QtmNotificationDelegate *delegate = [[QtmNotificationDelegate alloc] init];
    UNUserNotificationCenter.currentNotificationCenter.delegate = delegate;
    refresh();
    // The user may change it in System Settings while we run.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive) refresh();
    });
}

bool Notifications::supported() const
{
    return m_native || m_fallback;
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
    if (m_fallback) return;
    QPointer<Notifications> self(this);
    [UNUserNotificationCenter.currentNotificationCenter
        getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
            const QString p = fromStatus(settings.authorizationStatus);
            QMetaObject::invokeMethod(qApp, [self, p] {
                if (self && !self->m_fallback) self->setPermission(p);
            }, Qt::QueuedConnection);
        }];
}

void Notifications::requestPermission(std::function<void(const QString &)> done)
{
    if (m_fallback) {
        if (done) done(m_permission);
        return;
    }
    QPointer<Notifications> self(this);
    [UNUserNotificationCenter.currentNotificationCenter
        requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                      completionHandler:^(BOOL granted, NSError *error) {
            // An error means the system won't let this build post notifications at all (it
            // refuses some unsigned apps): use the osascript fallback from now on. A plain "no"
            // is the user's choice and stays "denied".
            const bool refused = error != nil;
            const QString why = refused ? QString::fromNSString(error.localizedDescription) : QString();
            const QString p = granted ? QStringLiteral("granted") : QStringLiteral("denied");
            QMetaObject::invokeMethod(qApp, [self, refused, why, p, done] {
                if (!self) return;
                if (refused) {
                    qWarning("Notifications: %s; using osascript instead", qPrintable(why));
                    self->m_fallback = true;
                    self->setPermission("granted");
                } else {
                    self->setPermission(p);
                }
                if (done) done(self->m_permission);
            }, Qt::QueuedConnection);
        }];
}

void Notifications::notify(const QString &title, const QString &body)
{
    if (m_fallback) return notifyFallback(title, body);
    UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
    content.title = title.toNSString();
    content.body = body.toNSString();
    content.sound = UNNotificationSound.defaultSound;
    // A new identifier each time: notifications stack like the browser's untagged ones.
    UNNotificationRequest *request =
        [UNNotificationRequest requestWithIdentifier:NSUUID.UUID.UUIDString content:content trigger:nil];
    QPointer<Notifications> self(this);
    [UNUserNotificationCenter.currentNotificationCenter
        addNotificationRequest:request
         withCompletionHandler:^(NSError *error) {
            if (!error) return;
            const QString why = QString::fromNSString(error.localizedDescription);
            QMetaObject::invokeMethod(qApp, [self, why, title, body] {
                if (!self) return;
                qWarning("Notifications: %s; using osascript instead", qPrintable(why));
                self->notifyFallback(title, body);
            }, Qt::QueuedConnection);
        }];
}

// `display notification` from AppleScript. The texts go in as arguments (argv), never into the
// script itself, so nothing needs escaping. The notification shows under Script Editor.
void Notifications::notifyFallback(const QString &title, const QString &body)
{
    QProcess::startDetached("/usr/bin/osascript",
                            {"-e", "on run argv",
                             "-e", "display notification (item 2 of argv) with title (item 1 of argv)",
                             "-e", "end run",
                             title, body});
}

} // namespace qtm
