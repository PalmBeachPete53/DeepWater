#include "WinPty.h"

#ifdef _WIN32

#include <QPointer>
#include <QWinEventNotifier>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include <windows.h>

namespace qtm {

namespace {

// CreatePseudoConsole & co. are looked up at run time: the SDK headers only declare them for NTDDI_WIN10_RS5 and later,
// whatever _WIN32_WINNT Qt or the toolchain picked.
using PseudoConsole = void *;
using CreatePseudoConsoleFn = HRESULT(WINAPI *)(COORD, HANDLE, HANDLE, DWORD, PseudoConsole *);
using ResizePseudoConsoleFn = HRESULT(WINAPI *)(PseudoConsole, COORD);
using ClosePseudoConsoleFn = void(WINAPI *)(PseudoConsole);

constexpr DWORD_PTR AttributePseudoConsole = 0x00020016;   // PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE

struct Api {
    CreatePseudoConsoleFn create = nullptr;
    ResizePseudoConsoleFn resize = nullptr;
    ClosePseudoConsoleFn close = nullptr;
};

const Api &api()
{
    static const Api a = [] {
        Api r;
        if (HMODULE k = GetModuleHandleW(L"kernel32.dll")) {
            r.create = reinterpret_cast<CreatePseudoConsoleFn>(GetProcAddress(k, "CreatePseudoConsole"));
            r.resize = reinterpret_cast<ResizePseudoConsoleFn>(GetProcAddress(k, "ResizePseudoConsole"));
            r.close = reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(k, "ClosePseudoConsole"));
        }
        return r;
    }();
    return a;
}

COORD coord(int cols, int rows)
{
    return COORD {SHORT(std::clamp(cols, 2, 32767)), SHORT(std::clamp(rows, 1, 32767))};
}

QString lastError()
{
    wchar_t *text = nullptr;
    const DWORD code = GetLastError();
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   code, 0, reinterpret_cast<wchar_t *>(&text), 0, nullptr);
    const QString s = text ? QString::fromWCharArray(text).trimmed() : QStringLiteral("error %1").arg(code);
    LocalFree(text);
    return s;
}

void closeHandle(void *&h)
{
    if (h && h != INVALID_HANDLE_VALUE)
        CloseHandle(static_cast<HANDLE>(h));
    h = nullptr;
}

} // namespace

struct WinPty::Shared {
    std::atomic<bool> alive {true};
    QPointer<QObject> context;
    std::function<void(const QByteArray &)> onData;
};

WinPty::~WinPty()
{
    close();
}

