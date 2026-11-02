// The Quant Terminal UI on its own, for trying it and for screenshots. See CMakeLists.txt.

#include "chart/Chart.h"
#include "core/Indicators.h"
#include "core/Series.h"
#include "server/Server.h"
#include "ui/App.h"
#include "ui/ChartPane.h"
#include "ui/Drawings.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <memory>

using namespace qtm;

namespace {

struct Step {
    QString cmd;
    int after = 1500;
};

void send(QWidget *w, QEvent::Type type, QPointF pos, Qt::MouseButton button = Qt::LeftButton)
{
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonPress || (type == QEvent::MouseMove && button != Qt::NoButton)
        ? Qt::MouseButtons(button) : Qt::MouseButtons(Qt::NoButton);
    QMouseEvent e(type, pos, w->mapToGlobal(pos), type == QEvent::MouseMove ? Qt::NoButton : button, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}

void clickChart(ChartPane *p, double x, double y)
{
    QWidget *c = p->chart;
    send(c, QEvent::MouseMove, QPointF(x, y), Qt::NoButton);
    send(c, QEvent::MouseButtonPress, QPointF(x, y));
    send(c, QEvent::MouseButtonRelease, QPointF(x, y));
}

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

} // namespace

// --compare a.png b.png out.png x y w h zoom: the same region of two images, zoomed, one above the other.
int compareImages(const QStringList &a)
{
    const QImage x(a.value(2)), y(a.value(3));
    const QRect r(a.value(5).toInt(), a.value(6).toInt(), a.value(7).toInt(), a.value(8).toInt());
    const int z = std::max(1, a.value(9).toInt());
    QImage out(r.width() * z, (r.height() * 2 + 4) * z, QImage::Format_RGB32);
    out.fill(Qt::magenta);
    QPainter p(&out);
    p.drawImage(QRect(0, 0, r.width() * z, r.height() * z), x.copy(r));
    p.drawImage(QRect(0, (r.height() + 4) * z, r.width() * z, r.height() * z), y.copy(r));
    p.end();
    return out.save(a.value(4)) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc > 1 && QString(argv[1]) == "--pixels") {   // --pixels img.png x y [x y ...]
        QGuiApplication g(argc, argv);
        const QStringList a = g.arguments();
        const QImage img(a.value(2));
        for (int k = 3; k + 1 < a.size(); k += 2)
            std::printf("%s,%s %s\n", qPrintable(a[k]), qPrintable(a[k + 1]),
                        qPrintable(img.pixelColor(a[k].toInt(), a[k + 1].toInt()).name(QColor::HexArgb)));
        return 0;
    }
    if (argc > 1 && QString(argv[1]) == "--compare") {
        QGuiApplication g(argc, argv);
        return compareImages(g.arguments());
    }
    QApplication qapp(argc, argv);
    QApplication::setOrganizationName("Deepwater");
    QApplication::setApplicationName("QtmUiDemo");

