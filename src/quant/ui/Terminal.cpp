#include "Terminal.h"

#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <utility>
#ifdef _WIN32
#include "WinPty.h"
#include <QStandardPaths>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#include <util.h>
#else
#include <pty.h>
#endif
#endif

namespace qtm {

namespace {

constexpr int ScrollbackLines = 5000;
constexpr int ThumbWidth = 8;

// The 16 ANSI colours: VS Code's terminal palettes, which read well on dark and light backgrounds.
QColor ansiColor(int i, bool light)
{
    static const QRgb dark[16] = {
        0x000000, 0xcd3131, 0x0dbc79, 0xe5e510, 0x2472c8, 0xbc3fbc, 0x11a8cd, 0xe5e5e5,
        0x666666, 0xf14c4c, 0x23d18b, 0xf5f543, 0x3b8eea, 0xd670d6, 0x29b8db, 0xe5e5e5,
    };
    static const QRgb bright[16] = {
        0x000000, 0xcd3131, 0x00bc00, 0x949800, 0x0451a5, 0xbc05bc, 0x0598bc, 0x555555,
        0x666666, 0xcd3131, 0x14ce14, 0xb5ba00, 0x0451a5, 0xbc05bc, 0x0598bc, 0xa5a5a5,
    };
    return QColor::fromRgb(light ? bright[i] : dark[i]);
}

// xterm's 256-colour table: the 16 ANSI colours, a 6x6x6 cube, then 24 greys.
QColor indexedColor(int idx, bool light)
{
    if (idx < 16)
        return ansiColor(idx, light);
    if (idx < 232) {
        static const int level[6] = {0, 95, 135, 175, 215, 255};
        const int i = idx - 16;
        return QColor(level[i / 36], level[(i / 6) % 6], level[i % 6]);
    }
    const int g = 8 + 10 * (idx - 232);
    return QColor(g, g, g);
}

struct Colors {
    QColor fg, bg, cursor, selection;
    bool light = false;
    static Colors current()
    {
        Colors c;
        c.fg = theme::color("--text");
        c.bg = theme::color("--bg");
        c.cursor = theme::color("--text");
        c.selection = theme::color("--accent", 0.35);
        c.light = c.bg.lightnessF() > 0.5;
        return c;
    }
    QColor resolve(const VTermColor &v, bool foreground) const
    {
        if (foreground && VTERM_COLOR_IS_DEFAULT_FG(&v)) return fg;
        if (!foreground && VTERM_COLOR_IS_DEFAULT_BG(&v)) return bg;
        if (VTERM_COLOR_IS_INDEXED(&v)) return indexedColor(v.indexed.idx, light);
        return QColor(v.rgb.red, v.rgb.green, v.rgb.blue);
    }
};

bool isBlank(const VTermScreenCell &c)
{
    return (c.chars[0] == 0 || c.chars[0] == ' ') && VTERM_COLOR_IS_DEFAULT_BG(&c.bg) && !c.attrs.reverse;
}

QString cellText(const VTermScreenCell &c)
{
    int n = 0;
    while (n < VTERM_MAX_CHARS_PER_CELL && c.chars[n] && c.chars[n] != uint32_t(-1)) ++n;
    return QString::fromUcs4(reinterpret_cast<const char32_t *>(c.chars), n);
}

#ifndef _WIN32
// A shell that exited is reaped when it's gone; one that ignores SIGHUP gets SIGKILL after 3 s.
void reap(pid_t pid, int tries = 0)
{
    int status = 0;
    if (waitpid(pid, &status, WNOHANG) != 0)
        return;
    if (tries == 30)
        kill(pid, SIGKILL);
    if (tries > 40 || !qApp)
        return;
    QTimer::singleShot(100, qApp, [pid, tries] { reap(pid, tries + 1); });
}
#endif

} // namespace

// ================================================================ TerminalView

TerminalView::TerminalView(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setCursor(Qt::IBeamCursor);
    updateFont();

    m_vt = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, &TerminalView::cbOutput, this);
    m_screen = vterm_obtain_screen(m_vt);
    static const VTermScreenCallbacks callbacks = {
        &TerminalView::cbDamage,
        nullptr,   // moverect: damage covers it
        &TerminalView::cbMoveCursor,
        &TerminalView::cbSetTermProp,
        &TerminalView::cbBell,
        nullptr,   // resize
        &TerminalView::cbPushLine,
        &TerminalView::cbPopLine,
        &TerminalView::cbClearScrollback,
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_SCROLL);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_enable_reflow(m_screen, true);
    vterm_screen_reset(m_screen, 1);
}

TerminalView::~TerminalView()
{
    closePty();
    vterm_free(m_vt);
}

