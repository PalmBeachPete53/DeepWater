#include "UiZoom.h"

#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QWidget>

#include <cmath>
#include <cstdlib>

namespace uizoom {

namespace {

bool g_restart = false;
QStringList g_arguments;
QString g_program;

// After main() has returned and the window, its local server and its files are closed: the same app again.
void restartNow()
{
    QProcess::startDetached(g_program, g_arguments);
}
int g_applied = 100;   // the zoom this run started with (the screen's sizes are in its units)

int toPercent(double zoom)
{
    return int(std::lround(zoom * 100));
}

} // namespace

const QVector<int> &levels()
{
    static const QVector<int> v {80, 90, 100, 110, 125, 150, 175};
    return v;
}

int current()
{
    const double z = QSettings().value("ui/zoom", 1.0).toDouble();
    const int p = toPercent(z);
    return levels().contains(p) ? p : 100;
}

void applyAtStartup()
{
    g_applied = current();
    if (!qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR")) {
        g_applied = toPercent(qEnvironmentVariable("QT_SCALE_FACTOR").toDouble());
        return;
    }
    if (g_applied != 100)
        qputenv("QT_SCALE_FACTOR", QByteArray::number(g_applied / 100.0));
}

bool fits(int percent)
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return percent <= 100;
    // The screen's room in the units of 100 %, against the window's least size at that zoom.
    const QSize room = screen->availableGeometry().size() * (g_applied / 100.0);
    return 960 * percent / 100 <= room.width() && 540 * percent / 100 <= room.height();
}

int step(int direction)
{
    const QVector<int> &v = levels();
    int i = int(v.indexOf(current()));
    for (i += direction; i >= 0 && i < v.size(); i += direction)
        if (fits(v[i]))
            return v[i];
    return current();
}

bool choose(QWidget *parent, int percent, const QString &page)
{
    if (percent == current() || !levels().contains(percent))
        return false;
    QSettings().setValue("ui/zoom", percent / 100.0);
    const auto answer = QMessageBox::question(parent, QApplication::translate("UiZoom", "Interface size"),
        QApplication::translate("UiZoom", "Deepwater restarts to show the interface at %1 %. Restart now?").arg(percent),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes)
        return false;   // saved: the next launch uses it
    g_restart = true;
    g_arguments = page.isEmpty() ? QStringList() : QStringList {"--page", page};
    g_program = QCoreApplication::applicationFilePath();
    std::atexit(restartNow);
    QApplication::quit();
    return true;
}

bool restartRequested()
{
    return g_restart;
}

QStringList restartArguments()
{
    return g_arguments;
}

} // namespace uizoom