bool WinPty::start(QObject *context, const QString &commandLine, const QString &dir, const QStringList &env, int cols,
                   int rows, std::function<void(const QByteArray &)> onData, std::function<void()> onExit,
                   QString *error)
{
    const Api &a = api();
    if (!a.create || !a.resize || !a.close) {
        *error = QStringLiteral("This version of Windows has no pseudo console (Windows 10 1809 or later is needed)");
        return false;
    }
    HANDLE inRead = nullptr, inWrite = nullptr, outRead = nullptr, outWrite = nullptr;
    if (!CreatePipe(&inRead, &inWrite, nullptr, 0) || !CreatePipe(&outRead, &outWrite, nullptr, 0)) {
        *error = lastError();
        for (HANDLE h : {inRead, inWrite, outRead, outWrite})
            if (h) CloseHandle(h);
        return false;
    }
    PseudoConsole console = nullptr;
    const HRESULT hr = a.create(coord(cols, rows), inRead, outWrite, 0, &console);
    // The console holds its own ends from now on.
    CloseHandle(inRead);
    CloseHandle(outWrite);
    if (FAILED(hr)) {
        *error = QStringLiteral("CreatePseudoConsole failed (0x%1)").arg(quint32(hr), 8, 16, QChar('0'));
        CloseHandle(inWrite);
        CloseHandle(outRead);
        return false;
    }

    // The process, attached to the console through its attribute list.
    STARTUPINFOEXW si {};
    si.StartupInfo.cb = sizeof si;
    // No standard handles of ours: a GUI app started from a console would otherwise pass its own along.
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<char> attrs(attrSize);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
    bool ok = InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize)
              && UpdateProcThreadAttribute(si.lpAttributeList, 0, AttributePseudoConsole, console, sizeof console,
                                           nullptr, nullptr);
    // Environment block: "NAME=value\0…\0\0", sorted by name without regard to case, as CreateProcess wants it.
    QStringList sorted = env;
    std::sort(sorted.begin(), sorted.end(), [](const QString &x, const QString &y) {
        return x.section('=', 0, 0).compare(y.section('=', 0, 0), Qt::CaseInsensitive) < 0;
    });
    std::wstring block;
    for (const QString &kv : std::as_const(sorted)) {
        block += kv.toStdWString();
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    std::wstring cmd = commandLine.toStdWString();   // CreateProcessW may write into it
    const std::wstring cwd = dir.toStdWString();
    PROCESS_INFORMATION pi {};
    if (ok)
        ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, block.data(),
                            cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi);
    if (si.lpAttributeList)
        DeleteProcThreadAttributeList(si.lpAttributeList);
    if (!ok) {
        *error = lastError();
        a.close(console);
        CloseHandle(inWrite);
        CloseHandle(outRead);
        return false;
    }

    m_console = console;
    m_input = inWrite;
    m_output = outRead;
    m_process = pi.hProcess;
    m_thread = pi.hThread;
    m_pid = pi.dwProcessId;

    m_shared = std::make_shared<Shared>();
    m_shared->context = context;
    m_shared->onData = std::move(onData);
    // The reader blocks in ReadFile; what it gets goes to the GUI thread, unless close() has begun.
    m_reader = std::thread([shared = m_shared, pipe = static_cast<HANDLE>(m_output)] {
        std::vector<char> buf(65536);
        for (;;) {
            DWORD n = 0;
            if (!ReadFile(pipe, buf.data(), DWORD(buf.size()), &n, nullptr) || n == 0)
                break;
            QByteArray bytes(buf.data(), int(n));
            QObject *ctx = shared->context;
            if (!ctx || !shared->alive)
                continue;   // keep draining: ClosePseudoConsole waits for the pipe to empty
            QMetaObject::invokeMethod(ctx, [shared, bytes] {
                if (shared->alive && shared->onData)
                    shared->onData(bytes);
            }, Qt::QueuedConnection);
        }
    });

    // The end of the shell: the console stays open after it, so the pipe alone wouldn't say.
    m_exit = new QWinEventNotifier(static_cast<HANDLE>(m_process), context);
    QObject::connect(m_exit, &QWinEventNotifier::activated, context, [shared = m_shared, onExit = std::move(onExit)] {
        if (shared->alive && onExit)
            onExit();
    });
    return true;
}

void WinPty::write(const QByteArray &bytes)
{
    if (!m_input || bytes.isEmpty())
        return;
    const char *p = bytes.constData();
    DWORD left = DWORD(bytes.size());
    while (left > 0) {
        DWORD n = 0;
        if (!WriteFile(static_cast<HANDLE>(m_input), p, left, &n, nullptr) || n == 0)
            break;
        p += n;
        left -= n;
    }
}

void WinPty::resize(int cols, int rows)
{
    if (m_console)
        api().resize(m_console, coord(cols, rows));
}

void WinPty::close()
{
    if (m_shared)
        m_shared->alive = false;
    if (m_exit) {   // close() may run from this notifier's own signal (the shell exited)
        m_exit->setEnabled(false);
        m_exit->deleteLater();
        m_exit = nullptr;
    }
    closeHandle(m_input);
    if (m_console) {
        api().close(m_console);   // ends the attached processes; returns once the reader has drained the output
        m_console = nullptr;
    }
    if (m_reader.joinable())
        m_reader.join();
    closeHandle(m_output);
    if (m_process && WaitForSingleObject(static_cast<HANDLE>(m_process), 0) == WAIT_TIMEOUT)
        TerminateProcess(static_cast<HANDLE>(m_process), 1);
    closeHandle(m_process);
    closeHandle(m_thread);
    m_pid = 0;
}

} // namespace qtm

#endif // _WIN32
