#pragma once

#include <QList>
#include <QSize>
#include <QString>

class MainWindow;

// Walks every page and section at each window size and writes to `dir`: report.txt (window size reached, what a page asks
// for at least, widgets past the window's edge, horizontal scrollbars, text cut short) and a capture of each view.
void runLayoutAudit(MainWindow *window, const QString &dir, const QList<QSize> &sizes);
