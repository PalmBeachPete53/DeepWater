#include "LayoutAudit.h"

#include "AiRead.h"
#include "MainWindow.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QMap>
#include <QPixmap>
#include <QScrollArea>
#include <QTableWidget>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <functional>

namespace {

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

// Inside a scroll area's viewport, a widget past the edge is reached by scrolling.
bool inScrollArea(const QWidget *w)
{
    for (const QWidget *p = w->parentWidget(); p; p = p->parentWidget())
        if (p->objectName() == "qt_scrollarea_viewport" && p->parentWidget() && p->parentWidget()->objectName() != "pagesScroll")
            return true;
    return false;
}

QString describe(const QWidget *w)
{
    QString text;
    if (auto *l = qobject_cast<const QLabel *>(w)) text = l->text();
    else if (auto *b = qobject_cast<const QAbstractButton *>(w)) text = b->text();
    text = text.left(40).replace('\n', ' ');
    return QString("%1#%2 \"%3\"").arg(QString::fromUtf8(w->metaObject()->className()), w->objectName(), text);
}

QSize smartMin(const QWidget *w)
{
    QSize s = w->minimumSizeHint().expandedTo(w->minimumSize());
    if (w->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored && w->minimumWidth() == 0) s.setWidth(0);
    if (w->sizePolicy().verticalPolicy() == QSizePolicy::Ignored && w->minimumHeight() == 0) s.setHeight(0);
    return s;
}

// What holds a page's minimum up: down the largest child, then the children of the widget where none explains it.
void chain(QTextStream &out, const char *what, QWidget *page, bool width)
{
    const auto size = [width](const QWidget *w) { return width ? smartMin(w).width() : smartMin(w).height(); };
    out << "  " << what << ":";
    QWidget *at = page;
    for (int depth = 0; depth < 30; ++depth) {
        QList<QWidget *> kids;
        for (QWidget *k : at->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
            if (k->isVisibleTo(at) && !k->isWindow()) kids << k;
        std::sort(kids.begin(), kids.end(), [&](QWidget *a, QWidget *b) { return size(a) > size(b); });
        out << ' ' << at->metaObject()->className() << '#' << at->objectName() << '=' << size(at) << " >";
        if (kids.isEmpty()) break;
        if (size(kids.first()) < size(at) * 0.8) {
            out << " [";
            for (int i = 0; i < std::min<int>(kids.size(), 12); ++i) out << ' ' << describe(kids[i]) << '=' << size(kids[i]);
            out << " ]";
            break;
        }
        at = kids.first();
    }
    out << '\n';
}

void audit(QTextStream &out, const QString &view, QMainWindow *window, QWidget *page)
{
    QSize pageMin = page->minimumSizeHint().expandedTo(page->minimumSize());
    if (page->layout()) pageMin = pageMin.expandedTo(page->layout()->totalMinimumSize());
    out << "VIEW " << view << " window=" << window->width() << 'x' << window->height() << " pageMin=" << pageMin.width() << 'x'
        << pageMin.height() << '\n';
    if (auto *pagesScroll = window->findChild<QAbstractScrollArea *>("pagesScroll"))
        out << "  PAGE-SCROLL h=" << pagesScroll->horizontalScrollBar()->maximum() << " v=" << pagesScroll->verticalScrollBar()->maximum() << '\n';
    if (pageMin.width() > 940) chain(out, "WIDE", page, true);
    if (pageMin.height() > 480) chain(out, "TALL", page, false);
    // A scroll area whose content is wider than it, with no bar to reach the rest: cut.
    for (QScrollArea *area : page->findChildren<QScrollArea *>()) {
        if (!area->isVisible() || !area->widget()) continue;
        const int excess = area->widget()->width() - area->viewport()->width();
        if (excess > 1 && area->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff) {
            out << "  CUT " << describe(area) << " by " << excess << '\n';
            chain(out, "  HOLDS", area->widget(), true);
        }
    }
    // Cells whose text doesn't fit their column, in what is on screen.
    for (QTableWidget *table : page->findChildren<QTableWidget *>()) {
        if (!table->isVisible() || table->columnCount() == 0) continue;
        QMap<int, int> cut;
        int cells = 0;
        const int rows = std::min(table->rowCount(), 40);
        for (int r = 0; r < rows; ++r) {
            if (table->isRowHidden(r)) continue;
            for (int c = 0; c < table->columnCount(); ++c) {
                const QTableWidgetItem *item = table->item(r, c);
                if (!item || item->text().isEmpty() || table->isColumnHidden(c) || table->wordWrap() && item->text().contains('\n')) continue;
                ++cells;
                const QFont font = item->font();
                if (QFontMetrics(font).horizontalAdvance(item->text()) + 8 > table->columnWidth(c)) ++cut[c];
            }
        }
        if (cut.isEmpty()) continue;
        out << "  ELIDED " << describe(table) << ' ' << table->viewport()->width() << "px, " << cells << " cells:";
        for (auto it = cut.begin(); it != cut.end(); ++it) {
            const QTableWidgetItem *head = table->horizontalHeaderItem(it.key());
            out << " [" << (head ? head->text() : QString::number(it.key())) << ' ' << it.value() << " at " << table->columnWidth(it.key()) << "px]";
        }
        out << '\n';
    }
    const QRect windowRect(QPoint(0, 0), window->size());
    for (QWidget *w : window->findChildren<QWidget *>()) {
        if (!w->isVisible() || w->isWindow() || w->width() <= 0 || w->height() <= 0) continue;
        const QRect at(w->mapTo(window, QPoint(0, 0)), w->size());
        if (!windowRect.contains(at) && !inScrollArea(w))
            out << "  CLIPPED " << describe(w) << " at " << at.x() << ',' << at.y() << ' ' << at.width() << 'x' << at.height() << '\n';
        if (auto *bar = qobject_cast<QScrollBar *>(w); bar && bar->orientation() == Qt::Horizontal && bar->maximum() > 0) {
            auto *area = qobject_cast<QAbstractScrollArea *>(bar->parentWidget() ? bar->parentWidget()->parentWidget() : nullptr);
            out << "  HSCROLL " << (area ? QString::fromUtf8(area->metaObject()->className()) : QString("?")) << " by " << bar->maximum()
                << " viewport " << bar->pageStep() << (area && !inScrollArea(area) ? " outer" : "") << '\n';
        }
        QString text;
        bool wrap = false;
        if (auto *l = qobject_cast<QLabel *>(w)) {
            text = l->text();
            wrap = l->wordWrap() || Qt::mightBeRichText(text);
        } else if (auto *b = qobject_cast<QAbstractButton *>(w)) {
            text = b->text();
        }
        text.remove('&');
        if (text.isEmpty() || wrap) continue;
        const int need = w->fontMetrics().horizontalAdvance(text);
        if (need > w->contentsRect().width() + 1)
            out << "  TRUNCATED " << describe(w) << " needs " << need << " has " << w->contentsRect().width() << '\n';
    }
}

} // namespace

void runLayoutAudit(MainWindow *window, const QString &dir, const QList<QSize> &sizes)
{
    QDir().mkpath(dir);
    QFile file(dir + "/report.txt");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    const QStringList pages = {"map", "overview", "economy", "forex", "charts", "calendar", "news", "research", "quant", "desk", "journal", "ai", "sources"};
    for (const QSize &size : sizes) {
        window->resize(size);
        wait(200);
        const QString tag = QString("%1x%2").arg(size.width()).arg(size.height());
        for (const QString &id : pages) {
            window->showPage(id);
            wait(1200);
            QWidget *page = window->currentPage();
            QSet<QAbstractButton *> seen;
            int shot = 0;
            auto record = [&](const QString &section) {
                const QString view = tag + " " + id + (section.isEmpty() ? QString() : " › " + section);
                audit(out, view, window, page);
                out.flush();
                window->grab().save(QString("%1/%2_%3_%4.png").arg(dir, tag, id).arg(shot++, 2, 10, QChar('0')));
            };
            record({});
            std::function<void(int)> visit = [&](int depth) {
                const QVector<QAbstractButton *> buttons = aiSectionButtons(page);
                for (QAbstractButton *b : buttons) {
                    if (seen.contains(b) || !b->isVisibleTo(page)) continue;
                    seen.insert(b);
                    const QString name = aiSectionName(b);
                    b->click();
                    wait(500);
                    record(name);
                    if (depth < 2) visit(depth + 1);
                }
            };
            visit(0);
            // The panels under the chart open at their own height.
            if (id == "charts") {
                for (QAbstractButton *b : page->findChildren<QAbstractButton *>()) {
                    if (!b->isVisible() || !QString(b->metaObject()->className()).contains("TabButton")) continue;
                    const QString name = b->text();
                    b->click();
                    wait(500);
                    record(name);
                }
            }
        }
    }
    file.close();
    QApplication::quit();
}
