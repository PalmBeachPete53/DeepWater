#include "DetailPanel.h"

#include "Chokepoints.h"
#include "DomesticFinance.h"
#include "KeyIndicators.h"
#include "KeyedSources.h"
#include "MacroData.h"
#include "OfficialSources.h"
#include "MacroIndicators.h"
#include "MacroPanel.h"
#include "ChokepointsPage.h"
#include "MarketData.h"
#include "Sparkline.h"
#include "Theme.h"
#include "VesselStore.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDateTime>
#include <QFile>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSet>
#include <QVBoxLayout>

#include <functional>

namespace {

QString formatPrice(double v)
{
    return QLocale().toString(v, 'f', v >= 1000 ? 0 : (v >= 100 ? 1 : (v >= 10 ? 2 : 3)));
}

QString formatChange(const Commodity &c)
{
    const double pct = c.changePct();
    if (std::isnan(pct))
        return QString();
    const double abs = c.price - c.previousClose;
    return QString("%1%2  (%1%3%)")
        .arg(pct >= 0 ? "+" : QString::fromUtf8("−"))
        .arg(formatPrice(std::abs(abs)))
        .arg(QLocale().toString(std::abs(pct), 'f', 2));
}

QString trendStyle(double pct)
{
    return QString("color: %1;").arg((pct >= 0 ? Theme::current().Up : Theme::current().Down).name());
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        else if (QLayout *l = item->layout())
            clearLayout(l);
        delete item;
    }
}

QLabel *sectionLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setObjectName("sectionLabel");
    return l;
}

QFrame *divider()
{
    auto *f = new QFrame;
    f->setObjectName("divider");
    f->setFixedHeight(1);
    return f;
}

// A part of a help popover: rich text, or a widget made when the popover opens (a chart).
struct PopupBlock {
    QString html;
    std::function<QWidget *()> widget;
};

// Popover with rich text (and widgets), closed by a click outside or Escape. Taller than `maxHeight`, it
// scrolls.
class InfoPopup : public QFrame
{
public:
    InfoPopup(const QVector<PopupBlock> &blocks, int width, int maxHeight)
        : QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint)
    {
        setObjectName("infoPopup");
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_TranslucentBackground);   // rounded corners, painted below
        // The click that closes the popover isn't passed on: clicking the button closes it
        // instead of reopening it.
        setAttribute(Qt::WA_NoMouseReplay);
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(16, 14, 16, 16);
        for (const PopupBlock &block : blocks) {
            QWidget *w = block.widget ? block.widget() : nullptr;
            if (!w) {
                auto *label = new QLabel(block.html);
                label->setTextFormat(Qt::RichText);
                label->setWordWrap(true);
                w = label;
            }
            w->setFixedWidth(width);
            layout->addWidget(w);
        }
        const int contentWidth = width + 32;
        content->setFixedSize(contentWidth, layout->totalHeightForWidth(contentWidth));
        auto *scroll = new QScrollArea;
        scroll->setObjectName("infoPopupScroll");
        scroll->setStyleSheet("#infoPopupScroll, #infoPopupScroll > QWidget > QWidget { background: transparent; }");
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(content);
        scroll->setFixedSize(contentWidth, std::min(content->height(), maxHeight));
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(scroll);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &pal = Theme::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(pal.TooltipBorder, 1));
        p.setBrush(pal.Tooltip);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    }
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape)
            close();
        else
            QFrame::keyPressEvent(e);
    }
};

// Circled question mark that opens an InfoPopup below it.
class HelpButton : public QAbstractButton
{
public:
    explicit HelpButton(const QString &html, int size = 20, int popupWidth = 300, QWidget *parent = nullptr)
        : QAbstractButton(parent)
        , m_blocks {{html, {}}}
        , m_popupWidth(popupWidth)
    {
        setFixedSize(size, size);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        connect(this, &QAbstractButton::clicked, this, &HelpButton::showPopup);
    }
    ~HelpButton() override
    {
        delete m_popup;   // while this button can still take its destroyed() signal
    }
    void setBlocks(const QVector<PopupBlock> &blocks) { m_blocks = blocks; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &pal = Theme::current();
        const QColor color = m_popup || underMouse() ? pal.Accent : pal.TextDim;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
        QFont f = font();
        f.setPixelSize(qRound(height() * 0.6));
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, "?");
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }

private:
    void showPopup()
    {
        // Below the button, kept on screen (above it when there's no room below).
        const QRect screen = this->screen()->availableGeometry();
        m_popup = new InfoPopup(m_blocks, m_popupWidth, screen.height() - 16);
        connect(m_popup, &QObject::destroyed, this, [this] { update(); });
        m_popup->adjustSize();
        QPoint pos = mapToGlobal(QPoint(-12, height() + 6));
        pos.setX(std::clamp(pos.x(), screen.left() + 8, screen.right() - m_popup->width() - 8));
        if (pos.y() + m_popup->height() > screen.bottom() - 8)
            pos.setY(std::max(screen.top() + 8, mapToGlobal(QPoint(0, 0)).y() - m_popup->height() - 6));
        m_popup->move(pos);
        m_popup->show();
        update();
    }

    QVector<PopupBlock> m_blocks;
    int m_popupWidth;
    QPointer<InfoPopup> m_popup;
};

// A help popover's text: a title (none for the next part of a popover), then sections with a small heading
// (`html` is rich text).
QString helpHtml(const QString &title, const QVector<QPair<QString, QString>> &sections)
{
    const Palette &pal = Theme::current();
    QString out = QString("<div style='color:%1; font-size:13px'>").arg(pal.Text.name());
    if (!title.isEmpty())
        out += QString("<span style='font-size:15px; font-weight:600'>%1</span>").arg(title.toHtmlEscaped());
    for (const auto &[heading, html] : sections)
        out += heading.isEmpty()
                   ? QString("<p style='margin:12px 0 0 0'>%1</p>").arg(html)
                   : QString("<p style='margin:12px 0 0 0'><span style='color:%1; font-size:10px; font-weight:600'>%2"
                             "</span><br>%3</p>")
                         .arg(pal.TextSection.name(), heading.toUpper(), html);
    return out + "</div>";
}

QString explanationHtml(MacroIndicators::Id id)
{
    const MacroIndicators::Explanation e = MacroIndicators::explanation(id);
    return helpHtml(MacroIndicators::info(id).name,
                    {{DetailPanel::tr("What it measures"), e.what.toHtmlEscaped()},
                     {DetailPanel::tr("How to read it"), e.reading.toHtmlEscaped()},
                     {DetailPanel::tr("Keep in mind"), e.caveat.toHtmlEscaped()}});
}

// A section title and its help button, centered on the title's text (below the label's top padding).
HelpButton *addTitleWithHelp(QVBoxLayout *layout, const QString &text, int popupWidth)
{
    auto *title = sectionLabel(text);
    auto *help = new HelpButton(QString(), 16, popupWidth);
    title->ensurePolished();
    auto *helpColumn = new QVBoxLayout;
    helpColumn->addSpacing(title->contentsMargins().top() + std::max(0, (title->fontMetrics().height() - help->height()) / 2));
    helpColumn->addWidget(help);
    helpColumn->addStretch();
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(2);
    titleRow->addWidget(title);
    titleRow->addLayout(helpColumn);
    titleRow->addStretch(1);
    layout->addLayout(titleRow);
    return help;
}

// A country's sector figures (World Bank), for the help popover of its economic sectors.
struct SectorFigures {
    QString country;   // HTML-escaped
    QString iso3;
    const EconomySectors *sectors = nullptr;   // for the long series; null until the data is there
    int gdpYear = 0, jobsYear = 0, base = 0;   // 0 when there's no figure
    double gdp[3] {}, jobs[3] {}, then[3] {};   // primary, secondary, tertiary, in % (then: in `base`)
    double manufacturing = std::nan("");
};

// A written account of a country's sector history (resources/sector_history.json, the larger economies): a
// summary and dated events, cause and effect, in the language of the interface.
struct SectorStory {
    struct Event {
        int from = 0, to = 0;   // to = 0 for a single year
        QString text;
    };
    QString summary;
    QVector<Event> events;
};

