#pragma once

#include <QWidget>

class CodeGraphView;
class QFrame;
class QStackedWidget;
class QVariantAnimation;

// Developer tools: a flap on the right edge of the page area (shown by the header's </> icon)
// that slides a tabbed side panel open. One tab for now: Codebase, the 3D code graph.
class DevToolsDock : public QWidget
{
    Q_OBJECT
public:
    // host is the widget whose right edge the dock hangs on; topOffset is the header's height.
    explicit DevToolsDock(QWidget *host, int topOffset);

    void setOpen(bool open, bool animate = true);
    bool isOpen() const { return m_open; }

signals:
    void openChanged(bool open);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void relayout();

    QWidget *m_host;
    int m_topOffset;
    QWidget *m_flap;
    QFrame *m_panel;
    QStackedWidget *m_stack;
    CodeGraphView *m_codebase;
    QVariantAnimation *m_anim;
    double m_shown = 0;   // 0 closed .. 1 open
    bool m_open = false;
};
