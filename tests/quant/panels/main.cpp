// Drives the panels of the Quant Terminal for screenshots. See CMakeLists.txt.

#include "chart/Chart.h"
#include "server/Server.h"
#include "ui/App.h"
#include "ui/ChartPane.h"
#include "ui/Panels.h"
#include "ui/PanelsKit.h"
#include "ui/Theme.h"
#include "src/Theme.h"   // deepwater's palettes, for --palette

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QApplication>
#include <QFontMetricsF>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QPointer>
#include <QTextEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <cstdio>
#include <memory>

using namespace qtm;

namespace {

// Deepwater's theme `id` on the terminal, as MainWindow::applyTheme sets it; "" = the built-in one.
void usePalette(const QString &id)
{
    if (id.isEmpty()) {
        qtm::theme::clearPalette();
        return;
    }
    ::Theme::setCurrent(id);
    const Palette &p = ::Theme::current();
    if (p.id != id) std::fprintf(stderr, "no palette %s\n", qPrintable(id));
    qtm::theme::setPalette(::Theme::chartVars(p), ::Theme::isLight(p));
}

struct Step {
    QString cmd;
    int after = 1500;
};

QImage renderWindow(QWidget *win)
{
    for (auto *c : win->findChildren<lw::Chart *>()) c->setProperty("lwDevicePixelRatio", 1.0);
    QImage img(win->size(), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(1);
    img.fill(Qt::black);
    win->render(&img);
    for (auto *c : win->findChildren<lw::Chart *>()) c->setProperty("lwDevicePixelRatio", QVariant());
    return img;
}

bool visible(QWidget *w, QWidget *win)
{
    return w->isVisibleTo(win) && w->isVisible();
}

void mouse(QWidget *w, QEvent::Type type, QPointF pos)
{
    QMouseEvent e(type, pos, w->mapToGlobal(pos), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                  type == QEvent::MouseButtonPress ? Qt::MouseButtons(Qt::LeftButton) : Qt::MouseButtons(Qt::NoButton),
                  Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}

} // namespace

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QElapsedTimer sinceStart;
    sinceStart.start();
    QApplication qapp(argc, argv);
    QApplication::setOrganizationName("Deepwater");
    QApplication::setApplicationName("QtmPanelsDemo");

    QString dataDir, customDir, shot, palette;
    int w = 1600, h = 980, wait = 6000;
    QVector<Step> steps;
    const QStringList args = qapp.arguments();
    for (int i = 1; i + 1 < args.size(); i += 2) {
        const QString k = args[i], v = args[i + 1];
        if (k == "--data") dataDir = v;
        else if (k == "--custom") customDir = v;
        else if (k == "--shot") shot = v;
        else if (k == "--w") w = v.toInt();
        else if (k == "--h") h = v.toInt();
        else if (k == "--wait") wait = v.toInt();
        else if (k == "--palette") palette = v;
        else if (k == "--do") steps.push_back({v});
        else if (k == "--after" && !steps.isEmpty()) steps.last().after = v.toInt();
    }
    QTemporaryDir tmp;
    if (dataDir.isEmpty()) dataDir = tmp.path() + "/data";
    if (customDir.isEmpty()) customDir = QFileInfo(dataDir).absolutePath() + "/custom";
    QDir().mkpath(dataDir);
    QDir().mkpath(customDir);
    const QString seed = customDir + "/ema-ribbon-squeeze.js";
    if (!QFile::exists(seed)) QFile::copy(QStringLiteral(QTM_QT_ROOT) + "/custom/ema-ribbon-squeeze.js", seed);

    Backend backend(dataDir, customDir);
    backend.loadCustomIndicators();
    backend.startAlerts();

    usePalette(palette);
    QMainWindow win;
    win.setWindowTitle("Quant Terminal");
    auto *app = new App;
    win.setCentralWidget(app);
    win.resize(w, h);
    win.show();

    QPointer<QLineEdit> lastEdit;
    auto run = std::make_shared<std::function<void(int)>>();
    *run = [&, run](int i) {
        if (i >= steps.size()) {
            if (!shot.isEmpty()) {
                renderWindow(&win).save(shot);
                std::printf("saved %s\n", qPrintable(shot));
                qapp.quit();
            }
            return;
        }
        const QStringList a = steps[i].cmd.split(' ', Qt::SkipEmptyParts);
        const QString c = a.value(0);
        const QString rest = QStringList(a.mid(1)).join(' ');
        Panels *panels = app->panels;
        if (c == "side") panels->showSide(a.value(1));
        else if (c == "bottom") panels->showBottom(a.value(1));
        else if (c == "collapse") panels->showBottom(a.value(1).isEmpty() ? "tester" : a.value(1), true);
        else if (c == "alert") panels->openAlertDialog();
        else if (c == "theme") app->toggleTheme();
        else if (c == "palette") {
            usePalette(rest == "none" ? QString() : rest);
            app->refreshTheme();
        }
        else if (c == "symbol") app->setSymbol(rest);
        else if (c == "tf") app->setTf(rest);
        else if (c == "button") {
            // any button: push buttons, the PTQ pills, message box buttons
            QAbstractButton *hit = nullptr;
            for (auto *b : win.findChildren<QAbstractButton *>())
                if (b->text() == rest && visible(b, &win)) { hit = b; break; }
            // clicked from the event loop, so a confirm() it opens doesn't block the next steps
            if (hit) QTimer::singleShot(0, hit, [b = QPointer<QAbstractButton>(hit)] { if (b) b->click(); });
            else std::fprintf(stderr, "no button: %s\n", qPrintable(rest));
        } else if (c == "scroll") {
            QVector<QScrollArea *> areas;
            QWidget *root = panels->bottomWidget();
            if (a.value(3) == "side") root = panels->sideWidget();
            for (auto *s : root->findChildren<QScrollArea *>())
                if (visible(s, &win)) areas << s;
            std::sort(areas.begin(), areas.end(), [](QScrollArea *x, QScrollArea *y) {
                return x->mapToGlobal(QPoint()).x() < y->mapToGlobal(QPoint()).x();
            });
            if (auto *s = areas.value(a.value(1).toInt())) s->verticalScrollBar()->setValue(a.value(2).toInt());
        } else if (c == "type") {
            if (auto *e = qobject_cast<QLineEdit *>(QApplication::focusWidget())) {
                e->setText(rest);
                emit e->textEdited(rest);
            }
        } else if (c == "focus") {
            // focus N: the N-th visible line edit of the window (top to bottom, left to right)
            QVector<QLineEdit *> edits;
            for (auto *e : win.findChildren<QLineEdit *>())
                if (visible(e, &win)) edits << e;
            std::sort(edits.begin(), edits.end(), [](QLineEdit *x, QLineEdit *y) {
                const QPoint px = x->mapToGlobal(QPoint()), py = y->mapToGlobal(QPoint());
                return px.y() != py.y() ? px.y() < py.y() : px.x() < py.x();
            });
            if (auto *e = edits.value(a.value(1).toInt())) e->setFocus();
        } else if (c == "bedit" || c == "sedit") {
            // bedit N TEXT: the N-th visible line edit of the bottom (side) panel gets TEXT
            QWidget *root = c == "bedit" ? panels->bottomWidget() : panels->sideWidget();
            QVector<QLineEdit *> edits;
            for (auto *e : root->findChildren<QLineEdit *>())
                if (visible(e, &win)) edits << e;
            std::sort(edits.begin(), edits.end(), [](QLineEdit *x, QLineEdit *y) {
                const QPoint px = x->mapToGlobal(QPoint()), py = y->mapToGlobal(QPoint());
                return px.y() != py.y() ? px.y() < py.y() : px.x() < py.x();
            });
            if (auto *e = edits.value(a.value(1).toInt())) {
                const QString t = QStringList(a.mid(2)).join(' ');
                e->setFocus();
                lastEdit = e;
                e->setText(t);
                emit e->textEdited(t);
            } else std::fprintf(stderr, "no edit %d\n", a.value(1).toInt());
        } else if (c == "code") {
            // code OLD => NEW: edits the visible code area (indicator editor / JSON modal)
            for (auto *t : win.findChildren<QTextEdit *>())
                if (visible(t, &win)) {
                    const QStringList parts = rest.split(" => ");
                    QString code = t->toPlainText();
                    code.replace(parts.value(0), parts.value(1));
                    t->setPlainText(code);
                    break;
                }
        } else if (c == "enter") {
            QLineEdit *e = lastEdit ? lastEdit.data() : qobject_cast<QLineEdit *>(QApplication::focusWidget());
            if (e) emit e->returnPressed();
        } else if (c == "hover" || c == "click") {
            const QPoint pt(a.value(1).toInt(), a.value(2).toInt());
            if (QWidget *t = win.childAt(pt)) {
                const QPointF lp = t->mapFrom(&win, QPointF(pt));
                QEnterEvent ee(lp, lp, t->mapToGlobal(lp));
                QApplication::sendEvent(t, &ee);
                mouse(t, QEvent::MouseMove, lp);
                if (c == "click") {
                    // from the event loop, so a confirm() the click opens doesn't block the next steps
                    QTimer::singleShot(0, t, [t, lp] {
                        mouse(t, QEvent::MouseButtonPress, lp);
                        mouse(t, QEvent::MouseButtonRelease, lp);
                    });
                }
            }
        } else if (c == "measure") {
            QFont f = QApplication::font();
            f.setPixelSize(a.value(1).toInt());
            std::printf("measure %s: %.2f\n", qPrintable(QStringList(a.mid(2)).join(' ')),
                        QFontMetricsF(f).horizontalAdvance(QStringList(a.mid(2)).join(' ')));
        } else if (c == "measurev") {
            // measurev TEXT: width of TEXT in a .stat .v font (16px semibold, tabular-nums)
            QFont f = QApplication::font();
            f.setPixelSize(16);
            f.setWeight(QFont::DemiBold);
            f.setFeature(QFont::Tag("tnum"), 1);
            std::printf("measurev %s: %.2f (family %s)\n", qPrintable(rest), QFontMetricsF(f).horizontalAdvance(rest), qPrintable(QFontInfo(f).family()));
        } else if (c == "replay") {
            // replay start N | step D | play | exit | speed X | sync on|off | pane N (makes chart N active)
            ChartPane *p = app->active;
            const QString sub = a.value(1);
            if (sub == "pane") {
                if (auto *pp = app->panes.value(a.value(2).toInt())) app->setActive(pp);
            } else if (!p) std::fprintf(stderr, "no active chart\n");
            else if (sub == "start") p->startReplay(a.value(2).toInt());
            else if (sub == "step") {
                const int n = a.value(2).toInt();
                for (int k = 0; k < std::abs(n); ++k) p->replayStep(n > 0 ? 1 : -1);
            } else if (sub == "play") p->toggleReplay();
            else if (sub == "exit") p->stopReplay();
            else if (sub == "speed" || sub == "sync") {
                for (auto *w : p->findChildren<QWidget *>())
                    if (sub == "speed" && w->objectName() == "qtmReplaySpeed") {
                        auto *s = static_cast<QComboBox *>(w);
                        const int idx = s->findText(a.value(2) + "x");
                        if (idx >= 0) s->setCurrentIndex(idx);
                    } else if (sub == "sync" && w->objectName() == "qtmReplaySync")
                        static_cast<QCheckBox *>(w)->setChecked(a.value(2) == "on");
            }
        } else if (c == "layout") {
            app->setLayout(rest);
        } else if (c == "bselect" || c == "bpick") {
            // bselect N VALUE: the N-th visible select (combo box) of the bottom panel gets VALUE
            // (bpick: as the user picking it, so the select's change handler runs)
            QVector<QComboBox *> boxes;
            for (auto *s : panels->bottomWidget()->findChildren<QComboBox *>())
                if (visible(s, &win)) boxes << s;
            std::sort(boxes.begin(), boxes.end(), [](QComboBox *x, QComboBox *y) {
                const QPoint px = x->mapToGlobal(QPoint()), py = y->mapToGlobal(QPoint());
                return px.y() != py.y() ? px.y() < py.y() : px.x() < py.x();
            });
            if (auto *s = boxes.value(a.value(1).toInt())) {
                const int idx = s->findData(a.value(2));
                s->setCurrentIndex(idx >= 0 ? idx : s->findText(a.value(2)));
                if (c == "bpick") emit s->activated(s->currentIndex());
            } else std::fprintf(stderr, "no select %d\n", a.value(1).toInt());
        } else if (c == "tablebtn") {
            // tablebtn TEXT DATA: presses the table-cell button TEXT (e.g. Close, Cancel) carrying DATA
            bool done = false;
            for (auto *t : win.findChildren<kit::Table *>()) {
                if (!visible(t, &win) || done) continue;
                const auto &rows = t->rows();
                for (int r = 0; r < rows.size() && !done; ++r)
                    for (int k = 0; k < rows[r].cells.size() && !done; ++k)
                        if (rows[r].cells[k].button == a.value(1) && (a.value(2).isEmpty() || rows[r].cells[k].data == a.value(2))) {
                            emit t->buttonClicked(r, k, rows[r].cells[k].data);
                            done = true;
                        }
            }
            if (!done) std::fprintf(stderr, "no table button %s %s\n", qPrintable(a.value(1)), qPrintable(a.value(2)));
        } else if (c == "pick") {
            // pick bottom|side|win N VALUE: the N-th visible select of that area gets VALUE as a user
            // choice (emits activated, so the panel reacts like on a click in the list)
            QWidget *root = a.value(1) == "side" ? panels->sideWidget() : a.value(1) == "bottom" ? panels->bottomWidget()
                                                                                                  : nullptr;
            QVector<QComboBox *> boxes;
            const auto all = root ? root->findChildren<QComboBox *>() : win.findChildren<QComboBox *>();
            for (auto *s : all)
                if (visible(s, &win) || (!root && s->isVisible())) boxes << s;
            if (!root)   // dialogs are other top-level windows
                for (auto *t : QApplication::topLevelWidgets())
                    if (t != &win && t->isVisible())
                        for (auto *s : t->findChildren<QComboBox *>())
                            if (s->isVisible() && !boxes.contains(s)) boxes << s;
            std::sort(boxes.begin(), boxes.end(), [](QComboBox *x, QComboBox *y) {
                const QPoint px = x->mapToGlobal(QPoint()), py = y->mapToGlobal(QPoint());
                return px.y() != py.y() ? px.y() < py.y() : px.x() < py.x();
            });
            if (auto *s = boxes.value(a.value(2).toInt())) {
                const QString v = QStringList(a.mid(3)).join(' ');
                int idx = s->findData(v);
                if (idx < 0) idx = s->findText(v);
                if (idx < 0) std::fprintf(stderr, "no option %s\n", qPrintable(v));
                else {
                    s->setCurrentIndex(idx);
                    emit s->activated(idx);
                }
            } else std::fprintf(stderr, "no select %s %d\n", qPrintable(a.value(1)), a.value(2).toInt());
        } else if (c == "checkbox") {
            // checkbox TEXT: clicks the visible check box with that text
            bool done = false;
            for (auto *b : win.findChildren<QCheckBox *>())
                if (!done && b->text() == rest && visible(b, &win)) {
                    b->click();
                    done = true;
                }
            if (!done) std::fprintf(stderr, "no checkbox %s\n", qPrintable(rest));
        } else if (c == "promptnext") {
            // promptnext TEXT: the next prompt() box gets TEXT and is accepted
            QTimer::singleShot(300, &qapp, [rest] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QInputDialog *>(w); d && d->isVisible()) {
                        d->setTextValue(rest);
                        d->accept();
                    }
            });
        } else if (c == "rejectnext") {
            // the next modal message box (confirm) is cancelled
            QTimer::singleShot(300, &qapp, [] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *m = qobject_cast<QMessageBox *>(w); m && m->isVisible()) m->reject();
            });
        } else if (c == "tooltip") {
            // tooltip X Y: asks the widget at X,Y (window coordinates) for its tooltip and prints it
            const QPoint pt(a.value(1).toInt(), a.value(2).toInt());
            if (QWidget *t = win.childAt(pt)) {
                QWidget *target = t;
                while (target && target->testAttribute(Qt::WA_TransparentForMouseEvents)) target = target->parentWidget();
                if (target) {
                    QHelpEvent he(QEvent::ToolTip, target->mapFrom(&win, pt), win.mapToGlobal(pt));
                    QApplication::sendEvent(target, &he);
                }
            }
            std::printf("TOOLTIP %s\n", qPrintable(QToolTip::isVisible() ? QToolTip::text() : QString()));
        } else if (c == "appwidth") {
            // appwidth W: forces the terminal (the App widget) to W px wide, below its layout minimum
            app->setFixedWidth(a.value(1).toInt());
            win.setMinimumSize(0, 0);
            win.resize(a.value(1).toInt(), win.height());
        } else if (c == "state") {
            // state KEY: prints app.state's watchlists / strategies / activeWatchlist
            QJsonObject wl;
            for (const auto &p : app->state.watchlists.items) wl[p.first] = QJsonArray::fromStringList(p.second);
            QStringList strat = app->state.strategies.keys();
            std::printf("STATE active=%s watchlists=%s strategies=%s\n", qPrintable(app->state.activeWatchlist),
                        QJsonDocument(wl).toJson(QJsonDocument::Compact).constData(), qPrintable(strat.join("|")));
        } else if (c == "acceptnext") {
            // the next modal message box (confirm) is accepted
            QTimer::singleShot(300, &qapp, [] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *m = qobject_cast<QMessageBox *>(w); m && m->isVisible()) {
                        if (auto *ok = m->button(QMessageBox::Ok)) ok->click();
                        else m->accept();
                    }
            });
        } else if (c == "paperdump") {
            // the live and replay paper accounts, and the active chart's replay index
            QJsonObject o {{"paper", app->state.paper}, {"replayPaper", app->state.replayPaper}};
            if (app->active && app->active->replay) o["replayIndex"] = app->active->replay->index;
            QJsonArray idx;
            for (auto *pp : app->panes) idx.append(pp->replay ? QJsonValue(pp->replay->index) : QJsonValue());
            o["paneReplay"] = idx;
            std::printf("PAPER %s\n", QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
        } else if (c == "toasts") {
            for (auto *w : win.findChildren<QFrame *>())
                if (w->property("toastHtml").isValid() && visible(w, &win))
                    std::printf("TOAST %s\n", qPrintable(w->property("toastHtml").toString()));
        } else if (c == "csvfile") {
            // csvfile PATH: the next file dialog (Import bars / signal) picks PATH instead of asking
            qapp.setProperty("qtmOpenFile", rest);
        } else if (c == "csvtext") {
            // the visible texts of the bottom panel (tester results), tags stripped
            static const QRegularExpression tags("<[^>]*>");
            for (auto *w : panels->bottomWidget()->findChildren<QWidget *>()) {
                if (!w->isVisibleTo(&win)) continue;
                if (auto *l = qobject_cast<QLabel *>(w)) {
                    if (!l->text().isEmpty())
                        std::printf("TEXT %s\n", qPrintable(QString(l->text()).replace(tags, " ").simplified()));
                } else if (auto *t = qobject_cast<kit::Text *>(w)) {
                    std::printf("TEXT %s\n", qPrintable(QString(t->html()).replace(tags, " ").simplified()));
                } else if (auto *tb = qobject_cast<kit::Table *>(w)) {
                    for (const auto &r : tb->rows()) {
                        QStringList cells;
                        for (const auto &c : r.cells) cells << QString(c.button.isEmpty() ? c.html : c.button).replace(tags, " ").simplified();
                        std::printf("ROW %s\n", qPrintable(cells.join(" | ")));
                    }
                }
            }
        } else if (c == "csvselect") {
            // csvselect VALUE: the first visible select of the bottom panel that has the option VALUE gets it
            bool done = false;
            for (auto *s : panels->bottomWidget()->findChildren<QComboBox *>())
                if (!done && visible(s, &win) && s->findData(rest) >= 0) {
                    s->setCurrentIndex(s->findData(rest));
                    done = true;
                }
            if (!done) std::fprintf(stderr, "no select with %s\n", qPrintable(rest));
        } else if (c == "csvind") {
            // csvind ID: adds indicator ID (e.g. custom:signal-my_signal) to the active chart
            if (app->active) app->active->addIndicator(rest);
        } else if (c == "csvchart") {
            // each chart's symbol, timeframe, bar count and first/last bar
            for (auto *pp : app->panes)
                std::printf("CHART %s %s bars=%d first=%.0f last=%.0f close=%.10g\n", qPrintable(pp->cfg.symbol),
                            qPrintable(pp->cfg.tf), int(pp->bars.size()), pp->bars.isEmpty() ? 0 : pp->bars.first().time,
                            pp->bars.isEmpty() ? 0 : pp->bars.last().time, pp->bars.isEmpty() ? 0 : pp->bars.last().close);
        } else if (c == "waitprice") {
            // waitprice SYMBOL: waits until the watchlist has a quote for SYMBOL, prints the ms since start
            const QString sym = a.value(1);
            auto poll = std::make_shared<std::function<void()>>();
            *poll = [&, poll, sym, i] {
                if (app->prices.contains(sym)) {
                    std::printf("price %s after %lld ms\n", qPrintable(sym), qint64(sinceStart.elapsed()));
                    QTimer::singleShot(steps[i].after, &qapp, [run, i] { (*run)(i + 1); });
                } else QTimer::singleShot(50, &qapp, *poll);
            };
            (*poll)();
            return;
        } else if (c == "shot") {
            renderWindow(&win).save(a.value(1));
            std::printf("saved %s\n", qPrintable(a.value(1)));
        } else if (c == "dump") {
            for (auto *b : win.findChildren<QPushButton *>())
                if (visible(b, &win)) {
                    const QPoint p = b->mapTo(&win, QPoint());
                    std::printf("button '%s' at %d,%d %dx%d\n", qPrintable(b->text()), p.x(), p.y(), b->width(), b->height());
                }
        } else {
            std::fprintf(stderr, "unknown command: %s\n", qPrintable(steps[i].cmd));
        }
        QTimer::singleShot(steps[i].after, &qapp, [run, i] { (*run)(i + 1); });
    };
    QObject::connect(app, &App::ready, &qapp, [&, run] { QTimer::singleShot(wait, &qapp, [run] { (*run)(0); }); });
    return qapp.exec();
}
