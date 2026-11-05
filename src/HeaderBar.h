#pragma once

#include <QHash>
#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QToolButton;

// Top application bar: brand on the left, navigation tabs, and the clock/
// settings on the right. Navigation items are registered with addNavItem(); clicking one
// emits navigationRequested() with its id. The pencil after the tabs toggles an edit mode
// where tabs are dragged horizontally to reorder them (navOrderChanged() on each drop),
// hidden with their × button, and shown again from the + button's list (navHiddenChanged()).
class HeaderBar : public QWidget
{
    Q_OBJECT
public:
    explicit HeaderBar(QWidget *parent = nullptr);

    void addNavItem(const QString &id, const QString &label);
    void setCurrent(const QString &id);
    // Reorders the tabs: known ids first in the given order, the others after, unchanged.
    void setNavOrder(const QStringList &ids);
    // Hides these tabs and shows the others; at least one tab always stays visible.
    void setNavHidden(const QStringList &ids);
    // Checks the </> icon (devToolsToggled() is not emitted).
    void setDevToolsChecked(bool on);
    // The update icon in the accent colour with a dot: a new build can be installed.
    void setUpdateAvailable(bool available);

signals:
    void navigationRequested(const QString &id);
    void navOrderChanged(const QStringList &ids);
    void navHiddenChanged(const QStringList &ids);
    void settingsRequested();
    void updateRequested();        // the update icon
    void helpRequested();          // the ? icon: the documentation window
    void devToolsToggled(bool on);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateClock();
    void setEditing(bool editing);
    void moveNavItem(int from, int to);
    void endDrag();
    void setNavItemHidden(const QString &id, bool hidden);
    QStringList hiddenNavItems() const;
    void updateEditControls();
    void showHiddenMenu();

    QHBoxLayout *m_navLayout;
    QButtonGroup *m_navGroup;
    QHash<QString, QToolButton *> m_navButtons;
    QStringList m_navOrder;              // ids in display order, hidden ones included
    QHash<QString, QToolButton *> m_removeButtons;   // the × shown on each tab in edit mode
    QToolButton *m_editButton;
    QToolButton *m_addButton;            // edit mode: lists the hidden tabs
    bool m_editing = false;
    QToolButton *m_dragButton = nullptr; // tab being dragged in edit mode
    int m_pressX = 0;
    bool m_dragMoved = false;
    QLabel *m_clock;
    QToolButton *m_devButton;
    QToolButton *m_updateButton;
};
