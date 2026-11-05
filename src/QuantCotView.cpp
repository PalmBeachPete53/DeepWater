#include "QuantViews.h"

#include "DeskWidgets.h"
#include "ResearchCharts.h"
#include "ResearchData.h"
#include "Theme.h"
#include "desk/Rates.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace quant {

// The positions table of one week: groups of traders across, positions / change / share of open interest /
// traders down.
class CotTable : public QWidget
{
    Q_OBJECT   // tr() must resolve to this class, so that the CotTable context of the .ts applies
public:
    explicit CotTable(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(176);
    }
    void setWeek(const CotReport::Week &week, bool havePrev, const CotReport::Week &prev)
    {
        m_week = week;
        m_havePrev = havePrev;
        m_prev = prev;
        m_valid = true;
        update();
    }
    void clear()
    {
        m_valid = false;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette &c = Theme::current();
        if (!m_valid) {
            p.setPen(c.TextFaint);
            p.setFont(chartFont(11));
            p.drawText(rect(), Qt::AlignCenter, tr("No data"));
            return;
        }
        struct Group { QString name; QStringList cols; };
        const QVector<Group> groups {{tr("Non-Commercial"), {tr("LONG"), tr("SHORT"), tr("SPREADS")}},
                                     {tr("Commercial"), {tr("LONG"), tr("SHORT")}},
                                     {tr("Total"), {tr("LONG"), tr("SHORT")}},
                                     {tr("Non-Reportable"), {tr("LONG"), tr("SHORT")}}};
        const CotReport::Week &w = m_week, &pv = m_prev;
        const double oi = w.openInterest;
        // The nine value columns, in the groups' order.
        const QVector<qint64> pos {w.ncLong, w.ncShort, w.ncSpread, w.cLong, w.cShort, w.trLong, w.trShort, w.nrLong, w.nrShort};
        const QVector<qint64> prevPos {pv.ncLong, pv.ncShort, pv.ncSpread, pv.cLong, pv.cShort, pv.trLong, pv.trShort, pv.nrLong, pv.nrShort};
        const QVector<int> traders {w.tNcLong, w.tNcShort, w.tNcSpread, w.tCLong, w.tCShort, w.tTrLong, w.tTrShort, -1, -1};
        const double labelW = 92, colW = (width() - labelW) / 9.0;
        const double groupH = 20, headH = 18, rowH = 28, top = 6;
        // Group names with a line under their span.
        p.setFont(chartFont(10, true));
        int col = 0;
        for (const Group &g : groups) {
            const QRectF r(labelW + col * colW, top, colW * g.cols.size(), groupH);
            p.setPen(c.Text);
            p.drawText(r, Qt::AlignCenter, g.name);
            p.setPen(QPen(c.PanelLine, 1));
            p.drawLine(QPointF(r.left() + 8, r.bottom()), QPointF(r.right() - 8, r.bottom()));
            p.setFont(chartFont(8));
            for (int i = 0; i < g.cols.size(); ++i) {
                p.setPen(c.TextDim);
                p.drawText(QRectF(labelW + (col + i) * colW, top + groupH, colW, headH), Qt::AlignCenter, g.cols[i]);
            }
            p.setFont(chartFont(10, true));
            col += g.cols.size();
        }
        const QStringList rows {tr("POSITIONS"), tr("CHANGE"), tr("% OF OI"), tr("TRADERS")};
        for (int r = 0; r < rows.size(); ++r) {
            const double y = top + groupH + headH + r * rowH;
            if (r % 2 == 1)
                p.fillRect(QRectF(0, y, width(), rowH), c.Hover);
            p.setFont(chartFont(9));
            p.setPen(c.TextDim);
            p.drawText(QRectF(8, y, labelW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, rows[r]);
            for (int i = 0; i < 9; ++i) {
                const QRectF cell(labelW + i * colW, y, colW, rowH);
                switch (r) {
                case 0:
                    p.setFont(chartFont(11, true));
                    p.setPen(c.Text);
                    p.drawText(cell, Qt::AlignCenter, thousands(pos[i]));
                    break;
                case 1: {
                    if (!m_havePrev) {
                        p.setPen(c.TextFaint);
                        p.drawText(cell, Qt::AlignCenter, "—");
                        break;
                    }
                    const qint64 d = pos[i] - prevPos[i];
                    const QColor color = d > 0 ? c.Up : d < 0 ? c.Down : c.TextDim;
                    p.setFont(chartFont(9, true));
                    const QString text = signedThousands(d);
                    const QFontMetrics fm(p.font());
                    const QRectF badge(cell.center().x() - fm.horizontalAdvance(text) / 2.0 - 6, cell.center().y() - 8,
                                       fm.horizontalAdvance(text) + 12, 16);
                    QColor tint = color;
                    tint.setAlpha(45);
                    p.setPen(Qt::NoPen);
                    p.setBrush(tint);
                    p.drawRoundedRect(badge, 2, 2);
                    p.setPen(color);
                    p.drawText(badge, Qt::AlignCenter, text);
                    break;
                }
                case 2:
                    p.setFont(chartFont(10));
                    p.setPen(c.Text);
                    p.drawText(cell, Qt::AlignCenter, oi > 0 ? QString::number(100.0 * pos[i] / oi, 'f', 1) + "%" : "—");
                    break;
                default:
                    p.setFont(chartFont(10));
                    p.setPen(traders[i] < 0 ? c.TextFaint : c.Text);
                    p.drawText(cell, Qt::AlignCenter, traders[i] < 0 ? "—" : QString::number(traders[i]));
                }
            }
        }
    }

private:
    CotReport::Week m_week, m_prev;
    bool m_havePrev = false, m_valid = false;
};

namespace {

class RowBackground : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const QVariant bg = index.data(Qt::BackgroundRole);
        if (bg.canConvert<QBrush>())
            p->fillRect(opt.rect, bg.value<QBrush>());
        QStyledItemDelegate::paint(p, opt, index);
    }
};

