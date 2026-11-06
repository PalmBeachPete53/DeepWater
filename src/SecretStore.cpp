#include "SecretStore.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>

#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#endif

namespace SecretStore {

#if defined(Q_OS_MACOS)

namespace {

constexpr char Service[] = "Deepwater";

CFStringRef cf(const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    return CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(utf8.constData()), utf8.size(), kCFStringEncodingUTF8, false);
}

CFMutableDictionaryRef query(const QString &name)
{
    CFMutableDictionaryRef q = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef service = cf(QString::fromLatin1(Service));
    CFStringRef account = cf(name);
    CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(q, kSecAttrService, service);
    CFDictionarySetValue(q, kSecAttrAccount, account);
    CFRelease(service);
    CFRelease(account);
    return q;
}

QString message(OSStatus status)
{
    CFStringRef text = SecCopyErrorMessageString(status, nullptr);
    if (!text)
        return QStringLiteral("Keychain error %1").arg(status);
    const QString out = QString::fromCFString(text);
    CFRelease(text);
    return out;
}

} // namespace

bool write(const QString &name, const QString &value, QString *error)
{
    const QByteArray utf8 = value.toUtf8();
    CFDataRef data = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(utf8.constData()), utf8.size());
    CFMutableDictionaryRef q = query(name);
    CFMutableDictionaryRef change = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(change, kSecValueData, data);
    OSStatus status = SecItemUpdate(q, change);
    if (status == errSecItemNotFound) {
        CFDictionarySetValue(q, kSecValueData, data);
        CFDictionarySetValue(q, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked);
        status = SecItemAdd(q, nullptr);
    }
    CFRelease(change);
    CFRelease(q);
    CFRelease(data);
    if (status != errSecSuccess && error)
        *error = message(status);
    return status == errSecSuccess;
}

QString read(const QString &name)
{
    CFMutableDictionaryRef q = query(name);
    CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(q, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(q, &result);
    CFRelease(q);
    if (status != errSecSuccess || !result)
        return {};
    const QString value = QString::fromUtf8(reinterpret_cast<const char *>(CFDataGetBytePtr(static_cast<CFDataRef>(result))),
                                            CFDataGetLength(static_cast<CFDataRef>(result)));
    CFRelease(result);
    return value;
}

bool remove(const QString &name)
{
    CFMutableDictionaryRef q = query(name);
    const OSStatus status = SecItemDelete(q);
    CFRelease(q);
    return status == errSecSuccess || status == errSecItemNotFound;
}

#elif defined(Q_OS_WIN)

namespace {

std::wstring target(const QString &name)
{
    return (QStringLiteral("Deepwater/") + name).toStdWString();
}

} // namespace

bool write(const QString &name, const QString &value, QString *error)
{
    QByteArray utf8 = value.toUtf8();
    std::wstring t = target(name);
    CREDENTIALW credential {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = t.data();
    credential.CredentialBlobSize = DWORD(utf8.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(utf8.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (CredWriteW(&credential, 0))
        return true;
    if (error)
        *error = QStringLiteral("Credential Manager error %1").arg(GetLastError());
    return false;
}

QString read(const QString &name)
{
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target(name).c_str(), CRED_TYPE_GENERIC, 0, &credential))
        return {};
    const QString value = QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob), int(credential->CredentialBlobSize));
    CredFree(credential);
    return value;
}

bool remove(const QString &name)
{
    return CredDeleteW(target(name).c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND;
}

#else

bool write(const QString &, const QString &, QString *error)
{
    if (error)
        *error = QStringLiteral("No secret store on this system");
    return false;
}

QString read(const QString &)
{
    return {};
}

bool remove(const QString &)
{
    return true;
}

#endif

int check()
{
    const QString name = QStringLiteral("deepwater-secret-check");
    const QString value = QStringLiteral("check-%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString error;
    if (!write(name, value, &error)) {
        qWarning().noquote() << "secret store: write failed:" << error;
        return 1;
    }
    if (read(name) != value) {
        qWarning().noquote() << "secret store: read back something else";
        remove(name);
        return 2;
    }
    if (!write(name, value + "2", &error) || read(name) != value + "2") {
        qWarning().noquote() << "secret store: replacing failed:" << error;
        remove(name);
        return 3;
    }
    if (!remove(name) || !read(name).isEmpty()) {
        qWarning().noquote() << "secret store: remove failed";
        return 4;
    }
    qWarning().noquote() << "secret store: ok";
    return 0;
}

} // namespace SecretStore
