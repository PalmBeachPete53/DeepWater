#pragma once

// The Terminal tab's pseudo-terminal on Windows: a ConPTY (CreatePseudoConsole, Windows 10 1809 and later) with the
// shell attached, read on a thread of its own. macOS and Linux use forkpty() in Terminal.cpp instead; this header
// declares nothing there. A plain class (no Q_OBJECT), so moc has nothing to do with it on any platform.

#ifdef _WIN32

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <thread>

class QWinEventNotifier;

namespace qtm {

class WinPty
{
public:
    WinPty() = default;
    ~WinPty();
    WinPty(const WinPty &) = delete;
    WinPty &operator=(const WinPty &) = delete;

    // Starts `commandLine` in `dir` with `env` ("NAME=value" entries) on a console of cols x rows. `onData` gets
    // what the console writes and `onExit` the end of the process, both on `context`'s thread (the GUI thread);
    // neither is called once close() has started. False with the reason in `error` on failure.
    bool start(QObject *context, const QString &commandLine, const QString &dir, const QStringList &env, int cols,
               int rows, std::function<void(const QByteArray &)> onData, std::function<void()> onExit,
               QString *error);
    void write(const QByteArray &bytes);
    void resize(int cols, int rows);
    // Closes the console (which ends the processes attached to it) and waits for the reader thread.
    void close();
    qint64 pid() const { return qint64(m_pid); }

private:
    struct Shared;
    std::shared_ptr<Shared> m_shared;   // what the reader thread posts through; outlives this object if needed
    void *m_console = nullptr;          // HPCON
    void *m_input = nullptr;            // our end of the console's input pipe
    void *m_output = nullptr;           // our end of its output pipe
    void *m_process = nullptr;
    void *m_thread = nullptr;
    unsigned long m_pid = 0;
    std::thread m_reader;
    QWinEventNotifier *m_exit = nullptr;
};

} // namespace qtm

#endif // _WIN32
