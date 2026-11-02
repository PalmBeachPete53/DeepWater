#include "Theme.h"

#include <QFontDatabase>
#include <QList>
#include <QPair>
#include <QSet>

namespace Theme {

namespace {

// Warm dark-brown palette of the original reference design.
Palette ember()
{
    Palette p;
    p.id = "ember";
    p.name = "Ember";
    p.Background = QColor("#171310");
    p.GridLine = QColor("#2a231d");
    p.OceanInner = QColor("#241c17");
    p.OceanOuter = QColor("#120e0c");
    p.Land = QColor("#332a23");
    p.LandBorder = QColor("#4a3e33");
    p.Graticule = QColor("#3a3027");
    p.Halo = QColor("#5a4530");
    p.Panel = QColor("#1c1713");
    p.PanelLine = QColor("#332a22");
    p.Hover = QColor("#2a221c");
    p.Active = QColor("#3a2f26");
    p.Text = QColor("#ece3d6");
    p.TextDim = QColor("#9a8c7c");
    p.TextSection = QColor("#7d6f60");
    p.TextFaint = QColor("#6f6356");
    p.Accent = QColor("#e8c468");
    p.Up = QColor("#8dbfa8");
    p.Down = QColor("#e07a4f");
    p.ControlBorder = QColor("#3d3229");
    p.ControlBorderHover = QColor("#5a4c3f");
    p.Tooltip = QColor("#241d18");
    p.TooltipBorder = QColor("#4a3e33");
    p.ClusterFill = QColor("#16120f");
    return p;
}

// Same structure, recolored in deep-sea tones: abyssal blues, slate seabed,
// bioluminescent cyan accent.
Palette abyss()
{
    Palette p;
    p.id = "abyss";
    p.name = "Abyss";
    p.Background = QColor("#0a1317");
    p.GridLine = QColor("#13222a");
    p.OceanInner = QColor("#0f2430");
    p.OceanOuter = QColor("#061015");
    p.Land = QColor("#1a2e36");
    p.LandBorder = QColor("#2a4651");
    p.Graticule = QColor("#173039");
    p.Halo = QColor("#1f6a7c");
    p.Panel = QColor("#0d191e");
    p.PanelLine = QColor("#1b2e36");
    p.Hover = QColor("#13252c");
    p.Active = QColor("#1b3540");
    p.Text = QColor("#d9ecef");
    p.TextDim = QColor("#7f9ba4");
    p.TextSection = QColor("#5f7f89");
    p.TextFaint = QColor("#4d6871");
    p.Accent = QColor("#6fe0d6");
    p.Up = QColor("#6fd3bf");
    p.Down = QColor("#ee8a6a");
    p.ControlBorder = QColor("#21404a");
    p.ControlBorderHover = QColor("#35626f");
    p.Tooltip = QColor("#11232a");
    p.TooltipBorder = QColor("#2a4651");
    p.ClusterFill = QColor("#08141a");
    return p;
}

// Forest-dark greens with a polished jade accent.
Palette jade()
{
    Palette p;
    p.id = "jade";
    p.name = "Jade";
    p.Background = QColor("#0d1411");
    p.GridLine = QColor("#17231d");
    p.OceanInner = QColor("#13241e");
    p.OceanOuter = QColor("#07100c");
    p.Land = QColor("#21342b");
    p.LandBorder = QColor("#33503f");
    p.Graticule = QColor("#1b2e25");
    p.Halo = QColor("#2e6b4b");
    p.Panel = QColor("#111a16");
    p.PanelLine = QColor("#1f2e27");
    p.Hover = QColor("#18261f");
    p.Active = QColor("#213a2e");
    p.Text = QColor("#e2ede6");
    p.TextDim = QColor("#8aa497");
    p.TextSection = QColor("#6a8778");
    p.TextFaint = QColor("#557063");
    p.Accent = QColor("#5cc98f");
    p.Up = QColor("#7fd6a4");
    p.Down = QColor("#e8866a");
    p.ControlBorder = QColor("#24392f");
    p.ControlBorderHover = QColor("#37584a");
    p.Tooltip = QColor("#15241d");
    p.TooltipBorder = QColor("#33503f");
    p.ClusterFill = QColor("#0a120e");
    return p;
}

// Night-sky purples: deep violet panels, a nebula-lavender accent.
Palette nova()
{
    Palette p;
    p.id = "nova";
    p.name = "Nova";
    p.Background = QColor("#110c19");
    p.GridLine = QColor("#1f172d");
    p.OceanInner = QColor("#1c1430");
    p.OceanOuter = QColor("#0a0711");
    p.Land = QColor("#2b2140");
    p.LandBorder = QColor("#46365f");
    p.Graticule = QColor("#251c37");
    p.Halo = QColor("#6b42a8");
    p.Panel = QColor("#161020");
    p.PanelLine = QColor("#2a2039");
    p.Hover = QColor("#221a32");
    p.Active = QColor("#2f2447");
    p.Text = QColor("#ece6f6");
    p.TextDim = QColor("#9d91b6");
    p.TextSection = QColor("#7e7298");
    p.TextFaint = QColor("#675b80");
    p.Accent = QColor("#b98cff");
    p.Up = QColor("#79d6b0");
    p.Down = QColor("#f07a8a");
    p.ControlBorder = QColor("#33284a");
    p.ControlBorderHover = QColor("#51406f");
    p.Tooltip = QColor("#1d1529");
    p.TooltipBorder = QColor("#46365f");
    p.ClusterFill = QColor("#0d0914");
    return p;
}

// Iron-oxide reds over dark rust.
Palette oxide()
{
    Palette p;
    p.id = "oxide";
    p.name = "Oxide";
    p.Background = QColor("#160f0d");
    p.GridLine = QColor("#291b17");
    p.OceanInner = QColor("#26150f");
    p.OceanOuter = QColor("#110a08");
    p.Land = QColor("#3a231c");
    p.LandBorder = QColor("#57342a");
    p.Graticule = QColor("#3a241d");
    p.Halo = QColor("#7a3422");
    p.Panel = QColor("#1b1210");
    p.PanelLine = QColor("#36231d");
    p.Hover = QColor("#2a1a16");
    p.Active = QColor("#3e231c");
    p.Text = QColor("#f0e2dc");
    p.TextDim = QColor("#a88a80");
    p.TextSection = QColor("#8a6c62");
    p.TextFaint = QColor("#73574e");
    p.Accent = QColor("#d9573a");
    p.Up = QColor("#8dbfa8");
    p.Down = QColor("#ef6a4c");
    p.ControlBorder = QColor("#432a23");
    p.ControlBorderHover = QColor("#64392e");
    p.Tooltip = QColor("#26170f");
    p.TooltipBorder = QColor("#57342a");
    p.ClusterFill = QColor("#130b09");
    return p;
}

// After the Bloomberg Terminal: amber text on pure black, white titles and accent, blue selection,
// bright green and red for moves.
Palette bloomberg()
{
    Palette p;
    p.id = "bloomberg";
    p.name = "Bloomberg";
    p.Background = QColor("#000000");
    p.GridLine = QColor("#141414");
    p.OceanInner = QColor("#07101c");
    p.OceanOuter = QColor("#000000");
    p.Land = QColor("#1b1b1b");
    p.LandBorder = QColor("#5c3b0c");
    p.Graticule = QColor("#16161a");
    p.Halo = QColor("#4a3006");
    p.Panel = QColor("#080808");
    p.PanelLine = QColor("#2b2b2b");
    p.Hover = QColor("#161616");
    p.Active = QColor("#0e2a55");
    p.Text = QColor("#ffa028");
    p.TextDim = QColor("#c27d22");
    p.TextSection = QColor("#e8e8e8");
    p.TextFaint = QColor("#7d5418");
    p.Accent = QColor("#ffffff");
    p.Up = QColor("#3ddc6a");
    p.Down = QColor("#ff433d");
    p.ControlBorder = QColor("#333333");
    p.ControlBorderHover = QColor("#8a5a14");
    p.Tooltip = QColor("#101010");
    p.TooltipBorder = QColor("#5c3b0c");
    p.ClusterFill = QColor("#000000");
    p.LimbShade = QColor(0, 0, 0, 150);
    // The default energy and precious colors are too close to the amber text: hotter orange, pure gold,
    // and cooler tones for the rest.
    p.categoryColors = {
        {"energy", QColor("#ff6a1a")},
        {"precious", QColor("#ffd400")},
        {"industrial", QColor("#9fb3c8")},
        {"agri", QColor("#b48cff")},
        {"softs", QColor("#35c2d6")},
        {"hub", QColor("#ffffff")},
    };
    return p;
}

// Light theme: ice-white land over pale glacial water, deep blue accent.
Palette glacier()
{
    Palette p;
    p.id = "glacier";
    p.name = "Glacier";
    p.Background = QColor("#eef3f6");
    p.GridLine = QColor("#dfe7ed");
    p.OceanInner = QColor("#d9e6ef");
    p.OceanOuter = QColor("#bed0de");
    p.Land = QColor("#f9fbfc");
    p.LandBorder = QColor("#b4c6d3");
    p.Graticule = QColor("#c6d5e0");
    p.Halo = QColor("#9cc2de");
    p.Panel = QColor("#f7fafc");
    p.PanelLine = QColor("#d8e2ea");
    p.Hover = QColor("#e7eff5");
    p.Active = QColor("#d9e8f3");
    p.Text = QColor("#1c2a35");
    p.TextDim = QColor("#5d7181");
    p.TextSection = QColor("#788c9a");
    p.TextFaint = QColor("#98a9b5");
    p.Accent = QColor("#2b7fc0");
    p.Up = QColor("#18916b");
    p.Down = QColor("#cf4f34");
    p.ControlBorder = QColor("#cdd9e2");
    p.ControlBorderHover = QColor("#a6bccb");
    p.Tooltip = QColor("#ffffff");
    p.TooltipBorder = QColor("#c2d1db");
    p.ClusterFill = QColor("#ffffff");
    p.LimbShade = QColor(30, 60, 90, 55);
    // Deeper, more saturated category colors so markers read on a light map.
    p.categoryColors = {
        {"energy", QColor("#d2602e")},
        {"precious", QColor("#c4911a")},
        {"industrial", QColor("#9a6a42")},
        {"agri", QColor("#7558b3")},
        {"softs", QColor("#2f9677")},
        {"hub", QColor("#34475a")},
    };
    return p;
}

// Light theme: pale sand land over a darker, dune-shadow sea, beige panels, ochre accent.
Palette dune()
{
    Palette p;
    p.id = "dune";
    p.name = "Dune";
    p.Background = QColor("#f4ecdc");
    p.GridLine = QColor("#e8dcc6");
    p.OceanInner = QColor("#e9dcc2");
    p.OceanOuter = QColor("#d6c29f");
    p.Land = QColor("#fcf6ea");
    p.LandBorder = QColor("#cdb48c");
    p.Graticule = QColor("#dccaa8");
    p.Halo = QColor("#e0a868");
    p.Panel = QColor("#f9f3e7");
    p.PanelLine = QColor("#e6d8bf");
    p.Hover = QColor("#f1e6d1");
    p.Active = QColor("#ecdcbd");
    p.Text = QColor("#3a2a1a");
    p.TextDim = QColor("#7a6550");
    p.TextSection = QColor("#957d63");
    p.TextFaint = QColor("#b29d84");
    p.Accent = QColor("#c4661f");
    p.Up = QColor("#3d8b5e");
    p.Down = QColor("#b8402c");
    p.ControlBorder = QColor("#e0cfb2");
    p.ControlBorderHover = QColor("#c9ae84");
    p.Tooltip = QColor("#fffaf1");
    p.TooltipBorder = QColor("#dcc7a4");
    p.ClusterFill = QColor("#fffaf1");
    p.LimbShade = QColor(110, 70, 30, 55);
    // Markers must stand out from sand and from the ochre accent: deeper reds, steel and teal
    // rather than browns.
    p.categoryColors = {
        {"energy", QColor("#c9452b")},
        {"precious", QColor("#b8860b")},
        {"industrial", QColor("#5b6f80")},
        {"agri", QColor("#7a4fa8")},
        {"softs", QColor("#2a8a74")},
        {"hub", QColor("#3c3a4a")},
    };
    return p;
}

// Light theme: petal-white land over pale rose water, blush panels, cherry accent.
Palette sakura()
{
    Palette p;
    p.id = "sakura";
    p.name = "Sakura";
    p.Background = QColor("#f9eef1");
    p.GridLine = QColor("#efdde3");
    p.OceanInner = QColor("#f1dbe2");
    p.OceanOuter = QColor("#dfbfc9");
    p.Land = QColor("#fdf7f8");
    p.LandBorder = QColor("#d3a8b6");
    p.Graticule = QColor("#e4c6cf");
    p.Halo = QColor("#f0a3b8");
    p.Panel = QColor("#fcf5f7");
    p.PanelLine = QColor("#ecd6dd");
    p.Hover = QColor("#f6e4e9");
    p.Active = QColor("#f1d4dc");
    p.Text = QColor("#3b2430");
    p.TextDim = QColor("#7a5a67");
    p.TextSection = QColor("#997583");
    p.TextFaint = QColor("#b899a4");
    p.Accent = QColor("#cf3f72");
    p.Up = QColor("#2e9466");
    p.Down = QColor("#c23b2e");
    p.ControlBorder = QColor("#e8cfd7");
    p.ControlBorderHover = QColor("#d1a7b5");
    p.Tooltip = QColor("#fffafb");
    p.TooltipBorder = QColor("#e0c2cc");
    p.ClusterFill = QColor("#fffafb");
    p.LimbShade = QColor(120, 50, 80, 55);
    // Markers must read on pink and apart from the cherry accent: no reds or pinks among them.
    p.categoryColors = {
        {"energy", QColor("#d1602a")},
        {"precious", QColor("#c4911a")},
        {"industrial", QColor("#5f6b86")},
        {"agri", QColor("#7a4fa8")},
        {"softs", QColor("#2a8a74")},
        {"hub", QColor("#4a3a4a")},
    };
    return p;
}

int &currentIndex()
{
    static int index = 0;
    return index;
}

} // namespace

const QVector<Palette> &palettes()
{
    static const QVector<Palette> all {ember(), abyss(), jade(), oxide(), nova(), bloomberg(), glacier(), dune(), sakura()};
    return all;
}

const Palette &current()
{
    return palettes().at(currentIndex());
}

void setCurrent(const QString &id)
{
    for (int i = 0; i < palettes().size(); ++i)
        if (palettes()[i].id == id)
            currentIndex() = i;
}

bool isLight(const Palette &p)
{
    return p.Background.lightnessF() > 0.5;
}

QHash<QString, QString> chartVars(const Palette &p)
{
    const bool light = isLight(p);
    const QColor a = p.Accent;
    const QString soft = QString("rgba(%1, %2, %3, %4)").arg(a.red()).arg(a.green()).arg(a.blue()).arg(light ? 0.12 : 0.18);
    // --gold / --gold-soft (the "Quant" box) keep the page's own values; the text on --accent
    // and the tester's short markers are worked out from --accent by the page.
    return {
        {"--bg", p.Background.name()},
        {"--panel", p.Panel.name()},
        {"--panel-2", p.Active.name()},
        {"--border", p.PanelLine.name()},
        {"--text", p.Text.name()},
        {"--muted", p.TextDim.name()},
        {"--accent", a.name()},
        {"--accent-soft", soft},
        {"--up", p.Up.name()},
        {"--down", p.Down.name()},
        {"--hover", p.Hover.name()},
        {"--grid", p.GridLine.name()},
        {"--input", p.Background.name()},
        {"--scroll", p.ControlBorderHover.name()},
        {"--tooltip", p.Tooltip.name()},
        {"--tooltip-border", p.TooltipBorder.name()},
        {"--crosshair", p.TextDim.name()},
        {"--crosshair-label", p.ControlBorderHover.name()},
    };
}

QString styleSheet(const QString &tmpl)
{
    const Palette &p = current();
    // Longer names first so e.g. @TextDim is not clobbered by @Text.
    const QList<QPair<QString, QColor>> tokens {
        {"ControlBorderHover", p.ControlBorderHover}, {"TooltipBorder", p.TooltipBorder},
        {"ControlBorder", p.ControlBorder}, {"TextSection", p.TextSection},
        {"ClusterFill", p.ClusterFill}, {"Background", p.Background},
        {"PanelLine", p.PanelLine}, {"TextFaint", p.TextFaint}, {"TextDim", p.TextDim},
        {"Tooltip", p.Tooltip}, {"Accent", p.Accent}, {"Active", p.Active},
        {"Hover", p.Hover}, {"Panel", p.Panel}, {"Text", p.Text}, {"Down", p.Down},
        {"Up", p.Up},
    };
    QStringList mono;
    for (const QString &f : monoFamilies())
        mono << '"' + f + '"';
    QString out = tmpl;
    out.replace("@MonoFamilies", mono.join(", "));
    for (const auto &[name, color] : tokens)
        out.replace('@' + name, color.name());
    return out;
}

QStringList installedFamilies(const QStringList &wanted)
{
    static const QSet<QString> installed = [] {
        QSet<QString> out;
        for (const QString &f : QFontDatabase::families())
            out.insert(f.toLower());
        return out;
    }();
    QStringList out;
    for (const QString &f : wanted)
        if (installed.contains(f.toLower()))
            out << f;
    return out.isEmpty() ? wanted : out;
}

const QStringList &monoFamilies()
{
    static const QStringList families = installedFamilies({"SF Mono", "Menlo", "Monaco"});
    return families;
}

} // namespace Theme
