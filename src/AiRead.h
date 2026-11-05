#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class QAbstractButton;
class QWidget;

// What the read_page tool of the read_data skill reads (LocalTools): a page of the app as data. An AI reads a table or a
// chart only through that tool, never in a prompt.
//
// aiReadPage walks the page's visible widgets in reading order. A table widget gives its whole model. Any other widget is
// painted on a device that records the text it draws, by position: a table or a card painted by hand reads as it shows, with no
// code of its own. Buttons, inputs and text editors are left out, and so is the terminal, which can hold anything.
// A chart's curve isn't text: a chart implements AiReadable to give its values.
class AiReadable
{
public:
    virtual ~AiReadable() = default;
    virtual QString aiRead() const = 0;   // empty: read what it draws instead
};

QString aiReadPage(QWidget *page);
// A page's sections: its visible sub-tabs, whose click switches them.
QVector<QAbstractButton *> aiSectionButtons(QWidget *page);
QString aiSectionName(const QAbstractButton *button);
QAbstractButton *aiFindSection(const QVector<QAbstractButton *> &buttons, const QString &name);

// What read_page and open_page ask for: a page by id (empty: the one on screen), a section of it, or a country's sheet
// (`country`, its `tab`) or an indicator's in Economy.
struct AiPageRequest {
    QString page, section, country, tab, indicator;
};
struct AiPageRead {
    QString page, section, text, error;
    QStringList sections;     // those of the page
    bool onScreen = true;
    bool stillLoading = false;
};

// "name (120 points, 2016-01 to 2026-09, low 1.2, high 9.1): 2026-07=3.1, 2026-08=3.2, …": the span, then the latest points.
QString aiSeries(const QString &name, const QStringList &labels, const QVector<double> &values, int latest = 24);
QString aiNumber(double value);