SectorStory sectorStory(const QString &iso3)
{
    static const QJsonObject all = [] {
        QFile file(":/sector_history.json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    }();
    const QString lang = QLocale().language() == QLocale::French ? "fr" : "en";
    const QJsonObject entry = all.value(iso3).toObject();
    SectorStory story;
    story.summary = entry.value("summary").toObject().value(lang).toString();
    for (const QJsonValue &v : entry.value("events").toArray()) {
        const QJsonObject e = v.toObject();
        story.events.push_back({e.value("from").toInt(), e.value("to").toInt(), e.value(lang).toString()});
    }
    return story;
}

// What the long series (since 1960 for GDP, 1991 for jobs) show, and the causes their shape points to: the
// rural exodus, an industrial peak and what fell after it, industrialisation, a secondary sector that swings
// with commodity prices, the rise of services, a sudden break.
struct SectorTrajectory {
    QString line;              // since when, from what to what
    QStringList causes;
};

SectorTrajectory sectorTrajectory(const SectorFigures &f, const QString (&names)[3])
{
    using S = EconomySectors;
    SectorTrajectory t;
    const QLocale loc;
    auto pct = [&](double v) { return DetailPanel::tr("%1%").arg(loc.toString(v, 'f', 1)); };
    auto pts = [&](double v) { return loc.toString(std::abs(v), 'f', 1); };
    const QMap<int, double> g[3] = {f.sectors->series(f.iso3, S::PrimaryGdp), f.sectors->series(f.iso3, S::SecondaryGdp),
                                    f.sectors->series(f.iso3, S::TertiaryGdp)};
    const QMap<int, double> m = f.sectors->series(f.iso3, S::ManufacturingGdp);
    const QMap<int, double> j[3] = {f.sectors->series(f.iso3, S::PrimaryJobs), f.sectors->series(f.iso3, S::SecondaryJobs),
                                    f.sectors->series(f.iso3, S::TertiaryJobs)};
    // The first and last years with all three sectors (the last may be older than the figures shown above,
    // for countries whose accounts stopped).
    int first = 0, last = 0;
    for (auto it = g[0].begin(); it != g[0].end(); ++it)
        if (g[1].contains(it.key()) && g[2].contains(it.key())) {
            first = first ? first : it.key();
            last = it.key();
        }
    QStringList lines;
    if (first && last - first >= 10)
        lines << DetailPanel::tr("Since %1, in share of GDP: %2 %3 → %4, %5 %6 → %7, %8 %9 → %10.")
                     .arg(first)
                     .arg(names[0], pct(g[0][first]), pct(g[0][last]))
                     .arg(names[1], pct(g[1][first]), pct(g[1][last]))
                     .arg(names[2], pct(g[2][first]), pct(g[2][last]));
    int jobsFirst = 0, jobsLast = 0;
    for (auto it = j[0].begin(); it != j[0].end(); ++it)
        if (j[1].contains(it.key()) && j[2].contains(it.key())) {
            jobsFirst = jobsFirst ? jobsFirst : it.key();
            jobsLast = it.key();
        }
    if (jobsFirst && jobsLast - jobsFirst >= 10)
        lines << DetailPanel::tr("Jobs since %1: %2 %3 → %4, %5 %6 → %7, %8 %9 → %10.")
                     .arg(jobsFirst)
                     .arg(names[0], pct(j[0][jobsFirst]), pct(j[0][jobsLast]))
                     .arg(names[1], pct(j[1][jobsFirst]), pct(j[1][jobsLast]))
                     .arg(names[2], pct(j[2][jobsFirst]), pct(j[2][jobsLast]));
    t.line = lines.join("<br>");

    // Rural exodus: farm jobs going to industry or services.
    if (jobsFirst && jobsLast - jobsFirst >= 10) {
        const double lost = j[0][jobsFirst] - j[0][jobsLast];
        if (lost >= 8) {
            const int to = j[1][jobsLast] - j[1][jobsFirst] > j[2][jobsLast] - j[2][jobsFirst] ? 1 : 2;
            t.causes << DetailPanel::tr("<b>Rural exodus</b>: the primary sector lost %1 pts of jobs since %2, mostly to "
                                        "the %3 sector: machines free farm workers and towns pay more.")
                            .arg(pts(lost)).arg(jobsFirst).arg(names[to]);
        }
    }
    if (!first || last - first < 10)
        return t;

    // The secondary sector's peak, and what fell after it.
    int peak = first;
    for (auto it = g[1].begin(); it != g[1].end(); ++it)
        if (it.key() >= first && it.value() > g[1][peak])
            peak = it.key();
    const double fall = g[1][peak] - g[1][last];
    const bool oilLike = m.contains(last) && m[last] < g[1][last] / 2;   // the secondary sector is mostly extraction
    if (peak <= last - 10 && fall >= 5) {
        // A peak in the first years of the series is only where the series starts.
        const QString head = peak <= first + 2
                                 ? DetailPanel::tr("<b>Industry has declined since %1</b>, from %2 of GDP to %3 in %4")
                                       .arg(first).arg(pct(g[1][first]), pct(g[1][last])).arg(last)
                                 : DetailPanel::tr("<b>Industry peaked in %1</b> at %2 of GDP (%3 in %4)")
                                       .arg(peak).arg(pct(g[1][peak]), pct(g[1][last])).arg(last);
        if (m.contains(peak) && m.contains(last)) {
            const double manufacturingFall = m[peak] - m[last];
            // Factories leave rich economies for cheaper ones; in poorer ones, services outgrow them.
            const bool rich = g[2][last] >= 60;
            t.causes << (manufacturingFall >= fall / 2 && rich
                             ? DetailPanel::tr("%1; manufacturing makes most of the fall (%2 pts): competition from "
                                               "lower-cost producers, offshoring, and productivity gains that make goods "
                                               "cheaper.").arg(head, pts(manufacturingFall))
                         : manufacturingFall >= fall / 2
                             ? DetailPanel::tr("%1; manufacturing makes most of the fall (%2 pts), while services grow "
                                               "faster, before the country is rich.").arg(head, pts(manufacturingFall))
                             : DetailPanel::tr("%1; most of the fall is outside manufacturing: mining, oil, energy or "
                                               "construction, which follow commodity prices and building cycles.").arg(head));
        } else {
            t.causes << head + ".";
        }
    } else if (g[1][last] - g[1][first] >= 5 && peak >= last - 5) {
        // Industrialisation, led by factories or by extraction.
        int mFirst = 0;
        for (auto it = m.begin(); it != m.end() && !mFirst; ++it)
            if (it.key() >= first)
                mFirst = it.key();
        const double gained = g[1][last] - g[1][first];
        if (mFirst && m.contains(last) && m[last] - m[mFirst] >= 3)
            t.causes << DetailPanel::tr("<b>Industrialisation</b>: the secondary sector gained %1 pts of GDP since %2, led by "
                                        "manufacturing (+%3 pts since %4): factories for export and the home market.")
                            .arg(pts(gained)).arg(first).arg(pts(m[last] - m[mFirst])).arg(mFirst);
        else
            t.causes << DetailPanel::tr("<b>A growing secondary sector</b>: +%1 pts of GDP since %2 with little manufacturing: "
                                        "mining, oil and gas or construction.").arg(pts(gained)).arg(first);
    }

    // A secondary sector that swings from year to year with oil and mineral prices.
    double swing = 0;
    int swings = 0, dropYear = 0;
    double drop = 0;
    for (auto it = g[1].begin(); it != g[1].end(); ++it) {
        if (it.key() <= std::max(first, last - 30) || !g[1].contains(it.key() - 1))
            continue;
        const double d = it.value() - g[1][it.key() - 1];
        swing += std::abs(d);
        ++swings;
        if (d < drop) {
            drop = d;
            dropYear = it.key();
        }
    }
    if (swings >= 10 && swing / swings >= 1.5 && oilLike)
        t.causes << DetailPanel::tr("<b>A volatile secondary sector</b>: its share moves %1 pts a year on average, with oil "
                                    "and mineral prices; its largest fall was in %2 (%3 pts).")
                        .arg(pts(swing / swings)).arg(dropYear).arg(QString::fromUtf8("−") + pts(drop));

    // The rise of services.
    if (g[2][last] - g[2][first] >= 10)
        t.causes << DetailPanel::tr("<b>The rise of services</b>: +%1 pts of GDP since %2. As incomes grow, spending moves "
                                    "to health, education, finance, trade and leisure, and services get dearer relative "
                                    "to goods, whose productivity rises faster.").arg(pts(g[2][last] - g[2][first])).arg(first);

    // A sudden break (unless it's the commodity swing already told).
    int breakYear = 0, breakSector = 0;
    double breakSize = 0;
    for (int k = 0; k < 3; ++k)
        for (auto it = g[k].begin(); it != g[k].end(); ++it)
            if (it.key() > first && g[k].contains(it.key() - 1) && std::abs(it.value() - g[k][it.key() - 1]) > std::abs(breakSize)) {
                breakSize = it.value() - g[k][it.key() - 1];
                breakYear = it.key();
                breakSector = k;
            }
    if (std::abs(breakSize) >= 5 && !(oilLike && breakSector == 1))
        t.causes << DetailPanel::tr("<b>A break in %1</b>: the %2 sector moved %3 pts of GDP in one year, the mark of a "
                                    "crisis, a war, a price shock or a change in how the accounts are drawn up.")
                        .arg(breakYear).arg(names[breakSector], (breakSize > 0 ? "+" : QString::fromUtf8("−")) + pts(breakSize));
    return t;
}

// Households, companies, the government: categorical, apart from the sector colors.
QColor debtColor(int k)
{
    static const char *const dark[] = {"#d98ab5", "#7fc4c9", "#c9b37a"};
    static const char *const light[] = {"#a8457a", "#2f8a91", "#8a6d1f"};
    return QColor((Theme::isLight(Theme::current()) ? light : dark)[std::clamp(k, 0, 2)]);
}

// A quarter or a year, from a fractional year (quarters are mid-quarter): "Q2 2026", "2024".
QString periodLabel(double t)
{
    const int year = int(t);
    if (t == year)
        return QString::number(year);
    return DetailPanel::tr("Q%1 %2").arg(int(std::floor((t - year) * 4)) + 1).arg(year);
}

// A signed percentage or point change: "+4.2%", "−3.0 pts".
QString signedText(double v, const QString &unit)
{
    const QLocale loc;
    const QString sign = v > 0.05 ? "+" : v < -0.05 ? QString::fromUtf8("−") : "";
    return sign + loc.toString(std::abs(v), 'f', 1) + unit;
}

// A country's house prices (BIS, real) and their valuation (OECD), for the help popover of its real estate.
struct PropertyFigures {
    QString country;   // HTML-escaped
    double last = 0;   // time of the latest real price
    double year = std::nan(""), fiveYears = std::nan(""), tenYears = std::nan(""), fromPeak = std::nan("");   // %
    double peakTime = 0;
    double priceToIncome = std::nan(""), priceToRent = std::nan("");   // % of the long-term average
    double householdCreditFiveYears = std::nan("");   // change, pts of GDP
    double householdCredit = std::nan("");             // % of GDP
};

QString valuationWord(double ratio)
{
    return ratio >= 130 ? DetailPanel::tr("very dear")
           : ratio >= 115 ? DetailPanel::tr("dear")
           : ratio >= 90  ? DetailPanel::tr("near its average")
                          : DetailPanel::tr("cheap");
}

QString propertyHelpHtml(const PropertyFigures &f)
{
    const QLocale loc;
    auto pct = [&](double v) { return DetailPanel::tr("%1%").arg(loc.toString(v, 'f', 0)); };
    QVector<QPair<QString, QString>> sections;
    sections.push_back({DetailPanel::tr("What it measures"),
                        DetailPanel::tr("The price of homes across the country, corrected for inflation (real prices, "
                                        "2010 = 100), so that a rise means homes got dearer than everything else. The "
                                        "OECD ratios compare prices with household incomes and with rents, in % of their "
                                        "own long-term average: above 100, housing is dearer than usual.")});
    QStringList reading;
    if (std::isfinite(f.fiveYears) && f.fiveYears >= 30)
        reading << DetailPanel::tr("<b>A boom</b>: real prices rose %1 in five years. Rises that fast often end in a "
                                   "correction, all the more when credit grows with them.").arg(signedText(f.fiveYears, "%"));
    if (std::isfinite(f.fromPeak) && f.fromPeak <= -20)
        reading << DetailPanel::tr("<b>A bust not yet made up</b>: real prices are still %1 below their peak of %2.")
                       .arg(pct(-f.fromPeak), periodLabel(f.peakTime));
    if (std::isfinite(f.year) && f.year <= -5)
        reading << DetailPanel::tr("<b>Falling prices</b>: %1 in a year in real terms, the mark of dearer credit or of "
                                   "a market that ran ahead of incomes.").arg(signedText(f.year, "%"));
    else if (std::isfinite(f.year) && f.year >= 5)
        reading << DetailPanel::tr("<b>Prices rising fast</b>: %1 in a year in real terms.").arg(signedText(f.year, "%"));
    if (std::isfinite(f.priceToIncome) && (f.priceToIncome >= 115 || f.priceToIncome < 90))
        reading << DetailPanel::tr("<b>Valuation</b>: against incomes, housing is %1 (%2 of its long-term average): a "
                                   "home costs %3 years of income than usual.")
                       .arg(valuationWord(f.priceToIncome), pct(f.priceToIncome),
                            f.priceToIncome >= 100 ? DetailPanel::tr("more") : DetailPanel::tr("fewer"));
    if (std::isfinite(f.priceToRent) && f.priceToRent >= 120)
        reading << DetailPanel::tr("Against rents, prices are at %1 of their average: owning yields less than renting "
                                   "out, which buyers accept when they expect prices to keep rising or rates to fall.")
                       .arg(pct(f.priceToRent));
    if (std::isfinite(f.householdCreditFiveYears) && std::abs(f.householdCreditFiveYears) >= 5)
        reading << (f.householdCreditFiveYears > 0
                        ? DetailPanel::tr("<b>Credit</b>: household debt rose %1 pts of GDP in five years, mostly "
                                          "mortgages: borrowing feeds house prices and prices feed borrowing.")
                        : DetailPanel::tr("<b>Credit</b>: household debt fell %1 pts of GDP in five years: households are "
                                          "paying down mortgages, which holds prices back."))
                           .arg(loc.toString(std::abs(f.householdCreditFiveYears), 'f', 1));
    if (reading.isEmpty() && std::isfinite(f.year))
        reading << DetailPanel::tr("<b>A calm market</b>: real prices moved %1 in a year, with no sign of a boom or a bust "
                                   "in the series.").arg(signedText(f.year, "%"));
    if (!reading.isEmpty())
        sections.push_back({DetailPanel::tr("What the figures say · %1").arg(f.country), reading.join("<br><br>")});
    sections.push_back({DetailPanel::tr("How to read it"),
                        DetailPanel::tr("Housing moves in long cycles: credit and low rates push prices up for years, "
                                        "then rising rates or a recession bring them down, slowly, often over several "
                                        "years in real terms. Booms financed by debt end worst, because owners who "
                                        "borrowed heavily cut their spending when prices fall.")});
    sections.push_back({DetailPanel::tr("Keep in mind"),
                        DetailPanel::tr("A national average hides cities and regions that can move apart. Indices are built "
                                        "differently from country to country: compare a country with its own past rather "
                                        "than with others. The OECD ratios measure the same thing: distance from the "
                                        "country's own average.")});
    return helpHtml(DetailPanel::tr("Real estate"), sections);
}

// A country's debt by borrower, for the help popover of its debt.
struct DebtFigures {
    QString country;   // HTML-escaped
    double level[3] = {std::nan(""), std::nan(""), std::nan("")};        // households, companies, government, % of GDP
    double fiveYears[3] = {std::nan(""), std::nan(""), std::nan("")};    // change, pts of GDP
    double tenYears[3] = {std::nan(""), std::nan(""), std::nan("")};
    double householdService = std::nan(""), householdServiceAverage = std::nan("");   // % of income
    double privateService = std::nan(""), privateServiceAverage = std::nan("");
    double badLoans = std::nan(""), privateCredit = std::nan("");
};

QString debtHelpHtml(const DebtFigures &f)
{
    const QLocale loc;
    auto pct = [&](double v) { return DetailPanel::tr("%1%").arg(loc.toString(v, 'f', 0)); };
    auto pts = [&](double v) { return loc.toString(std::abs(v), 'f', 0); };
    const QString who[3] = {DetailPanel::tr("households"), DetailPanel::tr("companies"), DetailPanel::tr("the government")};
    QVector<QPair<QString, QString>> sections;
    sections.push_back({DetailPanel::tr("What it measures"),
                        DetailPanel::tr("What each borrower owes, in % of the country's yearly GDP: households (mostly "
                                        "mortgages), non-financial companies (loans and bonds) and the government. Debt "
                                        "service is the share of income that goes to interest and repayments.")});
    QStringList reading;
    // BIS research (Cecchetti, Mohanty and Zampolli, 2011) finds debt starts to weigh on growth around 85% of GDP for
    // households and governments and 90% for companies.
    const double threshold[3] = {85, 90, 85};
    QStringList high;
    for (int k = 0; k < 3; ++k)
        if (std::isfinite(f.level[k]) && f.level[k] >= threshold[k])
            high << DetailPanel::tr("%1 (%2 of GDP)").arg(who[k], pct(f.level[k]));
    if (!high.isEmpty())
        reading << DetailPanel::tr("<b>Heavy debt</b>: %1. Beyond about 85–90% of GDP, BIS research finds that more debt "
                                   "tends to slow growth: interest crowds out spending and investment.").arg(high.join(", "));
    for (int k = 0; k < 2; ++k)
        if (std::isfinite(f.fiveYears[k]) && f.fiveYears[k] >= 15)
            reading << DetailPanel::tr("<b>Credit boom</b>: the debt of %1 rose %2 pts of GDP in five years. Fast credit "
                                       "growth is the best early sign of banking crises, because loans made in a boom go "
                                       "to weaker borrowers.").arg(who[k], pts(f.fiveYears[k]));
        else if (std::isfinite(f.tenYears[k]) && f.tenYears[k] <= -10)
            reading << DetailPanel::tr("<b>Deleveraging</b>: the debt of %1 fell %2 pts of GDP in ten years, paid down "
                                       "or outgrown by GDP; it frees income but holds back spending while it lasts.")
                           .arg(who[k], pts(f.tenYears[k]));
    if (std::isfinite(f.tenYears[2]) && f.tenYears[2] >= 20)
        reading << DetailPanel::tr("<b>Public debt building up</b>: +%1 pts of GDP in ten years (crises, deficits): "
                                   "the room left for the next downturn shrinks, and the cost rises with rates.")
                       .arg(pts(f.tenYears[2]));
    if (std::isfinite(f.householdService) && std::isfinite(f.householdServiceAverage)
        && f.householdService - f.householdServiceAverage >= 1.5)
        reading << DetailPanel::tr("<b>Strained households</b>: debt service takes %1 of their income, above its usual "
                                   "%2: higher rates or more debt bite into spending.")
                       .arg(DetailPanel::tr("%1%").arg(loc.toString(f.householdService, 'f', 1)),
                            DetailPanel::tr("%1%").arg(loc.toString(f.householdServiceAverage, 'f', 1)));
    if (std::isfinite(f.level[0]) && f.level[0] < 20 && (!std::isfinite(f.privateCredit) || f.privateCredit < 50))
        reading << DetailPanel::tr("<b>Shallow credit</b>: households owe only %1 of GDP. Few borrow to buy a home: "
                                   "homes are bought with savings or built over time, and credit is costly or hard to "
                                   "get.").arg(pct(f.level[0]));
    if (std::isfinite(f.badLoans) && f.badLoans >= 10)
        reading << DetailPanel::tr("<b>Bad loans</b>: %1 of bank loans are not being repaid; banks that carry them lend "
                                   "less.").arg(DetailPanel::tr("%1%").arg(loc.toString(f.badLoans, 'f', 1)));
    if (reading.isEmpty() && (std::isfinite(f.level[0]) || std::isfinite(f.level[2])))
        reading << DetailPanel::tr("<b>Debt within usual bounds</b>: no borrower stands above the levels where debt is "
                                   "found to weigh on growth, and none has borrowed fast lately.");
    if (!reading.isEmpty())
        sections.push_back({DetailPanel::tr("What the figures say · %1").arg(f.country), reading.join("<br><br>")});
    sections.push_back({DetailPanel::tr("How to read it"),
                        DetailPanel::tr("Debt is neither good nor bad in itself: it pays for homes, factories and roads. "
                                        "What matters is its pace (a fast rise is riskier than a high but stable level), "
                                        "who holds it, and the cost of servicing it. Household debt and house prices "
                                        "move together, since most of it is mortgages.")});
    sections.push_back({DetailPanel::tr("Keep in mind"),
                        DetailPanel::tr("These are gross debts, before deducting what borrowers own. Company debt includes "
                                        "loans between companies of the same group and abroad. The BIS and the IMF define "
                                        "debt slightly differently: compare figures from the same source. In % of GDP, "
                                        "a recession raises debt even when nobody borrows more.")});
    return helpHtml(DetailPanel::tr("Debt"), sections);
}

// Why the country's economy looks the way it does: its profile, the output per job of each sector and the
// change over ten years, read from its figures; its history, cause and effect (a written account for the
// larger economies, the long series for all); around the general picture.
QVector<PopupBlock> sectorsHelp(const SectorFigures &f)
{
    const QLocale loc;
    auto pct = [&](double v) { return DetailPanel::tr("%1%").arg(loc.toString(v, 'f', 1)); };
    auto pts = [&](double v) { return loc.toString(std::abs(v), 'f', 1); };
    const QString names[3] = {DetailPanel::tr("primary"), DetailPanel::tr("secondary"), DetailPanel::tr("tertiary")};
    QVector<QPair<QString, QString>> sections;
    sections.push_back({DetailPanel::tr("What it is"),
                        DetailPanel::tr("Primary: working nature (agriculture, forestry, fishing). Secondary: transforming "
                              "(industry, mining, energy, construction). Tertiary: services (trade, transport, finance, "
                              "health, government…). Shares are measured in value added (output less what the sector "
                              "buys from the others) and in jobs.")});

    QStringList reading;
    // Profile: the first that fits.
    if (f.gdpYear && f.gdp[2] >= 65)
        reading << DetailPanel::tr("<b>A service economy</b>: services make %1 of GDP. That is the profile of high-income "
                         "countries: once basic needs are met, spending goes to health, education, finance, trade "
                         "and leisure.").arg(pct(f.gdp[2]));
    else if (f.jobsYear && f.jobs[0] >= 30)
        reading << DetailPanel::tr("<b>A still largely agricultural economy</b>: %1 of jobs are in the primary sector, so "
                         "living standards follow harvests and farm prices closely.").arg(pct(f.jobs[0]));
    else if (f.gdpYear && f.gdp[1] >= 35 && std::isfinite(f.manufacturing) && f.manufacturing < f.gdp[1] / 2)
        reading << DetailPanel::tr("<b>An extractive economy</b>: the secondary sector makes %1 of GDP but manufacturing "
                         "only %2; the rest comes mostly from oil, gas or mines, so income follows commodity "
                         "prices.").arg(pct(f.gdp[1]), pct(f.manufacturing));
    else if (f.gdpYear && std::isfinite(f.manufacturing) && f.manufacturing >= 20)
        reading << DetailPanel::tr("<b>An industrial economy</b>: manufacturing makes %1 of GDP, among the highest shares in "
                         "the world, so the country depends on world demand for goods and on its exports.")
                       .arg(pct(f.manufacturing));
    else if (f.gdpYear && (f.jobsYear ? f.jobs[0] < 10 : f.gdp[0] < 5))
        reading << DetailPanel::tr("<b>A mature, diversified economy</b>: services make %1 of GDP, industry still holds "
                                   "%2 and the primary sector only %3; growth rests on both domestic spending and "
                                   "exports.").arg(pct(f.gdp[2]), pct(f.gdp[1]), pct(f.gdp[0]));
    else if (f.gdpYear)
        reading << DetailPanel::tr("<b>An economy in transition</b>: services make %1 of GDP, the secondary sector %2 and the "
                         "primary %3; workers keep moving from farming to industry and services.")
                       .arg(pct(f.gdp[2]), pct(f.gdp[1]), pct(f.gdp[0]));

    // Output per job of each sector against the country's average: share of GDP / share of jobs.
    if (f.gdpYear && f.jobsYear) {
        int low = -1, high = -1;
        double lowRatio = 1, highRatio = 1;
        for (int k = 0; k < 3; ++k) {
            if (f.jobs[k] < 0.5)
                continue;
            const double r = f.gdp[k] / f.jobs[k];
            if (r < lowRatio) { lowRatio = r; low = k; }
            if (r > highRatio) { highRatio = r; high = k; }
        }
        // The gap of farming is what pulls workers toward towns as a country develops; in another sector it
        // mostly mirrors a very productive one (oil, finance).
        if (low == 0 && lowRatio < 0.67)
            reading << DetailPanel::tr("<b>Productivity</b>: the primary sector has %1 of jobs but makes %2 of GDP, about %3 "
                                       "times less per job than the country's average. That gap is what pulls workers "
                                       "toward towns, industry and services as a country develops.")
                           .arg(pct(f.jobs[0]), pct(f.gdp[0]), loc.toString(1 / lowRatio, 'f', 1));
        else if (low > 0 && lowRatio < 0.67)
            reading << DetailPanel::tr("<b>Productivity</b>: the %1 sector has %2 of jobs but makes %3 of GDP, about %4 times "
                                       "less per job than the country's average.")
                           .arg(names[low], pct(f.jobs[low]), pct(f.gdp[low]), loc.toString(1 / lowRatio, 'f', 1));
        if (high == 1 && highRatio > 1.5)
            reading << DetailPanel::tr("A job in the secondary sector makes about %1 times the average: much capital or "
                                       "natural resources per worker (oil, gas, mines, heavy industry).")
                           .arg(loc.toString(highRatio, 'f', 1));
        else if (high == 2 && highRatio > 1.5)
            reading << DetailPanel::tr("A job in services makes about %1 times the average: finance, telecoms, software "
                                       "and business services make much per job.")
                           .arg(loc.toString(highRatio, 'f', 1));
        else if (high == 0 && highRatio > 1.5)
            reading << DetailPanel::tr("A job in the primary sector makes about %1 times the average: large, mechanised "
                                       "farms or fisheries.").arg(loc.toString(highRatio, 'f', 1));
        if ((low < 0 || lowRatio >= 0.67) && (high < 0 || highRatio <= 1.5))
            reading << DetailPanel::tr("<b>Productivity</b>: each sector's share of GDP is close to its share of jobs, so a job "
                             "makes about as much in one sector as in another.");
    }

    // Change since the base year.
    if (f.base) {
        const double d[3] = {f.gdp[0] - f.then[0], f.gdp[1] - f.then[1], f.gdp[2] - f.then[2]};
        QStringList moves;
        // Rich countries deindustrialise as factories move abroad and goods get cheaper; poorer ones that never
        // had a large industry turn to services before it grew (Rodrik's premature deindustrialisation); the
        // others rebalance after their industrial stage.
        if (d[1] <= -2 && f.gdp[2] >= 60)
            moves << DetailPanel::tr("<b>Deindustrialisation</b>: the secondary sector lost %1 pts of GDP since %2: factories "
                                     "moved abroad, goods got cheaper with productivity gains, and services once done "
                                     "in-house are bought from outside.").arg(pts(d[1])).arg(f.base);
        else if (d[1] <= -2 && f.then[1] >= 30)
            moves << DetailPanel::tr("<b>Rebalancing toward services</b>: the secondary sector lost %1 pts of GDP since %2, "
                                     "to services and domestic spending, after a strong industrial stage.")
                         .arg(pts(d[1])).arg(f.base);
        else if (d[1] <= -2)
            moves << DetailPanel::tr("<b>Early deindustrialisation</b>: the secondary sector lost %1 pts of GDP since %2 "
                                     "before the country got rich: services take over from farming without a large "
                                     "industrial stage (Rodrik).").arg(pts(d[1])).arg(f.base);
        else if (d[1] >= 2)
            moves << DetailPanel::tr("<b>Industrialisation</b>: the secondary sector gained %1 pts of GDP since %2.")
                         .arg(pts(d[1])).arg(f.base);
        if (d[0] <= -2)
            moves << DetailPanel::tr("The primary sector lost %1 pts of GDP since %2, as farming mechanises and the rest of "
                           "the economy grows faster.").arg(pts(d[0])).arg(f.base);
        if (d[2] >= 2)
            moves << DetailPanel::tr("<b>Tertiarisation</b>: services gained %1 pts of GDP since %2.").arg(pts(d[2])).arg(f.base);
        if (moves.isEmpty())
            moves << DetailPanel::tr("<b>A stable structure</b>: no sector moved by more than 2 pts of GDP since %1.").arg(f.base);
        reading << moves;
    }
    if (!reading.isEmpty())
        sections.push_back({DetailPanel::tr("What the figures say · %1").arg(f.country), reading.join("<br><br>")});

    // History: the written account and the long series, around the chart of the shares of GDP.
    QVector<PopupBlock> blocks;
    QVector<QPair<QString, QString>> after;
    if (f.sectors && (f.gdpYear || f.jobsYear)) {
        const SectorStory story = sectorStory(f.iso3);
        const SectorTrajectory trajectory = sectorTrajectory(f, names);
        const QString heading = DetailPanel::tr("History · cause and effect");
        sections.push_back({heading, story.summary.isEmpty() ? trajectory.line : story.summary.toHtmlEscaped()});
        blocks.push_back({helpHtml(DetailPanel::tr("Economic sectors"), sections), {}});
        sections.clear();

        using S = EconomySectors;
        QMap<int, double> shares[3] = {f.sectors->series(f.iso3, S::PrimaryGdp), f.sectors->series(f.iso3, S::SecondaryGdp),
                                       f.sectors->series(f.iso3, S::TertiaryGdp)};
        QVector<SectorHistoryChart::Event> marks;
        for (int i = 0; i < story.events.size(); ++i)
            marks.push_back({story.events[i].from, story.events[i].to, i + 1});
        const QStringList lineNames = {names[0], names[1], names[2]};
        const int span = shares[1].isEmpty() ? 0 : shares[1].lastKey() - shares[1].firstKey();
        if (span >= 10)
            blocks.push_back({{}, [shares, lineNames, marks] {
                                  auto *chart = new SectorHistoryChart;
                                  chart->setData(shares, lineNames, marks);
                                  return chart;
                              }});

        QStringList legend;
        for (int k = 0; k < 3; ++k)
            legend << QString("<span style='color:%1'>■</span>&nbsp;%2").arg(SectorChart::color(k).name(), names[k]);
        QString html = QString("<span style='color:%1; font-size:11px'>%2 · %3</span>")
                           .arg(Theme::current().TextDim.name(), DetailPanel::tr("share of GDP"), legend.join("&nbsp;&nbsp;"));
        if (!story.events.isEmpty()) {
            for (int i = 0; i < story.events.size(); ++i) {
                const SectorStory::Event &e = story.events[i];
                const QString when = e.to ? QString("%1–%2").arg(e.from).arg(e.to) : QString::number(e.from);
                html += QString("<br><br><b>%1 %2</b> · %3").arg(QChar(0x2460 + i), when, e.text.toHtmlEscaped());
            }
        }
        after.push_back({QString(), html});
        QString series = story.summary.isEmpty() ? QString() : trajectory.line;
        if (!trajectory.causes.isEmpty())
            series += (series.isEmpty() ? "" : "<br><br>") + trajectory.causes.join("<br><br>");
        if (!series.isEmpty())
            after.push_back({DetailPanel::tr("What the long series show"), series});
        after.push_back({QString(), QString("<span style='color:%1; font-size:11px'>%2</span>")
                                         .arg(Theme::current().TextDim.name(),
                                              story.events.isEmpty()
                                                  ? DetailPanel::tr("No written account for this country: the causes above "
                                                                    "are read from the shape of its figures.")
                                                  : DetailPanel::tr("Account written for Deepwater from established economic "
                                                                    "history; figures from the World Bank."))});
    }

    sections.push_back({DetailPanel::tr("Why it matters"),
                        DetailPanel::tr("As a country grows richer, farming mechanises, industry grows then shrinks, and "
                              "services end up dominating (Clark, Fourastié). The structure tells where it stands and "
                              "what its income depends on: commodity prices, world demand for goods or domestic "
                              "spending.")});
    sections.push_back({DetailPanel::tr("Keep in mind"),
                        DetailPanel::tr("Mining, oil and energy count in the secondary sector: an oil country looks industrial "
                              "and its share follows the oil price. Shares of GDP don't add up to 100% (taxes less "
                              "subsidies on products). Outsourcing moves industrial jobs to services. Jobs are ILO "
                              "estimates, rough for the informal economy.")});
    if (blocks.isEmpty())
        return {{helpHtml(DetailPanel::tr("Economic sectors"), sections), {}}};
    after += sections;
    blocks.push_back({helpHtml(QString(), after), {}});
    return blocks;
}

} // namespace

DetailPanel::DetailPanel(MarketData *data, VesselStore *vessels, ChokepointMonitor *chokepoints,
                         PortWatch *portWatch, MacroIndicators *macro, QWidget *parent)
    : QWidget(parent)
    , m_data(data)
    , m_vessels(vessels)
    , m_chokepoints(chokepoints)
    , m_portWatch(portWatch)
    , m_macro(macro)
{
    setObjectName("sidePanel");
    setAttribute(Qt::WA_StyledBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName("panelContent");
    m_layout = new QVBoxLayout(content);
    m_layout->setContentsMargins(20, 20, 20, 20);
    m_layout->setSpacing(8);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    m_scroll = scroll;

    connect(m_data, &MarketData::pricesUpdated, this, &DetailPanel::refreshQuote);
    connect(m_vessels, &VesselStore::changed, this, &DetailPanel::refreshVessel);
    connect(m_vessels, &VesselStore::changed, this, &DetailPanel::refreshChokepoint);
}

void DetailPanel::clear()
{
    clearLayout(m_layout);
    m_quoteCommodity.clear();
    m_vesselMmsi = 0;
    m_routePort.clear();
    m_routeKm = 0;
    m_zone = -1;
    m_zoneListShips.clear();
    m_zoneListTitle.clear();
    m_zoneButtons.clear();
    m_zoneButtonShips.clear();
    m_country.clear();
}

void DetailPanel::addHeader(const QString &kicker, const QColor &kickerColor, const QString &title,
                           const QString &subtitle, bool closable, QWidget *titleAccessory)
{
    auto *top = new QHBoxLayout;
    auto *k = new QLabel(kicker.toUpper());
    k->setObjectName("kicker");
    k->setStyleSheet(QString("color: %1;").arg(kickerColor.name()));
    top->addWidget(k, 1);
    if (closable) {
        auto *close = new QPushButton(QString::fromUtf8("✕"));
        close->setObjectName("closeButton");
        close->setCursor(Qt::PointingHandCursor);
        close->setToolTip(tr("Clear selection"));
        connect(close, &QPushButton::clicked, this, &DetailPanel::closeRequested);
        top->addWidget(close);
    }
    m_layout->addLayout(top);

    auto *t = new QLabel(title);
    t->setObjectName("title");
    t->setWordWrap(true);
    if (titleAccessory) {
        // Just after the text, centered on its first line (the title may wrap).
        t->ensurePolished();
        auto *column = new QVBoxLayout;
        column->addSpacing(std::max(0, (t->fontMetrics().height() - titleAccessory->sizeHint().height()) / 2));
        column->addWidget(titleAccessory);
        column->addStretch();
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(t);
        row->addLayout(column);
        row->addStretch(1);
        m_layout->addLayout(row);
    } else {
        m_layout->addWidget(t);
    }
    if (!subtitle.isEmpty()) {
        auto *s = new QLabel(subtitle);
        s->setObjectName("dim");
        s->setWordWrap(true);
        m_layout->addWidget(s);
    }
    m_layout->addSpacing(6);
    m_layout->addWidget(divider());
    m_layout->addSpacing(6);
}

void DetailPanel::addQuoteCard(const QString &commodityId)
{
    const Commodity *c = m_data->commodity(commodityId);
    if (!c)
        return;
    m_quoteCommodity = commodityId;

    auto *nameRow = new QHBoxLayout;
    auto *name = new QPushButton(c->name);
    name->setObjectName("linkButton");
    name->setCursor(Qt::PointingHandCursor);
    name->setToolTip(tr("Show all %1 sites").arg(c->name));
    connect(name, &QPushButton::clicked, this, [this, id = c->id] { emit commodityActivated(id); });
    m_unitLabel = new QLabel(c->unit);
    m_unitLabel->setObjectName("dim");
    nameRow->addWidget(name);
    nameRow->addStretch();
    nameRow->addWidget(m_unitLabel);
    m_layout->addLayout(nameRow);

    m_priceLabel = new QLabel;
    m_priceLabel->setObjectName("price");
    m_layout->addWidget(m_priceLabel);
    m_changeLabel = new QLabel;
    m_changeLabel->setObjectName("change");
    m_layout->addWidget(m_changeLabel);

    m_chart = new Sparkline;
    m_chart->setDetailed(true);
    m_layout->addWidget(m_chart);

    m_statsLabel = new QLabel;
    m_statsLabel->setObjectName("stats");
    m_statsLabel->setTextFormat(Qt::RichText);
    m_layout->addWidget(m_statsLabel);

    m_sourceLabel = new QLabel;
    m_sourceLabel->setObjectName("dim");
    m_sourceLabel->setWordWrap(true);
    m_sourceLabel->setTextFormat(Qt::RichText);
    m_layout->addWidget(m_sourceLabel);

    refreshQuote();
}

void DetailPanel::refreshQuote()
{
    const Commodity *c = m_data->commodity(m_quoteCommodity);
    if (!c || !m_priceLabel)
        return;
    const QLocale loc;
    const double pct = c->changePct();
    m_unitLabel->setText(c->unit);
    m_priceLabel->setText(c->hasPrice() ? formatPrice(c->price) : QString::fromUtf8("—"));
    m_changeLabel->setText(formatChange(*c));
    m_changeLabel->setStyleSheet(std::isnan(pct) ? QString() : trendStyle(pct));
    m_changeLabel->setVisible(!std::isnan(pct));
    m_chart->setValues(c->history);
    m_chart->setColor(std::isnan(pct) || pct >= 0 ? Theme::current().Up : Theme::current().Down);
    m_chart->setVisible(c->history.size() >= 2);

    const QString dim = Theme::current().TextDim.name();
    auto cell = [&](const QString &k, const QString &v) {
        return QString("<td style='padding:2px 14px 2px 0'><span style='color:%1'>%2</span><br>%3</td>")
            .arg(dim, k, v);
    };
    QString stats;
    if (c->history.size() >= 2) {
        const auto [lo, hi] = std::minmax_element(c->history.begin(), c->history.end());
        QString previous, high, low;
        switch (c->span) {
        case Commodity::Span::FiveDays:
            high = tr("5-day high"), low = tr("5-day low");
            break;
        case Commodity::Span::ThreeMonths:
            high = tr("3-month high"), low = tr("3-month low");
            break;
        case Commodity::Span::TwoYears:
            high = tr("2-year high"), low = tr("2-year low");
            break;
        }
        if (c->previousDate.isValid())
            previous = c->source == Commodity::Source::Monthly
                ? loc.toString(c->previousDate, "MMMM yyyy")
                : tr("Close %1").arg(loc.toString(c->previousDate, "d MMM"));
        stats = "<table><tr>"
                + (previous.isEmpty() ? QString() : cell(previous, formatPrice(c->previousClose)))
                + cell(high, formatPrice(*hi)) + cell(low, formatPrice(*lo)) + "</tr></table>";
    }
    m_statsLabel->setText(stats);
    m_statsLabel->setVisible(!stats.isEmpty());

    // Where the price comes from and how old it is.
    QStringList notes;
    switch (c->source) {
    case Commodity::Source::Futures: {
        const QString contract = c->contractName.isEmpty() ? c->contract
                                                           : QString("%1, %2").arg(c->contract, c->contractName);
        if (c->settlement)
            notes << tr("<b>Close of %1</b> · %2 futures (%3), via Yahoo Finance, which only carries this "
                        "contract's daily settlement.")
                         .arg(loc.toString(c->time.date(), "dddd d MMMM"), c->future.market, contract.toHtmlEscaped());
        else
            notes << tr("<b>Last trade %1</b> · %2 futures (%3), about 10 minutes behind the exchange, via "
                        "Yahoo Finance.")
                         .arg(loc.toString(c->time.toLocalTime(), "ddd d MMM, HH:mm"), c->future.market,
                              contract.toHtmlEscaped());
        if (!c->future.what.isEmpty())
            notes << c->future.what.toHtmlEscaped() + ".";
        if (!std::isnan(c->benchmark))
            notes << tr("IMF monthly average for %1 (%2): %3 %4.")
                         .arg(loc.toString(c->benchmarkMonth, "MMMM yyyy"), c->imf.what.toHtmlEscaped(),
                              formatPrice(c->benchmark), c->imf.unit);
        break;
    }
    case Commodity::Source::Monthly:
        notes << tr("<b>Average of %1</b> · IMF Primary Commodity Prices: %2.")
                     .arg(loc.toString(c->time.date(), "MMMM yyyy"), c->imf.what.toHtmlEscaped());
        notes << (c->future.symbol.isEmpty()
                      ? tr("No free, current futures quote exists for it; the IMF publishes a month's average "
                           "early the next month.")
                      : tr("Its futures contract has no recent quote, so the IMF's monthly average is shown."));
        break;
    case Commodity::Source::None:
        notes << tr("No price loaded yet: see the Sources page.");
        break;
    }
    m_sourceLabel->setText(notes.join(' '));
}

void DetailPanel::addSiteList(const QString &label, const QVector<int> &sites, int exclude)
{
    QVector<int> list;
    for (int s : sites)
        if (s != exclude) list.push_back(s);
    if (list.isEmpty())
        return;

    m_layout->addSpacing(8);
    m_layout->addWidget(sectionLabel(QString("%1 · %2").arg(label).arg(list.size())));
    for (int idx : std::as_const(list)) {
        const Site &s = m_data->sites()[idx];
        const Commodity *c = m_data->commodity(s.commodityId);
        auto *b = new QPushButton;
        b->setObjectName("siteButton");
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon([&] {
            QPixmap pm(20, 20);
            pm.setDevicePixelRatio(2);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(m_data->categoryColor(s.categoryId));
            p.drawEllipse(QPointF(5, 5), 4, 4);
            return QIcon(pm);
        }());
        b->setText(QString("%1\n%2 · %3").arg(s.name, c ? c->name : s.type, s.country));
        b->setToolTip(QString("%1 · %2").arg(s.type, s.country));
        // Let long names clip instead of widening the panel.
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        b->setMinimumWidth(0);
        connect(b, &QPushButton::clicked, this, [this, idx] { emit siteActivated(idx); });
        m_layout->addWidget(b);
    }
}

void DetailPanel::showSites(const QVector<int> &siteIndices)
{
    if (siteIndices.isEmpty())
        return;
    clear();

    if (siteIndices.size() == 1) {
        m_mode = Mode::Site;
        const Site &s = m_data->sites()[siteIndices.first()];
        const Category *cat = m_data->category(s.categoryId);
        addHeader(cat ? cat->name : QString(), m_data->categoryColor(s.categoryId), s.name,
                  QString("%1 · %2").arg(s.type, s.country));

        auto *coords = new QLabel(QString("%1° %2, %3° %4")
                                      .arg(std::abs(s.lat), 0, 'f', 2).arg(s.lat >= 0 ? "N" : "S")
                                      .arg(std::abs(s.lon), 0, 'f', 2).arg(s.lon >= 0 ? "E" : "W"));
        coords->setObjectName("dim");
        m_layout->addWidget(coords);
        if (const QString iso = m_macro->economy()->isoForName(s.rawCountry); !iso.isEmpty()) {
            auto *link = new QPushButton(tr("%1 economy →").arg(s.country));
            link->setObjectName("linkButton");
            link->setCursor(Qt::PointingHandCursor);
            connect(link, &QPushButton::clicked, this, [this, iso] { emit countryActivated(iso); });
            m_layout->addWidget(link, 0, Qt::AlignLeft);
        }
        m_layout->addSpacing(6);

        addQuoteCard(s.commodityId);

        QVector<int> related;
        for (const Site *o : m_data->sitesForCommodity(s.commodityId))
            related.push_back(o->index);
        const Commodity *c = m_data->commodity(s.commodityId);
        addSiteList(tr("Other %1 sites").arg(c ? c->name : QString()), related, s.index);
    } else {
        m_mode = Mode::Cluster;
        QSet<QString> countries;
        for (int i : siteIndices)
            countries.insert(m_data->sites()[i].country);
        const QString where = countries.size() == 1 ? *countries.begin()
                                                    : tr("%1 countries").arg(countries.size());
        addHeader(tr("Cluster"), Theme::current().TextDim, tr("%1 sites").arg(siteIndices.size()), where);

        // Category breakdown bar
        auto *bar = new QHBoxLayout;
        bar->setSpacing(2);
        for (const Category &cat : m_data->categories()) {
            int n = 0;
            for (int i : siteIndices)
                if (m_data->sites()[i].categoryId == cat.id) ++n;
            if (!n) continue;
            auto *seg = new QFrame;
            seg->setFixedHeight(6);
            seg->setToolTip(QString("%1: %2").arg(cat.name).arg(n));
            seg->setStyleSheet(QString("background:%1; border-radius:3px;").arg(cat.color.name()));
            bar->addWidget(seg, n);
        }
        m_layout->addLayout(bar);

        addSiteList(tr("Sites"), siteIndices);
    }
    m_layout->addStretch();
}

void DetailPanel::showCommodity(const QString &commodityId)
{
    const Commodity *c = m_data->commodity(commodityId);
    if (!c)
        return;
    clear();
    m_mode = Mode::Commodity;
    const Category *cat = m_data->category(c->categoryId);
    addHeader(cat ? cat->name : QString(), m_data->categoryColor(c->categoryId), c->name,
              c->future.symbol.isEmpty() ? tr("IMF monthly average price")
                                         : tr("%1 futures").arg(c->future.market));
    addQuoteCard(commodityId);

    QVector<int> sites;
    for (const Site *s : m_data->sitesForCommodity(commodityId))
        sites.push_back(s->index);
    addSiteList(tr("Sites"), sites);
    m_layout->addStretch();
}

void DetailPanel::showVessel(qint64 mmsi)
{
    const Vessel *v = m_vessels->vessel(mmsi);
    if (!v)
        return;
    clear();
    m_mode = Mode::Vessel;
    m_vesselMmsi = mmsi;

    const Vessel::Class cls = v->vesselClass();
    addHeader(cls == Vessel::Tanker  ? tr("Tanker")
              : cls == Vessel::Cargo ? tr("Cargo ship")
                                     : tr("Unidentified ship"),
              cls == Vessel::Tanker  ? m_data->categoryColor("energy")
              : cls == Vessel::Cargo ? m_data->categoryColor("industrial")
                                     : Theme::current().TextDim,
              v->name.isEmpty() ? QString("MMSI %1").arg(mmsi) : v->name,
              v->destination.isEmpty() ? tr("Destination not reported")
                                       : tr("Bound for %1").arg(v->destination));

    m_vesselStats = new QLabel;
    m_vesselStats->setObjectName("stats");
    m_vesselStats->setTextFormat(Qt::RichText);
    m_layout->addWidget(m_vesselStats);

    m_layout->addSpacing(10);
    auto *note = new QLabel(tr("Live AIS position. AIS does not report cargo: tankers carry oil, gas "
                               "or chemicals; cargo ships carry dry bulk or containers."));
    note->setObjectName("dim");
    note->setWordWrap(true);
    m_layout->addWidget(note);
    m_layout->addStretch();

    refreshVessel();
}

void DetailPanel::refreshVessel()
{
    if (m_mode != Mode::Vessel || !m_vesselStats)
        return;
    const Vessel *v = m_vessels->vessel(m_vesselMmsi);
    if (!v) {
        m_vesselStats->setText(tr("No longer tracked."));
        return;
    }

    const QLocale loc;
    const QString dim = Theme::current().TextDim.name();
    const QString na = QString::fromUtf8("\u2014");
    auto row = [&](const QString &k, const QString &val) {
        return QString("<tr><td style='padding:3px 18px 3px 0; color:%1'>%2</td><td>%3</td></tr>")
            .arg(dim, k, val);
    };
    const qint64 ago = (QDateTime::currentMSecsSinceEpoch() - v->lastSeenMs) / 1000;
    QString html = "<table>";
    if (!v->destination.isEmpty()) {
        if (m_routePort.isEmpty()) {
            html += row(tr("Destination port"), tr("Not recognised"));
        } else {
            const double nm = m_routeKm / 1.852;
            html += row(tr("Destination port"), m_routePort.toHtmlEscaped());
            html += row(tr("Distance by sea"), tr("%1 nm").arg(loc.toString(nm, 'f', 0)));
            if (v->sog >= 2) {
                const double hours = nm / v->sog;
                const QString eta = hours >= 48 ? tr("%1 days").arg(loc.toString(hours / 24, 'f', 1))
                                                : tr("%1 h").arg(loc.toString(hours, 'f', 0));
                html += row(tr("At current speed"), eta);
            }
        }
    }
    html += row(tr("Speed"), tr("%1 kn").arg(loc.toString(v->sog, 'f', 1)));
    html += row(tr("Course"), QString("%1\u00B0").arg(loc.toString(v->cog, 'f', 0)));
    html += row(tr("Length"), v->length > 0 ? tr("%1 m").arg(v->length) : na);
    html += row(tr("Draught"), v->draught > 0 ? tr("%1 m").arg(loc.toString(v->draught, 'f', 1)) : na);
    html += row(tr("Position"), QString("%1\u00B0 %2, %3\u00B0 %4")
                                    .arg(loc.toString(std::abs(v->lat), 'f', 3)).arg(v->lat >= 0 ? "N" : "S")
                                    .arg(loc.toString(std::abs(v->lon), 'f', 3)).arg(v->lon >= 0 ? "E" : "W"));
    html += row("MMSI", QString::number(v->mmsi));
    html += row("IMO", v->imo > 0 ? QString::number(v->imo) : na);
    html += row(tr("Call sign"), v->callSign.isEmpty() ? na : v->callSign.toHtmlEscaped());
    html += row(tr("AIS ship type"), QString::number(v->shipType));
    html += row(tr("Last report"), tr("%1 s ago").arg(ago));
    html += "</table>";
    m_vesselStats->setText(html);
}

void DetailPanel::setVesselRoute(const QString &port, double remainingKm)
{
    m_routePort = port;
    m_routeKm = remainingKm;
    refreshVessel();
}

void DetailPanel::showChokepoint(int zone)
{
    clear();
    m_mode = Mode::Chokepoint;
    m_zone = zone;
    const Chokepoint &z = m_chokepoints->chokepoints()[zone];
    addHeader(tr("Chokepoint"), Theme::current().Accent, m_chokepoints->name(zone),
              m_chokepoints->description(zone));

    m_zoneStats = new QLabel;
    m_zoneStats->setObjectName("stats");
    m_zoneStats->setTextFormat(Qt::RichText);
    m_zoneStats->setWordWrap(true);
    m_layout->addWidget(m_zoneStats);

    m_layout->addSpacing(6);
    m_layout->addWidget(sectionLabel(tr("Passages per hour · 48 h")));
    m_zoneChart = new HourlyChart(m_data);
    m_zoneChart->setMinimumHeight(110);
    m_layout->addWidget(m_zoneChart);

    auto *note = new QLabel(tr("Zone radius: %1 km. Counts are lower bounds: they depend on AIS coverage "
                               "and on ships having broadcast their type.").arg(int(z.radiusKm)));
    note->setObjectName("dim");
    note->setWordWrap(true);
    m_layout->addWidget(note);

    // Historical daily transits from IMF PortWatch.
    const PortWatchSummary s = summarize(m_portWatch, z.portwatchId);
    if (s.valid) {
        const QLocale loc;
        m_layout->addSpacing(8);
        m_layout->addWidget(sectionLabel(tr("Daily transits · IMF PortWatch")));
        const QString dim = Theme::current().TextDim.name();
        const QString trend = (s.changePct >= 0 ? Theme::current().Up : Theme::current().Down).name();
        auto *summary = new QLabel(
            tr("<b>%1</b> transits on %2: <span style='color:%3'>%4 tankers</span>, "
               "<span style='color:%5'>%6 dry bulk</span>, %7 container, %8 other.<br>"
               "<span style='color:%9'>Tanker capacity: %10 t · 7-day avg %11/day vs 90-day avg %12/day</span> "
               "<span style='color:%13'>(%14%)</span>")
                .arg(loc.toString(s.latest.total), loc.toString(s.latest.date, QLocale::ShortFormat),
                     m_data->categoryColor("energy").name(), loc.toString(s.latest.tanker),
                     m_data->categoryColor("industrial").name(), loc.toString(s.latest.dryBulk),
                     loc.toString(s.latest.container), loc.toString(s.latest.other), dim)
                .arg(loc.toString(s.latest.tankerCapacity), loc.toString(s.avg7, 'f', 0),
                     loc.toString(s.avg90, 'f', 0), trend,
                     QString("%1%2").arg(s.changePct >= 0 ? "+" : "", loc.toString(s.changePct, 'f', 0))));
        summary->setTextFormat(Qt::RichText);
        summary->setWordWrap(true);
        m_layout->addWidget(summary);
        auto *daily = new DailyChart(m_data);
        daily->setMinimumHeight(110);
        daily->setDays(m_portWatch->days(z.portwatchId));
        m_layout->addWidget(daily);
        if (*z.portwatchScope) {
            auto *scope = new QLabel(tr("PortWatch zone: %1").arg(QString::fromUtf8(z.portwatchScope)));
            scope->setObjectName("dim");
            m_layout->addWidget(scope);
        }
    }

    m_layout->addSpacing(8);
    m_zoneList = new QVBoxLayout;
    m_zoneList->setSpacing(2);
    m_layout->addLayout(m_zoneList);
    m_layout->addStretch();
    refreshChokepoint();
}

void DetailPanel::refreshChokepoint()
{
    if (m_mode != Mode::Chokepoint || m_zone < 0 || !m_zoneStats)
        return;
    const QLocale loc;
    const auto now = m_chokepoints->inZoneNow(m_zone);
    const auto day = m_chokepoints->passages(m_zone, qint64(ChokepointMonitor::WindowHours) * 3600 * 1000);
    const QString dim = Theme::current().TextDim.name();
    const QString tankerColor = m_data->categoryColor("energy").name();
    const QString cargoColor = m_data->categoryColor("industrial").name();
    auto cell = [&](const QString &label, const ChokepointMonitor::Counts &c) {
        return QString("<td style='padding:2px 28px 2px 0'><span style='color:%1'>%2</span><br>"
                       "<span style='font-size:22px; font-weight:600'>%3</span><br>"
                       "<span style='color:%4'>\u25B2 %5</span>&nbsp;&nbsp;<span style='color:%6'>\u25B2 %7</span></td>")
            .arg(dim, label, loc.toString(c.total()), tankerColor, loc.toString(c.tankers), cargoColor,
                 loc.toString(c.cargo));
    };
    QString html = "<table><tr>" + cell(tr("In zone now"), now) + cell(tr("Passages · 48 h"), day)
                   + "</tr></table>";
    html += QString("<p style='color:%1'>%2</p>")
                .arg(dim, tr("%1 h of the last %2 h recorded (shaded hours: no data).")
                              .arg(m_chokepoints->recordedHourCount())
                              .arg(ChokepointMonitor::WindowHours));
    if (now.unknown > 0)
        html += QString("<p style='color:%1'>%2</p>").arg(dim, tr("+%1 unidentified ships in the zone").arg(now.unknown));
    if (!m_chokepoints->hasCoverage(m_zone))
        html += QString("<p style='color:%1'>%2</p>")
                    .arg(Theme::current().Down.name(),
                         tr("No AIS reports from this zone right now: aisstream.io does not cover every region."));
    m_zoneStats->setText(html);
    m_zoneChart->setHours(m_chokepoints->hourly(m_zone), m_chokepoints->recordedHours());

    // Ships currently in the zone, fastest first.
    QVector<qint64> ships = m_chokepoints->vesselsInZone(m_zone);
    std::sort(ships.begin(), ships.end(), [this](qint64 a, qint64 b) {
        return m_vessels->vessel(a)->sog > m_vessels->vessel(b)->sog;
    });
    if (ships == m_zoneListShips && m_zoneList->count() > 0)
        return;
    m_zoneListShips = ships;
    // The title and the buttons are kept and refilled: rebuilding forty buttons every second, as
    // the ships' order changes, was most of this refresh. Buttons past the list are hidden.
    const QString title = QString("%1 \u00B7 %2").arg(tr("Ships in zone")).arg(ships.size());
    if (!m_zoneListTitle) {
        m_zoneListTitle = sectionLabel(title);
        m_zoneList->addWidget(m_zoneListTitle);
    } else {
        m_zoneListTitle->setText(title);
    }
    auto shipIcon = [this](const QColor &color) {
        QPixmap pm(20, 20);
        pm.setDevicePixelRatio(2);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(QPolygonF {QPointF(5, 1), QPointF(9, 9), QPointF(1, 9)});
        }
        return QIcon(pm);
    };
    const QIcon tankerIcon = shipIcon(m_data->categoryColor("energy"));
    const QIcon cargoIcon = shipIcon(m_data->categoryColor("industrial"));
    const int shown = std::min<int>(ships.size(), 40);
    while (m_zoneButtons.size() < shown) {
        const int index = m_zoneButtons.size();
        auto *b = new QPushButton;
        b->setObjectName("siteButton");
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        b->setMinimumWidth(0);
        connect(b, &QPushButton::clicked, this, [this, index] { emit vesselActivated(m_zoneButtonShips.value(index)); });
        m_zoneList->addWidget(b);
        m_zoneButtons.push_back(b);
        m_zoneButtonShips.push_back(0);
    }
    for (int i = 0; i < m_zoneButtons.size(); ++i) {
        QPushButton *b = m_zoneButtons[i];
        if (i >= shown) {
            b->hide();
            continue;
        }
        const Vessel *v = m_vessels->vessel(ships[i]);
        const bool tanker = v->vesselClass() == Vessel::Tanker;
        b->setIcon(tanker ? tankerIcon : cargoIcon);
        const QString kind = tanker ? tr("Tanker") : tr("Cargo ship");
        QString line2 = QString("%1 \u00B7 %2").arg(kind, tr("%1 kn").arg(loc.toString(v->sog, 'f', 1)));
        if (!v->destination.isEmpty())
            line2 += QString::fromUtf8(" \u2192 ") + v->destination;
        b->setText(QString("%1\n%2").arg(v->name.isEmpty() ? QString::number(v->mmsi) : v->name, line2));
        m_zoneButtonShips[i] = v->mmsi;
        b->show();
    }
}

// ---------------------------------------------------------------- macro

void DetailPanel::showIndicator(int id, int year)
{
    clear();
    m_mode = Mode::Indicator;
    m_indicator = id;
    m_year = year;
    const MacroIndicators::Info info = MacroIndicators::info(MacroIndicators::Id(id));
    // The indicator view is the sidebar's content while the indicator is selected, not a selection
    // laid over the overview: no close button. Clicking the indicator again (or Escape) brings the
    // overview back.
    auto *help = new HelpButton(explanationHtml(MacroIndicators::Id(id)));
    help->setToolTip(tr("About this indicator"));
    addHeader(tr("Indicator"), Theme::current().Accent, info.name, info.description, false, help);

    if (info.yearly) {
        m_yearLabel = new QLabel;
        m_yearLabel->setObjectName("stats");
        m_yearLabel->setWordWrap(true);
        m_layout->addWidget(m_yearLabel);
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setObjectName("yearSlider");
        slider->setRange(MacroIndicators::firstYear(), m_macro->lastYear(MacroIndicators::Id(id)));
        slider->setValue(year);
        slider->setPageStep(1);
        slider->setCursor(Qt::PointingHandCursor);
        connect(slider, &QSlider::valueChanged, this, [this](int y) {
            m_year = y;
            refreshRanking();
            emit yearChanged(y);
        });
        m_layout->addWidget(slider);
    } else {
        auto *when = new QLabel(MacroIndicators::latestNote(MacroIndicators::Id(id)));
        when->setObjectName("dim");
        when->setWordWrap(true);
        m_layout->addWidget(when);
    }

    m_layout->addSpacing(6);
    m_rankingSummary = new QLabel;
    m_rankingSummary->setObjectName("stats");
    m_rankingSummary->setTextFormat(Qt::RichText);
    m_rankingSummary->setWordWrap(true);
    m_layout->addWidget(m_rankingSummary);

    // Region filter for the ranking (the figures above stay worldwide). Two rows of chips that
    // share the sidebar's width, so they never widen it.
    m_layout->addSpacing(8);
    auto *group = new QButtonGroup(this);
    const struct { const char *code; const char *label; } choices[] = {
        {"", QT_TR_NOOP("All")}, {"EU", QT_TR_NOOP("Europe")}, {"AS", QT_TR_NOOP("Asia")},
        {"AF", QT_TR_NOOP("Africa")}, {"NA", QT_TR_NOOP("N. America")}, {"SA", QT_TR_NOOP("S. America")},
        {"OC", QT_TR_NOOP("Oceania")}};
    QHBoxLayout *row = nullptr;
    int k = 0;
    for (const auto &choice : choices) {
        if (k == 0 || k == 4) {
            row = new QHBoxLayout;
            row->setSpacing(6);
            m_layout->addLayout(row);
        }
        auto *b = new QPushButton(tr(choice.label));
        b->setObjectName("segButton");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(26);
        b->setStyleSheet("padding: 4px 4px;");   // narrower than segButton's 12 px, to fit the labels
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        b->setChecked(m_region == QLatin1String(choice.code));
        group->addButton(b);
        row->addWidget(b);
        ++k;
        connect(b, &QPushButton::clicked, this, [this, code = QString::fromLatin1(choice.code)] {
            m_region = code;
            refreshRanking();
        });
    }

    m_layout->addSpacing(6);
    m_layout->addWidget(sectionLabel(tr("Ranking")));
    m_ranking = new RankingList;
    connect(m_ranking, &RankingList::countryClicked, this, &DetailPanel::countryActivated);
    m_layout->addWidget(m_ranking);

    auto *source = new QLabel(tr("Source: %1").arg(info.source));
    source->setObjectName("dim");
    source->setWordWrap(true);
    m_layout->addWidget(source);
    m_layout->addStretch();
    refreshRanking();
}

void DetailPanel::refreshRanking()
{
    if (m_mode != Mode::Indicator || !m_ranking)
        return;
    const auto id = MacroIndicators::Id(m_indicator);
    const MacroIndicators::Info info = MacroIndicators::info(id);
    if (m_yearLabel)
        m_yearLabel->setText(m_macro->yearNote(id, m_year));

    const QHash<QString, double> values = m_macro->values(id, m_year);
    const MacroIndicators::Range range = m_macro->range(id, values);
    QVector<RankingList::Row> rows;
    for (auto it = values.begin(); it != values.end(); ++it)
        rows.push_back({it.key(), m_macro->economy()->displayName(it.key()), it.value(),
                        m_macro->format(id, it.value()), m_macro->color(id, it.value(), range)});
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.value > b.value; });
    const double scaleMax = info.scale == MacroIndicators::Scale::Log ? std::pow(10.0, range.hi)
                                                                      : std::max(std::abs(range.lo), std::abs(range.hi));
    // The ranking shows the selected region only, ranked within it, on the worldwide bar scale.
    QVector<RankingList::Row> shown;
    for (const RankingList::Row &r : std::as_const(rows)) {
        const WorldEconomy::Country *c = m_macro->economy()->country(r.iso3);
        if (m_region.isEmpty() || (c && c->region == m_region))
            shown.push_back(r);
    }
    m_ranking->setRows(shown, info.scale == MacroIndicators::Scale::Diverging, scaleMax);

    if (rows.isEmpty()) {
        m_rankingSummary->setText(m_macro->hasData(id)                                   ? tr("No data for this year.")
                                  : info.group == MacroIndicators::Group::SupplyChain ? tr("Loading…")
                                                                                       : tr("Loading IMF data…"));
        return;
    }
    const QString dim = Theme::current().TextDim.name();
    auto cell = [&](const QString &k, const QString &v, const QString &sub) {
        return QString("<td style='padding:2px 16px 2px 0'><span style='color:%1'>%2</span><br>"
                       "<span style='font-size:17px; font-weight:600'>%3</span><br>"
                       "<span style='color:%1'>%4</span></td>")
            .arg(dim, k, v, sub.toHtmlEscaped());
    };
    const RankingList::Row &median = rows[rows.size() / 2];
    m_rankingSummary->setText("<table><tr>"
                              + cell(tr("Median"), m_macro->format(id, median.value),
                                     tr("%1 countries").arg(rows.size()))
                              + cell(tr("Highest"), rows.first().text, rows.first().name)
                              + cell(tr("Lowest"), rows.last().text, rows.last().name) + "</tr></table>");
}