QTableWidgetItem *cell(const QString &text, const QColor &color, Qt::Alignment align = Qt::AlignCenter, bool bold = false)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(align | Qt::AlignVCenter);
    if (color.isValid())
        it->setForeground(color);
    if (bold)
        it->setFont(mono(11, true));
    return it;
}

} // namespace

CotView::CotView(CotReport *report, QWidget *parent)
    : View(parent)
    , m_report(report)
{
    const Palette &c = Theme::current();
    QHBoxLayout *h;
    auto *top = bar(&h);
    m_market = new QComboBox;
    m_market->setObjectName("resCombo");
    m_market->setCursor(Qt::PointingHandCursor);
    m_market->setMinimumWidth(220);
    for (const CotReport::Market &m : m_report->markets())
        m_market->addItem(m.label);
    h->addWidget(m_market);
    h->addStretch(1);
    h->addWidget(label(tr("COT — Commitments of Traders: weekly CFTC report of futures positions (futures only, legacy format)."), "qtNote"));
    content()->addWidget(top);

    QVBoxLayout *body;
    auto *main = card({}, &body);
    body->setContentsMargins(14, 12, 14, 12);
    body->setSpacing(8);
    auto *head = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    m_title = label("—", "qtTitle");
    titles->addWidget(m_title);
    titles->addWidget(label(tr("FUTURES ONLY POSITIONS"), "qtKicker"));
    m_code = label({}, "qtNote");
    titles->addWidget(m_code);
    head->addLayout(titles);
    head->addStretch(1);
    auto *dates = new QVBoxLayout;
    dates->setSpacing(4);
    m_asOf = label("—", "qtAsOf");
    m_asOf->setAlignment(Qt::AlignRight);
    dates->addWidget(m_asOf);
    m_release = label("—", "qtChipLabel");
    m_release->setAlignment(Qt::AlignRight);
    dates->addWidget(m_release, 0, Qt::AlignRight);
    head->addLayout(dates);
    body->addLayout(head);
    m_table = new CotTable;
    body->addWidget(m_table);
    auto *foot = new QHBoxLayout;
    m_units = label({}, "qtNote");
    foot->addWidget(m_units);
    foot->addStretch(1);
    m_totals = label({}, "qtNote");
    m_totals->setTextFormat(Qt::RichText);
    foot->addWidget(m_totals);
    body->addLayout(foot);

    auto *charts = new QHBoxLayout;
    charts->setSpacing(10);
    QVBoxLayout *lb;
    auto *lsCard = card({}, &lb);
    lb->setContentsMargins(4, 4, 4, 4);
    m_longShort = new LongShortBars;
    m_longShort->setTitle({{tr("Long vs. Short"), c.Text}});
    m_longShort->setFixedSize(320, 230);
    lb->addWidget(m_longShort);
    charts->addWidget(lsCard);
    auto *netCard = card({}, &lb);
    lb->setContentsMargins(4, 4, 4, 4);
    m_net = new StepChart;
    m_net->setTitle({{tr("Net Positions"), c.Text}});
    m_net->setFixedHeight(230);
    m_net->setDateFormat("yyyy-MM-dd");
    lb->addWidget(m_net);
    charts->addWidget(netCard, 1);
    body->addLayout(charts);
    content()->addWidget(main);

    // How stretched the positioning is: this week against the last 52.
    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(10);
    tiles->addWidget(deskui::tile(tr("Speculators, net"), &m_specNet, &m_specNetSub), 1);
    tiles->addWidget(deskui::tile(tr("Speculators long"), &m_specLong, &m_specLongSub), 1);
    tiles->addWidget(deskui::tile(tr("Speculators in their 52-week range"), &m_specRange, &m_specRangeSub), 1);
    tiles->addWidget(deskui::tile(tr("Commercials in their 52-week range"), &m_commRange, &m_commRangeSub), 1);
    content()->addLayout(tiles);
    QHBoxLayout *readHead;
    QVBoxLayout *readBody;
    auto *readCard = card(tr("Plain reading"), &readBody, &readHead);
    auto *askButton = new QPushButton(tr("Ask the AI to read it"));
    askButton->setObjectName("qtChip");
    askButton->setCursor(Qt::PointingHandCursor);
    readHead->addWidget(askButton);
    connect(askButton, &QPushButton::clicked, this, [this] {
        emit askAi(tr("Read the COT section of the Quant page with your tool for reading pages, then say in a few sentences what the positioning of %1 shows and what it doesn't. Don't predict the price.").arg(m_market->currentText()));
    });
    m_reading = label({}, "qtNote");
    m_reading->setWordWrap(true);
    readBody->addWidget(m_reading);
    content()->addWidget(readCard);

    // Who holds it, for the financial markets: the Traders in Financial Futures report.
    QVBoxLayout *tffBody;
    QHBoxLayout *tffHead;
    m_tffCard = card(tr("Traders in financial futures"), &tffBody, &tffHead);
    m_tffDate = label({}, "qtNote");
    tffHead->addWidget(m_tffDate);
    auto *tffRow = new QHBoxLayout;
    tffRow->setSpacing(10);
    m_tff = deskui::table({tr("Category"), tr("Long"), tr("Short"), tr("Net"), tr("Change"), tr("% of OI"), tr("52-week range")});
    m_tff->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);   // names stay whole
    tffRow->addWidget(m_tff, 3, Qt::AlignTop);
    m_tffChart = new MultiLineChart;
    m_tffChart->setMinimumHeight(190);
    m_tffChart->setZeroLine(true);
    m_tffChart->setLegend(true);
    tffRow->addWidget(m_tffChart, 2);
    tffBody->addLayout(tffRow);
    auto *tffNote = label(tr("The CFTC's split of a financial future's holders, published with the legacy report. Asset managers are mostly hedged long-term money; leveraged funds are the fast money. Where a market has several contracts, this is its main one."), "qtNote");
    tffNote->setWordWrap(true);
    tffBody->addWidget(tffNote);
    content()->addWidget(m_tffCard);

    QHBoxLayout *hh;
    auto *history = card({}, &body, &hh);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    hh->setContentsMargins(12, 8, 12, 8);
    m_historyTitle = label(tr("HISTORICAL DATA"), "qtCardTitle");
    m_historyTitle->setStyleSheet(QString("color: %1").arg(c.Text.name()));
    hh->insertWidget(0, m_historyTitle);
    m_historyToggle = new QPushButton("▲");
    m_historyToggle->setObjectName("qtIconButton");
    m_historyToggle->setCursor(Qt::PointingHandCursor);
    hh->addWidget(m_historyToggle);
    m_history = new QTableWidget(0, 6);
    m_history->setObjectName("qtTable");
    m_history->setItemDelegate(new RowBackground(m_history));
    m_history->setHorizontalHeaderLabels({tr("AS OF"), tr("Open Interest"), tr("Δ OI"), tr("NC Net"), tr("Comm Net"), tr("NR Net")});
    m_history->verticalHeader()->hide();
    m_history->verticalHeader()->setDefaultSectionSize(26);
    m_history->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_history->horizontalHeader()->setHighlightSections(false);
    m_history->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->setSelectionMode(QAbstractItemView::NoSelection);
    m_history->setFocusPolicy(Qt::NoFocus);
    m_history->setShowGrid(false);
    m_history->setFrameShape(QFrame::NoFrame);
    m_history->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_history->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int col = 0; col < 6; ++col)
        m_history->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    body->addWidget(m_history);
    content()->addWidget(history);
    content()->addStretch(1);

    m_market->setCurrentIndex(std::clamp(QSettings().value("quant/cot/market", 0).toInt(), 0, m_market->count() - 1));
    connect(m_market, &QComboBox::currentIndexChanged, this, [this](int index) {
        QSettings().setValue("quant/cot/market", index);
        fill();
    });
    connect(m_historyToggle, &QPushButton::clicked, this, [this] {
        m_history->setVisible(!m_history->isVisible());
        m_historyToggle->setText(m_history->isVisible() ? "▲" : "▼");
    });
    connect(m_report, &RemoteDataSet::updated, this, &CotView::fill);
}

