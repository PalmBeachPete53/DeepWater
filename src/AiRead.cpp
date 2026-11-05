#include "AiRead.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitterHandle>
#include <QTabBar>
#include <QTableView>
#include <QTextEdit>
#include <QTextItem>
#include <QWidget>

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

namespace {

struct Drawn {
    QPointF at;
    double width = 0;
    QString text;
};

// Records the text drawn, by position; the rest is dropped.
class TextEngine : public QPaintEngine
{
public:
    TextEngine() : QPaintEngine(AllFeatures) {}
    QVector<Drawn> drawn;

    bool begin(QPaintDevice *) override { return true; }
    bool end() override { return true; }
    Type type() const override { return User; }
    void updateState(const QPaintEngineState &state) override
    {
        if (state.state() & DirtyTransform) m_transform = state.transform();
    }
    void drawTextItem(const QPointF &p, const QTextItem &item) override
    {
        const QString text = item.text().trimmed();
        if (!text.isEmpty()) drawn.push_back({m_transform.map(p), item.width() * m_transform.m11(), text});
    }
    void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override {}
    void drawPolygon(const QPointF *, int, PolygonDrawMode) override {}
    using QPaintEngine::drawPolygon;

private:
    QTransform m_transform;
};

class TextDevice : public QPaintDevice
{
public:
    explicit TextDevice(const QWidget *like) : m_like(like) {}
    QPaintEngine *paintEngine() const override { return &m_engine; }
    QVector<Drawn> take() { return std::exchange(m_engine.drawn, {}); }

protected:
    int metric(PaintDeviceMetric m) const override
    {
        switch (m) {
        case PdmWidth: return m_like->width();
        case PdmHeight: return m_like->height();
        case PdmDepth: return 32;
        case PdmNumColors: return INT_MAX;
        case PdmDpiX: case PdmPhysicalDpiX: return m_like->logicalDpiX();
        case PdmDpiY: case PdmPhysicalDpiY: return m_like->logicalDpiY();
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled: return int(QPaintDevice::devicePixelRatioFScale());
        case PdmDevicePixelRatioF_EncodedA: case PdmDevicePixelRatioF_EncodedB: return 0;
        default: return 0;
        }
    }

private:
    const QWidget *m_like;
    mutable TextEngine m_engine;
};

// The text a widget draws itself, line by line, its cells apart.
QString drawnText(QWidget *w)
{
    TextDevice device(w);
    w->render(&device, QPoint(), QRegion(), QWidget::RenderFlags());
    QVector<Drawn> drawn = device.take();
    if (drawn.isEmpty()) return {};
    std::stable_sort(drawn.begin(), drawn.end(), [](const Drawn &a, const Drawn &b) { return a.at.y() < b.at.y(); });
    QStringList lines;
    for (int i = 0; i < drawn.size();) {
        int end = i;
        while (end < drawn.size() && drawn[end].at.y() - drawn[i].at.y() < 5) ++end;
        QVector<Drawn> line = drawn.mid(i, end - i);
        std::stable_sort(line.begin(), line.end(), [](const Drawn &a, const Drawn &b) { return a.at.x() < b.at.x(); });
        // Text is often drawn in runs (a number cut at its thin space): a cell ends where a gap opens.
        QString cells;
        double right = 0;
        for (const Drawn &d : std::as_const(line)) {
            if (!cells.isEmpty()) cells += d.at.x() - right > 12 ? " | " : " ";
            cells += d.text;
            right = d.at.x() + d.width;
        }
        lines << cells;
        i = end;
    }
    return lines.join('\n');
}

QString viewText(const QAbstractItemView *view)
{
    const QAbstractItemModel *model = view->model();
    if (!model) return {};
    const auto *table = qobject_cast<const QTableView *>(view);
    QStringList lines, head;
    QVector<int> columns;
    for (int c = 0; c < model->columnCount(); ++c) {
        if (table && table->isColumnHidden(c)) continue;
        columns << c;
        head << model->headerData(c, Qt::Horizontal).toString();
    }
    for (int r = 0; r < model->rowCount(); ++r) {
        if (table && table->isRowHidden(r)) continue;   // filtered out on the page
        QStringList cells;
        int filled = 0;
        for (int c : std::as_const(columns)) {
            const QString text = model->index(r, c).data(Qt::DisplayRole).toString().trimmed();
            cells << text;
            filled += !text.isEmpty() && text != QString::fromUtf8("…") && text != QString::fromUtf8("—");
        }
        if (filled >= std::min<qsizetype>(2, columns.size())) lines << cells.join(" | ");   // not a name without a value
    }
    if (lines.isEmpty()) return {};
    return head.join(" | ") + '\n' + lines.join('\n');
}

bool leftOut(const QWidget *w)
{
    if (qobject_cast<const QAbstractButton *>(w) || qobject_cast<const QLineEdit *>(w) || qobject_cast<const QComboBox *>(w)
        || qobject_cast<const QTextEdit *>(w) || qobject_cast<const QPlainTextEdit *>(w) || qobject_cast<const QScrollBar *>(w)
        || qobject_cast<const QAbstractSpinBox *>(w) || qobject_cast<const QTabBar *>(w) || qobject_cast<const QSplitterHandle *>(w))
        return true;
    // By name: no text of theirs to read (GL, web content), or anything at all in it (the terminal).
    const QMetaObject *meta = w->metaObject();
    for (const char *name : {"QOpenGLWidget", "QWebEngineView", "QQuickWidget", "QMacCocoaViewContainer", "QWindowContainer"})
        if (meta->inherits(&QWidget::staticMetaObject) && w->inherits(name)) return true;
    const QString className = QString::fromLatin1(meta->className());
    return className.contains("Terminal");
}

void walk(QWidget *w, QWidget *root, QStringList *blocks)
{
    if (w != root && !w->isVisibleTo(root)) return;
    if (leftOut(w)) return;
    if (const auto *view = qobject_cast<const QAbstractItemView *>(w)) {
        const QString text = viewText(view);
        if (!text.isEmpty()) blocks->push_back(text);
        return;
    }
    if (const auto *readable = dynamic_cast<const AiReadable *>(w)) {
        const QString text = readable->aiRead();
        if (!text.isEmpty()) {
            blocks->push_back(text);
            return;
        }
    }
    if (const QString text = drawnText(w); !text.isEmpty()) blocks->push_back(text);
    QVector<QWidget *> children;
    for (QObject *o : w->children())
        if (auto *child = qobject_cast<QWidget *>(o); child && !child->isWindow()) children << child;
    std::stable_sort(children.begin(), children.end(), [](const QWidget *a, const QWidget *b) {
        return a->y() != b->y() ? a->y() < b->y() : a->x() < b->x();
    });
    for (QWidget *child : std::as_const(children)) walk(child, root, blocks);
}

} // namespace

