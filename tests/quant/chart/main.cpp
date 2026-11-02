// Chart engine demo and checks. See CMakeLists.txt for the command line.

#include "Scenes.h"
#include "chart/ChartPrivate.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QRawFont>
#include <QTextLayout>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTabWidget>
#include <QTextStream>
#include <QWheelEvent>

#include <cmath>
#include <cstdio>

using namespace lw;

namespace {

void mouseMove(QWidget *w, QPointF pos, Qt::MouseButtons buttons = Qt::NoButton)
{
    QMouseEvent e(QEvent::MouseMove, pos, w->mapToGlobal(pos), Qt::NoButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}
void mousePress(QWidget *w, QPointF pos)
{
    QMouseEvent e(QEvent::MouseButtonPress, pos, w->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                  Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}
void mouseRelease(QWidget *w, QPointF pos)
{
    QMouseEvent e(QEvent::MouseButtonRelease, pos, w->mapToGlobal(pos), Qt::LeftButton, Qt::NoButton,
                  Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}
void mouseDouble(QWidget *w, QPointF pos)
{
    QMouseEvent e(QEvent::MouseButtonDblClick, pos, w->mapToGlobal(pos), Qt::LeftButton,
                  Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}
void wheel(QWidget *w, QPointF pos, QPoint angle)
{
    QWheelEvent e(pos, w->mapToGlobal(pos), QPoint(), angle, Qt::NoButton, Qt::NoModifier,
                  Qt::NoScrollPhase, false);
    QApplication::sendEvent(w, &e);
}

QImage renderChart(Chart *c, double dpr)
{
    QImage img(int(c->width() * dpr), int(c->height() * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::black);
    c->render(&img);
    return img;
}

Chart *buildScene(const Scene &s, const SceneData &data, double dpr)
{
    Chart *c = s.build(data);
    c->setProperty("lwDevicePixelRatio", dpr);
    c->resize(s.width, s.height);
    if (s.mouseX >= 0) {
        c->takeScreenshot();   // lay out first
        mouseMove(c, QPointF(s.mouseX, s.mouseY));
    }
    return c;
}

int shots(const QString &dir, double dpr, const QString &only)
{
    QDir().mkpath(dir);
    const SceneData data = makeData();
    for (const Scene &s : scenes()) {
        if (!only.isEmpty() && s.name != only)
            continue;
        Chart *c = buildScene(s, data, dpr);
        const QImage img = renderChart(c, dpr);
        const QString path = dir + '/' + s.name + ".png";
        img.save(path);
        const auto r = c->timeScale()->getVisibleLogicalRange();
        std::printf("%s width %.1f range %.3f..%.3f\n", qPrintable(path), c->timeScale()->width(),
                    r ? r->from : 0, r ? r->to : 0);
        delete c;
    }
    return 0;
}

// Every label drawn by each scene: DIR/<scene>.json, [{t, x, y, bold, color}] with x, y the left end
// of the text baseline in CSS pixels. ref/texts.mjs writes the same from the real library.
int texts(const QString &dir, const QString &only, double dpr)
{
    QDir().mkpath(dir);
    const SceneData data = makeData();
    for (const Scene &s : scenes()) {
        if (!only.isEmpty() && s.name != only && !(only.endsWith('*') && s.name.startsWith(only.chopped(1))))
            continue;
        Chart *c = buildScene(s, data, dpr);
        renderChart(c, dpr);   // settle layout
        QStringList items;
        lw::detail::textTrace = [&](const QString &t, const QPointF &pt, bool bold, const QColor &col) {
            QString esc = t;
            esc.replace('\\', "\\\\").replace('"', "\\\"");
            items << QStringLiteral("{\"t\":\"%1\",\"x\":%2,\"y\":%3,\"bold\":%4,\"color\":\"%5\"}")
                         .arg(esc).arg(pt.x(), 0, 'f', 2).arg(pt.y(), 0, 'f', 2)
                         .arg(bold ? "true" : "false", col.name(QColor::HexArgb));
        };
        renderChart(c, dpr);
        lw::detail::textTrace = nullptr;
        QFile f(dir + '/' + s.name + ".json");
        if (f.open(QIODevice::WriteOnly))
            f.write(('[' + items.join(",\n") + "]\n").toUtf8());
        delete c;
    }
    return 0;
}

int exportData(const QString &dir)
{
    QDir().mkpath(dir);
    QFile f(dir + "/data.js");
    if (!f.open(QIODevice::WriteOnly))
        return 1;
    f.write(exportJs(makeData()).toUtf8());
    QStringList list;
    for (const Scene &s : scenes())
        list << QStringLiteral("%1 %2 %3 %4 %5").arg(s.name).arg(s.width).arg(s.height).arg(s.mouseX).arg(s.mouseY);
    QFile g(dir + "/scenes.txt");
    if (!g.open(QIODevice::WriteOnly))
        return 1;
    g.write((list.join('\n') + '\n').toUtf8());
    return 0;
}

// ---------------------------------------------------------------- checks

int failures = 0;
void check(bool ok, const QString &what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
    if (!ok)
        ++failures;
}
bool near(double a, double b, double eps = 1e-6) { return std::abs(a - b) <= eps; }

// float index under x, from the visible logical range (TimeScale._coordinateToFloatIndex)
double floatIndexAt(Chart *c, double x)
{
    const auto r = c->timeScale()->getVisibleLogicalRange();
    const double w = c->timeScale()->width();
    const double bs = w / (r->to - r->from + 1);
    return r->to - (w - 1 - x) / bs;
}

int tests()
{
    const SceneData data = makeData();
    const Scene scene = scenes()[0];   // main: candles + volume + EMA + RSI pane
    Chart *c = scene.build(data);
    c->resize(scene.width, scene.height);
    c->show();
    QApplication::processEvents();
    const int n = data.daily.size();
    TimeScale *ts = c->timeScale();

    // initial range: setVisibleLogicalRange(n - 160, n + 8)
    auto r = ts->getVisibleLogicalRange();
    check(r && near(r->from, n - 160) && near(r->to, n + 8), "initial visible logical range");

    // logical <-> coordinate round trip
    bool rt = true;
    for (int i = n - 150; i < n; ++i) {
        const auto x = ts->logicalToCoordinate(i);
        const auto l = ts->coordinateToLogical(*x);
        rt = rt && x && l && *l == i;
    }
    check(rt, "logicalToCoordinate / coordinateToLogical round trip");
    check(ts->logicalToCoordinate(n - 10.5) == 0.0, "fractional logical maps to 0 (as Lightweight Charts)");

    // price <-> coordinate round trip on the main series
    Series *main = nullptr;
    // the first series added is the candlestick one; find it through the crosshair test below
    Q_UNUSED(main);

    // crosshair events report the bar under the mouse
    MouseEventParams last;
    int events = 0;
    QObject::connect(c, &Chart::crosshairMoved, [&](const MouseEventParams &p) {
        last = p;
        ++events;
    });
    const int k = n - 30;
    const double xk = *ts->logicalToCoordinate(k);
    mouseMove(c, QPointF(xk + 0.4, 120));
    check(events > 0 && last.logical && int(*last.logical) == k, "crosshair logical index");
    check(last.time && *last.time == data.daily[k].time, "crosshair time");
    check(last.point && near(last.point->x(), xk + 0.4) && near(last.point->y(), 120) && last.paneIndex == 0,
          "crosshair point and pane");
    // mouse in the RSI pane
    const double paneH = c->panes()[0]->getHeight();
    mouseMove(c, QPointF(xk, paneH + 1 + 20));
    check(last.paneIndex == 1 && last.point && near(last.point->y(), 20), "crosshair in second pane");

    // wheel zoom keeps the bar under the cursor in place
    const double zx = 400;
    const double before = floatIndexAt(c, zx);
    const double bsBefore = ts->width() / (ts->getVisibleLogicalRange()->to - ts->getVisibleLogicalRange()->from + 1);
    wheel(c, QPointF(zx, 150), QPoint(0, 120));
    const double after = floatIndexAt(c, zx);
    const double bsAfter = ts->width() / (ts->getVisibleLogicalRange()->to - ts->getVisibleLogicalRange()->from + 1);
    check(near(before, after, 1e-4), QStringLiteral("wheel zoom keeps the bar under the cursor (%1 -> %2)").arg(before).arg(after));
    check(near(bsAfter, bsBefore * 1.1, 1e-6), QStringLiteral("wheel notch zooms by 10% (%1 -> %2)").arg(bsBefore).arg(bsAfter));
    wheel(c, QPointF(zx, 150), QPoint(0, -120));
    check(near(floatIndexAt(c, zx), before, 1e-4), "wheel zoom out keeps the bar under the cursor");

    // horizontal wheel scrolls 80 px per notch
    const auto r0 = *ts->getVisibleLogicalRange();
    const double bs0 = ts->width() / (r0.to - r0.from + 1);
    wheel(c, QPointF(zx, 150), QPoint(-120, 0));
    const auto r1 = *ts->getVisibleLogicalRange();
    check(near(r1.to - r0.to, 80 / bs0, 1e-6), "horizontal wheel scrolls");
    ts->setVisibleLogicalRange({r0.from, r0.to});

    // drag scrolls: the content follows the mouse once the drag started
    const auto d0 = *ts->getVisibleLogicalRange();
    const double bs = ts->width() / (d0.to - d0.from + 1);
    mousePress(c, QPointF(300, 150));
    mouseMove(c, QPointF(303, 150), Qt::LeftButton);    // below the 5 px threshold
    mouseMove(c, QPointF(306, 150), Qt::LeftButton);    // drag starts (anchor)
    mouseMove(c, QPointF(307, 150), Qt::LeftButton);    // scroll starts here
    mouseMove(c, QPointF(407, 150), Qt::LeftButton);
    mouseRelease(c, QPointF(407, 150));
    const auto d1 = *ts->getVisibleLogicalRange();
    check(near(d0.to - d1.to, 100 / bs, 1e-6), QStringLiteral("drag scrolls by 100 px (%1 bars)").arg(d0.to - d1.to));

    // clicks report the pane and point
    std::optional<MouseEventParams> clicked;
    QObject::connect(c, &Chart::clicked, [&](const MouseEventParams &p) { clicked = p; });
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 600)
        QApplication::processEvents();   // leave the double-click window
    mousePress(c, QPointF(200, paneH + 1 + 30));
    mouseRelease(c, QPointF(200, paneH + 1 + 30));
    check(clicked && clicked->paneIndex == 1 && clicked->point && near(clicked->point->x(), 200)
              && near(clicked->point->y(), 30),
          "click reports pane 1 and the point in it");
    check(clicked && clicked->logical && *clicked->logical == *ts->coordinateToLogical(200), "click logical");
    // a second click within 500 ms is a double click, not a click
    clicked.reset();
    mousePress(c, QPointF(200, paneH + 1 + 30));
    mouseRelease(c, QPointF(200, paneH + 1 + 30));
    check(!clicked, "second click within 500 ms is swallowed (double click)");

    // time axis drag scales, double click resets
    t.restart();
    while (t.elapsed() < 600)
        QApplication::processEvents();
    const double axisY = scene.height - 10;
    const auto a0 = *ts->getVisibleLogicalRange();
    mousePress(c, QPointF(500, axisY));
    mouseMove(c, QPointF(520, axisY), Qt::LeftButton);
    mouseMove(c, QPointF(600, axisY), Qt::LeftButton);
    mouseRelease(c, QPointF(600, axisY));
    const auto a1 = *ts->getVisibleLogicalRange();
    // dragging right shrinks the bar spacing by (w - x) / (w - x0), keeping the right edge
    const double w = ts->width();
    const double bsA0 = w / (a0.to - a0.from + 1), bsA1 = w / (a1.to - a1.from + 1);
    check(near(bsA1, bsA0 * (w - 600) / (w - 500), 1e-6) && near(a1.to, a0.to, 1e-6),
          QStringLiteral("time axis drag scales around the right edge (%1 -> %2)").arg(bsA0).arg(bsA1));
    t.restart();
    while (t.elapsed() < 600)
        QApplication::processEvents();
    mousePress(c, QPointF(500, axisY));
    mouseRelease(c, QPointF(500, axisY));
    mouseDouble(c, QPointF(500, axisY));
    mouseRelease(c, QPointF(500, axisY));
    const auto a2 = *ts->getVisibleLogicalRange();
    check(near(ts->width() / (a2.to - a2.from + 1), 6) && near(a2.to, n - 1 + 8),
          "double click on the time axis resets bar spacing and right offset");

    // price axis drag turns autoscale off, double click turns it back on
    t.restart();
    while (t.elapsed() < 600)
        QApplication::processEvents();
    const double ax = ts->width() + 10;
    mousePress(c, QPointF(ax, 100));
    mouseMove(c, QPointF(ax, 110), Qt::LeftButton);
    mouseMove(c, QPointF(ax, 160), Qt::LeftButton);
    mouseRelease(c, QPointF(ax, 160));
    check(!c->priceScale("right")->options().autoScale, "price axis drag turns autoscale off");
    t.restart();
    while (t.elapsed() < 600)
        QApplication::processEvents();
    mousePress(c, QPointF(ax, 100));
    mouseRelease(c, QPointF(ax, 100));
    mouseDouble(c, QPointF(ax, 100));
    mouseRelease(c, QPointF(ax, 100));
    check(c->priceScale("right")->options().autoScale, "double click on the price axis restores autoscale");

    // price <-> coordinate round trip
    {
        SeriesOptions so;
        Series *probe = c->addSeries(SeriesType::Line, so, 0);
        QVector<DataItem> pd;
        for (const Bar &b : data.daily) {
            DataItem x;
            x.time = b.time;
            x.value = b.close;
            pd.push_back(x);
        }
        probe->setData(pd);
        bool ok = true;
        for (double price : {25000.0, 30000.0, 41234.5}) {
            const auto y = probe->priceToCoordinate(price);
            const auto p = y ? probe->coordinateToPrice(*y) : std::nullopt;
            ok = ok && y && p && near(*p, price, 1e-6);
        }
        check(ok, "priceToCoordinate / coordinateToPrice round trip");
        c->removeSeries(probe);
    }

    // separator drag resizes panes
    {
        const double h0 = c->panes()[1]->getHeight();
        const double sepY = c->panes()[0]->getHeight();
        mousePress(c, QPointF(300, sepY + 0.5));
        mouseMove(c, QPointF(300, sepY - 10), Qt::LeftButton);
        mouseMove(c, QPointF(300, sepY - 60), Qt::LeftButton);
        mouseRelease(c, QPointF(300, sepY - 60));
        const double h1 = c->panes()[1]->getHeight();
        check(h1 > h0 + 40, QStringLiteral("separator drag resizes the panes (%1 -> %2)").arg(h0).arg(h1));
    }

    std::printf("%d failure(s)\n", failures);
    delete c;
    return failures ? 1 : 0;
}

// ---------------------------------------------------------------- performance

int bench(int lines)
{
    const int N = 20000;
    ChartOptions o;
    o.background = QColor("#131722");
    o.textColor = QColor("#d1d4dc");
    o.fontSize = 11;
    o.crosshairMode = CrosshairMode::Normal;
    o.rightOffset = 8;
    o.priceFormatter = [](double p) { return fmtPrice(p); };
    Chart c(o);
    c.setProperty("lwDevicePixelRatio", 2.0);
    c.resize(1400, 800);
    QVector<DataItem> candles, vol;
    double price = 30000;
    unsigned seed = 1;
    auto rnd = [&seed] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffff) / 65536.0; };
    for (int i = 0; i < N; ++i) {
        DataItem d;
        d.time = 1500000000 + i * 3600.0;
        d.open = price;
        price *= 1 + (rnd() - 0.5) * 0.02;
        d.close = price;
        d.high = std::max(d.open, d.close) * (1 + rnd() * 0.005);
        d.low = std::min(d.open, d.close) * (1 - rnd() * 0.005);
        candles.push_back(d);
        DataItem v;
        v.time = d.time;
        v.value = 1000 + rnd() * 5000;
        v.color = d.close >= d.open ? cssColor("rgba(8,153,129,0.45)") : cssColor("rgba(242,54,69,0.45)");
        vol.push_back(v);
    }
    QElapsedTimer t;
    t.start();
    SeriesOptions so;
    Series *m = c.addSeries(SeriesType::Candlestick, so);
    m->setData(candles);
    SeriesOptions vo;
    vo.priceScaleId = "vol";
    vo.priceFormat.type = PriceFormatType::Volume;
    Series *v = c.addSeries(SeriesType::Histogram, vo);
    v->setData(vol);
    auto ps = c.priceScale("vol")->options();   // as chart.js: volume in the bottom 18%
    ps.scaleMarginTop = 0.82;
    ps.scaleMarginBottom = 0;
    c.priceScale("vol")->applyOptions(ps);
    for (int k = 0; k < lines; ++k) {
        SeriesOptions lo;
        lo.lineWidth = 1;
        lo.lastValueVisible = k % 4 == 0;
        lo.priceLineVisible = false;
        Series *s = c.addSeries(SeriesType::Line, lo, k < lines * 3 / 4 ? 0 : 1 + (k - lines * 3 / 4) / 5);
        QVector<DataItem> ld;
        ld.reserve(N);
        double acc = candles[0].close;
        for (int i = 0; i < N; ++i) {
            acc = acc * 0.9 + candles[i].close * 0.1 * (1 + k * 0.001);
            DataItem x;
            x.time = candles[i].time;
            x.value = acc;
            ld.push_back(x);
        }
        s->setData(ld);
    }
    const double setDataMs = t.nsecsElapsed() / 1e6;
    c.timeScale()->setVisibleLogicalRange({N - 160.0, N + 8.0});
    QImage img(1400 * 2, 800 * 2, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(2);
    auto frame = [&] {
        QPainter p(&img);
        c.render(&p);
    };
    frame();
    auto measure = [&](const char *label, std::function<void()> step, int frames) {
        QElapsedTimer tt;
        tt.start();
        for (int i = 0; i < frames; ++i) {
            step();
            frame();
        }
        std::printf("%-40s %7.2f ms/frame\n", label, tt.nsecsElapsed() / 1e6 / frames);
    };
    std::printf("setData: 20,000 candles + volume + %d line series: %.1f ms\n", lines, setDataMs);
    measure("168 bars visible, scrolling", [&] {
        auto r = *c.timeScale()->getVisibleLogicalRange();
        c.timeScale()->setVisibleLogicalRange({r.from - 1, r.to - 1});
    }, 60);
    c.timeScale()->fitContent();
    measure("all 20,000 bars visible (fitContent)", [] {}, 30);
    measure("all bars, crosshair moving", [&] {
        static int x = 100;
        x = (x + 7) % 1200;
        mouseMove(&c, QPointF(x, 300));
    }, 30);
    c.timeScale()->setVisibleLogicalRange({N - 2000.0, N + 8.0});
    measure("2,000 bars visible", [] {}, 30);
    return 0;
}

// ---------------------------------------------------------------- image comparison

// Ours above, reference below (or a zoomed crop of both), for looking at them side by side.
int compare(const QString &ours, const QString &ref, const QString &out, QRect crop, int zoom)
{
    QImage a(ours), b(ref);
    if (a.isNull() || b.isNull())
        return 1;
    a = a.convertToFormat(QImage::Format_RGB32);
    b = b.convertToFormat(QImage::Format_RGB32);
    if (crop.isValid()) {
        a = a.copy(crop).scaled(crop.size() * zoom, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        b = b.copy(crop).scaled(crop.size() * zoom, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    QImage img(std::max(a.width(), b.width()), a.height() + b.height() + 4, QImage::Format_RGB32);
    img.fill(Qt::magenta);
    QPainter p(&img);
    p.drawImage(0, 0, a);
    p.drawImage(0, a.height() + 4, b);
    p.end();
    img.save(out);
    // a rough difference figure: share of pixels that differ by more than 48 in a channel
    QImage ra(ours), rb(ref);
    ra = ra.convertToFormat(QImage::Format_RGB32);
    rb = rb.convertToFormat(QImage::Format_RGB32);
    long diff = 0, total = 0;
    for (int y = 0; y < std::min(ra.height(), rb.height()); ++y)
        for (int x = 0; x < std::min(ra.width(), rb.width()); ++x) {
            const QRgb c1 = ra.pixel(x, y), c2 = rb.pixel(x, y);
            ++total;
            if (std::abs(qRed(c1) - qRed(c2)) > 48 || std::abs(qGreen(c1) - qGreen(c2)) > 48
                || std::abs(qBlue(c1) - qBlue(c2)) > 48)
                ++diff;
        }
    std::printf("%s: %.2f%% of pixels differ\n", qPrintable(out), total ? 100.0 * diff / total : 0);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = app.arguments();
    auto value = [&](const QString &k) {
        const int i = args.indexOf(k);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : QString();
    };
    if (args.contains("--export"))
        return exportData(value("--export"));
    if (args.contains("--shot")) {
        const QString dpr = value("--dpr");
        return shots(value("--shot"), dpr.isEmpty() ? 1.0 : dpr.toDouble(), value("--scene"));
    }
    if (args.contains("--texts"))
        return texts(value("--texts"), value("--scene"), value("--dpr").isEmpty() ? 1.0 : value("--dpr").toDouble());
    if (args.contains("--compare")) {
        // --compare ours.png ref.png out.png [x y w h zoom]
        const int i = args.indexOf("--compare");
        QRect crop;
        int zoom = 1;
        if (args.size() > i + 8) {
            crop = QRect(args[i + 4].toInt(), args[i + 5].toInt(), args[i + 6].toInt(), args[i + 7].toInt());
            zoom = args[i + 8].toInt();
        }
        return compare(args[i + 1], args[i + 2], args[i + 3], crop, zoom);
    }
    if (args.contains("--metrics")) {
        QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        f.setPixelSize(11);
        QFontMetricsF fm(f);
        std::printf("%s ascent %.3f descent %.3f xh %.3f cap %.3f\n", qPrintable(f.family()), fm.ascent(), fm.descent(), fm.xHeight(), fm.capHeight());
        for (const char *t : {"−10.00%", "10.00%", "90.00%", "52,500.00", "36,718.09", "00,000.00", "Apr", "May", "2024", "17", "Apr0", "Buy", "ETHUSDT", "05 Mar '24", "0"}) {
            const QString s = QString::fromUtf8(t);
            const QRectF r = fm.tightBoundingRect(s);
            std::printf("%-12s w %.3f  tight asc %.3f desc %.3f\n", t, fm.horizontalAdvance(s), -r.top(), r.bottom());
        }
        for (QChar ch : QString::fromUtf8("0123456789.,%−-+:'ABCMVWamrt &"))
            std::printf("%s=%.3f ", qPrintable(QString(ch)), fm.horizontalAdvance(ch));
        std::printf("\n,0=%.3f AV=%.3f\n", fm.horizontalAdvance(",0"), fm.horizontalAdvance("AV"));
        {
            QRawFont raw = QRawFont::fromFont(f);
            for (const char *t : {"52,500.00", "36,718.09", "10.00%", "−10.00%", "Apr0", "ETHUSDT", "05 Mar '24", "AV", "2024", "0"}) {
                const QString str = QString::fromUtf8(t);
                QList<quint32> gl = raw.glyphIndexesForString(str);
                double w1 = 0, w2 = 0, w3 = 0;
                for (auto p : raw.advancesForGlyphIndexes(gl, QRawFont::SeparateAdvances)) w1 += p.x();
                for (auto p : raw.advancesForGlyphIndexes(gl, QRawFont::KernedAdvances)) w2 += p.x();
                for (auto p : raw.advancesForGlyphIndexes(gl, QRawFont::LayoutFlags(QRawFont::KernedAdvances | QRawFont::UseDesignMetrics))) w3 += p.x();
                std::printf("raw %-12s sep %.3f kern %.3f kern+design %.3f\n", t, w1, w2, w3);
            }
        }
        for (int variant = 0; variant < 4; ++variant) {
            QFont g = f;
            if (variant & 1) g.setHintingPreference(QFont::PreferNoHinting);
            if (variant & 2) g.setStyleStrategy(QFont::PreferQuality);
            QFontMetricsF gm(g);
            QTextLayout tl(QString::fromUtf8("52,500.00"), g);
            tl.beginLayout(); QTextLine line = tl.createLine(); tl.endLayout();
            std::printf("variant %d: 52,500.00 adv %.3f layout %.3f  ,0 %.3f  '0' %.3f\n", variant, gm.horizontalAdvance("52,500.00"), line.naturalTextWidth(), gm.horizontalAdvance(",0"), gm.horizontalAdvance(QString("0")));
        }
        return 0;
    }
    if (args.contains("--test"))
        return tests();
    if (args.contains("--bench"))
    {
        std::printf("--- candles + volume + 40 line series (3 panes), 1400x800 at 2x\n");
        bench(40);
        std::printf("--- candles + volume + 6 line series (2 panes), 1400x800 at 2x\n");
        return bench(6);
    }

    const SceneData data = makeData();
    QTabWidget tabs;
    for (const Scene &s : scenes()) {
        Chart *c = s.build(data);
        tabs.addTab(c, s.name);
    }
    tabs.resize(1000, 640);
    tabs.show();
    return app.exec();
}