void DetailPanel::showCountry(const QString &iso3)
{
    clear();
    m_mode = Mode::Country;
    m_country = iso3;
    WorldEconomy *economy = m_macro->economy();
    const int year = MacroIndicators::currentYear();
    const double gdp = m_macro->value(MacroIndicators::GdpSize, iso3, year);
    addHeader(tr("Economy"), Theme::current().Accent, economy->displayName(iso3),
              std::isfinite(gdp) ? tr("GDP %1 in %2").arg(m_macro->format(MacroIndicators::GdpSize, gdp)).arg(year)
                                 : QString());
    if (!economy->hasData()) {
        auto *l = new QLabel(economy->errorString().isEmpty() ? tr("Loading IMF data…") : economy->errorString());
        l->setObjectName("dim");
        m_layout->addWidget(l);
        m_layout->addStretch();
        return;
    }

    // Headline figures.
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    auto addStat = [&](int col, MacroIndicators::Id id, const QString &label) {
        auto *v = new QLabel(m_macro->format(id, m_macro->value(id, iso3, year)));
        v->setObjectName("midStat");
        auto *l = new QLabel(label.toUpper());
        l->setObjectName("sectionLabelTight");
        l->setWordWrap(true);
        l->setToolTip(MacroIndicators::info(id).name);
        grid->addWidget(v, 0, col);
        grid->addWidget(l, 1, col);
    };
    addStat(0, MacroIndicators::GdpGrowth, tr("Growth %1").arg(year));
    addStat(1, MacroIndicators::Inflation, tr("Inflation %1").arg(year));
    addStat(2, MacroIndicators::PolicyRate, tr("Policy rate"));
    m_layout->addLayout(grid);
    m_layout->addSpacing(8);

    // Annual indicators over three years: around the current year for the economy, the
    // latest reported years for capital flows and trade.
    const QString dim = Theme::current().TextDim.name();
    const QString accent = Theme::current().Accent.name();
    auto addTable = [&](MacroIndicators::Group group, int lastYear) {
        QString html = QString("<table width='100%'><tr><td></td>");
        for (int y = lastYear - 2; y <= lastYear; ++y)
            html += QString("<td align='right' style='padding:2px 0 4px 10px; color:%1'>%2</td>").arg(dim).arg(y);
        html += "</tr>";
        for (int i = 0; i < MacroIndicators::Count; ++i) {
            const auto id = MacroIndicators::Id(i);
            const MacroIndicators::Info info = MacroIndicators::info(id);
            if (!info.yearly || info.group != group)
                continue;
            html += QString("<tr><td style='padding:3px 0; color:%1'>%2</td>")
                        .arg(i == m_activeIndicator ? accent : dim, info.name);
            for (int y = lastYear - 2; y <= lastYear; ++y)
                html += QString("<td align='right' style='padding:3px 0 3px 10px; white-space:nowrap'>%1</td>")
                            .arg(m_macro->format(id, m_macro->value(id, iso3, y)));
            html += "</tr>";
        }
        html += "</table>";
        auto *table = new QLabel(html);
        table->setObjectName("stats");
        table->setTextFormat(Qt::RichText);
        table->setWordWrap(true);
        m_layout->addWidget(table);
    };
    addTable(MacroIndicators::Group::Economy, year + 1);
    addLatestFigures(iso3);
    addSectors(iso3);
    addProperty(iso3);
    addDebt(iso3);
    m_layout->addSpacing(8);
    m_layout->addWidget(sectionLabel(tr("Capital flows and trade")));
    addTable(MacroIndicators::Group::FlowsAndTrade, m_macro->defaultYear(MacroIndicators::NetCapitalInflows));
    {
        // Crop yields and road freight over three years, when a source covers the country.
        const int last = std::max(m_macro->defaultYear(MacroIndicators::WheatYield),
                                  m_macro->defaultYear(MacroIndicators::RoadFreight));
        bool any = false;
        for (int i = MacroIndicators::WheatYield; i <= MacroIndicators::RoadFreight && !any; ++i)
            for (int y = last - 2; y <= last && !any; ++y)
                any = std::isfinite(m_macro->value(MacroIndicators::Id(i), iso3, y));
        if (any) {
            m_layout->addSpacing(8);
            m_layout->addWidget(sectionLabel(tr("Supply chains")));
            addTable(MacroIndicators::Group::SupplyChain, last);
        }
    }

    // Policy rate.
    double rate, change;
    QDate month;
    const WorldEconomy::Country *country = economy->country(iso3);
    if (m_macro->policyRate(iso3, &rate, &month, &change)) {
        const QLocale loc;
        m_layout->addSpacing(8);
        m_layout->addWidget(sectionLabel(tr("Policy rate")));
        QString text = tr("%1 at the end of %2").arg(m_macro->format(MacroIndicators::PolicyRate, rate),
                                                     loc.toString(month, "MMMM yyyy"));
        if (std::isfinite(change))
            text += " · " + tr("%1%2 pts over 12 months")
                                .arg(change > 0 ? "+" : change < 0 ? QString::fromUtf8("\u2212") : "")
                                .arg(loc.toString(std::abs(change), 'f', 2));
        if (country && m_macro->rates()->isEuroArea(country->iso2))
            text += "<br>" + tr("Euro area member: ECB rate.");
        auto *l = new QLabel(text);
        l->setObjectName("dim");
        l->setTextFormat(Qt::RichText);
        l->setWordWrap(true);
        m_layout->addWidget(l);
        QVector<double> values;
        for (const auto &p : m_macro->rates()->history(country->iso2))
            values.push_back(p.rate);
        auto *spark = new Sparkline;
        spark->setDetailed(true);
        spark->setMinimumHeight(110);
        spark->setValues(values);
        spark->setColor(Theme::current().Accent);
        m_layout->addWidget(spark);
    }

    // History and projections.
    auto addChart = [&](MacroIndicators::Id id, const QColor &pos, const QColor &neg) {
        QMap<int, double> series;
        for (int y = year - 10; y <= year + 5; ++y)
            if (const double v = m_macro->value(id, iso3, y); std::isfinite(v))
                series.insert(y, v);
        if (series.isEmpty())
            return;
        m_layout->addSpacing(8);
        m_layout->addWidget(sectionLabel(MacroIndicators::info(id).name));
        auto *chart = new AnnualChart;
        chart->setSeries(series, year, pos, neg, [this, id](double v) { return m_macro->format(id, v); });
        m_layout->addWidget(chart);
    };
    addChart(MacroIndicators::GdpGrowth, Theme::current().Up, Theme::current().Down);
    addChart(MacroIndicators::Inflation, Theme::current().Accent, Theme::current().TextDim);

    QVector<int> sites;
    for (const Site &s : m_data->sites())
        if (economy->isoForName(s.rawCountry) == iso3)
            sites.push_back(s.index);
    addSiteList(tr("Commodity sites"), sites);
    addTradeByProduct(iso3);
    addOfficialSources(iso3);

    m_layout->addSpacing(8);
    auto *note = new QLabel(tr("Source: IMF World Economic Outlook, IMF balance of payments statistics and BIS. "
                               "Figures for %1 and later are IMF estimates and projections; capital flows and "
                               "trade are reported figures.").arg(year)
                            + ' ' + tr("Sectors: World Bank, with jobs from ILO modelled estimates."));
    note->setObjectName("dim");
    note->setWordWrap(true);
    m_layout->addWidget(note);
    m_layout->addStretch();
}

