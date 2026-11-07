#pragma once

#include <QStringList>
#include <QVector>

class QWidget;

// The size of the whole interface, as a browser's zoom: Qt's QT_SCALE_FACTOR, which it reads only when the app starts, so a
// change restarts the app (on the page that was open). Kept in the settings as "ui/zoom" (1.0 = 100 %). README › Display size.
namespace uizoom {

const QVector<int> &levels();   // in percent, from 80 to 175
int current();                  // the saved one, in percent
// Before the QApplication: sets QT_SCALE_FACTOR from the saved zoom, unless the environment already has one.
void applyAtStartup();
// Whether the window (at least 960 x 540 at 100 %) still fits the main screen at that zoom.
bool fits(int percent);
// The next or previous level that fits, from the current one; the current one at the end of the range.
int step(int direction);
// Saves `percent` and, if the user agrees, restarts on page `page`. False when nothing changed or the user said no.
bool choose(QWidget *parent, int percent, const QString &page);
// Read by main() after the event loop: start the app again with these arguments.
bool restartRequested();
QStringList restartArguments();

} // namespace uizoom