bool TerminalView::start(const QString &dir)
{
    if (m_pid > 0)
        return true;
    updateGridSize();
#ifdef _WIN32
    // PowerShell 7 if installed, else the Windows PowerShell every Windows has, else cmd.exe.
    QString program = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
    QString commandLine;
    if (!program.isEmpty()) {
        commandLine = QStringLiteral("\"%1\" -NoLogo").arg(QDir::toNativeSeparators(program));
    } else if (!(program = QStandardPaths::findExecutable(QStringLiteral("powershell"))).isEmpty()) {
        commandLine = QStringLiteral("\"%1\" -NoLogo").arg(QDir::toNativeSeparators(program));
    } else {
        program = qEnvironmentVariable("COMSPEC", QStringLiteral("cmd.exe"));
        commandLine = QStringLiteral("\"%1\"").arg(program);
    }
    QProcessEnvironment winEnv = QProcessEnvironment::systemEnvironment();
    winEnv.insert("TERM", "xterm-256color");
    winEnv.insert("COLORTERM", "truecolor");
    winEnv.insert("TERM_PROGRAM", "Deepwater");
    auto *pty = new WinPty;
    QString why;
    const bool started = pty->start(
        this, commandLine, QDir::toNativeSeparators(dir.isEmpty() ? QDir::homePath() : dir), winEnv.toStringList(),
        m_cols, m_rows,
        [this](const QByteArray &bytes) {
            vterm_input_write(m_vt, bytes.constData(), size_t(bytes.size()));
            vterm_screen_flush_damage(m_screen);
        },
        [this] {
            vterm_screen_flush_damage(m_screen);
            closePty();
            update();
            emit finished();
        },
        &why);
    if (!started) {
        delete pty;
        m_error = why;
        return false;
    }
    m_winpty = pty;
    m_fd = 0;
    m_pid = pty->pid();
    m_process = QFileInfo(program).completeBaseName();
    return true;
#else

    QByteArray shell = qgetenv("SHELL");
    if (shell.isEmpty() || !QFileInfo(QString::fromLocal8Bit(shell)).isExecutable())
        shell = "/bin/zsh";
    // "-zsh": a login shell, like Terminal.app and VS Code on macOS (reads .zprofile, sets PATH).
    const QByteArray argv0 = "-" + QFileInfo(QString::fromLocal8Bit(shell)).fileName().toLocal8Bit();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "xterm-256color");
    env.insert("COLORTERM", "truecolor");
    env.insert("TERM_PROGRAM", "Deepwater");
    env.remove("TERM_SESSION_ID");
    // Opened from Finder the app has no locale: without one zsh shows accents as escapes.
    if (env.value("LANG").isEmpty() && env.value("LC_ALL").isEmpty() && env.value("LC_CTYPE").isEmpty())
        env.insert("LANG", "en_US.UTF-8");
    // Everything the child needs is built before fork(): after it, only async-signal-safe calls.
    QList<QByteArray> envStore;
    for (const QString &kv : env.toStringList())
        envStore << kv.toLocal8Bit();
    std::vector<char *> envp;
    for (QByteArray &kv : envStore)
        envp.push_back(kv.data());
    envp.push_back(nullptr);
    QByteArray argv0Copy = argv0;
    char *argv[] = {argv0Copy.data(), nullptr};
    const QByteArray cwd = QFile::encodeName(dir.isEmpty() ? QDir::homePath() : dir);
    const int maxFd = int(std::min<long>(sysconf(_SC_OPEN_MAX), 4096));

    struct winsize ws = {};
    ws.ws_row = ushort(m_rows);
    ws.ws_col = ushort(m_cols);
    ws.ws_xpixel = ushort(m_cols * m_cw);
    ws.ws_ypixel = ushort(m_rows * m_ch);
    int fd = -1;
    const pid_t pid = forkpty(&fd, nullptr, nullptr, &ws);
    if (pid < 0) {
        m_error = QString::fromLocal8Bit(strerror(errno));
        return false;
    }
    if (pid == 0) {
        for (int i = 3; i < maxFd; ++i)
            ::close(i);
        for (int sig : {SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGCHLD, SIGTSTP, SIGTTIN, SIGTTOU, SIGWINCH})
            signal(sig, SIG_DFL);
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        if (chdir(cwd.constData()) != 0)
            (void)chdir("/");
        execve(shell.constData(), argv, envp.data());
        _exit(127);
    }

    m_fd = fd;
    m_pid = pid;
    fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) | O_NONBLOCK);
    fcntl(m_fd, F_SETFD, FD_CLOEXEC);
    m_readNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &TerminalView::readPty);
    m_writeNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Write, this);
    m_writeNotifier->setEnabled(false);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, &TerminalView::flushWrites);
    m_process = QFileInfo(QString::fromLocal8Bit(shell)).fileName();
    return true;
#endif
}

void TerminalView::closePty()
{
#ifdef _WIN32
    if (WinPty *pty = std::exchange(m_winpty, nullptr)) {
        pty->close();
        delete pty;
    }
    m_fd = -1;
    m_pid = -1;
    m_pendingWrite.clear();
#else
    delete m_readNotifier;
    m_readNotifier = nullptr;
    delete m_writeNotifier;
    m_writeNotifier = nullptr;
    if (m_fd >= 0) {
        ::close(m_fd);   // the foreground job gets SIGHUP
        m_fd = -1;
    }
    if (m_pid > 0) {
        kill(pid_t(m_pid), SIGHUP);
        reap(pid_t(m_pid));
        m_pid = -1;
    }
#endif
}

void TerminalView::readPty()
{
#ifndef _WIN32
    char buf[65536];
    qint64 total = 0;
    for (;;) {
        const ssize_t n = ::read(m_fd, buf, sizeof buf);
        if (n > 0) {
            vterm_input_write(m_vt, buf, size_t(n));
            total += n;
            if (total >= 1 << 20)   // let the UI breathe during a flood (cat of a big file)
                break;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && errno == EAGAIN)
            break;
        // 0 or EIO: the shell has gone.
        vterm_screen_flush_damage(m_screen);
        closePty();
        update();
        emit finished();
        return;
    }
    vterm_screen_flush_damage(m_screen);
    if (!m_processAt.isValid() || m_processAt.elapsed() > 300)
        updateProcessName();
#endif
}

void TerminalView::updateProcessName()
{
    m_processAt.start();
#ifdef __APPLE__   // the shell's name stays on the tab elsewhere
    if (m_fd < 0)
        return;
    const pid_t pg = tcgetpgrp(m_fd);
    if (pg <= 0)
        return;
    char name[2 * MAXCOMLEN + 1] = {};
    if (proc_name(pg, name, sizeof name) <= 0)
        return;
    const QString n = QString::fromLocal8Bit(name);
    if (n != m_process) {
        m_process = n;
        emit processNameChanged();
    }
#endif
}

void TerminalView::writePty(const QByteArray &bytes)
{
    if (m_fd < 0)
        return;
    m_pendingWrite += bytes;
    flushWrites();
}

void TerminalView::flushWrites()
{
#ifdef _WIN32
    if (m_winpty)
        m_winpty->write(m_pendingWrite);
    m_pendingWrite.clear();
#else
    while (!m_pendingWrite.isEmpty() && m_fd >= 0) {
        const ssize_t n = ::write(m_fd, m_pendingWrite.constData(), size_t(m_pendingWrite.size()));
        if (n > 0) {
            m_pendingWrite.remove(0, n);
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        break;   // EAGAIN: the rest goes when the pty can take it
    }
    if (m_writeNotifier)
        m_writeNotifier->setEnabled(!m_pendingWrite.isEmpty());
#endif
    // Output for a key while scrolled back: back to the prompt, like VS Code.
}

// ---------------------------------------------------------------- libvterm callbacks

int TerminalView::cbDamage(VTermRect r, void *user)
{
    auto *t = static_cast<TerminalView *>(user);
    if (t->m_scroll > 0) {
        t->update();
        return 1;
    }
    t->update(t->cellRect(r.start_row, r.start_col, r.end_col - r.start_col)
                  .united(t->cellRect(r.end_row - 1, r.start_col, r.end_col - r.start_col)));
    return 1;
}

int TerminalView::cbMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
    auto *t = static_cast<TerminalView *>(user);
    t->m_cursorRow = pos.row;
    t->m_cursorCol = pos.col;
    t->m_cursorVisible = visible;
    const int s = t->m_scroll;
    t->update(t->cellRect(oldpos.row + s, oldpos.col, 2).adjusted(-2, -2, 2, 2));
    t->update(t->cellRect(pos.row + s, pos.col, 2).adjusted(-2, -2, 2, 2));
    return 1;
}

