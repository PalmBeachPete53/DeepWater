#include "UpdateDialog.h"

#include "Updater.h"

#include <QDesktopServices>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QString buildLine(const QString &commit, qint64 time)
{
    if (commit.isEmpty())
        return UpdateDialog::tr("unknown build");
    const QDateTime when = QDateTime::fromSecsSinceEpoch(time);
    return QString("%1 · %2").arg(update::shortCommit(commit), QLocale().toString(when, QLocale::ShortFormat));
}

} // namespace

UpdateDialog::UpdateDialog(Updater *updater, QWidget *parent)
    : QDialog(parent), m_updater(updater)
{
    setObjectName("settingsDialog");
    setWindowTitle(tr("Updates"));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("Updates"));
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_installed = new QLabel;
    m_installed->setObjectName("dim");
    m_installed->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_installed->setWordWrap(true);
    layout->addWidget(m_installed);

    m_changesTitle = new QLabel(tr("What's new"));
    m_changesTitle->setObjectName("dim");
    layout->addSpacing(4);
    layout->addWidget(m_changesTitle);

    m_changes = new QLabel;
    m_changes->setWordWrap(true);
    m_changes->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_changes->setTextFormat(Qt::PlainText);
    auto *scroll = new QScrollArea;
    scroll->setWidget(m_changes);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(140);
    layout->addWidget(scroll, 1);

    m_progress = new QProgressBar;
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    layout->addWidget(m_progress);

    m_auto = new QCheckBox(tr("Listen for new versions"));
    m_auto->setObjectName("calCheck");
    m_auto->setChecked(m_updater->autoCheck());
    connect(m_auto, &QCheckBox::toggled, m_updater, &Updater::setAutoCheck);
    layout->addWidget(m_auto);

    auto *buttons = new QHBoxLayout;
    m_check = new QPushButton(tr("Check now"));
    m_check->setObjectName("siteButton");
    // Mouse only: this window takes the focus while the user may be typing elsewhere, and a key must never start an
    // update. Enter would go to a default button, and space to whichever has the focus.
    m_check->setAutoDefault(false);
    m_check->setFocusPolicy(Qt::NoFocus);
    m_check->setCursor(Qt::PointingHandCursor);
    connect(m_check, &QPushButton::clicked, m_updater, &Updater::check);
    buttons->addWidget(m_check);
    buttons->addStretch();
    m_action = new QPushButton;
    m_action->setObjectName("primaryButton");
    m_action->setAutoDefault(false);
    m_action->setFocusPolicy(Qt::NoFocus);
    m_action->setCursor(Qt::PointingHandCursor);
    connect(m_action, &QPushButton::clicked, this, &UpdateDialog::onAction);
    buttons->addWidget(m_action);
    layout->addLayout(buttons);

    connect(m_updater, &Updater::changed, this, &UpdateDialog::refresh);
    connect(m_updater, &Updater::progress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(int(received * 1000 / total));
        }
        m_status->setText(tr("Downloading: %1 of %2 MB.").arg(received / 1048576).arg(total / 1048576));
    });
    refresh();
}

void UpdateDialog::onAction()
{
    switch (m_updater->state()) {
    case Updater::State::Ready:
        m_updater->install();
        break;
    default:
        if (Updater::installsInPlace())
            m_updater->download();
        else
            QDesktopServices::openUrl(m_updater->release().browserUrl);
    }
}

void UpdateDialog::refresh()
{
    const Updater::State state = m_updater->state();
    const update::Release release = m_updater->release();
    const QString target = m_updater->target();
    const bool elsewhere = !target.isEmpty() && QFileInfo(target).absolutePath() == "/Applications"
                           && !QCoreApplication::applicationDirPath().startsWith("/Applications/");

    QString status;
    QString action;
    switch (state) {
    case Updater::State::Idle:
        status = tr("Not checked yet.");
        break;
    case Updater::State::Checking:
        status = tr("Checking GitHub…");
        break;
    case Updater::State::UpToDate:
        status = tr("Deepwater is up to date.");
        break;
    case Updater::State::Available:
        if (!Updater::installsInPlace()) {
            status = tr("A new build is ready. Download the installer and run it: your data and settings aren't touched.");
            action = tr("Download the installer");
            break;
        }
        status = elsewhere ? tr("A new build is ready. This copy runs from a build folder, so it will be installed in "
                                "Applications and started from there.")
                           : tr("A new build is ready. Your data and settings aren't touched.");
        action = tr("Update");
        break;
    case Updater::State::Downloading:
        status = tr("Downloading…");
        break;
    case Updater::State::Preparing:
        status = tr("Checking the new version…");
        break;
    case Updater::State::Ready:
        status = tr("The new version is ready. Deepwater will close and start again.");
        action = tr("Restart and install");
        break;
    case Updater::State::Failed:
        status = tr("The update failed: %1").arg(m_updater->error());
        if (release.isValid() && update::isNewer(Updater::currentCommit(), Updater::currentTime(), release))
            action = tr("Try again");
        break;
    }
    m_status->setText(status);

    QString installed = tr("Installed: %1").arg(buildLine(Updater::currentCommit(), Updater::currentTime()));
    if (release.isValid() && release.commit != Updater::currentCommit())
        installed += "\n" + tr("Latest: %1").arg(buildLine(release.commit, release.commitTime));
    m_installed->setText(installed);

    const QStringList changes = m_updater->changes();
    const bool showChanges = m_updater->hasUpdate() && !changes.isEmpty();
    m_changesTitle->setVisible(showChanges);
    m_changes->parentWidget()->parentWidget()->setVisible(showChanges);
    QStringList lines;
    for (const QString &subject : changes)
        lines << QString::fromUtf8("• ") + subject;
    m_changes->setText(lines.join('\n'));

    const bool working = state == Updater::State::Downloading || state == Updater::State::Preparing;
    m_progress->setVisible(working);
    if (state == Updater::State::Preparing)
        m_progress->setRange(0, 0);
    m_action->setVisible(!action.isEmpty());
    m_action->setText(action);
    m_check->setEnabled(!working && state != Updater::State::Checking && state != Updater::State::Ready);
    adjustSize();
}