void DetailPanel::addLatestFigures(const QString &iso3)
{
    const KeyIndicators *key = m_macro->keyIndicators();
    if (!key)
        return;
    m_layout->addSpacing(8);
    HelpButton *help = addTitleWithHelp(m_layout, tr("Latest figures"), 420);
    help->setBlocks({{tr("The monthly and quarterly figures the markets follow, as the national statistical offices "
                         "publish them, read from four free compilations: Eurostat (EU, EFTA and candidate countries), "
                         "the OECD (its members and the large emerging economies), the IMF's consumer price database "
                         "(nearly every country) and, for the United States, the Bureau of Labor Statistics directly. "
                         "The most direct source available is used for each line."),
                      {}},
                     {tr("<b>Source</b> names the national office that publishes the figure for this country, then the "
                         "compilation it was read from. Previous is the period before. Confidence indicators are in "
                         "the units of the national survey (a balance of answers or an index), so they compare over "
                         "time, not between countries."),
                      {}}});
    auto addNote = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setObjectName("dim");
        l->setWordWrap(true);
        m_layout->addWidget(l);
    };
    if (!key->hasData()) {
        addNote(key->status().isEmpty() ? tr("Loading the official statistics…") : key->status());
        return;
    }
    const OfficialSources &catalogue = OfficialSources::instance();
    const QString dim = Theme::current().TextDim.name();
    QString html = QString("<table width='100%' cellspacing='0'><tr>"
                           "<td></td><td align='right' style='color:%1; padding-left:8px'>%2</td>"
                           "<td align='right' style='color:%1; padding-left:8px'>%3</td>"
                           "<td align='right' style='color:%1; padding-left:8px'>%4</td></tr>")
                       .arg(dim, tr("Last"), tr("Previous"), tr("Period"));
    int rows = 0;
    QStringList sourceNotes;
    QSet<QString> seen;
    for (int i = 0; i < KeyIndicators::Count; ++i) {
        const auto id = KeyIndicators::Id(i);
        const KeyIndicators::Figure f = key->figure(iso3, id);
        if (!f.valid())
            continue;
        const KeyIndicators::Info info = KeyIndicators::info(id);
        // The office credited by Trading Economics for this country and indicator, then the compilation read.
        QString source = f.via;
        if (const OfficialSources::Source *s = catalogue.sourceFor(iso3, info.teSlug); s && !s->name.isEmpty()
            && !s->name.contains(f.via, Qt::CaseInsensitive) && !f.via.contains(s->name, Qt::CaseInsensitive))
            source = s->name + " · " + f.via;
        const QString tip = tr("%1 · %2").arg(info.name, source).toHtmlEscaped();
        html += QString("<tr title='%5'><td style='padding:3px 0; color:%1'>%2</td>"
                        "<td align='right' style='padding:3px 0 3px 8px; white-space:nowrap'><b>%3</b></td>"
                        "<td align='right' style='padding:3px 0 3px 8px; white-space:nowrap; color:%1'>%4</td>"
                        "<td align='right' style='padding:3px 0 3px 8px; white-space:nowrap; color:%1'>%6</td></tr>")
                    .arg(dim, info.name, key->format(id, f.value), key->format(id, f.previous), tip, f.period());
        ++rows;
        // Each body once in the note, whatever line it came with.
        for (const QString &part : source.split(QStringLiteral(" · ")))
            if (!seen.contains(part.toLower())) {
                seen.insert(part.toLower());
                sourceNotes << part;
            }
    }
    html += "</table>";
    if (!rows) {
        addNote(tr("No monthly figures for this country in Eurostat, the OECD, the IMF's price database or the BLS."));
        return;
    }
    auto *table = new QLabel(html);
    table->setObjectName("stats");
    table->setTextFormat(Qt::RichText);
    table->setWordWrap(true);
    m_layout->addWidget(table);
    addNote(tr("Sources: %1.").arg(sourceNotes.join(" · ")));
}