int TerminalView::cbSetTermProp(VTermProp prop, VTermValue *val, void *user)
{
    auto *t = static_cast<TerminalView *>(user);
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        t->m_cursorVisible = val->boolean;
        break;
    case VTERM_PROP_CURSORSHAPE:
        t->m_cursorShape = val->number;
        break;
    case VTERM_PROP_ALTSCREEN:
        t->m_altScreen = val->boolean;
        t->m_scroll = 0;
        t->m_selStart = t->m_selEnd = {};
        break;
    case VTERM_PROP_MOUSE:
        t->m_mouseMode = val->number;
        break;
    default:
        return 1;
    }
    t->update();
    return 1;
}

int TerminalView::cbBell(void *)
{
    return 1;   // silent, like VS Code
}

int TerminalView::cbPushLine(int cols, const VTermScreenCell *cells, void *user)
{
    auto *t = static_cast<TerminalView *>(user);
    int n = cols;
    while (n > 0 && isBlank(cells[n - 1])) --n;
    t->m_scrollback.emplace_back(cells, cells + n);
    if (int(t->m_scrollback.size()) > ScrollbackLines) {
        t->m_scrollback.pop_front();
        ++t->m_dropped;
    }
    // Scrolled back: the view stays on the same lines while new ones arrive.
    if (t->m_scroll > 0)
        t->m_scroll = std::min<int>(t->m_scroll + 1, int(t->m_scrollback.size()));
    return 1;
}

int TerminalView::cbPopLine(int cols, VTermScreenCell *cells, void *user)
{
    auto *t = static_cast<TerminalView *>(user);
    if (t->m_scrollback.empty())
        return 0;
    const Line line = std::move(t->m_scrollback.back());
    t->m_scrollback.pop_back();
    VTermColor fg, bg;
    vterm_state_get_default_colors(vterm_obtain_state(t->m_vt), &fg, &bg);
    for (int i = 0; i < cols; ++i) {
        if (i < line.size()) {
            cells[i] = line[i];
        } else {
            VTermScreenCell blank = {};
            blank.width = 1;
            blank.fg = fg;
            blank.bg = bg;
            cells[i] = blank;
        }
    }
    if (t->m_scroll > 0)
        t->m_scroll = std::max(0, t->m_scroll - 1);
    return 1;
}

int TerminalView::cbClearScrollback(void *user)
{
    auto *t = static_cast<TerminalView *>(user);
    t->m_dropped += int(t->m_scrollback.size());
    t->m_scrollback.clear();
    t->m_scroll = 0;
    t->update();
    return 1;
}

void TerminalView::cbOutput(const char *bytes, size_t len, void *user)
{
    static_cast<TerminalView *>(user)->writePty(QByteArray(bytes, qsizetype(len)));
}

// ---------------------------------------------------------------- geometry

void TerminalView::updateFont()
{
    m_font = theme::monoFont(12);
    m_bold = m_font;
    m_bold.setWeight(QFont::Bold);
    const QFontMetricsF fm(m_font);
    m_cw = fm.horizontalAdvance(QLatin1Char('M'));
    m_ch = std::ceil(fm.height() * 1.15);
    m_ascent = qRound((m_ch - fm.height()) / 2 + fm.ascent());
}

void TerminalView::updateGridSize()
{
    const int cols = std::max(2, int((width() - m_pad.left() - m_pad.right() - ThumbWidth) / m_cw));
    const int rows = std::max(1, int((height() - m_pad.top() - m_pad.bottom()) / m_ch));
    if (rows == m_rows && cols == m_cols)
        return;
    m_rows = rows;
    m_cols = cols;
    vterm_set_size(m_vt, rows, cols);
    vterm_screen_flush_damage(m_screen);
    m_scroll = std::min<int>(m_scroll, int(m_scrollback.size()));
#ifdef _WIN32
    if (m_winpty)
        m_winpty->resize(cols, rows);
#else
    if (m_fd >= 0) {
        struct winsize ws = {};
        ws.ws_row = ushort(rows);
        ws.ws_col = ushort(cols);
        ws.ws_xpixel = ushort(cols * m_cw);
        ws.ws_ypixel = ushort(rows * m_ch);
        ioctl(m_fd, TIOCSWINSZ, &ws);
    }
#endif
    update();
}

void TerminalView::resizeEvent(QResizeEvent *)
{
    updateGridSize();
}

QRect TerminalView::cellRect(int row, int col, int width) const
{
    const int x0 = m_pad.left() + qRound(col * m_cw), x1 = m_pad.left() + qRound((col + width) * m_cw);
    const int y0 = m_pad.top() + qRound(row * m_ch), y1 = m_pad.top() + qRound((row + 1) * m_ch);
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

int TerminalView::lineCount() const
{
    return int(m_scrollback.size()) + m_rows;
}

int TerminalView::firstVisibleLine() const
{
    return int(m_scrollback.size()) - m_scroll;
}

bool TerminalView::cellAt(int line, int col, VTermScreenCell *cell) const
{
    const int sb = int(m_scrollback.size());
    if (line < 0 || col < 0 || col >= m_cols)
        return false;
    if (line < sb) {
        const Line &l = m_scrollback[size_t(line)];
        if (col >= l.size())
            return false;
        *cell = l[col];
        return true;
    }
    if (line - sb >= m_rows)
        return false;
    *cell = {};
    return vterm_screen_get_cell(m_screen, VTermPos {line - sb, col}, cell);
}

QString TerminalView::lineText(int line, int from, int to) const
{
    QString out;
    for (int col = from; col < to; ++col) {
        VTermScreenCell c;
        if (!cellAt(line, col, &c))
            break;
        if (c.chars[0] == uint32_t(-1))
            continue;
        out += c.chars[0] ? cellText(c) : QStringLiteral(" ");
    }
    while (out.endsWith(' ')) out.chop(1);
    return out;
}

QRect TerminalView::thumbRect() const
{
    const int total = lineCount();
    if (m_scrollback.empty() || total <= m_rows || m_altScreen)
        return {};
    const int h = height() - 4;
    const int th = std::max(24, h * m_rows / total);
    const int y = 2 + (h - th) * firstVisibleLine() / std::max(1, total - m_rows);
    return QRect(width() - ThumbWidth - 1, y, ThumbWidth - 2, th);
}

void TerminalView::scrollTo(int offset)
{
    offset = std::clamp(offset, 0, int(m_scrollback.size()));
    if (offset == m_scroll)
        return;
    m_scroll = offset;
    m_thumbShown = true;
    m_fadeTimer.start(1200, this);
    update();
}

void TerminalView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_fadeTimer.timerId()) {
        m_fadeTimer.stop();
        m_thumbShown = false;
        update(thumbRect().adjusted(-2, -height(), 2, height()));
    }
    QWidget::timerEvent(e);
}

