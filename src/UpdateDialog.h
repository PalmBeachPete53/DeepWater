#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class Updater;

// What the header's update icon opens: the version installed, the build found with what's new in it, and the button
// that downloads it and then restarts the app on it. Follows the Updater's state and asks it nothing of its own.
class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(Updater *updater, QWidget *parent = nullptr);

private:
    void refresh();
    void onAction();

    Updater *m_updater;
    QLabel *m_status;
    QLabel *m_installed;
    QLabel *m_changesTitle;
    QLabel *m_changes;
    QProgressBar *m_progress;
    QCheckBox *m_auto;
    QPushButton *m_check;
    QPushButton *m_action;
};
