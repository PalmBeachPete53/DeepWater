#pragma once

#include <QDateTime>
#include <QFutureWatcher>
#include <QVector3D>
#include <QWidget>

#include <memory>

class QCheckBox;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;
class QToolButton;

// The project's code graph as Graphify writes it (graphify-out/graph.json and graph.html) and a 3D
// version of graph.html for the developer panel.
namespace codegraph {

struct Node {
    QString id;
    QString label;
    QString file;       // repo-relative source file
    QString location;   // "L85"
    QString type;       // code, document, concept...
    int community = -1;
};

struct Edge {
    int a = 0, b = 0;   // node indices, source -> target
};

// A community as graph.html draws it: one dot, sized by its members, Tableau colour by id.
struct Community {
    int cid = -1;
    QString label;
    QColor color;
    double size = 10;   // dot radius in graph.html's canvas pixels
    int degree = 0;
    int members = 0;
};

struct CommunityLink {
    int a = 0, b = 0;   // indices into communities
    int count = 1;      // cross-community edges aggregated
};

struct Graph {
    QVector<Node> nodes;
    QVector<Edge> edges;
    QStringList communityNames;          // by community id
    QVector<Community> communities;      // graph.html's nodes (rebuilt from the JSON when it is missing)
    QVector<CommunityLink> communityLinks;
    QString path;
    QDateTime modified;
    QString error;
};

// graphify-out/graph.json, looked for upward from the app bundle and the working directory
// (DEEPWATER_GRAPH_ROOT or the devtools/graphRoot setting win). Empty if none.
QString locate();
// Reads graph.json and, next to it, graph.html's RAW_NODES / RAW_EDGES.
Graph load(const QString &path);

} // namespace codegraph

// graph.html's network in 3D: the same dots, colours and dashed links, laid out by vis.js's
// ForceAtlas2 rules (gravity, springs, overlap avoidance) run in three dimensions and flattened into
// a disk. Drag to orbit, wheel to zoom, hover a dot for details, click to pin it, double-click to open
// it. The scene is cached and only re-rendered when the view changes.
class GraphCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit GraphCanvas(QWidget *parent = nullptr);

    struct Node {
        QString label;
        QString title;      // tooltip second line
        QColor color;
        double size = 10;   // radius, layout pixels
        int ref = -1;       // community id or node index
        int group = -1;     // legend entry (community id, or file index inside a community)
        // graph.html's "Node info" fields.
        QString type;
        QString community;
        QString file;
        int members = -1;
        bool hidden = false;
        QVector3D pos, vel;
        QPointF screen;
        double depth = 0;   // 0 (far) .. 1 (near)
        double scale = 1;   // perspective factor
    };
    struct Edge {
        int a = 0, b = 0;   // from -> to
        double width = 1;
        double opacity = 0.35;
        bool dashed = false;
        QString title;
    };

    void setNetwork(const QVector<Node> &nodes, const QVector<Edge> &edges);
    void setMessage(const QString &text);
    // Legend checkboxes: nodes of these groups are not drawn (nor their links).
    void setHiddenGroups(const QSet<int> &groups);
    void setSelected(int index);
    int selected() const { return m_selected; }
    const QVector<Node> &nodes() const { return m_nodes; }
    const QVector<int> &neighbours(int index) const { return m_adjacent[index]; }
    int lastRenderMs() const { return m_renderMs; }

signals:
    void activated(int ref);          // double-click on a node
    void selectionChanged(int index); // click (-1: background)
    void rendered();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    void tick();
    void seedLayout();
    bool stepLayout();   // false once stable
    void project();
    void renderScene();
    void drawNode(QPainter &p, int i, bool highlighted, double alpha);
    void drawLabel(QPainter &p, int i, double alpha, QVector<QRectF> *taken);
    int hitTest(const QPointF &p) const;
    void invalidate() { m_sceneDirty = true; update(); }

    QVector<Node> m_nodes;
    QVector<Edge> m_edges;
    QVector<QVector<int>> m_adjacent;
    QVector<int> m_degree;
    QPixmap m_cache;
    bool m_sceneDirty = true;
    int m_renderMs = 0;
    QString m_message;
    QTimer *m_timer;
    double m_yaw = 0.3, m_pitch = 0.6, m_zoom = 1.0;
    int m_steps = 0;
    bool m_settled = true;
    bool m_dragging = false;
    bool m_moved = false;
    QPointF m_pressPos, m_lastPos, m_cursor;
    int m_hover = -1;
    int m_selected = -1;
};

class CodeGraphView : public QWidget
{
    Q_OBJECT
public:
    explicit CodeGraphView(QWidget *parent = nullptr);

    void setGraphPath(const QString &path);
    void reload();

private:
    struct LegendEntry {
        int group = -1;
        QString label;
        QColor color;
        int count = 0;
    };
    void applyGraph();
    void showCommunities();
    void showCommunity(int cid);
    void setLegend(const QVector<LegendEntry> &entries);
    void legendChanged();
    void showInfo(int index);
    void runSearch(const QString &text);
    void updateStatus(const QString &extra = {});

    QToolButton *m_back;
    QLabel *m_title;
    QToolButton *m_reload;
    GraphCanvas *m_canvas;
    QLabel *m_status;
    // Sidebar, as graph.html's.
    QLineEdit *m_search;
    QListWidget *m_results;
    QLabel *m_info;
    QCheckBox *m_selectAll;
    QListWidget *m_legend;
    QLabel *m_stats;
    bool m_legendUpdating = false;
    QFileSystemWatcher *m_watcher;
    QTimer *m_reloadDelay;
    QFutureWatcher<codegraph::Graph> *m_loader = nullptr;
    QString m_path;
    std::shared_ptr<codegraph::Graph> m_graph;
    const codegraph::Graph *m_summaryOf = nullptr;   // the graph m_summary describes
    QStringList m_summary;
    int m_community = -1;   // -1: the communities, else the members of this one
    bool m_loading = false;
};