void DetailPanel::addTradeByProduct(const QString &iso3)
{
    ComtradeProducts *comtrade = m_macro->comtrade();
    if (!comtrade || !comtrade->isEnabled())
        return;
    m_layout->addSpacing(8);
    HelpButton *help = addTitleWithHelp(m_layout, tr("Trade by product"), 420);
    help->setBlocks({{tr("The country's exports and imports by product (chapters of the Harmonised System) for the latest "
                         "full year it has reported to UN Comtrade, as a share of its total. Without a Comtrade key the "
                         "public preview endpoint is used, which allows a few requests a day; a free key (Sources) lifts "
                         "the limit."),
                      {}}});
    auto addNote = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setObjectName("dim");
        l->setWordWrap(true);
        m_layout->addWidget(l);
    };
    // Ask first: a cached copy is read at once, a download comes back through ready().
    if (!comtrade->isFetching(iso3) && !comtrade->trade(iso3).valid() && comtrade->trade(iso3).error.isEmpty()) {
        connect(comtrade, &ComtradeProducts::ready, this, [this, iso3](const QString &done) {
            if (done == iso3 && m_mode == Mode::Country && m_country == iso3)
                refreshMacro();
        }, Qt::SingleShotConnection);
        comtrade->request(iso3);
    }
    const ComtradeProducts::Trade trade = comtrade->trade(iso3);
    if (!trade.valid()) {
        addNote(!trade.error.isEmpty() ? trade.error : tr("Downloading from UN Comtrade…"));
        return;
    }
    const QString dim = Theme::current().TextDim.name();
    const QLocale loc;
    auto table = [&](const QString &title, const QVector<ComtradeProducts::Product> &list, double total) {
        if (list.isEmpty() || total <= 0)
            return;
        QString html = QString("<table width='100%' cellspacing='0'><tr><td colspan='3' style='color:%1; padding:4px 0'>%2 · %3</td></tr>")
                           .arg(dim, title, tr("$%1 bn").arg(loc.toString(total / 1e9, 'f', total >= 1e11 ? 0 : 1)));
        for (int i = 0; i < list.size() && i < 8; ++i) {
            const ComtradeProducts::Product &p = list[i];
            QString name = p.name;
            if (name.size() > 46)
                name = name.left(44).trimmed() + "…";
            html += QString("<tr title='%4'><td style='padding:2px 0; color:%1'>%2</td>"
                            "<td align='right' style='padding:2px 0 2px 8px; white-space:nowrap'>%3</td>"
                            "<td align='right' style='padding:2px 0 2px 8px; white-space:nowrap; color:%1'>%5%</td></tr>")
                        .arg(dim, name.toHtmlEscaped(), tr("$%1 bn").arg(loc.toString(p.value / 1e9, 'f', p.value >= 1e10 ? 0 : 1)),
                             (p.code + " · " + p.name).toHtmlEscaped(), loc.toString(100 * p.value / total, 'f', 1));
        }
        html += "</table>";
        auto *label = new QLabel(html);
        label->setObjectName("stats");
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        m_layout->addWidget(label);
    };
    table(tr("Exports %1").arg(trade.year), trade.exports, trade.exportTotal);
    table(tr("Imports %1").arg(trade.year), trade.imports, trade.importTotal);
    addNote(tr("Source: UN Comtrade, goods, as reported by the country, HS chapters."));
}