void CotView::load()
{
    m_report->refreshIfStale();
    fill();
}

void CotView::fillReading(const CotReport::Series &s)
{
    const CotReport::Week &w = s.weeks.last();
    QVector<double> spec, comm;
    for (const CotReport::Week &k : s.weeks) {
        spec.push_back(k.ncNet());
        comm.push_back(k.cNet());
    }
    const double specRange = desk::rangePosition(spec), commRange = desk::rangePosition(comm);
    const double change = s.weeks.size() >= 2 ? spec.last() - spec[spec.size() - 2] : 0;
    const qint64 gross = w.ncLong + w.ncShort;
    const double longShare = gross > 0 ? 100.0 * w.ncLong / gross : 0;
    deskui::setSigned(m_specNet, signedThousands(w.ncNet()), w.ncNet());
    m_specNetSub->setText(tr("%1 this week").arg(signedThousands(change)));
    m_specLong->setText(QString("%1%").arg(deskui::amount(longShare, 0)));
    m_specLongSub->setText(tr("of their long and short contracts"));
    m_specRange->setText(QString("%1%").arg(deskui::amount(specRange, 0)));
    m_specRangeSub->setText(tr("0 the lowest net of the year, 100 the highest"));
    m_commRange->setText(QString("%1%").arg(deskui::amount(commRange, 0)));
    m_commRangeSub->setText(tr("usually the other side of the speculators"));

    QStringList text;
    text << (w.ncNet() >= 0 ? tr("Speculators are net long %1 contracts").arg(thousands(w.ncNet()))
                            : tr("Speculators are net short %1 contracts").arg(thousands(-w.ncNet())));
    if (specRange >= 80)
        text << tr("close to the most long they have been in a year");
    else if (specRange <= 20)
        text << tr("close to the most short they have been in a year");
    else
        text << tr("inside their range of the last year");
    QString sentence = text.join(", ") + ". ";
    if (s.weeks.size() >= 2)
        sentence += change > 0 ? tr("Their net position rose by %1 this week. ").arg(thousands(change))
                  : change < 0 ? tr("Their net position fell by %1 this week. ").arg(thousands(-change))
                               : tr("Their net position didn't change this week. ");
    sentence += tr("Positions are as of Tuesday, so a few days old, and an extreme says the trade is crowded, not when it turns.");
    m_reading->setText(sentence);
}

