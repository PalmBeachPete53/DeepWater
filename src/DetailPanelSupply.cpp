// The supply chain sheets of the map page's left sidebar: the overview shown with the globe's supply chain layer,
// and the sheets of its ports, oil storage sites and disruptions (README › Supply chains).

#include "DetailPanel.h"

#include "MacroData.h"
#include "MacroIndicators.h"
#include "MacroPanel.h"
#include "MarketData.h"
#include "SupplyChain.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using supply::SupplyChain;

namespace {

QLabel *heading(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setObjectName("sectionLabel");
    l->setWordWrap(true);   // long headings would otherwise widen the sidebar
    return l;
}

QLabel *paragraph(const QString &html, const char *object = "dim")
{
    auto *l = new QLabel(html);
    l->setObjectName(object);
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    l->setOpenExternalLinks(true);
    return l;
}

QString signedPct(double v, int decimals = 1)
{
    if (!std::isfinite(v))
        return QString::fromUtf8("—");
    const QString num = QLocale().toString(std::abs(v), 'f', decimals);
    return QCoreApplication::translate("MacroIndicators", "%1%")
        .arg((v > 0 ? "+" : v < 0 ? QString::fromUtf8("−") : QString()) + num);
}

QString colored(double v, const QString &text)
{
    if (!std::isfinite(v))
        return text;
    return QString("<span style='color:%1'>%2</span>")
        .arg((v >= 0 ? Theme::current().Up : Theme::current().Down).name(), text);
}

// Tonnes, as "1.2 Mt" or "350 kt".
QString tonnes(double t)
{
    const QLocale loc;
    if (t >= 1e6)
        return QCoreApplication::translate("DetailPanel", "%1 Mt").arg(loc.toString(t / 1e6, 'f', 1));
    if (t >= 1e3)
        return QCoreApplication::translate("DetailPanel", "%1 kt").arg(loc.toString(t / 1e3, 'f', 0));
    return QCoreApplication::translate("DetailPanel", "%1 t").arg(loc.toString(t, 'f', 0));
}

// Million barrels from thousand barrels.
QString barrels(double kbbl)
{
    return QCoreApplication::translate("DetailPanel", "%1 million barrels")
        .arg(QLocale().toString(kbbl / 1000, 'f', kbbl >= 100000 ? 0 : 1));
}

// A dated series as the chart's fractional years.
QMap<double, double> toYears(const supply::DatedSeries &s, const QDate &since)
{
    QMap<double, double> out;
    for (auto it = s.begin(); it != s.end(); ++it)
        if (it.key() >= since)
            out.insert(it.key().year() + (it.key().dayOfYear() - 0.5) / it.key().daysInYear(), it.value());
    return out;
}

QPixmap dotIcon(const QColor &color, bool diamond = false, bool ring = false)
{
    QPixmap pm(20, 20);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    if (ring) {
        p.setPen(QPen(color, 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(5, 5), 3.6, 3.6);
    } else if (diamond) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(QPolygonF {QPointF(5, 0.5), QPointF(9, 5), QPointF(5, 9.5), QPointF(1, 5)});
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(5, 5), 4, 4);
    }
    return pm;
}

QString period(const supply::PortActivity::Disruption &d)
{
    const QLocale loc;
    if (!d.to.isValid())
        return QCoreApplication::translate("DetailPanel", "since %1").arg(loc.toString(d.from, QLocale::ShortFormat));
    if (d.to == d.from)
        return loc.toString(d.from, QLocale::ShortFormat);
    return QString("%1 – %2").arg(loc.toString(d.from, QLocale::ShortFormat), loc.toString(d.to, QLocale::ShortFormat));
}

} // namespace

// The country of a port in the interface's language (PortWatch writes it in English).
static QString portCountry(MacroIndicators *macro, const supply::PortActivity::Port &p)
{
    const QString name = macro->economy()->displayName(p.iso3);
    return name.isEmpty() || name == p.iso3 ? p.country : name;
}