// ---------------------------------------------------------------- painting

void TerminalView::paintEvent(QPaintEvent *e)
{
    const Colors colors = Colors::current();
    QPainter p(this);
    p.fillRect(e->rect(), colors.bg);

    const int first = firstVisibleLine();
    const int r0 = std::max(0, int((e->rect().top() - m_pad.top()) / m_ch));
    const int r1 = std::min(m_rows - 1, int((e->rect().bottom() - m_pad.top()) / m_ch));
    const bool focused = hasFocus();
    const int cursorRow = m_cursorRow + m_scroll;
    const bool blockCursor = focused && m_cursorVisible && m_fd >= 0 && cursorRow < m_rows && m_preedit.isEmpty()
                             && m_cursorShape == VTERM_PROP_CURSORSHAPE_BLOCK;

    struct Drawn {
        QColor fg, bg;
        QString text;
        int width = 1;
        bool bold = false, italic = false, underline = false, strike = false, skip = false, cursor = false;
    };
    QVector<Drawn> cells(m_cols);
    for (int row = r0; row <= r1; ++row) {
        const int line = first + row;
        for (int col = 0; col < m_cols; ++col) {
            Drawn &d = cells[col];
            d = Drawn();
            VTermScreenCell c;
            if (!cellAt(line, col, &c)) {
                d.fg = colors.fg;
                d.bg = colors.bg;
            } else {
                d.skip = c.chars[0] == uint32_t(-1);
                d.fg = colors.resolve(c.fg, true);
                d.bg = colors.resolve(c.bg, false);
                if (c.attrs.reverse) std::swap(d.fg, d.bg);
                if (c.attrs.conceal) d.fg = d.bg;
                d.text = cellText(c);
                d.width = std::max<int>(1, c.width);
                d.bold = c.attrs.bold;
                d.italic = c.attrs.italic;
                d.underline = c.attrs.underline;
                d.strike = c.attrs.strike;
            }
            if (blockCursor && row == cursorRow && col == m_cursorCol) {
                d.cursor = true;
                d.bg = colors.cursor;
                d.fg = colors.bg;
            }
        }
        // backgrounds, then the selection over them
        for (int col = 0; col < m_cols; ++col) {
            const Drawn &d = cells[col];
            if (d.skip) continue;
            if (d.bg != colors.bg) p.fillRect(cellRect(row, col, d.width), d.bg);
            if (selected(line, col)) p.fillRect(cellRect(row, col, d.width), colors.selection);
        }
        // text: runs of plain ASCII in one style go in one drawText; anything else cell by cell so
        // a glyph from a fallback font can't push the rest of the line out of the grid
        const int baseline = m_pad.top() + qRound(row * m_ch) + m_ascent;
        int col = 0;
        while (col < m_cols) {
            const Drawn &d = cells[col];
            if (d.skip || d.text.isEmpty() || (d.text == QLatin1String(" ") && !d.underline && !d.strike)) {
                ++col;
                continue;
            }
            const bool ascii = d.text.size() == 1 && d.text[0].unicode() < 0x80 && !d.cursor;
            QString run = d.text;
            int end = col + d.width;
            if (ascii) {
                while (end < m_cols) {
                    const Drawn &n = cells[end];
                    if (n.skip || n.cursor || n.fg != d.fg || n.bold != d.bold || n.italic != d.italic
                        || n.underline != d.underline || n.strike != d.strike)
                        break;
                    if (n.text.isEmpty()) run += ' ';
                    else if (n.text.size() == 1 && n.text[0].unicode() < 0x80) run += n.text;
                    else break;
                    ++end;
                }
            }
            QFont f = d.bold ? m_bold : m_font;
            f.setItalic(d.italic);
            p.setFont(f);
            p.setPen(d.fg);
            const int x = m_pad.left() + qRound(col * m_cw);
            p.drawText(QPointF(x, baseline), run);
            const int x1 = m_pad.left() + qRound(end * m_cw);
            if (d.underline) p.fillRect(QRect(x, baseline + 2, x1 - x, 1), d.fg);
            if (d.strike) p.fillRect(QRect(x, baseline - m_ascent / 3, x1 - x, 1), d.fg);
            col = end;
        }
    }

    // the cursor when it isn't a filled block: a bar, an underline, or a hollow box out of focus
    if (m_cursorVisible && m_fd >= 0 && cursorRow < m_rows && !blockCursor && cursorRow >= r0 && cursorRow <= r1) {
        const QRect r = cellRect(cursorRow, m_cursorCol);
        if (!m_preedit.isEmpty()) {
            // the dead key being composed (^ then e = ê), underlined at the cursor
            p.setFont(m_font);
            p.setPen(colors.fg);
            const int baseline = r.top() + m_ascent;
            p.fillRect(QRect(r.left(), r.top(), qRound(m_preedit.size() * m_cw), r.height()), colors.bg);
            p.drawText(QPointF(r.left(), baseline), m_preedit);
            p.fillRect(QRect(r.left(), baseline + 2, qRound(m_preedit.size() * m_cw), 1), colors.fg);
        } else if (!focused) {
            p.setPen(QPen(colors.cursor, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(0, 0, -1, -1));
        } else if (m_cursorShape == VTERM_PROP_CURSORSHAPE_UNDERLINE) {
            p.fillRect(QRect(r.left(), r.bottom() - 1, r.width(), 2), colors.cursor);
        } else {
            p.fillRect(QRect(r.left(), r.top(), 2, r.height()), colors.cursor);
        }
    }

    if ((m_hover || m_thumbShown || m_thumbDrag) && !m_scrollback.empty() && !m_altScreen) {
        const QRect t = thumbRect();
        if (!t.isEmpty()) {
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::color("--scroll"));
            p.drawRoundedRect(QRectF(t), 3, 3);
        }
    }
}