void DetailPanel::addOfficialSources(const QString &iso3)
{
    const OfficialSources &catalogue = OfficialSources::instance();
    const QVector<OfficialSources::CountrySource> list = catalogue.forCountry(iso3);
    if (list.isEmpty())
        return;
    m_layout->addSpacing(8);
    HelpButton *help = addTitleWithHelp(m_layout, tr("Official sources"), 420);
    help->setBlocks({{tr("The bodies behind this country's statistics (statistical office, central bank, ministries, "
                         "international organisations, private institutes), with the number of indicators each one "
                         "publishes. Click a name to open its site. The access notes say whether the data is free and "
                         "how a program can read it; nearly all official bodies publish for free, private institutes "
                         "publish their headline figures for free and sell the series. The whole list, with the state of "
                         "each site, is in the Sources tab."),
                      {}}});
    const QString dim = Theme::current().TextDim.name();
    const QString accent = Theme::current().Accent.name();
    QString html = "<table width='100%' cellspacing='0'>";
    int total = 0;
    for (const auto &cs : list) {
        const OfficialSources::Source &s = catalogue.sources()[cs.source];
        total += cs.indicators;
        QStringList titles;
        for (int i = 0; i < cs.slugs.size() && i < 12; ++i)
            titles << OfficialSources::slugTitle(cs.slugs[i]);
        if (cs.slugs.size() > 12)
            titles << tr("and %1 more").arg(cs.slugs.size() - 12);
        const QString name = s.url.isEmpty()
                                 ? s.name.toHtmlEscaped()
                                 : QString("<a href='%1' style='color:%2; text-decoration:none'>%3</a>")
                                       .arg(s.url.toHtmlEscaped(), accent, s.name.toHtmlEscaped());
        QString access = OfficialSources::accessName(s.access);
        if (s.api != "web" && !s.api.isEmpty())
            access += " · " + s.api;
        html += QString("<tr title='%5'><td style='padding:3px 0'>%1<br><span style='color:%2'>%3 · %4</span></td>"
                        "<td align='right' valign='top' style='padding:3px 0 3px 8px; white-space:nowrap; color:%2'>%6</td></tr>")
                    .arg(name, dim, OfficialSources::kindName(s.kind), access.toHtmlEscaped(),
                         titles.join(", ").toHtmlEscaped(), tr("%n indicator(s)", nullptr, cs.indicators));
    }
    html += "</table>";
    auto *table = new QLabel(html);
    table->setObjectName("stats");
    table->setTextFormat(Qt::RichText);
    table->setWordWrap(true);
    table->setOpenExternalLinks(true);
    table->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_layout->addWidget(table);
    auto *note = new QLabel(tr("%n indicator(s) published by %1 sources.", nullptr, total).arg(list.size()));
    note->setObjectName("dim");
    note->setWordWrap(true);
    m_layout->addWidget(note);
}