QPushButton *DetailPanel::supplyLink(const QString &text, const QString &key)
{
    auto *b = new QPushButton(text);
    b->setObjectName("siteButton");
    b->setCursor(Qt::PointingHandCursor);
    b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    b->setMinimumWidth(0);
    connect(b, &QPushButton::clicked, this, [this, key] { emit supplyPointActivated(key); });
    return b;
}

void DetailPanel::showSupplyOverview()
{
    clear();
    m_mode = Mode::SupplyOverview;
    m_supplyKey.clear();
    SupplyChain *sc = m_macro->supplyChain();
    if (!sc)
        return;
    const QLocale loc;
    const QString dim = Theme::current().TextDim.name();
    addHeader(tr("Supply chains"), Theme::current().Accent, tr("Global supply chains"),
              tr("Ports, disruptions, oil storage and freight. Click a point on the globe for its details; the "
                 "Supply chains indicators on the right color the countries."));

    // Legend of the layer.
    {
        auto *legend = new QVBoxLayout;
        legend->setSpacing(2);
        auto row = [&](const QPixmap &icon, const QString &text) {
            auto *h = new QHBoxLayout;
            auto *i = new QLabel;
            i->setPixmap(icon);
            i->setFixedWidth(14);
            h->addWidget(i);
            auto *t = new QLabel(text);
            t->setObjectName("dim");
            t->setWordWrap(true);
            h->addWidget(t, 1);
            legend->addLayout(h);
        };
        row(dotIcon(Theme::current().Up), tr("Ports: more port calls over the last 7 days than a year before"));
        row(dotIcon(Theme::current().Down), tr("Ports: fewer port calls (full color at ±40%; grey: no figures)"));
        row(dotIcon(m_data->categoryColor("energy"), true), tr("Oil storage sites, terminals and strategic reserves"));
        row(dotIcon(SupplyChain::alertColor("RED"), false, true), tr("Disruptions (cyclones, earthquakes, floods, conflicts)"));
        m_layout->addLayout(legend);
        m_layout->addWidget(paragraph(tr("Small ports appear as you zoom in.")));
    }

    // The New York Fed's pressure index.
    m_layout->addSpacing(8);
    m_layout->addWidget(heading(tr("Supply chain pressure · New York Fed")));
    const supply::DatedSeries gscpi = sc->pressure()->values();
    if (gscpi.isEmpty()) {
        const QString e = sc->pressure()->errorString();
        m_layout->addWidget(paragraph(e.isEmpty() ? tr("Loading…") : tr("Unavailable: %1").arg(e.toHtmlEscaped())));
    } else {
        const double last = gscpi.last();
        const double before = gscpi.size() > 1 ? std::prev(gscpi.end(), 2).value() : last;
        const double yearAgo = gscpi.value(gscpi.lastKey().addYears(-1), std::nan(""));
        const QString reading = last >= 1.0    ? tr("well above its average: strained supply chains")
                                : last >= 0.3  ? tr("above its average")
                                : last > -0.3  ? tr("near its average")
                                : last > -1.0  ? tr("below its average: supply chains running smoothly")
                                               : tr("well below its average");
        m_layout->addWidget(paragraph(
            tr("<span style='font-size:17px; font-weight:600'>%1</span> in %2, %3 (%4 the month before, %5 a year "
               "before).")
                .arg(loc.toString(last, 'f', 2), loc.toString(gscpi.lastKey(), "MMMM yyyy"), reading,
                     loc.toString(before, 'f', 2),
                     std::isfinite(yearAgo) ? loc.toString(yearAgo, 'f', 2) : QString::fromUtf8("—")),
            "stats"));
        auto *chart = new TimeSeriesChart;
        chart->setMinimumHeight(130);
        chart->setLines({{tr("Pressure index"), Theme::current().Accent, toYears(gscpi, QDate(2017, 1, 1))}},
                        [](double v) { return QLocale().toString(v, 'f', 1); }, 0);
        m_layout->addWidget(chart);
        m_layout->addWidget(paragraph(
            tr("Standard deviations from the average since 1997, built from shipping and air freight costs and the "
               "delivery times, backlogs and purchased stocks of manufacturing surveys in seven economies.")));
    }

    // Port calls over the window against a year before.
    m_layout->addSpacing(8);
    supply::PortActivity *pa = sc->ports();
    m_layout->addWidget(heading(tr("Port calls · last 7 days against a year before")));
    if (pa->ports().isEmpty()) {
        const QString e = pa->errorString();
        m_layout->addWidget(paragraph(pa->isFetching() ? tr("Loading the PortWatch ports…")
                                      : e.isEmpty()   ? tr("Loading…")
                                                      : tr("Unavailable: %1").arg(e.toHtmlEscaped())));
    } else if (pa->windowPairs() == 0) {
        m_layout->addWidget(paragraph(pa->isFetching() ? tr("Downloading the port calls, one day at a time (PortWatch answers slowly)…")
                                                       : tr("No port calls yet: %1").arg(pa->errorString().toHtmlEscaped())));
    } else {
        double calls = 0, callsBefore = 0, cargo = 0, cargoBefore = 0;
        QVector<const supply::PortActivity::Port *> big;
        for (const auto &p : pa->ports()) {
            if (p.calls < 0 || p.callsYearAgo <= 0)
                continue;
            calls += p.calls;
            callsBefore += p.callsYearAgo;
            cargo += p.imports + p.exports;
            cargoBefore += p.importsYearAgo + p.exportsYearAgo;
            if (p.callsYearAgo >= 40)
                big.push_back(&p);
        }
        const double cc = SupplyChain::changePct(calls, callsBefore), cg = SupplyChain::changePct(cargo, cargoBefore);
        const QDate end = pa->windowEnd();
        m_layout->addWidget(paragraph(
            tr("Week to %1, the ports that reported both weeks: <b>%2</b> port calls (%3), <b>%4</b> of cargo loaded "
               "and unloaded (%5).")
                .arg(loc.toString(end, QLocale::ShortFormat), loc.toString(calls, 'f', 0),
                     colored(cc, signedPct(cc)), tonnes(cargo), colored(cg, signedPct(cg))),
            "stats"));
        if (pa->windowPairs() < supply::PortActivity::WindowDays)
            m_layout->addWidget(paragraph(tr("%1 of the 7 days so far; PortWatch answers slowly, the others follow.")
                                              .arg(pa->windowPairs())));
        std::sort(big.begin(), big.end(), [](auto *a, auto *b) {
            return double(a->calls) / a->callsYearAgo < double(b->calls) / b->callsYearAgo;
        });
        auto list = [&](const QString &title, bool falls) {
            m_layout->addWidget(paragraph(QString("<span style='color:%1'>%2</span>").arg(dim, title)));
            const int n = std::min<int>(6, big.size());
            for (int i = 0; i < n; ++i) {
                const auto *p = falls ? big[i] : big[big.size() - 1 - i];
                const double c = SupplyChain::changePct(p->calls, p->callsYearAgo);
                auto *b = supplyLink(tr("%1 · %2\n%3 port calls, %4 a year before (%5)")
                                         .arg(p->name, portCountry(m_macro, *p))
                                         .arg(p->calls)
                                         .arg(p->callsYearAgo)
                                         .arg(signedPct(c, 0)),
                                     "port:" + p->id);
                b->setIcon(QIcon(dotIcon(SupplyChain::portColor(*p))));
                m_layout->addWidget(b);
            }
        };
        if (!big.isEmpty()) {
            list(tr("Largest falls among busy ports"), true);
            list(tr("Largest rises"), false);
        }
    }

    // Disruptions.
    m_layout->addSpacing(8);
    const QVector<supply::PortActivity::Disruption> events = pa->recentDisruptions(30);
    m_layout->addWidget(heading(tr("Disruptions · last 30 days and ongoing · %1").arg(events.size())));
    if (events.isEmpty())
        m_layout->addWidget(paragraph(tr("None reported.")));
    for (const auto &d : events) {
        auto *b = supplyLink(QString("%1\n%2 · %3 · %4")
                                 .arg(d.name, SupplyChain::disruptionTypeName(d.type), period(d),
                                      tr("%n port(s) affected", nullptr, int(d.ports.size()))),
                             "event:" + QString::number(d.id));
        b->setIcon(QIcon(dotIcon(SupplyChain::alertColor(d.alert), false, true)));
        m_layout->addWidget(b);
    }

    // US freight.
    m_layout->addSpacing(8);
    m_layout->addWidget(heading(tr("US freight · FRED")));
    supply::FreightIndices *fi = sc->freight();
    if (!fi->hasData()) {
        const QString e = fi->errorString();
        m_layout->addWidget(paragraph(e.isEmpty() ? tr("Loading…") : e.toHtmlEscaped()));
    } else {
        QString html = "<table width='100%'>";
        for (const auto &info : supply::FreightIndices::infos()) {
            const supply::DatedSeries s = fi->series(info.id);
            if (s.isEmpty())
                continue;
            const double yearAgo = s.value(s.lastKey().addYears(-1), std::nan(""));
            const double c = SupplyChain::changePct(s.last(), yearAgo);
            html += QString("<tr><td style='padding:3px 0' title='%5'>%1<br><span style='color:%2'>%3</span></td>"
                            "<td align='right' style='padding:3px 0 3px 10px; white-space:nowrap'>%4<br>%6</td></tr>")
                        .arg(info.name.toHtmlEscaped(), dim, loc.toString(s.lastKey(), "MMM yyyy"),
                             loc.toString(s.last(), 'f', 1), info.note.toHtmlEscaped(),
                             colored(c, tr("%1 on a year").arg(signedPct(c))));
        }
        html += "</table>";
        m_layout->addWidget(paragraph(html, "stats"));
    }

    // US crude stocks.
    m_layout->addSpacing(8);
    m_layout->addWidget(heading(tr("US crude stocks · EIA weekly")));
    supply::EiaOilStocks *eia = sc->eia();
    if (!eia->hasData()) {
        const QString e = eia->errorString();
        m_layout->addWidget(paragraph(e.isEmpty() ? tr("Loading…") : e.toHtmlEscaped()));
    } else {
        const QStringList names {tr("Cushing, Oklahoma"), tr("Gulf Coast (PADD 3)"), tr("Strategic Petroleum Reserve"),
                                 tr("Commercial stocks, United States")};
        QString html = "<table width='100%'>";
        const QStringList ids = supply::EiaOilStocks::seriesIds();
        for (int i = 0; i < ids.size(); ++i) {
            const supply::DatedSeries s = eia->series(ids[i]);
            if (s.size() < 2)
                continue;
            const double week = s.last() - std::prev(s.end(), 2).value();
            html += QString("<tr><td style='padding:3px 0'>%1<br><span style='color:%2'>%3</span></td>"
                            "<td align='right' style='padding:3px 0 3px 10px; white-space:nowrap'>%4<br>%5</td></tr>")
                        .arg(names.value(i), dim, loc.toString(s.lastKey(), QLocale::ShortFormat), barrels(s.last()),
                             colored(week, tr("%1 kb on the week").arg((week > 0 ? "+" : "") + loc.toString(week, 'f', 0))));
        }
        html += "</table>";
        m_layout->addWidget(paragraph(html, "stats"));
    }

    m_layout->addSpacing(8);
    m_layout->addWidget(paragraph(tr("Sources: IMF PortWatch (ports, port calls from satellite AIS, disruptions from "
                                     "GDACS and other alerts), Federal Reserve Bank of New York, FRED, US Energy "
                                     "Information Administration.")));
    m_layout->addStretch();
}