void TerminalView::refreshTheme()
{
    updateFont();
    updateGridSize();
    update();
}

// ---------------------------------------------------------------- keyboard

bool TerminalView::event(QEvent *e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        // Keys go to the shell, not to the window's shortcuts (Space, Escape on the map...), except
        // Command shortcuts this widget doesn't use and Control+` (show / hide the terminal).
        auto *k = static_cast<QKeyEvent *>(e);
        const Qt::KeyboardModifiers mods = k->modifiers();
#ifndef Q_OS_MACOS
        // Windows and Linux: every key goes to the shell (Ctrl+R, Ctrl+L…) except Ctrl+` (show / hide the terminal).
        if ((mods & Qt::ControlModifier) && k->key() == Qt::Key_QuoteLeft)
            return QWidget::event(e);
        e->accept();
        return true;
#endif
        if (mods & Qt::ControlModifier) {
            static const QList<int> mine = {Qt::Key_C, Qt::Key_V, Qt::Key_A, Qt::Key_K, Qt::Key_Left, Qt::Key_Right,
                                            Qt::Key_Backspace};
            if (mine.contains(k->key())) {
                e->accept();
                return true;
            }
            return QWidget::event(e);
        }
        if ((mods & Qt::MetaModifier) && k->key() == Qt::Key_QuoteLeft)
            return QWidget::event(e);
        e->accept();
        return true;
    }
    return QWidget::event(e);
}

void TerminalView::sendKey(int vtermKey, int mod)
{
    vterm_keyboard_key(m_vt, VTermKey(vtermKey), VTermModifier(mod));
}

void TerminalView::keyPressEvent(QKeyEvent *e)
{
    const Qt::KeyboardModifiers mods = e->modifiers();
    const bool shift = mods & Qt::ShiftModifier;
    const int key = e->key();
#ifdef Q_OS_MACOS
    // On macOS Qt reports Command as Control and the Control key as Meta.
    const bool command = mods & Qt::ControlModifier;
    const bool control = mods & Qt::MetaModifier;
    const bool option = mods & Qt::AltModifier;
#else
    // Windows and Linux, like Windows Terminal: Ctrl sends the control codes; the terminal's own shortcuts are
    // Ctrl+Shift+C / V / A / K, plus Ctrl+C with a selection (copy) and Ctrl+V (paste).
    const bool ctrl = mods & Qt::ControlModifier;
    const bool shortcutKey = key == Qt::Key_C || key == Qt::Key_V || key == Qt::Key_A || key == Qt::Key_K;
    const bool command = ctrl && ((shift && shortcutKey) || (key == Qt::Key_C && hasSelection()) || key == Qt::Key_V);
    const bool control = ctrl && !command;
    const bool option = mods & Qt::AltModifier;
#endif

    if (command) {
        switch (key) {
        case Qt::Key_C: copySelection(); break;
        case Qt::Key_V: paste(QApplication::clipboard()->text()); break;
        case Qt::Key_A: setSelection({m_dropped, 0}, {m_dropped + lineCount() - 1, m_cols}); break;
        case Qt::Key_K: clearScreen(); break;
        case Qt::Key_Left: scrollTo(0); writePty("\x01"); break;    // start of line
        case Qt::Key_Right: scrollTo(0); writePty("\x05"); break;   // end of line
        case Qt::Key_Backspace: scrollTo(0); writePty("\x15"); break;   // delete to start of line
        default: e->ignore(); return;
        }
        e->accept();
        return;
    }
    // Shift+PageUp / PageDown / Home / End scroll the history.
    if (shift && !control && !option && !m_altScreen) {
        switch (key) {
        case Qt::Key_PageUp: scrollTo(m_scroll + m_rows - 1); return;
        case Qt::Key_PageDown: scrollTo(m_scroll - (m_rows - 1)); return;
        case Qt::Key_Home: scrollTo(int(m_scrollback.size())); return;
        case Qt::Key_End: scrollTo(0); return;
        default: break;
        }
    }
    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Meta || key == Qt::Key_Alt
        || key == Qt::Key_CapsLock)
        return;

    scrollTo(0);
    m_selStart = m_selEnd = {};
    int mod = VTERM_MOD_NONE;
    if (shift) mod |= VTERM_MOD_SHIFT;
    if (control) mod |= VTERM_MOD_CTRL;

    switch (key) {
    case Qt::Key_Return: sendKey(VTERM_KEY_ENTER, mod); return;
    case Qt::Key_Enter: sendKey(VTERM_KEY_ENTER, mod); return;
    case Qt::Key_Tab: sendKey(VTERM_KEY_TAB, mod); return;
    case Qt::Key_Backtab: sendKey(VTERM_KEY_TAB, mod | VTERM_MOD_SHIFT); return;
    case Qt::Key_Backspace:
        if (option) writePty("\x17");   // delete the previous word
        else sendKey(VTERM_KEY_BACKSPACE, mod);
        return;
    case Qt::Key_Escape: sendKey(VTERM_KEY_ESCAPE, mod); return;
    case Qt::Key_Up: sendKey(VTERM_KEY_UP, mod | (option ? VTERM_MOD_ALT : 0)); return;
    case Qt::Key_Down: sendKey(VTERM_KEY_DOWN, mod | (option ? VTERM_MOD_ALT : 0)); return;
    case Qt::Key_Left:
        if (option && !control) writePty("\x1b" "b");   // word back
        else sendKey(VTERM_KEY_LEFT, mod);
        return;
    case Qt::Key_Right:
        if (option && !control) writePty("\x1b" "f");   // word forward
        else sendKey(VTERM_KEY_RIGHT, mod);
        return;
    case Qt::Key_Insert: sendKey(VTERM_KEY_INS, mod); return;
    case Qt::Key_Delete: sendKey(VTERM_KEY_DEL, mod); return;
    case Qt::Key_Home: sendKey(VTERM_KEY_HOME, mod); return;
    case Qt::Key_End: sendKey(VTERM_KEY_END, mod); return;
    case Qt::Key_PageUp: sendKey(VTERM_KEY_PAGEUP, mod); return;
    case Qt::Key_PageDown: sendKey(VTERM_KEY_PAGEDOWN, mod); return;
    default: break;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        sendKey(VTERM_KEY_FUNCTION(key - Qt::Key_F1 + 1), mod);
        return;
    }
    if (control) {
        // The control codes, written as terminals always have (not libvterm's CSI u form).
        char c = 0;
        if (key >= Qt::Key_A && key <= Qt::Key_Z) c = char(key - Qt::Key_A + 1);
        else if (key == Qt::Key_Space || key == Qt::Key_At || key == Qt::Key_2) c = 0;
        else if (key == Qt::Key_BracketLeft) c = 27;
        else if (key == Qt::Key_Backslash) c = 28;
        else if (key == Qt::Key_BracketRight) c = 29;
        else if (key == Qt::Key_AsciiCircum || key == Qt::Key_6) c = 30;
        else if (key == Qt::Key_Underscore || key == Qt::Key_Minus) c = 31;
        else if (key == Qt::Key_Question) c = 127;
        else return;
        writePty(QByteArray(1, c));
        return;
    }
    // Plain and Option characters: what the keyboard layout typed.
    const QString text = e->text();
    if (!text.isEmpty())
        writePty(text.toUtf8());
}

