#pragma once

#include <QWidget>

// The flap on the right edge of the page area that slides a side panel open (DevToolsDock, AiDock): a rounded tab
// with a chevron and a vertical label ("DEV", "AI").
class SideFlap : public QWidget
{
    Q_OBJECT
public:
    static constexpr int Width = 22;
    static constexpr int Height = 84;
    static constexpr int Top = 24;   // from the top of the page area, for the highest flap

    explicit SideFlap(const QString &label, QWidget *parent = nullptr);
    void setOpen(bool open);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }

private:
    QString m_label;
    bool m_open = false;
};

// The panel's close button: a cross.
class QToolButton;
QToolButton *sidePanelCloseButton(QWidget *parent = nullptr);