void CotView::fetchTff(const CotReport::Market &market)
{
    const int serial = ++m_tffSerial;
    m_tffCard->setVisible(!market.tff.isEmpty());
    if (market.tff.isEmpty())
        return;
    m_tff->setRowCount(0);
    m_tffDate->setText(tr("Loading…"));
    const QString since = QDate::currentDate().addDays(-2 * 365).toString(Qt::ISODate);
    const QString where = QString("cftc_contract_market_code='%1' AND report_date_as_yyyy_mm_dd>='%2'").arg(market.tff, since);
    QUrl url("https://publicreporting.cftc.gov/resource/gpe5-46if.json");
    url.setQuery(QString("$where=%1&$order=report_date_as_yyyy_mm_dd&$limit=200").arg(QString::fromLatin1(QUrl::toPercentEncoding(where))), QUrl::StrictMode);
    research::Client::instance()->raw("cftc:tff:" + market.tff, url, 12 * 3600, true, this, [this, serial](const QByteArray &body, const QString &error) {
        if (serial != m_tffSerial)
            return;
        const QVector<desk::TffWeek> weeks = desk::parseTff(body);
        const Palette &pal = Theme::current();
        if (weeks.isEmpty()) {
            m_tffDate->setText(tr("Unavailable: %1").arg(error.isEmpty() ? tr("no data") : error));
            return;
        }
        m_tffDate->setText(tr("as of %1").arg(weeks.last().date.toString(Qt::ISODate)));
        struct Row {
            QString name;
            std::function<qint64(const desk::TffWeek &)> lg, sh;
        };
        const QVector<Row> rows {
            {tr("Dealers"), [](const desk::TffWeek &w) { return w.dealerLong; }, [](const desk::TffWeek &w) { return w.dealerShort; }},
            {tr("Asset managers"), [](const desk::TffWeek &w) { return w.assetLong; }, [](const desk::TffWeek &w) { return w.assetShort; }},
            {tr("Leveraged funds"), [](const desk::TffWeek &w) { return w.levLong; }, [](const desk::TffWeek &w) { return w.levShort; }},
            {tr("Other reportable"), [](const desk::TffWeek &w) { return w.otherLong; }, [](const desk::TffWeek &w) { return w.otherShort; }},
            {tr("Non-reportable"), [](const desk::TffWeek &w) { return w.nonReptLong; }, [](const desk::TffWeek &w) { return w.nonReptShort; }},
        };
        const desk::TffWeek &w = weeks.last();
        m_tff->setRowCount(rows.size());
        for (int r = 0; r < rows.size(); ++r) {
            const qint64 lg = rows[r].lg(w), sh = rows[r].sh(w), net = lg - sh;
            QVector<double> nets;
            for (const desk::TffWeek &k : weeks)
                nets.push_back(rows[r].lg(k) - rows[r].sh(k));
            const double change = nets.size() >= 2 ? nets.last() - nets[nets.size() - 2] : 0;
            m_tff->setItem(r, 0, deskui::cell(rows[r].name, {}, Qt::AlignLeft, true));
            m_tff->setItem(r, 1, deskui::cell(thousands(lg)));
            m_tff->setItem(r, 2, deskui::cell(thousands(sh)));
            m_tff->setItem(r, 3, deskui::cell(signedThousands(net), net >= 0 ? pal.Up : pal.Down));
            m_tff->setItem(r, 4, deskui::cell(signedThousands(change), change >= 0 ? pal.Up : pal.Down));
            m_tff->setItem(r, 5, deskui::cell(w.openInterest > 0 ? QString("%1%").arg(deskui::amount(100.0 * (lg + sh) / w.openInterest / 2, 1)) : "—", pal.TextDim));
            m_tff->setItem(r, 6, deskui::cell(QString("%1%").arg(deskui::amount(desk::rangePosition(nets), 0))));
        }
        deskui::fitHeight(m_tff);
        QVector<QDate> dates;
        QVector<double> asset, lev;
        for (const desk::TffWeek &k : weeks) {
            dates.push_back(k.date);
            asset.push_back(k.assetLong - k.assetShort);
            lev.push_back(k.levLong - k.levShort);
        }
        m_tffChart->setData(dates, {{tr("Asset managers"), pal.Accent, asset}, {tr("Leveraged funds"), pal.Up, lev}}, [](double v) { return shortNumber(v); });
    });
}