void TerminalView::inputMethodEvent(QInputMethodEvent *e)
{
    if (!e->commitString().isEmpty()) {
        scrollTo(0);
        writePty(e->commitString().toUtf8());
    }
    if (m_preedit != e->preeditString()) {
        m_preedit = e->preeditString();
        update(cellRect(m_cursorRow + m_scroll, m_cursorCol, std::max<int>(2, int(m_preedit.size()) + 1)));
    }
    e->accept();
}

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery q) const
{
    if (q == Qt::ImCursorRectangle || q == Qt::ImAnchorRectangle)
        return cellRect(m_cursorRow + m_scroll, m_cursorCol);
    if (q == Qt::ImFont)
        return m_font;
    if (q == Qt::ImEnabled)
        return true;
    if (q == Qt::ImHints)
        return int(Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    return QWidget::inputMethodQuery(q);
}

void TerminalView::paste(const QString &text)
{
    if (text.isEmpty())
        return;
    scrollTo(0);
    QString t = text;
    t.replace(QLatin1String("\r\n"), QLatin1String("\r"));
    t.replace('\n', '\r');
    // Bracketed paste when the shell asked for it (zsh does): a pasted command isn't run on its own.
    vterm_keyboard_start_paste(m_vt);
    writePty(t.toUtf8());
    vterm_keyboard_end_paste(m_vt);
}

void TerminalView::copySelection()
{
    if (!hasSelection())
        return;
    QString out;
    const VTermState *state = vterm_obtain_state(m_vt);
    for (int id = m_selStart.line; id <= m_selEnd.line; ++id) {
        const int line = id - m_dropped;
        const int from = id == m_selStart.line ? m_selStart.col : 0;
        const int to = id == m_selEnd.line ? m_selEnd.col : m_cols;
        if (id > m_selStart.line) {
            // a line the shell wrapped joins the previous one; scrollback lines don't keep that
            const int screenRow = line - int(m_scrollback.size());
            const VTermLineInfo *info = screenRow > 0 && screenRow < m_rows ? vterm_state_get_lineinfo(state, screenRow) : nullptr;
            if (!(info && info->continuation))
                out += '\n';
        }
        if (to > from)
            out += lineText(line, from, to);
    }
    QApplication::clipboard()->setText(out);
}

void TerminalView::clearScreen()
{
    // Cmd+K, as in Terminal.app and VS Code: history gone, the shell redraws its prompt (Ctrl+L).
    m_dropped += int(m_scrollback.size());
    m_scrollback.clear();
    m_scroll = 0;
    m_selStart = m_selEnd = {};
    writePty("\x0c");
    update();
}

// ---------------------------------------------------------------- mouse

TerminalView::Point TerminalView::pointAt(const QPointF &pos) const
{
    const int row = std::clamp(int(std::floor((pos.y() - m_pad.top()) / m_ch)), 0, m_rows - 1);
    const int col = std::clamp(int(std::lround((pos.x() - m_pad.left()) / m_cw)), 0, m_cols);
    return {firstVisibleLine() + row + m_dropped, col};
}

bool TerminalView::selected(int line, int col) const
{
    if (!hasSelection())
        return false;
    const Point p {line + m_dropped, col};
    return !(p < m_selStart) && p < m_selEnd;
}

void TerminalView::setSelection(Point a, Point b)
{
    if (b < a) std::swap(a, b);
    if (a == m_selStart && b == m_selEnd)
        return;
    m_selStart = a;
    m_selEnd = b;
    update();
}

void TerminalView::selectWord(Point p)
{
    const int line = p.line - m_dropped;
    auto isWord = [&](int col) {
        VTermScreenCell c;
        if (!cellAt(line, col, &c) || !c.chars[0]) return false;
        const QString t = cellText(c);
        return !t.isEmpty() && (t[0].isLetterOrNumber() || QStringLiteral("-_./~:@%+=#").contains(t[0]));
    };
    int a = std::min(p.col, m_cols - 1), b = a;
    if (!isWord(a)) {
        setSelection({p.line, a}, {p.line, a + 1});
        return;
    }
    while (a > 0 && isWord(a - 1)) --a;
    while (b + 1 < m_cols && isWord(b + 1)) ++b;
    setSelection({p.line, a}, {p.line, b + 1});
}

bool TerminalView::mouseReporting(const QMouseEvent *e) const
{
    // Shift+drag still selects, as in other terminals.
    return m_mouseMode != VTERM_PROP_MOUSE_NONE && !(e->modifiers() & Qt::ShiftModifier) && m_scroll == 0;
}

void TerminalView::reportMouse(QMouseEvent *e, bool press)
{
    const int row = std::clamp(int((e->position().y() - m_pad.top()) / m_ch), 0, m_rows - 1);
    const int col = std::clamp(int((e->position().x() - m_pad.left()) / m_cw), 0, m_cols - 1);
    int mod = VTERM_MOD_NONE;
#ifdef Q_OS_MACOS
    if (e->modifiers() & Qt::MetaModifier) mod |= VTERM_MOD_CTRL;
#else
    if (e->modifiers() & Qt::ControlModifier) mod |= VTERM_MOD_CTRL;
#endif
    if (e->modifiers() & Qt::AltModifier) mod |= VTERM_MOD_ALT;
    vterm_mouse_move(m_vt, row, col, VTermModifier(mod));
    if (e->type() == QEvent::MouseMove)
        return;
    const int button = e->button() == Qt::LeftButton ? 1 : e->button() == Qt::MiddleButton ? 2 : e->button() == Qt::RightButton ? 3 : 0;
    if (button) vterm_mouse_button(m_vt, button, press, VTermModifier(mod));
}

void TerminalView::mousePressEvent(QMouseEvent *e)
{
    setFocus(Qt::MouseFocusReason);
    const QRect thumb = thumbRect();
    if (e->button() == Qt::LeftButton && !thumb.isEmpty() && e->position().x() >= width() - ThumbWidth - 2) {
        m_thumbDrag = true;
        m_thumbGrab = thumb.contains(e->position().toPoint()) ? int(e->position().y()) - thumb.top() : thumb.height() / 2;
        mouseMoveEvent(e);
        return;
    }
    if (mouseReporting(e)) {
        reportMouse(e, true);
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    const Point p = pointAt(e->position());
    // triple click: the whole line
    if (m_clickAt.isValid() && m_clickAt.elapsed() < QApplication::doubleClickInterval() && p.line == m_clickPoint.line) {
        m_clickAt.invalidate();
        setSelection({p.line, 0}, {p.line + 1, 0});
        return;
    }
    m_anchor = p;
    m_selecting = true;
    if (e->modifiers() & Qt::ShiftModifier && hasSelection()) {
        setSelection(m_selStart, p);
        return;
    }
    setSelection(p, p);
}

void TerminalView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_thumbDrag) {
        const int total = lineCount();
        const QRect t = thumbRect();
        const int h = height() - 4 - t.height();
        const double frac = h > 0 ? std::clamp((e->position().y() - m_thumbGrab - 2) / h, 0.0, 1.0) : 1.0;
        const int first = qRound(frac * (total - m_rows));
        scrollTo(int(m_scrollback.size()) - first);
        return;
    }
    if (mouseReporting(e)) {
        if (m_mouseMode == VTERM_PROP_MOUSE_MOVE || (m_mouseMode == VTERM_PROP_MOUSE_DRAG && e->buttons()))
            reportMouse(e, false);
        return;
    }
    const bool overThumb = !m_scrollback.empty() && e->position().x() >= width() - ThumbWidth - 2;
    setCursor(overThumb ? Qt::ArrowCursor : Qt::IBeamCursor);
    if (!m_selecting || !(e->buttons() & Qt::LeftButton))
        return;
    // dragging past the top or bottom scrolls the history
    if (e->position().y() < 0) scrollTo(m_scroll + 1);
    else if (e->position().y() > height()) scrollTo(m_scroll - 1);
    setSelection(m_anchor, pointAt(e->position()));
}