void DetailPanel::showSupplyPoint(const QString &key)
{
    clear();
    m_mode = Mode::SupplyPoint;
    m_supplyKey = key;
    if (!m_macro->supplyChain())
        return;
    const int colon = key.indexOf(':');
    const QString kind = key.left(colon), id = key.mid(colon + 1);
    if (kind == "port")
        addPortSheet(id);
    else if (kind == "oil")
        addOilSiteSheet(id);
    else if (kind == "event")
        addDisruptionSheet(id.toInt());
    m_layout->addStretch();
}

void DetailPanel::addPortSheet(const QString &id)
{
    SupplyChain *sc = m_macro->supplyChain();
    const supply::PortActivity::Port *p = sc->ports()->port(id);
    if (!p) {
        addHeader(tr("Port"), Theme::current().Accent, tr("Unknown port"), QString());
        return;
    }
    const QLocale loc;
    const QString dim = Theme::current().TextDim.name();
    addHeader(tr("Port"), SupplyChain::portColor(*p), p->name,
              p->locode.isEmpty() ? portCountry(m_macro, *p) : QString("%1 · %2").arg(portCountry(m_macro, *p), p->locode));

    const QDate end = sc->ports()->windowEnd();
    if (p->calls >= 0 && end.isValid()) {
        const double cc = SupplyChain::changePct(p->calls, p->callsYearAgo);
        const double ci = SupplyChain::changePct(p->imports, p->importsYearAgo);
        const double ce = SupplyChain::changePct(p->exports, p->exportsYearAgo);
        auto cell = [&](const QString &k, const QString &v, const QString &sub) {
            return QString("<td style='padding:2px 16px 2px 0'><span style='color:%1'>%2</span><br>"
                           "<span style='font-size:17px; font-weight:600'>%3</span><br>%4</td>")
                .arg(dim, k, v, sub);
        };
        m_layout->addWidget(paragraph(
            tr("Seven days to %1, against the same weekdays a year before:").arg(loc.toString(end, QLocale::LongFormat))));
        m_layout->addWidget(paragraph("<table><tr>" + cell(tr("Port calls"), QString::number(p->calls), colored(cc, signedPct(cc, 0)))
                                          + cell(tr("Unloaded"), tonnes(p->imports), colored(ci, signedPct(ci, 0)))
                                          + cell(tr("Loaded"), tonnes(p->exports), colored(ce, signedPct(ce, 0)))
                                          + "</tr></table>",
                                      "stats"));
    } else {
        m_layout->addWidget(paragraph(tr("No port calls reported over the last seven days published.")));
    }

    m_layout->addSpacing(8);
    m_layout->addWidget(heading(tr("Profile")));
    QString html = tr("%1 vessels counted by PortWatch: %2 container ships, %3 dry bulk carriers, %4 tankers.")
                       .arg(loc.toString(p->vessels), loc.toString(p->container), loc.toString(p->dryBulk),
                            loc.toString(p->tanker));
    if (p->shareImports > 0 || p->shareExports > 0)
        html += "<br>" + tr("%1% of %2's seaborne imports and %3% of its exports go through the port.")
                             .arg(loc.toString(p->shareImports, 'f', 1), portCountry(m_macro, *p).toHtmlEscaped(),
                                  loc.toString(p->shareExports, 'f', 1));
    if (!p->industries.isEmpty())
        html += "<br>" + tr("Main industries served: %1.").arg(p->industries.join(", ").toHtmlEscaped());
    m_layout->addWidget(paragraph(html, "stats"));

    // Disruptions that named the port.
    QVector<supply::PortActivity::Disruption> hits;
    for (const auto &d : sc->ports()->disruptions())
        if (d.ports.contains(p->name, Qt::CaseInsensitive))
            hits.push_back(d);
    std::sort(hits.begin(), hits.end(), [](const auto &a, const auto &b) { return a.from > b.from; });
    if (!hits.isEmpty()) {
        m_layout->addSpacing(8);
        m_layout->addWidget(heading(tr("Disruptions that hit the port · %1").arg(hits.size())));
        for (int i = 0; i < hits.size() && i < 8; ++i) {
            auto *b = supplyLink(QString("%1\n%2 · %3").arg(hits[i].name, SupplyChain::disruptionTypeName(hits[i].type),
                                                           period(hits[i])),
                                 "event:" + QString::number(hits[i].id));
            b->setIcon(QIcon(dotIcon(SupplyChain::alertColor(hits[i].alert), false, true)));
            m_layout->addWidget(b);
        }
    }

    auto *country = new QPushButton(tr("Open the country sheet"));
    country->setObjectName("linkButton");
    country->setCursor(Qt::PointingHandCursor);
    connect(country, &QPushButton::clicked, this, [this, iso = p->iso3] { emit countryActivated(iso); });
    m_layout->addSpacing(6);
    m_layout->addWidget(country, 0, Qt::AlignLeft);
    m_layout->addSpacing(6);
    m_layout->addWidget(paragraph(tr("Source: IMF PortWatch, port calls and cargo estimated from satellite AIS, "
                                     "published weekly a few days behind.")));
}