void DetailPanel::addSectors(const QString &iso3)
{
    using S = EconomySectors;
    const EconomySectors *sectors = m_macro->sectors();
    m_layout->addSpacing(8);
    // The help explains the sectors in general until the country's figures are known.
    SectorFigures figures;
    figures.country = m_macro->economy()->displayName(iso3).toHtmlEscaped();
    HelpButton *help = addTitleWithHelp(m_layout, tr("Economic sectors"), 420);
    help->setBlocks(sectorsHelp(figures));
    auto addNote = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setObjectName("dim");
        l->setWordWrap(true);
        m_layout->addWidget(l);
    };
    if (!sectors->hasData()) {
        addNote(!sectors->isEnabled()      ? tr("The World Bank is turned off in Sources.")
                : sectors->errorString().isEmpty() ? tr("Loading World Bank data…")
                                                   : sectors->errorString());
        return;
    }
    // The latest year with all three sectors.
    auto latest = [&](S::Series first) {
        for (int y = sectors->latestYear(iso3, S::Series(first + 2)); y > 0 && y > MacroIndicators::currentYear() - 14; --y)
            if (std::isfinite(sectors->value(iso3, first, y)) && std::isfinite(sectors->value(iso3, S::Series(first + 1), y))
                && std::isfinite(sectors->value(iso3, S::Series(first + 2), y)))
                return y;
        return 0;
    };
    const S::Series gdpSeries[3] = {S::PrimaryGdp, S::SecondaryGdp, S::TertiaryGdp};
    const S::Series jobSeries[3] = {S::PrimaryJobs, S::SecondaryJobs, S::TertiaryJobs};
    const int gdpYear = latest(S::PrimaryGdp);
    const int jobsYear = latest(S::PrimaryJobs);
    if (!gdpYear && !jobsYear) {
        addNote(tr("The World Bank has no figures for this country."));
        return;
    }
    const QString names[3] = {tr("Primary"), tr("Secondary"), tr("Tertiary")};
    const QString scopes[3] = {tr("agriculture, forestry, fishing"), tr("industry, mining, energy, construction"),
                               tr("services")};
    const QLocale loc;
    auto pct = [&](double v) { return std::isfinite(v) ? tr("%1%").arg(loc.toString(v, 'f', 1)) : QString::fromUtf8("—"); };
    const double gdp = gdpYear ? m_macro->value(MacroIndicators::GdpSize, iso3, gdpYear) : std::nan("");
    auto worth = [&](double share) {
        return std::isfinite(share) && std::isfinite(gdp) ? m_macro->format(MacroIndicators::GdpSize, gdp * share / 100)
                                                           : QString::fromUtf8("—");
    };

    QVector<SectorChart::Bar> bars;
    if (gdpYear) {
        SectorChart::Bar b {tr("Share of GDP · %1").arg(gdpYear), {}, {}};
        for (int k = 0; k < 3; ++k) {
            b.shares[k] = sectors->value(iso3, gdpSeries[k], gdpYear);
            b.tips[k] = tr("%1 (%2): %3 of GDP, %4").arg(names[k], scopes[k], pct(b.shares[k]), worth(b.shares[k]));
        }
        bars.push_back(b);
    }
    if (jobsYear) {
        SectorChart::Bar b {tr("Share of jobs · %1").arg(jobsYear), {}, {}};
        for (int k = 0; k < 3; ++k) {
            b.shares[k] = sectors->value(iso3, jobSeries[k], jobsYear);
            b.tips[k] = tr("%1 (%2): %3 of jobs").arg(names[k], scopes[k], pct(b.shares[k]));
        }
        bars.push_back(b);
    }
    auto *chart = new SectorChart;
    chart->setBars(bars);
    m_layout->addWidget(chart);
    m_layout->addSpacing(4);

    // Table: share of GDP, value added in dollars, share of jobs.
    const QString dim = Theme::current().TextDim.name();
    auto head = [&](const QString &text) {
        return QString("<td align='right' style='padding:2px 0 4px 10px; color:%1'>%2</td>").arg(dim, text);
    };
    auto cell = [](const QString &text) {
        return QString("<td align='right' style='padding:3px 0 3px 10px; white-space:nowrap'>%1</td>").arg(text);
    };
    QString html = "<table width='100%'><tr><td></td>" + head(tr("GDP")) + head(tr("Value"))
                   + head(tr("Jobs")) + "</tr>";
    for (int k = 0; k < 3; ++k) {
        const double share = gdpYear ? sectors->value(iso3, gdpSeries[k], gdpYear) : std::nan("");
        html += QString("<tr><td style='padding:3px 0; white-space:nowrap'><span style='color:%1'>■</span>&nbsp;%2</td>")
                    .arg(SectorChart::color(k).name(), names[k])
                + cell(pct(share)) + cell(worth(share))
                + cell(pct(jobsYear ? sectors->value(iso3, jobSeries[k], jobsYear) : std::nan(""))) + "</tr>";
        if (k == 1 && gdpYear) {
            const double manufacturing = sectors->value(iso3, S::ManufacturingGdp, gdpYear);
            if (std::isfinite(manufacturing))
                html += QString("<tr><td style='padding:3px 0 3px 14px; color:%1'>%2</td>")
                            .arg(dim, tr("of which manufacturing"))
                        + cell(pct(manufacturing)) + cell(worth(manufacturing)) + cell(QString()) + "</tr>";
        }
    }
    html += "</table>";
    auto *table = new QLabel(html);
    table->setObjectName("stats");
    table->setTextFormat(Qt::RichText);
    table->setWordWrap(true);
    m_layout->addWidget(table);

    // Change in the shares of GDP over ten years, or since the oldest year at least five years back.
    int base = 0;
    for (int y = gdpYear - 10; gdpYear && y <= gdpYear - 5 && !base; ++y)
        if (std::isfinite(sectors->value(iso3, S::PrimaryGdp, y)) && std::isfinite(sectors->value(iso3, S::SecondaryGdp, y))
            && std::isfinite(sectors->value(iso3, S::TertiaryGdp, y)))
            base = y;
    QStringList changes;
    for (int k = 0; k < 3 && base; ++k) {
        const double delta = sectors->value(iso3, gdpSeries[k], gdpYear) - sectors->value(iso3, gdpSeries[k], base);
        const QString sign = delta > 0.05 ? "+" : delta < -0.05 ? QString::fromUtf8("−") : "";
        changes << tr("%1 %2%3 pts").arg(names[k].toLower(), sign, loc.toString(std::abs(delta), 'f', 1));
    }
    QString text;
    if (changes.size() == 3)
        text = tr("Since %1, in share of GDP: %2.").arg(base).arg(changes.join(", ")) + "<br>";

    figures.gdpYear = gdpYear;
    figures.jobsYear = jobsYear;
    figures.base = changes.size() == 3 ? base : 0;
    for (int k = 0; k < 3; ++k) {
        figures.gdp[k] = sectors->value(iso3, gdpSeries[k], gdpYear);
        figures.jobs[k] = sectors->value(iso3, jobSeries[k], jobsYear);
        figures.then[k] = sectors->value(iso3, gdpSeries[k], base);
    }
    figures.manufacturing = sectors->value(iso3, S::ManufacturingGdp, gdpYear);
    figures.iso3 = iso3;
    figures.sectors = sectors;
    help->setBlocks(sectorsHelp(figures));
    text += tr("Primary: %1. Secondary: %2. Tertiary: %3.").arg(scopes[0], scopes[1], scopes[2]);
    auto *note = new QLabel(text);
    note->setObjectName("dim");
    note->setTextFormat(Qt::RichText);
    note->setWordWrap(true);
    m_layout->addWidget(note);
}