void TerminalView::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_thumbDrag) {
        m_thumbDrag = false;
        m_fadeTimer.start(1200, this);
        return;
    }
    if (mouseReporting(e)) {
        reportMouse(e, false);
        return;
    }
    m_selecting = false;
}

void TerminalView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (mouseReporting(e)) {
        reportMouse(e, true);
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    m_clickPoint = pointAt(e->position());
    m_clickAt.start();
    selectWord(m_clickPoint);
}

void TerminalView::wheelEvent(QWheelEvent *e)
{
    // Lines: whole notches of a mouse wheel (3 lines), or the trackpad's pixels.
    double lines = !e->pixelDelta().isNull() ? e->pixelDelta().y() / m_ch : e->angleDelta().y() / 120.0 * 3;
    lines += m_wheelRest;
    const int n = int(lines);
    m_wheelRest = lines - n;
    if (n == 0) {
        e->accept();
        return;
    }
    if (m_mouseMode != VTERM_PROP_MOUSE_NONE && !(e->modifiers() & Qt::ShiftModifier) && m_scroll == 0) {
        const int row = std::clamp(int((e->position().y() - m_pad.top()) / m_ch), 0, m_rows - 1);
        const int col = std::clamp(int((e->position().x() - m_pad.left()) / m_cw), 0, m_cols - 1);
        vterm_mouse_move(m_vt, row, col, VTERM_MOD_NONE);
        for (int i = 0; i < std::abs(n); ++i) vterm_mouse_button(m_vt, n > 0 ? 4 : 5, true, VTERM_MOD_NONE);
    } else if (m_altScreen) {
        // full-screen programs without mouse support (less, man): the arrows, like VS Code
        for (int i = 0; i < std::abs(n); ++i) sendKey(n > 0 ? VTERM_KEY_UP : VTERM_KEY_DOWN, VTERM_MOD_NONE);
    } else {
        scrollTo(m_scroll + n);
    }
    e->accept();
}

void TerminalView::contextMenuEvent(QContextMenuEvent *e)
{
    if (m_mouseMode != VTERM_PROP_MOUSE_NONE && m_scroll == 0)
        return;
    QMenu menu(this);
    QAction *copy = menu.addAction("Copy", QKeySequence::Copy, this, &TerminalView::copySelection);
    copy->setEnabled(hasSelection());
    menu.addAction("Paste", QKeySequence::Paste, this, [this] { paste(QApplication::clipboard()->text()); });
    menu.addAction("Select All", QKeySequence::SelectAll, this,
                   [this] { setSelection({m_dropped, 0}, {m_dropped + lineCount() - 1, m_cols}); });
    menu.addSeparator();
    menu.addAction("Clear", QKeySequence(Qt::CTRL | Qt::Key_K), this, &TerminalView::clearScreen);
    menu.exec(e->globalPos());
}

void TerminalView::focusInEvent(QFocusEvent *e)
{
    update(cellRect(m_cursorRow + m_scroll, m_cursorCol, 2).adjusted(-2, -2, 2, 2));
    QWidget::focusInEvent(e);
}

void TerminalView::focusOutEvent(QFocusEvent *e)
{
    update(cellRect(m_cursorRow + m_scroll, m_cursorCol, 2).adjusted(-2, -2, 2, 2));
    QWidget::focusOutEvent(e);
}