void DetailPanel::addOilSiteSheet(const QString &id)
{
    SupplyChain *sc = m_macro->supplyChain();
    const supply::OilSite *s = sc->oilSite(id);
    if (!s)
        return;
    const QLocale loc;
    const QString countryName = m_macro->economy()->displayName(s->iso3);
    addHeader(SupplyChain::siteKindName(s->kind), m_data->categoryColor("energy"), QCoreApplication::translate("OilSites", s->name.toUtf8().constData()),
              countryName);
    m_layout->addWidget(paragraph(QCoreApplication::translate("OilSites", s->about.toUtf8().constData()), "stats"));

    if (!s->eia.isEmpty()) {
        m_layout->addSpacing(8);
        m_layout->addWidget(heading(tr("Crude stocks · EIA weekly")));
        const supply::DatedSeries series = sc->eia()->series(s->eia);
        if (series.size() < 2) {
            const QString e = sc->eia()->errorString();
            m_layout->addWidget(paragraph(e.isEmpty() ? tr("Loading…") : e.toHtmlEscaped()));
        } else {
            const double week = series.last() - std::prev(series.end(), 2).value();
            const double yearAgo = [&] {
                // The week closest to a year before.
                const auto it = series.lowerBound(series.lastKey().addDays(-364));
                return it == series.end() ? std::nan("") : it.value();
            }();
            const double c = SupplyChain::changePct(series.last(), yearAgo);
            m_layout->addWidget(paragraph(
                tr("<span style='font-size:17px; font-weight:600'>%1</span> in the week to %2: %3 on the week, %4 on "
                   "a year.")
                    .arg(barrels(series.last()), loc.toString(series.lastKey(), QLocale::ShortFormat),
                         colored(week, (week > 0 ? "+" : "") + tr("%1 kb").arg(loc.toString(week, 'f', 0))),
                         colored(c, signedPct(c))),
                "stats"));
            auto *chart = new TimeSeriesChart;
            chart->setMinimumHeight(130);
            chart->setLines({{tr("Crude stocks"), m_data->categoryColor("energy"), toYears(series, QDate(2000, 1, 1))}},
                            [](double v) { return QLocale().toString(v / 1000, 'f', 0); }, std::nan(""));
            m_layout->addWidget(chart);
            m_layout->addWidget(paragraph(s->eia == "WCSSTUS1"
                                              ? tr("Million barrels. The EIA publishes the total of the four reserve sites, "
                                                   "not each one.")
                                              : s->eia == "WCESTP31" ? tr("Million barrels, excluding the strategic reserve: "
                                                                          "the whole Gulf Coast region (PADD 3).")
                                                                     : tr("Million barrels, excluding the strategic reserve.")));
        }
    }

    // The country's stocks reported to JODI.
    m_layout->addSpacing(8);
    m_layout->addWidget(heading(tr("Crude stocks of %1 · JODI").arg(countryName)));
    supply::OilStocks::Latest l;
    if (sc->oil()->latest(s->iso3, &l)) {
        m_layout->addWidget(paragraph(tr("<span style='font-size:17px; font-weight:600'>%1</span> at the end of %2, %3 "
                                         "on a year.")
                                          .arg(barrels(l.value), loc.toString(l.month, "MMMM yyyy"),
                                               colored(l.change, signedPct(l.change))),
                                      "stats"));
        const supply::DatedSeries monthly = sc->oil()->series(s->iso3);
        auto *chart = new TimeSeriesChart;
        chart->setMinimumHeight(120);
        chart->setLines({{tr("Crude stocks"), m_data->categoryColor("energy"), toYears(monthly, QDate(2018, 1, 1))}},
                        [](double v) { return QLocale().toString(v / 1000, 'f', 0); }, std::nan(""));
        m_layout->addWidget(chart);
        m_layout->addWidget(paragraph(tr("Million barrels, the whole country's crude stocks as it reports them, not this "
                                         "site alone.")));
    } else {
        const QString e = sc->oil()->errorString();
        m_layout->addWidget(paragraph(!sc->oil()->hasData() ? (e.isEmpty() ? tr("Loading…") : e.toHtmlEscaped())
                                                            : tr("%1 reports no recent crude stocks to JODI.").arg(countryName)));
    }
    if (s->id == "rotterdam" || s->id == "amsterdam" || s->id == "antwerp" || s->id == "fujairah" || s->id == "singapore")
        m_layout->addWidget(paragraph(tr("Weekly stocks of the ARA, Fujairah and Singapore hubs are published by Insights "
                                         "Global, S&P Global (FEDCom) and Enterprise Singapore, without a free API.")));

    auto *country = new QPushButton(tr("Open the country sheet"));
    country->setObjectName("linkButton");
    country->setCursor(Qt::PointingHandCursor);
    connect(country, &QPushButton::clicked, this, [this, iso = s->iso3] { emit countryActivated(iso); });
    m_layout->addSpacing(6);
    m_layout->addWidget(country, 0, Qt::AlignLeft);
}

