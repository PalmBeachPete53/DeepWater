#pragma once

// The Terminal tab of the bottom panel (a deepwater addition, not in quant-terminal): your login
// shell in a pseudo-terminal, like VS Code's integrated terminal. libvterm (third_party/) decodes
// what the shell writes (xterm escape codes, colours, alternate screen, mouse modes) and encodes
// the keys; TerminalView draws its screen with the Charts theme.
//
// Keys: what you type goes to the shell, so Option keeps typing the characters of your keyboard
// layout (@ [ { | ~ on French Macs), Control+letter sends the control code, and only these Command
// shortcuts are kept: Cmd+C copies the selection, Cmd+V pastes, Cmd+A selects all, Cmd+K clears.
// Option+Left / Right move by word, Cmd+Left / Right to the start / end of the line, like VS Code.

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QSocketNotifier>
#include <QVector>
#include <QWidget>

#include <deque>

#include <vterm.h>

class QHBoxLayout;
class QStackedWidget;

namespace qtm {

class WinPty;   // the pty on Windows (WinPty.h); forkpty() elsewhere

// One shell session: the pty, the libvterm screen with its scrollback, and the widget that shows it.
class TerminalView : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalView(QWidget *parent = nullptr);
    ~TerminalView() override;

    // Starts the user's login shell ($SHELL, else /bin/zsh; on Windows PowerShell 7, else Windows PowerShell, else
    // cmd.exe, on a ConPTY) in `dir`. False when the pty or the
    // process can't be created (errorString() says why).
    bool start(const QString &dir);
    bool isRunning() const { return m_pid > 0; }
    QString errorString() const { return m_error; }
    // The program in the foreground ("zsh", "vim", "python3"...), for the session's tab.
    QString processName() const { return m_process; }
    void refreshTheme();

signals:
    void processNameChanged();
    // The shell exited (or the pty closed).
    void finished();

protected:
    bool event(QEvent *e) override;
    bool focusNextPrevChild(bool) override { return false; }   // Tab goes to the shell
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *e) override;
    void inputMethodEvent(QInputMethodEvent *e) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void timerEvent(QTimerEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    // libvterm screen callbacks
    static int cbDamage(VTermRect rect, void *user);
    static int cbMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
    static int cbSetTermProp(VTermProp prop, VTermValue *val, void *user);
    static int cbBell(void *user);
    static int cbPushLine(int cols, const VTermScreenCell *cells, void *user);
    static int cbPopLine(int cols, VTermScreenCell *cells, void *user);
    static int cbClearScrollback(void *user);
    static void cbOutput(const char *bytes, size_t len, void *user);

    using Line = QVector<VTermScreenCell>;

    void readPty();
    void writePty(const QByteArray &bytes);
    void flushWrites();
    void closePty();
    void updateGridSize();
    void updateFont();
    void updateProcessName();
    void scrollTo(int offset);   // lines scrolled back, 0 = live
    void sendKey(int vtermKey, int mod);
    void paste(const QString &text);
    void copySelection();
    void clearScreen();

    // Absolute line index: scrollback lines first (0 = oldest kept), then the screen rows.
    int lineCount() const;
    int firstVisibleLine() const;
    bool cellAt(int line, int col, VTermScreenCell *cell) const;
    QString lineText(int line, int from, int to) const;
    struct Point {
        int line = 0, col = 0;
        bool operator<(const Point &o) const { return line != o.line ? line < o.line : col < o.col; }
        bool operator==(const Point &o) const { return line == o.line && col == o.col; }
    };
    Point pointAt(const QPointF &pos) const;
    bool selected(int line, int col) const;
    bool hasSelection() const { return m_selStart < m_selEnd; }
    void setSelection(Point a, Point b);
    void selectWord(Point p);
    bool mouseReporting(const QMouseEvent *e) const;
    void reportMouse(QMouseEvent *e, bool press);
    QRect cellRect(int row, int col, int width = 1) const;
    QRect thumbRect() const;

    VTerm *m_vt = nullptr;
    VTermScreen *m_screen = nullptr;
    int m_rows = 24, m_cols = 80;

    int m_fd = -1;                // the pty; on Windows 0 while the ConPTY is open
    qint64 m_pid = -1;
    WinPty *m_winpty = nullptr;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QByteArray m_pendingWrite;
    QString m_error;
    QString m_process;
    QElapsedTimer m_processAt;

    std::deque<Line> m_scrollback;
    int m_scroll = 0;             // lines scrolled back from the live screen
    int m_dropped = 0;            // scrollback lines dropped off the top (selection bookkeeping)

    QFont m_font, m_bold;
    qreal m_cw = 7, m_ch = 15;
    int m_ascent = 11;
    QMargins m_pad {8, 4, 8, 4};

    // The cursor doesn't blink (VS Code's default): no timer repainting it twice a second.
    int m_cursorRow = 0, m_cursorCol = 0;
    bool m_cursorVisible = true;
    int m_cursorShape = 1;        // VTERM_PROP_CURSORSHAPE_BLOCK
    bool m_altScreen = false;
    int m_mouseMode = 0;          // VTERM_PROP_MOUSE_*
    QString m_preedit;            // dead key / input method text being composed

    // Selection in absolute lines (+ m_dropped), [start, end).
    Point m_selStart, m_selEnd, m_anchor;
    bool m_selecting = false;
    bool m_thumbDrag = false;
    int m_thumbGrab = 0;
    bool m_hover = false;
    QBasicTimer m_fadeTimer;      // hides the scrollbar thumb a moment after scrolling
    bool m_thumbShown = false;
    qreal m_wheelRest = 0;
    QElapsedTimer m_clickAt;      // triple click = select the line
    Point m_clickPoint;
};

// The tab's content: a strip with one button per session, "+" and the kill button, over the
// sessions themselves. Sessions live as long as the Charts page (theme and language changes keep
// them), and a session whose shell exits closes.
class TerminalPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalPanel(QWidget *parent = nullptr);

    void newSession();
    void focusCurrent();
    void refreshTheme();
    int sessionCount() const { return int(m_sessions.size()); }

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;

private:
    void closeSession(TerminalView *view);
    void setCurrent(TerminalView *view);
    void rebuildStrip();

    QWidget *m_strip = nullptr;
    QHBoxLayout *m_tabsLayout = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_empty = nullptr;
    QVector<QPointer<TerminalView>> m_sessions;
    QPointer<TerminalView> m_current;
    int m_nextId = 1;
};

} // namespace qtm