QString aiReadPage(QWidget *page)
{
    QStringList blocks;
    walk(page, page, &blocks);
    return blocks.join("\n\n");
}

QVector<QAbstractButton *> aiSectionButtons(QWidget *page)
{
    QVector<QAbstractButton *> out;
    for (QAbstractButton *b : page->findChildren<QAbstractButton *>()) {
        const QString name = b->objectName();
        if ((name == "qtSubTab" || name == "resSubTab" || name == "calSubTab") && b->isCheckable() && b->isVisibleTo(page)) out << b;
    }
    return out;
}

QString aiSectionName(const QAbstractButton *button)
{
    return QString(button->text()).replace("&&", "&").trimmed();
}

QAbstractButton *aiFindSection(const QVector<QAbstractButton *> &buttons, const QString &name)
{
    const QString wanted = name.trimmed();
    for (QAbstractButton *b : buttons)
        if (aiSectionName(b).compare(wanted, Qt::CaseInsensitive) == 0) return b;
    for (QAbstractButton *b : buttons)
        if (aiSectionName(b).contains(wanted, Qt::CaseInsensitive)) return b;
    return nullptr;
}

QString aiNumber(double value)
{
    return std::isfinite(value) ? QString::number(value, 'g', 6) : QStringLiteral("-");
}

QString aiSeries(const QString &name, const QStringList &labels, const QVector<double> &values, int latest)
{
    QVector<int> known;
    for (int i = 0; i < values.size(); ++i)
        if (std::isfinite(values[i])) known << i;
    if (known.isEmpty()) return {};
    double lo = values[known.first()], hi = lo;
    for (int i : std::as_const(known)) {
        lo = std::min(lo, values[i]);
        hi = std::max(hi, values[i]);
    }
    const auto label = [&](int i) { return i < labels.size() ? labels[i] : QString::number(i + 1); };
    QStringList points;
    for (qsizetype k = std::max<qsizetype>(0, known.size() - latest); k < known.size(); ++k)
        points << label(known[k]) + '=' + aiNumber(values[known[k]]);
    return QString("%1 (%2 points, %3 to %4, low %5, high %6): %7")
        .arg(name).arg(known.size()).arg(label(known.first()), label(known.last()), aiNumber(lo), aiNumber(hi), points.join(", "));
}
