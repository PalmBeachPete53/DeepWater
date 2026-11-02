#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// A complete color palette. Painting code reads colors from Theme::current();
// the stylesheet is generated from the same palette (see Theme::styleSheet).
struct Palette {
    QString id;
    QString name;   // shown as-is in every language

    QColor Background;
    QColor GridLine;
    QColor OceanInner;
    QColor OceanOuter;
    QColor Land;
    QColor LandBorder;
    QColor Graticule;
    QColor Halo;
    QColor Panel;
    QColor PanelLine;
    QColor Hover;
    QColor Active;
    QColor Text;
    QColor TextDim;
    QColor TextSection;
    QColor TextFaint;
    QColor Accent;
    QColor Up;
    QColor Down;
    QColor ControlBorder;
    QColor ControlBorderHover;
    QColor Tooltip;
    QColor TooltipBorder;
    QColor ClusterFill;
    QColor LimbShade {0, 0, 0, 120};   // darkening toward the globe's edge

    // Per-theme replacements for category colors (category id -> color), for
    // palettes where the defaults from markets.json lack contrast.
    QHash<QString, QColor> categoryColors;
};

namespace Theme {

const QVector<Palette> &palettes();
const Palette &current();
void setCurrent(const QString &id);

// Replaces @Token placeholders (Palette field names, and @MonoFamilies) in a stylesheet template.
QString styleSheet(const QString &tmpl);

// The families of `wanted` that are installed, in order; `wanted` itself when none is. Qt spends
// tens of milliseconds resolving font aliases the first time it is asked for a family that isn't
// installed, so font lists only name installed ones. Needs the QApplication.
QStringList installedFamilies(const QStringList &wanted);
// The UI's monospaced families (SF Mono, Menlo, Monaco), those installed.
const QStringList &monoFamilies();

// A light palette (Glacier, Dune, Sakura): light background.
bool isLight(const Palette &p);
// The palette as the Charts page's CSS variables (qtm::theme::setPalette, src/quant/ui/Theme.h).
QHash<QString, QString> chartVars(const Palette &p);

} // namespace Theme