void DetailPanel::addProperty(const QString &iso3)
{
    const MacroIndicators::Finance &fin = m_macro->finance();
    const QLocale loc;
    m_layout->addSpacing(8);
    PropertyFigures figures;
    figures.country = m_macro->economy()->displayName(iso3).toHtmlEscaped();
    HelpButton *help = addTitleWithHelp(m_layout, tr("Real estate"), 360);
    auto addNote = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setObjectName("dim");
        l->setTextFormat(Qt::RichText);
        l->setWordWrap(true);
        m_layout->addWidget(l);
    };

    const QMap<double, double> real = fin.bis->series(iso3, PropertyCredit::RealHousePrices);
    double incomeTime = 0, rentTime = 0;
    figures.priceToIncome = fin.oecd->latest(iso3, HousePriceRatios::PriceToIncome, &incomeTime);
    figures.priceToRent = fin.oecd->latest(iso3, HousePriceRatios::PriceToRent, &rentTime);
    double creditTime = 0;
    figures.householdCredit = fin.bis->latest(iso3, PropertyCredit::HouseholdCredit, &creditTime);
    if (std::isfinite(figures.householdCredit))
        figures.householdCreditFiveYears =
            figures.householdCredit - fin.bis->at(iso3, PropertyCredit::HouseholdCredit, creditTime - 5);
    if (real.isEmpty() && !std::isfinite(figures.priceToIncome)) {
        addNote(!fin.bis->hasData() ? (fin.bis->isEnabled() ? tr("Loading BIS data…")
                                                            : tr("The BIS is turned off in Sources."))
                                    : tr("Neither the BIS nor the OECD publishes house prices for this country."));
        help->setBlocks({{propertyHelpHtml(figures), {}}});
        return;
    }

    if (!real.isEmpty()) {
        figures.last = real.lastKey();
        const double now = real.last();
        auto change = [&](double years) {
            const double then = fin.bis->at(iso3, PropertyCredit::RealHousePrices, figures.last - years);
            return std::isfinite(then) && then > 0 ? (now / then - 1) * 100 : std::nan("");
        };
        figures.year = change(1);
        figures.fiveYears = change(5);
        figures.tenYears = change(10);
        auto peak = std::max_element(real.begin(), real.end());
        figures.peakTime = peak.key();
        figures.fromPeak = (now / peak.value() - 1) * 100;

        // Headline: the change over a year and ten years, and the distance from the peak.
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(12);
        auto addStat = [&](int col, double value, const QString &label) {
            auto *v = new QLabel(std::isfinite(value) ? signedText(value, tr("%1%").arg(QString())) : QString::fromUtf8("—"));
            v->setObjectName("midStat");
            auto *l = new QLabel(label.toUpper());
            l->setObjectName("sectionLabelTight");
            l->setWordWrap(true);
            grid->addWidget(v, 0, col);
            grid->addWidget(l, 1, col);
        };
        addStat(0, figures.year, tr("Real prices, 1 year"));
        addStat(1, figures.tenYears, tr("Real prices, 10 years"));
        addStat(2, figures.fromPeak, figures.peakTime == figures.last ? tr("At their peak") : tr("From the %1 peak").arg(int(figures.peakTime)));
        m_layout->addLayout(grid);
        m_layout->addSpacing(4);

        auto *chart = new TimeSeriesChart;
        chart->setLines({{tr("Real house prices (2010 = 100)"), Theme::current().Accent, real}},
                        [loc](double v) { return loc.toString(v, 'f', 0); }, 100);
        m_layout->addWidget(chart);
    }

    // Valuation against incomes and rents.
    if (std::isfinite(figures.priceToIncome) || std::isfinite(figures.priceToRent)) {
        const QString dim = Theme::current().TextDim.name();
        QString html = "<table width='100%'>";
        auto row = [&](const QString &name, double ratio) {
            if (!std::isfinite(ratio))
                return;
            html += QString("<tr><td style='padding:3px 0'>%1</td>"
                            "<td align='right' style='padding:3px 0 3px 10px; white-space:nowrap'>%2</td>"
                            "<td align='right' style='padding:3px 0 3px 10px; color:%3; white-space:nowrap'>%4</td></tr>")
                        .arg(name, tr("%1%").arg(loc.toString(ratio, 'f', 0)), dim, valuationWord(ratio));
        };
        row(tr("Price to income"), figures.priceToIncome);
        row(tr("Price to rent"), figures.priceToRent);
        html += "</table>";
        auto *table = new QLabel(html);
        table->setObjectName("stats");
        table->setTextFormat(Qt::RichText);
        table->setWordWrap(true);
        m_layout->addWidget(table);
    }

    QStringList sources;
    if (!real.isEmpty())
        sources << tr("Real prices: BIS, %1 (national index deflated by consumer prices).").arg(periodLabel(figures.last));
    if (std::isfinite(figures.priceToIncome) || std::isfinite(figures.priceToRent))
        sources << tr("Ratios: OECD, %1, in % of their long-term average.").arg(periodLabel(std::max(incomeTime, rentTime)));
    addNote(sources.join(' '));
    help->setBlocks({{propertyHelpHtml(figures), {}}});
}

void DetailPanel::addDebt(const QString &iso3)
{
    const MacroIndicators::Finance &fin = m_macro->finance();
    const QLocale loc;
    m_layout->addSpacing(8);
    DebtFigures figures;
    figures.country = m_macro->economy()->displayName(iso3).toHtmlEscaped();
    HelpButton *help = addTitleWithHelp(m_layout, tr("Debt"), 360);
    auto pct = [&](double v) { return std::isfinite(v) ? tr("%1%").arg(loc.toString(v, 'f', 1)) : QString::fromUtf8("—"); };
    auto change = [&](double v) { return std::isfinite(v) ? signedText(v, tr(" pts")) : QString::fromUtf8("—"); };

    // Households and companies: the BIS (quarterly, recent) when it covers the country, else the IMF (annual).
    const int bisSeries[2] = {PropertyCredit::HouseholdCredit, PropertyCredit::CorporateCredit};
    const int imfSeries[2] = {GlobalDebt::HouseholdDebt, GlobalDebt::CorporateDebt};
    QString privateSource;
    QMap<double, double> history[3];
    double median[3] = {std::nan(""), std::nan(""), std::nan("")};
    for (int k = 0; k < 2; ++k) {
        const CountrySeriesSet *set = fin.bis;
        int series = bisSeries[k];
        if (fin.bis->series(iso3, series).isEmpty()) {
            set = fin.imf;
            series = imfSeries[k];
        }
        double t = 0;
        figures.level[k] = set->latest(iso3, series, &t);
        if (!std::isfinite(figures.level[k]))
            continue;
        const double tolerance = set == fin.bis ? 0.3 : 0.1;
        figures.fiveYears[k] = figures.level[k] - set->at(iso3, series, t - 5, tolerance);
        figures.tenYears[k] = figures.level[k] - set->at(iso3, series, t - 10, tolerance);
        median[k] = set->median(series);
        history[k] = set->series(iso3, series);
        privateSource = set == fin.bis ? tr("BIS, %1").arg(periodLabel(t)) : tr("IMF Global Debt Database, %1").arg(int(t));
    }
    // The government: the WEO (this year's estimate, as in the table above), its long history from the IMF database.
    const int year = MacroIndicators::currentYear();
    figures.level[2] = m_macro->value(MacroIndicators::GovDebt, iso3, year);
    figures.fiveYears[2] = figures.level[2] - m_macro->value(MacroIndicators::GovDebt, iso3, year - 5);
    figures.tenYears[2] = figures.level[2] - m_macro->value(MacroIndicators::GovDebt, iso3, year - 10);
    {
        QVector<double> all;
        for (double v : m_macro->values(MacroIndicators::GovDebt, year))
            all.push_back(v);
        std::sort(all.begin(), all.end());
        if (!all.isEmpty())
            median[2] = all[all.size() / 2];
    }
    history[2] = fin.imf->series(iso3, GlobalDebt::GovernmentDebt);
    if (history[2].isEmpty())
        for (int y = MacroIndicators::firstYear(); y <= year; ++y)
            if (const double v = m_macro->value(MacroIndicators::GovDebt, iso3, y); std::isfinite(v))
                history[2].insert(y, v);

    double serviceTime = 0, privateServiceTime = 0;
    figures.householdService = fin.bis->latest(iso3, PropertyCredit::HouseholdDebtService, &serviceTime);
    figures.privateService = fin.bis->latest(iso3, PropertyCredit::PrivateDebtService, &privateServiceTime);
    serviceTime = std::max(serviceTime, privateServiceTime);
    auto average = [](const QMap<double, double> &s) {
        double sum = 0;
        for (double v : s)
            sum += v;
        return s.isEmpty() ? std::nan("") : sum / s.size();
    };
    figures.householdServiceAverage = average(fin.bis->series(iso3, PropertyCredit::HouseholdDebtService));
    figures.privateServiceAverage = average(fin.bis->series(iso3, PropertyCredit::PrivateDebtService));
    double creditYear = 0, badYear = 0;
    figures.privateCredit = fin.worldBank->latest(iso3, BankCredit::PrivateCredit, &creditYear);
    figures.badLoans = fin.worldBank->latest(iso3, BankCredit::BadLoans, &badYear);

    // Table: % of GDP, change over ten years, median of the countries covered.
    const QString dim = Theme::current().TextDim.name();
    auto head = [&](const QString &text) {
        return QString("<td align='right' style='padding:2px 0 4px 10px; color:%1'>%2</td>").arg(dim, text);
    };
    auto cell = [](const QString &text, const QString &color = QString()) {
        return QString("<td align='right' style='padding:3px 0 3px 10px; white-space:nowrap%1'>%2</td>")
            .arg(color.isEmpty() ? QString() : "; color:" + color, text);
    };
    const QString names[3] = {tr("Households"), tr("Companies"), tr("Government")};
    QString html = "<table width='100%'><tr><td></td>" + head(tr("% GDP")) + head(tr("10 years")) + head(tr("Median"))
                   + "</tr>";
    double total = 0, totalChange = 0;
    bool complete = true;
    for (int k = 0; k < 3; ++k) {
        html += QString("<tr><td style='padding:3px 0; white-space:nowrap'><span style='color:%1'>■</span>&nbsp;%2</td>")
                    .arg(debtColor(k).name(), names[k])
                + cell(pct(figures.level[k])) + cell(change(figures.tenYears[k])) + cell(pct(median[k]), dim) + "</tr>";
        complete = complete && std::isfinite(figures.level[k]) && std::isfinite(figures.tenYears[k]);
        total += figures.level[k];
        totalChange += figures.tenYears[k];
    }
    if (complete)
        html += QString("<tr><td style='padding:3px 0'><b>%1</b></td>").arg(tr("Total"))
                + cell("<b>" + pct(total) + "</b>") + cell(change(totalChange)) + cell(QString()) + "</tr>";
    html += "</table>";
    auto *table = new QLabel(html);
    table->setObjectName("stats");
    table->setTextFormat(Qt::RichText);
    table->setWordWrap(true);
    m_layout->addWidget(table);

    if (!history[0].isEmpty() || !history[1].isEmpty() || history[2].size() > 1) {
        QVector<TimeSeriesChart::Line> lines;
        for (int k = 0; k < 3; ++k)
            lines.push_back({names[k], debtColor(k), history[k]});
        auto *chart = new TimeSeriesChart;
        chart->setLines(lines, [loc](double v) { return tr("%1%").arg(loc.toString(v, 'f', 0)); }, std::nan(""));
        m_layout->addWidget(chart);
    }

    // Debt service, bank credit, bad loans, then the sources.
    QStringList lines;
    if (std::isfinite(figures.householdService))
        lines << tr("<b>Debt service</b>: households %1 of their income (%2 on average since %3)")
                     .arg(pct(figures.householdService), pct(figures.householdServiceAverage))
                     .arg(int(fin.bis->series(iso3, PropertyCredit::HouseholdDebtService).firstKey()))
                     + (std::isfinite(figures.privateService)
                            ? tr(", households and companies %1 (%2 on average).")
                                  .arg(pct(figures.privateService), pct(figures.privateServiceAverage))
                            : QStringLiteral("."));
    else if (std::isfinite(figures.privateService))
        lines << tr("<b>Debt service</b>: households and companies %1 of their income (%2 on average).")
                     .arg(pct(figures.privateService), pct(figures.privateServiceAverage));
    if (std::isfinite(figures.privateCredit))
        lines << tr("<b>Bank credit</b> to the private sector: %1 of GDP (%2).").arg(pct(figures.privateCredit)).arg(int(creditYear));
    if (std::isfinite(figures.badLoans))
        lines << tr("<b>Bad loans</b>: %1 of bank loans (%2).").arg(pct(figures.badLoans)).arg(int(badYear));
    if (lines.isEmpty() && !std::isfinite(figures.level[0]) && !std::isfinite(figures.level[2]))
        lines << tr("No debt figures for this country.");
    if (!lines.isEmpty()) {
        auto *l = new QLabel(lines.join("<br>"));
        l->setTextFormat(Qt::RichText);
        l->setWordWrap(true);
        m_layout->addWidget(l);
    }
    QStringList sources;
    if (!privateSource.isEmpty())
        sources << tr("Households and companies: %1; median of the countries it covers.").arg(privateSource);
    if (std::isfinite(figures.level[2]))
        sources << tr("Government: IMF World Economic Outlook, %1 estimate.").arg(year);
    if (std::isfinite(figures.householdService) || std::isfinite(figures.privateService))
        sources << tr("Debt service: BIS, %1.").arg(periodLabel(serviceTime));
    if (std::isfinite(figures.privateCredit) || std::isfinite(figures.badLoans))
        sources << tr("Bank credit and bad loans: World Bank.");
    if (!sources.isEmpty()) {
        auto *note = new QLabel(sources.join(' '));
        note->setObjectName("dim");
        note->setWordWrap(true);
        m_layout->addWidget(note);
    }
    help->setBlocks({{debtHelpHtml(figures), {}}});
}

void DetailPanel::scrollTo(double fraction)
{
    if (m_scroll)
        m_scroll->verticalScrollBar()->setValue(int(m_scroll->verticalScrollBar()->maximum() * fraction));
}

void DetailPanel::refreshMacro()
{
    if (m_mode == Mode::Indicator)
        showIndicator(m_indicator, m_year);
    else if (m_mode == Mode::Country)
        showCountry(QString(m_country));   // a copy: showCountry() clears m_country first
    else if (m_mode == Mode::SupplyOverview)
        showSupplyOverview();
    else if (m_mode == Mode::SupplyPoint)
        showSupplyPoint(QString(m_supplyKey));
}