void TerminalView::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    if (!m_scrollback.empty()) update(QRect(width() - ThumbWidth - 2, 0, ThumbWidth + 2, height()));
    QWidget::enterEvent(e);
}

void TerminalView::leaveEvent(QEvent *e)
{
    m_hover = false;
    update(QRect(width() - ThumbWidth - 2, 0, ThumbWidth + 2, height()));
    QWidget::leaveEvent(e);
}

// ================================================================ TerminalPanel

namespace {

// A session's button in the strip: its number and foreground program, underlined when shown.
class SessionChip : public QAbstractButton
{
public:
    SessionChip(QWidget *parent = nullptr) : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(24);
    }
    bool current = false;
    QSize sizeHint() const override
    {
        return QSize(QFontMetrics(theme::font(12)).horizontalAdvance(text()) + 20, 24);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (current || underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(theme::color(current ? "--panel-2" : "--hover"));
            p.drawRoundedRect(QRectF(rect()).adjusted(0, 1, 0, -1), 4, 4);
        }
        p.setFont(theme::font(12, current ? 600 : 400));
        p.setPen(theme::color(current ? "--text" : "--muted"));
        p.drawText(rect(), Qt::AlignCenter, text());
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
};

QPushButton *iconButton(const QString &svg, const QString &tip)
{
    auto *b = new QPushButton;
    b->setIcon(theme::svgIcon(svg, theme::color("--muted"), 16));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(26, 24);
    b->setFlat(true);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setProperty("svg", svg);
    theme::setCls(b, "icon-btn");
    return b;
}

const char *PlusSvg = "<path d='M12 5v14M5 12h14'/>";
const char *TrashSvg = "<path d='M4 7h16M10 11v6M14 11v6M5 7l1 12a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2l1-12M9 7V4h6v3'/>";

} // namespace

TerminalPanel::TerminalPanel(QWidget *parent) : QWidget(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    m_strip = new QWidget;
    m_strip->setFixedHeight(30);
    auto *h = new QHBoxLayout(m_strip);
    h->setContentsMargins(8, 3, 8, 3);
    h->setSpacing(2);
    m_tabsLayout = new QHBoxLayout;
    m_tabsLayout->setSpacing(2);
    h->addLayout(m_tabsLayout);
    h->addStretch(1);
    auto *add = iconButton(PlusSvg, "New terminal");
    connect(add, &QPushButton::clicked, this, &TerminalPanel::newSession);
    auto *kill = iconButton(TrashSvg, "Kill terminal");
    connect(kill, &QPushButton::clicked, this, [this] {
        if (m_current) closeSession(m_current);
    });
    h->addWidget(add);
    h->addWidget(kill);
    v->addWidget(m_strip);

    m_stack = new QStackedWidget;
    m_empty = new QWidget;
    auto *ev = new QVBoxLayout(m_empty);
    ev->addStretch(1);
    auto *msg = new QLabel("No terminal open.");
    msg->setAlignment(Qt::AlignCenter);
    theme::setCls(msg, "muted");
    ev->addWidget(msg);
    auto *open = new QPushButton("New terminal");
    theme::setCls(open, "btn small");
    open->setCursor(Qt::PointingHandCursor);
    connect(open, &QPushButton::clicked, this, &TerminalPanel::newSession);
    ev->addWidget(open, 0, Qt::AlignHCenter);
    ev->addStretch(1);
    m_stack->addWidget(m_empty);
    v->addWidget(m_stack, 1);
}

void TerminalPanel::showEvent(QShowEvent *e)
{
    // The first shell starts the first time the tab is opened, not with the app.
    if (m_nextId == 1)
        newSession();
    QWidget::showEvent(e);
}

void TerminalPanel::newSession()
{
    auto *view = new TerminalView;
    m_stack->addWidget(view);
    view->setProperty("sessionId", m_nextId++);
    // Lay it out at the stack's size first, so the shell starts with the right number of columns.
    view->resize(m_stack->size());
    if (!view->start(QDir::homePath())) {
        const QString why = view->errorString();
        delete view;
        auto *msg = m_empty->findChild<QLabel *>();
        if (msg) msg->setText("The terminal couldn't start: " + why);
        setCurrent(nullptr);
        return;
    }
    m_sessions << view;
    connect(view, &TerminalView::processNameChanged, this, &TerminalPanel::rebuildStrip);
    connect(view, &TerminalView::finished, this, [this, view] { closeSession(view); });
    setCurrent(view);
}

void TerminalPanel::closeSession(TerminalView *view)
{
    const int i = int(m_sessions.indexOf(view));
    if (i < 0)
        return;
    m_sessions.removeAt(i);
    m_stack->removeWidget(view);
    view->deleteLater();
    if (m_current == view)
        setCurrent(m_sessions.isEmpty() ? nullptr : m_sessions[std::min<int>(i, int(m_sessions.size()) - 1)].data());
    else
        rebuildStrip();
}

void TerminalPanel::setCurrent(TerminalView *view)
{
    m_current = view;
    m_stack->setCurrentWidget(view ? static_cast<QWidget *>(view) : m_empty);
    rebuildStrip();
    if (view && isVisible())
        view->setFocus(Qt::OtherFocusReason);
}

void TerminalPanel::focusCurrent()
{
    if (m_current)
        m_current->setFocus(Qt::ShortcutFocusReason);
}

void TerminalPanel::rebuildStrip()
{
    while (QLayoutItem *item = m_tabsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const QPointer<TerminalView> &s : std::as_const(m_sessions)) {
        if (!s) continue;
        auto *chip = new SessionChip;
        chip->setText(QString("%1: %2").arg(s->property("sessionId").toInt()).arg(s->processName()));
        chip->current = s == m_current;
        TerminalView *view = s;
        connect(chip, &QAbstractButton::clicked, this, [this, view] { setCurrent(view); });
        m_tabsLayout->addWidget(chip);
    }
}

void TerminalPanel::refreshTheme()
{
    for (QPushButton *b : m_strip->findChildren<QPushButton *>())
        b->setIcon(theme::svgIcon(b->property("svg").toString(), theme::color("--muted"), 16));
    for (const QPointer<TerminalView> &s : std::as_const(m_sessions))
        if (s) s->refreshTheme();
    update();
}

void TerminalPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), theme::color("--bg"));
}

} // namespace qtm
