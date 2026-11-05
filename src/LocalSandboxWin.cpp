#include "LocalSandboxWin.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>

#include <memory>
#include <vector>

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>

namespace {

const wchar_t ContainerName[] = L"Deepwater.LocalEngine";

struct Sid {
    PSID sid = nullptr;
    ~Sid()
    {
        if (sid) FreeSid(sid);
    }
};

// The container's identity, created the first time.
HRESULT containerSid(PSID *out)
{
    HRESULT hr = CreateAppContainerProfile(ContainerName, L"Deepwater local engine",
                                           L"Runs a local model without network and without access to the user's files",
                                           nullptr, 0, out);
    if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
        hr = DeriveAppContainerSidFromAppContainerName(ContainerName, out);
    return hr;
}

QString windowsMessage(DWORD code)
{
    wchar_t *text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                   reinterpret_cast<wchar_t *>(&text), 0, nullptr);
    const QString out = text ? QString::fromWCharArray(text).trimmed() : QString::number(code);
    if (text) LocalFree(text);
    return out;
}

// Adds read and execute for the container to the path's access list, for what's inside too when it's a folder.
DWORD grantRead(const QString &path, PSID sid, bool folder)
{
    std::wstring wide = QDir::toNativeSeparators(path).toStdWString();
    PACL oldAcl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    DWORD result = GetNamedSecurityInfoW(wide.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &oldAcl,
                                         nullptr, &descriptor);
    if (result != ERROR_SUCCESS)
        return result;
    EXPLICIT_ACCESSW access {};
    access.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
    access.grfAccessMode = GRANT_ACCESS;
    access.grfInheritance = folder ? SUB_CONTAINERS_AND_OBJECTS_INHERIT : NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    access.Trustee.ptstrName = static_cast<LPWSTR>(sid);
    PACL newAcl = nullptr;
    result = SetEntriesInAclW(1, &access, oldAcl, &newAcl);
    if (result == ERROR_SUCCESS)
        result = SetNamedSecurityInfoW(wide.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, newAcl, nullptr);
    if (newAcl) LocalFree(newAcl);
    if (descriptor) LocalFree(descriptor);
    return result;
}

// What CreateProcess reads: it must live until the call, so the modifier that QProcess keeps holds it.
struct Launch {
    Sid container;
    SECURITY_CAPABILITIES capabilities {};
    std::vector<char> attributes;
    STARTUPINFOEXW startup {};
};

// Closing the job ends the process: its life follows the QProcess it hangs from.
class Job : public QObject
{
public:
    explicit Job(HANDLE handle, QObject *parent) : QObject(parent), m_handle(handle) {}
    ~Job() override { CloseHandle(m_handle); }

private:
    HANDLE m_handle;
};

void attachJob(QProcess *process)
{
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                                              | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    limits.BasicLimitInformation.ActiveProcessLimit = 1;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    if (HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, DWORD(process->processId()))) {
        AssignProcessToJobObject(job, child);
        CloseHandle(child);
    }
    new Job(job, process);
}

} // namespace

namespace localsandbox {

bool prepare(const QStringList &files, const QString &folder, QString *error)
{
    Sid sid;
    const HRESULT hr = containerSid(&sid.sid);
    if (FAILED(hr)) {
        *error = QObject::tr("Windows wouldn't create the sandbox: %1").arg(windowsMessage(HRESULT_CODE(hr)));
        return false;
    }
    for (const QString &file : files) {
        const DWORD result = grantRead(QFileInfo(file).absoluteFilePath(), sid.sid, false);
        if (result != ERROR_SUCCESS) {
            *error = QObject::tr("Windows wouldn't open %1 to the sandbox: %2").arg(QFileInfo(file).fileName(), windowsMessage(result));
            return false;
        }
    }
    // Under Program Files only an administrator may change this, and containers can already read there.
    grantRead(folder, sid.sid, true);
    return true;
}

void applyTo(QProcess *process)
{
    auto launch = std::make_shared<Launch>();
    process->setCreateProcessArgumentsModifier([launch](QProcess::CreateProcessArguments *args) {
        // Never outside the sandbox: these two flags together make CreateProcess refuse.
        const DWORD refuse = CREATE_NEW_CONSOLE | DETACHED_PROCESS;
        if (FAILED(containerSid(&launch->container.sid))) {
            args->flags = refuse;
            return;
        }
        launch->capabilities.AppContainerSid = launch->container.sid;
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        launch->attributes.resize(size);
        auto *list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(launch->attributes.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size)
            || !UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &launch->capabilities,
                                          sizeof launch->capabilities, nullptr, nullptr)) {
            args->flags = refuse;
            return;
        }
        launch->startup.StartupInfo = *args->startupInfo;
        launch->startup.StartupInfo.cb = sizeof launch->startup;
        launch->startup.lpAttributeList = list;
        args->startupInfo = &launch->startup.StartupInfo;
        args->flags |= EXTENDED_STARTUPINFO_PRESENT;
    });
    QObject::connect(process, &QProcess::started, process, [process] { attachJob(process); });
}

} // namespace localsandbox