void CotView::fill()
{
    const Palette &c = Theme::current();
    const CotReport::Market &market = m_report->markets()[m_market->currentIndex()];
    const CotReport::Series &s = m_report->series(market.id);
    fetchTff(market);
    if (s.weeks.isEmpty()) {
        setStatus(m_report->errorString().isEmpty() ? tr("Loading the CFTC report…") : m_report->errorString());
        m_table->clear();
        m_title->setText(market.label);
        m_code->setText(tr("Code %1").arg(market.code));
        m_history->setRowCount(0);
        return;
    }
    setStatus(m_report->errorString().isEmpty() ? QString() : tr("Showing the last copy saved: %1").arg(m_report->errorString()));
    const CotReport::Week &w = s.weeks.last();
    const bool havePrev = s.weeks.size() >= 2;
    const CotReport::Week prev = havePrev ? s.weeks[s.weeks.size() - 2] : CotReport::Week();
    m_title->setText(s.name.toUpper());
    m_code->setText(tr("Code %1").arg(market.code));
    m_asOf->setText(tr("AS OF: %1").arg(w.date.toString(Qt::ISODate)));
    // Positions are as of Tuesday and published the Friday after (later on holiday weeks).
    m_release->setText(tr("RELEASE: %1").arg(w.date.addDays(3).toString(Qt::ISODate)));
    m_release->setToolTip(tr("The CFTC publishes Tuesday's positions on Friday at 15:30 ET, a day later on holiday weeks."));
    m_table->setWeek(w, havePrev, prev);
    fillReading(s);
    m_units->setText(s.units);
    const qint64 dOi = havePrev ? w.openInterest - prev.openInterest : 0;
    m_totals->setText(QString("%1 <b style='color:%2'>%3</b> · Δ OI: <b style='color:%4'>%5</b> · %6 <b style='color:%2'>%7</b>")
                          .arg(tr("Open Interest:"), c.Text.name(), thousands(w.openInterest),
                               (dOi > 0 ? c.Up : dOi < 0 ? c.Down : c.TextDim).name(), havePrev ? signedThousands(dOi) : "—",
                               tr("Traders:"), QString::number(w.traders)));
    m_longShort->setData({{tr("Non-Commercial"), double(w.ncLong), double(w.ncShort)},
                          {tr("Commercial"), double(w.cLong), double(w.cShort)},
                          {tr("Non-Reportable"), double(w.nrLong), double(w.nrShort)}});
    QVector<QDate> dates;
    QVector<double> comm, nc, nr;
    for (const CotReport::Week &k : s.weeks) {
        dates.push_back(k.date);
        comm.push_back(k.cNet());
        nc.push_back(k.ncNet());
        nr.push_back(k.nrNet());
    }
    m_net->setData(dates, {{tr("Commercial"), c.Text, comm}, {tr("NonCommercial"), research::palette::blue(), nc},
                           {tr("Non Rept"), research::palette::red(), nr}});

    m_historyTitle->setText(tr("HISTORICAL DATA (%1 records)").arg(s.weeks.size()));
    m_history->setRowCount(s.weeks.size());
    QColor accentTint = c.Accent;
    accentTint.setAlpha(40);
    for (int r = 0; r < s.weeks.size(); ++r) {
        const CotReport::Week &k = s.weeks[s.weeks.size() - 1 - r];
        const bool hasPrev = s.weeks.size() - 2 - r >= 0;
        const qint64 d = hasPrev ? k.openInterest - s.weeks[s.weeks.size() - 2 - r].openInterest : 0;
        m_history->setItem(r, 0, cell(k.date.toString(Qt::ISODate), c.Text));
        m_history->setItem(r, 1, cell(thousands(k.openInterest), c.Text));
        auto *dItem = cell(hasPrev ? signedThousands(d) : "—", d > 0 ? c.Up : d < 0 ? c.Down : c.TextDim, Qt::AlignCenter, true);
        m_history->setItem(r, 2, dItem);
        m_history->setItem(r, 3, cell(signedThousands(k.ncNet()), c.Text));
        m_history->setItem(r, 4, cell(signedThousands(k.cNet()), c.Text));
        m_history->setItem(r, 5, cell(signedThousands(k.nrNet()), c.Text));
        if (r == 0)
            for (int col = 0; col < 6; ++col)
                m_history->item(r, col)->setBackground(accentTint);
    }
    int h = m_history->horizontalHeader()->height();
    for (int r = 0; r < m_history->rowCount(); ++r)
        h += m_history->rowHeight(r);
    m_history->setFixedHeight(h + 4);
}

} // namespace quant

#include "QuantCotView.moc"
