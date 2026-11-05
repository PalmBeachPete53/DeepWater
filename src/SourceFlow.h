#pragma once

#include "fusion/Store.h"

#include <QDateTime>
#include <QHash>
#include <QPainterPath>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;

namespace sourceflow {

// One data set of the Sources list, as the diagram sees it.
struct Row {
    enum State { Neutral, Good, Bad, Off };
    QString id;
    QString name;       // translated, as the list shows it
    QString provider;   // the list's provider line: the source of a row the diagram has no entry for
    QString section;    // the list's section ("Map · supply chains"): its part of the app, for the same rows
    QString host;       // the hosts it reads ("api.bcb.gov.br · apisidra.ibge.gov.br"): which catalogue bodies are read
    State state = Neutral;
};

// The diagram's model: the bodies and companies data comes from, the parts of the app it feeds, and a link for
// each (source, part) pair with the data sets that make it. Built from the rows by a table in SourceFlow.cpp (which
// body is behind a row, which parts it feeds); a row the table doesn't know is drawn from its provider and section.
struct Graph {
    struct Source {
        QString id, name, group;
        int groupOrder = 0;
        QVector<int> rows;
        Row::State state = Row::Neutral;
    };
    struct Part {
        QString id, tab, name;   // name empty: the tab has a single part, shown by the tab's name
        int order = 0;
        QVector<int> rows;
        int sources = 0;
    };
    struct Link {
        int source = 0, part = 0;
        QVector<int> rows;
        Row::State state = Row::Neutral;
        bool staged = false;        // drawn through the fusion block (the stage)
    };
    // Data fusion between some sources and the parts their golden records feed (Economy › Countries and
    // Indicators): the links of the sources that gave points to a saved record pass through it. From the
    // records' index (fusion/Store.h).
    struct Stage {
        bool present = false;
        QVector<int> parts, sources;
        int records = 0, countries = 0, indicators = 0, points = 0, added = 0, spliced = 0, unverified = 0, conflicts = 0;
        QDateTime latest;
    };
    // The catalogue of official sources (Sources › List, below the app's own) less the bodies the app reads, by
    // kind: listed, not read. A body is read when its site's domain is one a row reads from.
    struct Listed {
        int kind = 0;               // OfficialSources::Kind
        QString name;               // plural: "Statistical offices"
        QVector<int> sources;       // catalogue indexes
        int free = 0, headline = 0, paid = 0, reachable = 0;
    };
    // The disc view's underground floor. An AI of the app, as readAis() finds it: a model in a slot, opencode, Claude,
    // the training of AI › Machine learning.
    struct Ai {
        enum Kind { Local, Agent, Cloud, Training };
        QString id, name;
        QString model;          // a slot's full model name
        Kind kind = Local;
        QStringList skills;     // ids of modelslots::skills()
        bool on = true;         // false: granted but not in force (slot suspended, serving off)
        QVector<int> accesses;  // what it reaches
    };
    // What an AI can reach of Deepwater's data: a tool or a few of LocalTools, which read these rows, this part of the
    // app or the golden records.
    struct Access {
        QString id, name, what;
        QString skill;          // the name of the skill that grants it; empty: none needed
        QStringList tools;
        QVector<int> rows, sources, parts;
        bool stage = false;     // reads the fusion block
        Row::State state = Row::Neutral;
    };
    struct AiLink {
        int ai = 0, access = 0;
        bool on = true;
    };
    QVector<Ai> ais;
    QVector<Access> accesses;
    QVector<AiLink> aiLinks;
    QVector<Row> rows;
    QVector<Source> sources;
    QVector<Listed> listed;
    int listedRead = 0;             // catalogue bodies the app reads (left out of listed)
    QVector<Part> parts;
    QVector<Link> links;
    Stage stage;
};
Graph build(const QVector<Row> &rows, const QVector<fusion::Summary> &fusion = {}, const QVector<Graph::Ai> &ais = {});
// The AIs of the app as they stand: the slots' models, opencode, Claude, machine learning.
QVector<Graph::Ai> readAis();

class Canvas;

} // namespace sourceflow

// Sources › Data: where Deepwater's data comes from and which parts of the app it feeds, as a diagram. The bodies
// and companies on the left, grouped (international bodies, US agencies, national offices, markets…), the pages
// and sections of the app on the right, a link for each data set between them, as thick as the data sets it
// carries. The dots and links take the states of the list (up to date, failing, off). Hovering a source, a part or
// a link brings its data out and lists the data sets beside the diagram; a click pins it; "Show in the list" opens
// those data sets in the Sources list. A Fusion block sits between the sources whose series data fusion combined and
// the parts that show its golden records. "Zoom out" shows the whole diagram at once, laid out on a flat disc seen in
// isometric view (sources on its left arc, parts on its right one, rows turned outward; still hoverable and
// clickable); "Zoom in" goes back to it as it was scrolled. Under that disc, a smaller one, the underground floor: the
// AIs on its right, what they can reach of the data on its left, a link for each, and shafts up to the sources, the
// fusion block and the Charts part that data comes from. Built from LocalTools' tools, the slots and the MCP server.
// The up and down arrows move the camera between the floors: the floor in focus has the view to itself at its full size,
// its rim lights up; the other shows past its edge, faded, and leaves the mouse alone; a small rectangle of the floor's
// colour at the bottom of the view says where you are. A click on an item looks at it closely: it rises, the camera comes close,
// what it comes from on its left and what it goes to on its right (a click on one goes there, across the floors); the left and
// right arrows go to the previous and the next one; Escape or a click beside goes back to the disc.
class SourceFlowView : public QWidget
{
    Q_OBJECT
public:
    explicit SourceFlowView(QWidget *parent = nullptr);
    // The list's rows with their states (every second while the page is shown): rebuilds the diagram when the rows
    // change, else only takes the new states.
    void setRows(const QVector<sourceflow::Row> &rows);
    // Dev flag --sources-disc: the disc view, on a floor ("floor:1"), with an item pinned ("ai:0", "access:2") or looked at
    // closely ("open:source:3") when given.
    void showDisc(const QString &pin);

signals:
    // "Show in the list": the data sets of the pinned source, part or link.
    void showRows(const QString &title, const QStringList &ids);
    // "Show in the list" on a group of the catalogue: its section of the list (an OfficialSources::Kind).
    void showCatalogue(int kind);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateDetail();

    sourceflow::Canvas *m_canvas;
    QScrollArea *m_scroll;
    QLabel *m_summary;
    QLabel *m_detail;
    QPushButton *m_show;
    QPushButton *m_zoom;     // the overviews: the whole diagram on a disc,
    QPushButton *m_layers;   // or on plates standing one behind the other
    QStringList m_rowIds;   // the rows of the model, to tell a change of rows from a change of states
    QStringList m_aiIds;    // likewise for the AIs: name, skills, in force or not
    // The fusion index, read again when its file changes; true when it did.
    bool reloadFusion();
    QVector<fusion::Summary> m_fusion;
    QDateTime m_fusionStamp;
    bool m_fusionRead = false;
};