void DetailPanel::addDisruptionSheet(int id)
{
    SupplyChain *sc = m_macro->supplyChain();
    const supply::PortActivity::Disruption *d = nullptr;
    for (const auto &e : sc->ports()->disruptions())
        if (e.id == id)
            d = &e;
    if (!d)
        return;
    addHeader(SupplyChain::disruptionTypeName(d->type), SupplyChain::alertColor(d->alert), d->name, d->countries);
    const QString alert = d->alert == "RED"      ? tr("Red alert")
                          : d->alert == "ORANGE" ? tr("Orange alert")
                                                 : tr("Green alert");
    QString html = QString("<b>%1</b> · %2").arg(alert, period(*d));
    if (!d->to.isValid())
        html += " · " + tr("ongoing");
    if (!d->severity.isEmpty())
        html += "<br>" + d->severity.toHtmlEscaped();
    m_layout->addWidget(paragraph(html, "stats"));

    m_layout->addSpacing(8);
    m_layout->addWidget(heading(tr("Ports affected · %1").arg(d->ports.size())));
    if (d->ports.isEmpty())
        m_layout->addWidget(paragraph(tr("No port within the affected area.")));
    for (const QString &name : d->ports) {
        // The port of that name in the database, preferring one in a country the event names.
        const supply::PortActivity::Port *match = nullptr;
        for (const auto &p : sc->ports()->ports())
            if (p.name.compare(name, Qt::CaseInsensitive) == 0
                && (!match || d->countries.contains(p.country, Qt::CaseInsensitive)))
                match = &p;
        if (!match) {
            m_layout->addWidget(paragraph(name.toHtmlEscaped(), "stats"));
            continue;
        }
        const double c = SupplyChain::changePct(match->calls, match->callsYearAgo);
        auto *b = supplyLink(QString("%1 · %2\n%3").arg(match->name, portCountry(m_macro, *match),
                                                        match->calls >= 0 ? tr("%1 port calls over the last 7 days (%2)")
                                                                                .arg(match->calls)
                                                                                .arg(signedPct(c, 0))
                                                                          : tr("no recent port calls")),
                             "port:" + match->id);
        b->setIcon(QIcon(dotIcon(SupplyChain::portColor(*match))));
        m_layout->addWidget(b);
    }
    m_layout->addSpacing(6);
    m_layout->addWidget(paragraph(tr("Source: IMF PortWatch, from the Global Disaster Alert and Coordination System "
                                     "(GDACS) and its own monitoring.")));
}