    QString dataDir, customDir, statePath, shot;
    int w = 1600, h = 980, wait = 6000;
    QVector<Step> steps;
    const QStringList args = qapp.arguments();
    for (int i = 1; i + 1 < args.size(); i += 2) {
        const QString k = args[i], v = args[i + 1];
        if (k == "--data") dataDir = v;
        else if (k == "--custom") customDir = v;
        else if (k == "--state") statePath = v;
        else if (k == "--shot") shot = v;
        else if (k == "--w") w = v.toInt();
        else if (k == "--h") h = v.toInt();
        else if (k == "--wait") wait = v.toInt();
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
    if (!statePath.isEmpty()) {
        QFile::remove(dataDir + "/state.json");
        QFile::copy(statePath, dataDir + "/state.json");
    }

    Backend backend(dataDir, customDir);
    backend.loadCustomIndicators();
    backend.startAlerts();

    QMainWindow win;
    win.setWindowTitle("Quant Terminal");
    auto *app = new App;
    win.setCentralWidget(app);
    win.resize(w, h);
    win.show();

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
        ChartPane *p = app->active;
        auto num = [&](int k) { return a.value(k).toDouble(); };
        if (c == "layout") app->setLayout(a.value(1));
        else if (c == "theme") app->toggleTheme();
        else if (c == "tf") app->setTf(a.value(1));
        else if (c == "charttype") { p->cfg.type = a.value(1); p->render(true); app->syncToolbar(); }
        else if (c == "symbol") app->setSymbol(a.value(1));
        else if (c == "compare") { p->cfg.compare.push_back(a.value(1)); p->load(true); }
        else if (c == "log") { p->cfg.log = !p->cfg.log; p->render(false); app->syncToolbar(); }
        else if (c == "ind") {
            QJsonObject params;
            for (int k = 2; k + 1 < a.size(); k += 2) {
                bool ok = false;
                const double d = a[k + 1].toDouble(&ok);
                params[a[k]] = ok ? QJsonValue(d) : QJsonValue(a[k + 1]);
            }
            p->addIndicator(a.value(1), params);
        }
        else if (c == "indsrc") {   // indsrc ID PARENT_INDEX OUTPUT_KEY [k v]...: built on another indicator
            QJsonObject params;
            params["source"] = QString("ind:%1:%2").arg(p->cfg.indicators.value(a.value(2).toInt()).uid, a.value(3));
            for (int k = 4; k + 1 < a.size(); k += 2) params[a[k]] = a[k + 1].toDouble();
            p->addIndicator(a.value(1), params);
        }
        else if (c == "rmind") p->removeIndicator(p->cfg.indicators.value(a.value(1).toInt()).uid);
        else if (c == "hideind") { auto &x = p->cfg.indicators[a.value(1).toInt()]; x.hidden = !x.hidden; p->render(false); }
        else if (c == "settings") {   // settings KEY VALUE (bars | utc | volume | syncCrosshair | syncReplay | stay)
            auto &s = app->settings();
            const QString k = a.value(1);
            if (k == "bars") s.bars = a.value(2).toInt();
            else if (k == "utc") s.utc = a.value(2) == "1";
            else if (k == "volume") s.volume = a.value(2) == "1";
            else if (k == "syncCrosshair") s.syncCrosshair = a.value(2) == "1";
            else if (k == "syncReplay") s.syncReplay = a.value(2) == "1";
            else if (k == "stay") s.stayInDrawMode = a.value(2) == "1";
            for (auto *x : app->panes) x->load(true);
        }
        else if (c == "picker") app->pickAnchor(p, p->cfg.indicators.last().uid, a.value(1));
        else if (c == "csv") {   // csv FILE: the CSV export of the active chart, without the save dialog
            QVector<QPair<QString, Values>> extra;
            for (const auto &ind : p->cfg.indicators) {
                const IndicatorDefPtr def = indicatorById(ind.id);
                const IndicatorResult res = p->indResults.value(ind.uid);
                if (!def) continue;
                for (const auto &o : def->outputs) if (res.values.contains(o.key)) extra.push_back({QString("%1_%2").arg(ind.id, o.key), res.values[o.key]});
            }
            QFile f(a.value(1));
            if (f.open(QIODevice::WriteOnly)) f.write(toCsv(p->shown, extra).toUtf8());
        }
        else if (c == "range") {   // range: each pane's visible logical range and width
            for (auto *x : app->panes) {
                const auto r = x->chart->timeScale()->getVisibleLogicalRange();
                std::printf("pane w=%d from=%.2f to=%.2f bars=%d\n", x->width(), r ? r->from : -1.0, r ? r->to : -1.0, int(x->shown.size()));
            }
            std::fflush(stdout);
        }
        else if (c == "focus") {   // focus OBJECTNAME: give that widget the keyboard focus
            if (auto *w = app->findChild<QWidget *>(a.value(1))) { win.activateWindow(); w->setFocus(); }
        }
        else if (c == "inds") {   // inds: the active chart's indicators
            for (const auto &x : p->cfg.indicators)
                std::printf("%s %s %s tf=%s\n", qPrintable(x.uid), qPrintable(x.id), QJsonDocument(x.params).toJson(QJsonDocument::Compact).constData(), qPrintable(x.tf));
            std::fflush(stdout);
        }
        else if (c == "snap") p->screenshot().save(a.value(1));
        else if (c == "indtf") { p->cfg.indicators.last().tf = a.value(1); p->render(false); }
        else if (c == "modal") {
            const QString m = a.value(1);
            if (m == "indicators") app->openIndicatorPicker();
            else if (m == "symbol") app->openSymbolSearch(a.value(2));
            else if (m == "settings") app->openSettings();
            else if (m == "indsettings") app->openIndicatorSettings(p, &p->cfg.indicators[a.value(2).toInt()]);
            else if (m == "compare") app->openSymbolSearch(QString(), [](const QString &) {}, QString::fromUtf8("Compare with…"));
        }
        else if (c == "replay") p->startReplay(p->bars.size() - a.value(1).toInt());
        else if (c == "step") for (int k = 0; k < a.value(1).toInt(); ++k) p->replayStep(1);
        else if (c == "play") p->toggleReplay();
        else if (c == "exit-replay") p->stopReplay();
        else if (c == "tool") app->setTool(a.value(1));
        else if (c == "click") clickChart(p, num(1), num(2));
        else if (c == "move") send(p->chart, QEvent::MouseMove, QPointF(num(1), num(2)), Qt::NoButton);
        else if (c == "draw") {
            app->setTool(a.value(1));
            for (int k = 2; k + 1 < a.size(); k += 2) {
                send(p->chart, QEvent::MouseMove, QPointF(num(k), num(k + 1)), Qt::NoButton);
                clickChart(p, num(k), num(k + 1));
                QApplication::processEvents();
            }
        }
        else if (c == "select") { app->setTool("cursor"); clickChart(p, num(1), num(2)); }
        else if (c == "delete") p->deleteSelectedDrawing();
        else if (c == "key") {
            QWidget *target = QApplication::focusWidget() ? QApplication::focusWidget() : win.focusWidget() ? win.focusWidget() : app;
            const QString t = a.value(1);
            QKeyEvent e(QEvent::KeyPress, t.size() == 1 ? QKeySequence(t.toUpper())[0].key() : Qt::Key_Escape, Qt::NoModifier, t.size() == 1 ? t : QString());
            QApplication::sendEvent(target, &e);
        }
        else if (c == "delkey") {
            QWidget *target = win.focusWidget() ? win.focusWidget() : app;
            QKeyEvent e(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
            QApplication::sendEvent(target, &e);
        }
        else if (c == "enter") {
            QWidget *target = win.focusWidget() ? win.focusWidget() : app;
            QKeyEvent e(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, "\r");
            QApplication::sendEvent(target, &e);
        }
        else if (c == "type") {
            QWidget *target = win.focusWidget() ? win.focusWidget() : app;
            for (QChar ch : a.value(1)) {
                QKeyEvent e(QEvent::KeyPress, QKeySequence(QString(ch).toUpper())[0].key(), Qt::NoModifier, QString(ch));
                QApplication::sendEvent(target, &e);
            }
        }
        else if (c == "state") {
            std::printf("tool=%s layout=%s panes=%d theme=%s drawings=%d modal=%d replay=%d focus=%s\n", qPrintable(app->tool),
                        qPrintable(app->state.layout), int(app->panes.size()), qPrintable(app->settings().theme),
                        int(p->drawings->items.size()), int(app->modalOpen()), int(p->replay.has_value()),
                        QApplication::focusWidget() ? QApplication::focusWidget()->metaObject()->className() : "none");
            std::fflush(stdout);
        }
        else if (c == "esc") {
            QWidget *target = win.focusWidget() ? win.focusWidget() : app;
            QKeyEvent e(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(target, &e);
        }
        else if (c == "toast") app->toast(QStringList(a.mid(1)).join(' '));
        else if (c == "toast-err") app->toast(QStringList(a.mid(1)).join(' '), "err");
        else if (c == "selfirst") { if (!p->drawings->items.isEmpty()) p->drawings->selected = p->drawings->items[0].id; p->drawings->update(); }
        else if (c == "hover") {
            const QPoint pt(num(1), num(2));
            QWidget *t = win.childAt(pt);
            if (t) send(t, QEvent::MouseMove, t->mapFrom(&win, QPointF(pt)), Qt::NoButton);
        }
        else if (c == "wclick") {   // wclick X Y: a real click in window coordinates, routed by Qt like the mouse
            QWindow *wh = win.windowHandle();
            const QPointF pt(num(1), num(2));
            for (auto t : {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
                const Qt::MouseButton b = t == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
                QMouseEvent e(t, pt, pt, wh->mapToGlobal(pt), b, t == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(wh, &e);
                // The event loop runs between a real press and its release: deleteLater() happens there.
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            }
        }
        else if (c == "active") app->setActive(app->panes.value(a.value(1).toInt()));
        else if (c == "shot") { renderWindow(&win).save(a.value(1)); std::printf("saved %s\n", qPrintable(a.value(1))); }
        else std::fprintf(stderr, "unknown command: %s\n", qPrintable(steps[i].cmd));
        QTimer::singleShot(steps[i].after, &qapp, [run, i] { (*run)(i + 1); });
    };
    QObject::connect(app, &App::ready, &qapp, [&, run] { QTimer::singleShot(wait, &qapp, [run] { (*run)(0); }); });
    return qapp.exec();
}
