#pragma once

// Scenes of the chart demo. Each one mirrors how quant-terminal uses Lightweight Charts
// (public/js/chart.js, panels.js, research.js); ref/scenes.js builds the same scene with the real
// library from the data exported by `qtm_chart_demo --export`.

#include "chart/Chart.h"

#include <QString>
#include <QVector>
#include <QMap>

#include <array>

#include <functional>

struct Bar {
    double time, open, high, low, close, volume;
};

struct SceneData {
    QVector<Bar> daily;      // 600 daily bars
    QVector<Bar> intraday;   // 5 minute bars over a few days
    QVector<Bar> growth;     // exponential growth for the log scale
    QVector<Bar> cmpA, cmpB; // compare symbols
    QVector<double> ema, rsi;   // on daily (NaN where undefined)
    struct Year { int year; QVector<std::pair<int, double>> points; };   // day of year, %
    QVector<Year> years;
    QVector<std::pair<int, double>> average;
    QVector<std::pair<double, double>> equity;   // time, value
    // price axis cases (scenes "tk-*"): bars and an optional indicator pane
    struct Tick {
        QString name;
        int width, height;
        QString type;    // candles | line | area | bars
        int mode = 0;    // PriceScaleMode
        QString pane;    // "" | rsi | macd | obv (indicator in a second pane)
        int visible = 160;   // bars in view (initialRange), 0: fitContent
        QVector<Bar> bars;
        QVector<double> ind;
    };
    QVector<Tick> ticks;
    // strategy tester equity curves on other bar sizes: step (s) -> [time, equity, buy & hold]
    QMap<int, QVector<std::array<double, 3>>> equities;
};

SceneData makeData();
QString exportJs(const SceneData &d);
QString fmtPrice(double v);   // api.js fmtPrice (en-US)

struct Scene {
    QString name;
    int width, height;
    // builds the chart; mouse: optional crosshair position (x, y) in widget coordinates
    std::function<lw::Chart *(const SceneData &)> build;
    double mouseX = -1, mouseY = -1;
};

QVector<Scene> scenes();
