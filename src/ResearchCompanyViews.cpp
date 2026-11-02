// The Research page's company sections: key metrics, financials, financial charts, ownership, ratings.

#include "ResearchViews.h"

#include "Theme.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>

namespace research {

// ---------------------------------------------------------------- building blocks

QFont mono(int px, bool bold)
{
    QFont f;
    f.setFamilies(Theme::monoFamilies());
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

QFrame *panel(const QString &title, QVBoxLayout **body, QHBoxLayout **head)
{
    auto *frame = new QFrame;
    frame->setObjectName("resPanel");
    auto *v = new QVBoxLayout(frame);
    v->setContentsMargins(16, 12, 16, 14);
    v->setSpacing(10);
    auto *h = new QHBoxLayout;
    h->setSpacing(8);
    auto *t = new QLabel(title.toUpper(), frame);   // parented first: showing a parentless label opens a window
    t->setObjectName("resPanelTitle");
    t->setVisible(!title.isEmpty());
    h->addWidget(t);
    h->addStretch(1);
    v->addLayout(h);
    if (body)
        *body = v;
    if (head)
        *head = h;
    return frame;
}

QWidget *segments(const QStringList &labels, QButtonGroup **group, int checked)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto *g = new QButtonGroup(w);
    g->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i].toUpper());
        b->setObjectName("resSeg");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(i == checked);
        g->addButton(b, i);
        h->addWidget(b);
    }
    *group = g;
    return w;
}

namespace {
// Paints an item's background role under the style's own drawing (the stylesheet's item rules ignore it).
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
}

QTableWidget *table(const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size());
    t->setItemDelegate(new RowBackground(t));
    t->setObjectName("resTable");
    QStringList upper;
    for (const QString &h : headers)
        upper << h.toUpper();
    t->setHorizontalHeaderLabels(upper);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(24);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->horizontalHeader()->setStretchLastSection(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->setMouseTracking(true);
    t->setFrameShape(QFrame::NoFrame);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    t->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    return t;
}

namespace {

QTableWidgetItem *cell(const QString &text, const QColor &color = QColor(), Qt::Alignment align = Qt::AlignLeft,
                       bool bold = false)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(align | Qt::AlignVCenter);
    if (color.isValid())
        it->setForeground(color);
    if (bold) {
        QFont f = mono(11, true);
        it->setFont(f);
    }
    return it;
}

// A table sized to show all its rows, inside a scrolling page.
void fitHeight(QTableWidget *t)
{
    int h = t->horizontalHeader()->isVisible() ? t->horizontalHeader()->height() : 0;
    for (int r = 0; r < t->rowCount(); ++r)
        h += t->rowHeight(r);
    t->setFixedHeight(h + 4);
}

QLabel *label(const QString &text, const char *name)
{
    auto *l = new QLabel(text);
    l->setObjectName(name);
    return l;
}

QString dateText(const QDate &d)
{
    return d.isValid() ? d.toString(Qt::ISODate) : "N/A";
}

QDate epochDate(double secs)
{
    return ok(secs) ? QDateTime::fromSecsSinceEpoch(qint64(secs), QTimeZone::UTC).date() : QDate();
}

double lastValue(const Series &s)
{
    return s.isEmpty() ? NaN : s.last().value;
}

// Growth a year over the last `years` years of a yearly series (NaN if the ends don't allow it).
double cagr(const Series &s, int years)
{
    if (s.size() <= years)
        return NaN;
    const double end = s.last().value, start = s[s.size() - 1 - years].value;
    if (!ok(end) || !ok(start) || start <= 0 || end <= 0)
        return NaN;
    return std::pow(end / start, 1.0 / years) - 1;
}

double totalGrowth(const Series &s, int years)
{
    if (s.size() <= years)
        return NaN;
    const double end = s.last().value, start = s[s.size() - 1 - years].value;
    return ok(end) && ok(start) && start != 0 ? end / std::abs(start) - 1 : NaN;
}

double ratio(double a, double b)
{
    return ok(a) && ok(b) && b != 0 ? a / b : NaN;
}

// A line's value in the newest column (the latest period), NaN if that period lacks it: an older value
// isn't taken, so figures shown together always come from the same period.
double newest(const Statement &s, const char *line)
{
    const QVector<double> v = s.lines.value(line);
    return v.isEmpty() ? NaN : v.first();
}

double valueAt(const Statement &s, const char *line, const QDate &end)
{
    const int i = s.ends.indexOf(end);
    return i < 0 ? NaN : s.lines.value(line).value(i, NaN);
}

// The newest fiscal year in which every line has a value (yearly statements share their period ends): its
// end and the values, in order; an invalid date and NaNs when there's none.
QDate sameYear(const QVector<std::pair<const Statement *, const char *>> &lines, QVector<double> *values)
{
    const Statement *first = lines.isEmpty() ? nullptr : lines.first().first;
    if (first)
        for (const QDate &end : first->ends) {
            QVector<double> v;
            for (const auto &[st, line] : lines)
                v << valueAt(*st, line, end);
            if (std::all_of(v.begin(), v.end(), [](double x) { return ok(x); })) {
                *values = v;
                return end;
            }
        }
    *values = QVector<double>(lines.size(), NaN);
    return {};
}

QString periodLabel(const QDate &end, bool quarterly)
{
    if (!quarterly)
        return QString::number(end.year());
    return QString("Q%1 %2").arg((end.month() - 1) / 3 + 1).arg(end.year());
}

} // namespace

QString experimentalReason()
{
    return QObject::tr("Experimental: Deepwater's own method, not validated against what prices did afterwards. Its "
                       "assumptions are fixed and its results move a lot with them (one point of discount rate moves "
                       "Apple's intrinsic value by about 17 %). Use it as a rough indication, next to the analysts' figures.");
}

QLabel *experimentalTag(const QString &why)
{
    auto *l = new QLabel(QObject::tr("EXPERIMENTAL"));
    l->setObjectName("resExperimental");
    l->setToolTip(why.isEmpty() ? experimentalReason() : why + "\n\n" + experimentalReason());
    l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return l;
}

QColor changeColor(double v)
{
    const Palette &c = Theme::current();
    return !ok(v) || v == 0 ? c.TextDim : v > 0 ? c.Up : c.Down;
}

StatList::StatList(QWidget *parent)
    : QWidget(parent)
    , m_grid(new QGridLayout(this))
{
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(10);
    m_grid->setVerticalSpacing(5);
    m_grid->setColumnStretch(0, 1);
}

void StatList::clear()
{
    while (QLayoutItem *it = m_grid->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    m_pairs.clear();
    m_colors.clear();
    m_rows = 0;
}

void StatList::end()
{
    for (int i = m_rows; i < m_pairs.size(); ++i) {
        m_pairs[i].first->hide();
        m_pairs[i].second->hide();
    }
}

void StatList::add(const QString &labelText, const QString &value, const QColor &color, const QString &tip)
{
    if (m_rows == m_pairs.size()) {
        auto *l = label(labelText, "resStatLabel");
        l->setWordWrap(true);
        auto *v = label(value, "resStatValue");
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        v->setWordWrap(true);
        m_grid->addWidget(l, m_rows, 0);
        m_grid->addWidget(v, m_rows, 1);
        m_pairs.push_back({l, v});
        m_colors.push_back(QColor());
    }
    auto [l, v] = m_pairs[m_rows];
    l->setText(labelText);
    v->setText(value);
    if (color != m_colors[m_rows]) {   // a stylesheet change re-polishes the label, even to the same colour
        m_colors[m_rows] = color;
        v->setStyleSheet(color.isValid() ? QString("color: %1;").arg(color.name()) : QString());
    }
    l->setToolTip(tip);
    v->setToolTip(tip);
    if (l->isHidden()) {   // left over by an earlier, shorter fill
        l->show();
        v->show();
    }
    ++m_rows;
}

FlowGrid::FlowGrid(int minColumnWidth, QWidget *parent)
    : QWidget(parent)
    , m_grid(new QGridLayout(this))
    , m_minWidth(minColumnWidth)
{
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(12);
}

void FlowGrid::addWidget(QWidget *w)
{
    m_items.push_back(w);
    relayout(true);
}

void FlowGrid::clear()
{
    for (QWidget *w : std::as_const(m_items))
        delete w;
    m_items.clear();
    relayout(true);
}

void FlowGrid::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    relayout(false);
}

void FlowGrid::relayout(bool force)
{
    const int cols = std::max(1, (width() + m_grid->spacing()) / (m_minWidth + m_grid->spacing()));
    if (!force && cols == m_columns)
        return;
    m_columns = cols;
    while (m_grid->count())
        delete m_grid->takeAt(0);
    for (int c = 0; c < 12; ++c)
        m_grid->setColumnStretch(c, c < cols ? 1 : 0);
    for (int i = 0; i < m_items.size(); ++i)
        m_grid->addWidget(m_items[i], i / cols, i % cols, Qt::AlignTop);
}

// ---------------------------------------------------------------- View

View::View(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_status = label({}, "resStatus");
    m_status->hide();
    m_status->setWordWrap(true);
    outer->addWidget(m_status);
    m_scroll = new QScrollArea;
    m_scroll->setObjectName("resScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    inner->setObjectName("resContent");
    m_content = new QVBoxLayout(inner);
    m_content->setContentsMargins(16, 14, 16, 16);
    m_content->setSpacing(14);
    m_scroll->setWidget(inner);
    outer->addWidget(m_scroll, 1);
}

void View::setSymbol(const QString &symbol)
{
    if (symbol == m_symbol)
        return;
    m_symbol = symbol;
    if (!dependsOnSymbol() && !m_dirty)
        return;
    m_dirty = true;
    if (isVisible()) {
        m_dirty = false;
        load();
    }
}

void View::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (m_dirty && !m_symbol.isEmpty()) {
        m_dirty = false;
        load();
    }
}

void View::setStatus(const QString &text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

// ---------------------------------------------------------------- Key metrics

KeyMetricsView::KeyMetricsView(QWidget *parent)
    : View(parent)
{
    // Profile on the left, general info on the right.
    QVBoxLayout *top;
    m_header = panel({}, &top);
    auto *row = new QHBoxLayout;
    row->setSpacing(24);
    top->addLayout(row);
    auto *left = new QVBoxLayout;
    left->setSpacing(8);
    auto *idRow = new QHBoxLayout;
    m_initial = label({}, "resInitial");
    m_initial->setFixedSize(46, 46);
    m_initial->setAlignment(Qt::AlignCenter);
    idRow->addWidget(m_initial);
    auto *idCol = new QVBoxLayout;
    idCol->setSpacing(1);
    m_name = label({}, "resCompanyName");
    m_ticker = label({}, "resDim");
    auto *priceRow = new QHBoxLayout;
    m_price = label({}, "resBigPrice");
    m_change = label({}, "resChange");
    priceRow->setSpacing(10);
    priceRow->addWidget(m_price);
    priceRow->addWidget(m_change);
    priceRow->addStretch(1);
    idCol->addWidget(m_name);
    idCol->addWidget(m_ticker);
    idCol->addLayout(priceRow);
    idRow->addLayout(idCol, 1);
    left->addLayout(idRow);
    m_tags = new QHBoxLayout;
    m_tags->setSpacing(6);
    left->addLayout(m_tags);
    m_description = label({}, "resDescription");
    m_description->setWordWrap(true);
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    left->addWidget(m_description);
    m_more = new QPushButton(tr("Show more"));
    m_more->setObjectName("resLink");
    m_more->setCursor(Qt::PointingHandCursor);
    connect(m_more, &QPushButton::clicked, this, [this] {
        m_expanded = !m_expanded;
        fill();
    });
    left->addWidget(m_more, 0, Qt::AlignLeft);
    m_links = new QHBoxLayout;
    m_links->setSpacing(6);
    left->addLayout(m_links);
    left->addStretch(1);
    row->addLayout(left, 3);
    auto *right = new QVBoxLayout;
    right->addWidget(label(tr("GENERAL INFO"), "resPanelTitle"));
    m_general = new StatList;
    right->addWidget(m_general);
    right->addStretch(1);
    auto *rightBox = new QWidget;
    rightBox->setLayout(right);
    rightBox->setMinimumWidth(320);
    row->addWidget(rightBox, 2);
    content()->addWidget(m_header);

    // Employees and the financial overview.
    auto *charts = new QHBoxLayout;
    charts->setSpacing(14);
    QVBoxLayout *eb;
    auto *employees = panel(tr("Employee Evolution"), &eb);
    m_employeeTitle = qobject_cast<QLabel *>(employees->findChild<QLabel *>("resPanelTitle"));
    m_employeeChart = new LineChart;
    m_employeeChart->setMinimumHeight(220);
    eb->addWidget(m_employeeChart);
    charts->addWidget(employees, 1);
    QVBoxLayout *ob;
    auto *overview = panel(tr("Financial Overview"), &ob);
    auto *orow = new QHBoxLayout;
    m_overview = new DonutChart;
    m_overview->setFixedSize(200, 200);
    m_overview->setStartAngle(90);
    orow->addWidget(m_overview);
    m_overviewLegend = new StatList;
    orow->addWidget(m_overviewLegend, 1);
    ob->addLayout(orow);
    charts->addWidget(overview, 1);
    content()->addLayout(charts);

    m_cards = new FlowGrid(240);
    content()->addWidget(m_cards);
    content()->addStretch(1);
}

void KeyMetricsView::scheduleFill()
{
    if (m_fillPending)
        return;
    m_fillPending = true;
    QTimer::singleShot(0, this, [this] {
        m_fillPending = false;
        fill();
    });
}

void KeyMetricsView::load()
{
    const QString sym = m_symbol;
    m_summary = {};
    m_fund = {};
    m_balance = m_income = m_balanceQ = {};
    m_employees.clear();
    m_expanded = false;
    setStatus(tr("Loading %1…").arg(sym));
    scheduleFill();
    Client *c = Client::instance();
    c->summary(sym, this, [this, sym](const QJsonObject &s, const QString &err) {
        if (sym != m_symbol)
            return;
        m_summary = s;
        setStatus(s.isEmpty() ? tr("Couldn't load %1: %2").arg(sym, err) : QString());
        scheduleFill();
    });
    c->fundamentals(sym, this, [this, sym](const Fundamentals &f, const QString &) {
        if (sym != m_symbol)
            return;
        m_fund = f;
        scheduleFill();
    });
    c->statement(sym, "balance", false, this, [this, sym](const Statement &s, const QString &) {
        if (sym != m_symbol)
            return;
        m_balance = s;
        scheduleFill();
    });
    c->statement(sym, "balance", true, this, [this, sym](const Statement &s, const QString &) {
        if (sym != m_symbol)
            return;
        m_balanceQ = s;
        scheduleFill();
    });
    c->statement(sym, "income", false, this, [this, sym](const Statement &s, const QString &) {
        if (sym != m_symbol)
            return;
        m_income = s;
        scheduleFill();
    });
    c->employees(sym, this, [this, sym](const Series &s, const QString &) {
        if (sym != m_symbol)
            return;
        m_employees = s;
        scheduleFill();
    });
    c->indexes(this, [this, sym](const Indexes &idx, const QString &) {
        if (sym != m_symbol)
            return;
        m_indexes = idx.indexesOf(sym);
        scheduleFill();
    });
}

void KeyMetricsView::fill()
{
    const Palette &pal = Theme::current();
    const QJsonObject profile = m_summary.value("assetProfile").toObject();
    const QJsonObject price_ = m_summary.value("price").toObject();
    const QJsonObject sd = m_summary.value("summaryDetail").toObject();
    const QJsonObject ks = m_summary.value("defaultKeyStatistics").toObject();
    const QJsonObject fd = m_summary.value("financialData").toObject();
    const QJsonObject qt = m_summary.value("quoteType").toObject();
    const QString cur = text(price_, "currency").isEmpty() ? QString("USD") : text(price_, "currency");
    const QString fcur = text(fd, "financialCurrency").isEmpty() ? cur : text(fd, "financialCurrency");

    const QString name = text(price_, "longName").isEmpty() ? text(price_, "shortName") : text(price_, "longName");
    m_name->setText(name.isEmpty() ? m_symbol : name);
    m_ticker->setText(m_symbol + (text(price_, "exchangeName").isEmpty() ? QString() : "  ·  " + text(price_, "exchangeName")));
    m_initial->setText(m_symbol.left(1));
    const double last = raw(price_, "regularMarketPrice"), ch = raw(price_, "regularMarketChange"),
                 chp = raw(price_, "regularMarketChangePercent");
    m_price->setText(money(last, cur));
    m_change->setText(ok(ch) ? QString("%1 (%2)").arg(money(ch, cur), percent(chp)) : QString());
    m_change->setStyleSheet(QString("color: %1;").arg(changeColor(ch).name()));

    auto clearLayout = [](QHBoxLayout *l) {
        while (QLayoutItem *it = l->takeAt(0)) {
            delete it->widget();
            delete it;
        }
    };
    clearLayout(m_tags);
    QString ceo;
    for (const QJsonValue &o : profile.value("companyOfficers").toArray()) {
        const QString title = o.toObject().value("title").toString();
        if (title.contains("CEO") || title.contains("Chief Executive")) {
            ceo = o.toObject().value("name").toString();
            ceo.remove(QRegularExpression("^(Mr\\.|Ms\\.|Mrs\\.|Dr\\.)\\s+"));
            ceo = ceo.simplified();
            break;
        }
    }
    const QString country = QCoreApplication::translate("Country", text(profile, "country").toUtf8().constData());
    for (const QString &t : {text(profile, "sectorDisp").isEmpty() ? text(profile, "sector") : text(profile, "sectorDisp"),
                             text(profile, "industryDisp").isEmpty() ? text(profile, "industry") : text(profile, "industryDisp"),
                             country, ceo.isEmpty() ? QString() : tr("CEO: %1").arg(ceo)})
        if (!t.isEmpty())
            m_tags->addWidget(label(t, "resChip"));
    m_tags->addStretch(1);

    const QString desc = text(profile, "longBusinessSummary");
    const bool longDesc = desc.size() > 260;
    m_description->setText(m_expanded || !longDesc ? desc : desc.left(desc.lastIndexOf(' ', 250)) + "…");
    m_more->setVisible(longDesc);
    m_more->setText(m_expanded ? tr("Show less") : tr("Show more"));

    clearLayout(m_links);
    const QString website = text(profile, "website");
    if (!website.isEmpty()) {
        auto *b = new QPushButton("🌐 " + tr("Website"));
        b->setObjectName("resChipButton");
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(website);
        connect(b, &QPushButton::clicked, this, [website] { QDesktopServices::openUrl(QUrl(website)); });
        m_links->addWidget(b);
    }
    const QDate ipo = epochDate(raw(qt, "firstTradeDateEpochUtc"));
    if (ipo.isValid())
        m_links->addWidget(label("📅 " + tr("First trade: %1").arg(dateText(ipo)), "resChip"));
    const double employees = raw(profile, "fullTimeEmployees");
    if (ok(employees))
        m_links->addWidget(label("👥 " + tr("%1 Employees").arg(compact(employees)), "resChip"));
    m_links->addStretch(1);

    // General info.
    // Enterprise value, one definition on the whole page: market cap + debt − cash and short-term investments, from
    // the latest quarterly balance sheet (statements in another currency converted first); Yahoo's only without one.
    const double mcap = raw(sd, "marketCap");
    const double ev = [&] {
        const double fx = financialToQuote(m_summary), d = newest(m_balanceQ, "TotalDebt"),
                     c = newest(m_balanceQ, "CashCashEquivalentsAndShortTermInvestments");
        if (ok(mcap) && ok(d) && ok(c) && ok(fx))
            return mcap + (d - c) * fx;
        return fcur == cur ? raw(ks, "enterpriseValue") : NaN;
    }();
    m_general->begin();
    m_general->add(tr("Market Cap"), money(mcap, cur));
    m_general->add(tr("Enterprise Value"), money(ev, cur));
    m_general->add(tr("P/E (TTM)"), plain(fcur == cur ? raw(sd, "trailingPE")
                                                     : ratio(mcap, raw(ks, "netIncomeToCommon") * financialToQuote(m_summary))));
    m_general->add(tr("Forward P/E"), plain(raw(sd, "forwardPE")));
    m_general->add(tr("Shares Outstanding"), compact(raw(ks, "sharesOutstanding")));
    m_general->add(tr("Shares Float"), compact(raw(ks, "floatShares")));
    m_general->add(tr("Indexes"), m_indexes.isEmpty() ? tr("None") : m_indexes.join(", "));
    m_general->end();

    // Employees: Wikidata's yearly counts on the line; Yahoo's current figure only in the title (two sources on
    // one line would draw a change that may only be a difference between them).
    const Series emp = m_employees;
    QStringList cats;
    QVector<double> vals;
    for (const Point &p : emp) {
        cats << QString::number(p.date.year());
        vals << p.value;
    }
    if (m_employeeTitle)
        m_employeeTitle->setText(tr("EMPLOYEE EVOLUTION · WIKIDATA%1")
                                     .arg(ok(employees) ? tr("   (today %1 · Yahoo Finance)").arg(compact(employees)) : QString()));
    m_employeeChart->setData(cats, {{tr("Employees"), palette::cyan(), vals, true, true}}, [](double v) { return compact(v); });
    m_employeeChart->setToolTip(tr("Head counts recorded on Wikidata, by year; years without a record are missing"));

    // Financial overview: market cap against the trailing year's revenue and profit.
    const double revenue = raw(fd, "totalRevenue"), netIncome = raw(ks, "netIncomeToCommon");
    const double fx = ok(financialToQuote(m_summary)) ? financialToQuote(m_summary) : 1;   // slices in one currency
    m_overview->setData({{tr("Market Cap"), ok(mcap) ? mcap : 0, pal.PanelLine},
                         {tr("Revenue"), ok(revenue) ? revenue * fx : 0, palette::purple()},
                         {tr("Net Income"), ok(netIncome) && netIncome > 0 ? netIncome * fx : 0, palette::cyan()}});
    m_overviewLegend->begin();
    m_overviewLegend->add("■ " + tr("Market Cap"), money(mcap, cur), {});
    m_overviewLegend->add("■ " + tr("Revenue (TTM)"), money(revenue, fcur), palette::purple());
    m_overviewLegend->add("■ " + tr("Net Income (TTM)"), money(netIncome, fcur), palette::cyan());
    m_overviewLegend->end();

    // Cards: the same ones every time, refilled; whatever this fill doesn't use is hidden.
    int cardsUsed = 0;
    StatList *filling = nullptr;   // the card being filled, ended when the next one starts
    // While a symbol loads there are no cards: the whole grid is hidden, which is one layout pass instead
    // of one per card going and coming back.
    if (m_summary.isEmpty()) {
        m_cards->hide();
        return;
    }
    if (m_cards->isHidden())
        m_cards->show();
    struct EndCards {
        const QVector<QPair<QFrame *, StatList *>> &pool;
        const int &used;
        StatList *const &filling;
        ~EndCards()
        {
            if (filling)
                filling->end();
            for (int i = used; i < pool.size(); ++i)
                if (!pool[i].first->isHidden())
                    pool[i].first->hide();
        }
    } endCards {m_cardPool, cardsUsed, filling};
    auto card = [this, &cardsUsed, &filling](const QString &title) {
        if (filling)
            filling->end();
        if (cardsUsed < m_cardPool.size()) {
            auto [f, list] = m_cardPool[cardsUsed++];
            if (auto *t = f->findChild<QLabel *>("resPanelTitle"))
                t->setText(title.toUpper());
            if (f->isHidden())
                f->show();
            list->begin();
            return filling = list;
        }
        QVBoxLayout *b;
        auto *f = panel(title, &b);
        auto *list = new StatList;
        b->addWidget(list);
        b->addStretch(1);
        m_cards->addWidget(f);
        m_cardPool.push_back({f, list});
        ++cardsUsed;
        return filling = list;
    };
    // Statements in another currency than the quote (ADRs): figures compared with the market cap are converted
    // first, and per-share figures use the quoted shares (market cap / price).
    const double fxq = financialToQuote(m_summary);
    auto inQuote = [fxq](double v) { return ok(fxq) && ok(v) ? v * fxq : NaN; };
    const bool sameCurrency = fcur == cur;
    const double price = raw(price_, "regularMarketPrice");
    const double shares = ok(mcap) && ok(price) && price > 0 ? mcap / price : raw(ks, "sharesOutstanding");
    const double ebitda = raw(fd, "ebitda"), fcf = raw(fd, "freeCashflow"), ocf = raw(fd, "operatingCashflow");
    const double opIncome = ok(raw(fd, "operatingMargins")) && ok(revenue) ? raw(fd, "operatingMargins") * revenue : NaN;
    const QDate earningsDate = [&] {
        const QJsonArray d = m_summary.value("calendarEvents").toObject().value("earnings").toObject().value("earningsDate").toArray();
        return d.isEmpty() ? QDate() : epochDate(d.first().toObject().value("raw").toDouble(NaN));
    }();
    // The latest balance sheet (quarterly), one period for every balance figure on the page.
    const Statement &bq = m_balanceQ;
    const QString bqPeriod = bq.ends.isEmpty() ? QString() : periodLabel(bq.ends.first(), true);
    const double assets = newest(bq, "TotalAssets"), liabilities = newest(bq, "TotalLiabilitiesNetMinorityInterest");
    const double equity = newest(bq, "StockholdersEquity"), tangible = newest(bq, "TangibleBookValue");
    const double curAssets = newest(bq, "CurrentAssets"), curLiab = newest(bq, "CurrentLiabilities");
    const double cashSt = newest(bq, "CashCashEquivalentsAndShortTermInvestments");
    const double cashOnly = newest(bq, "CashAndCashEquivalents"), receivables = newest(bq, "Receivables");
    const double debt = newest(bq, "TotalDebt"), ltDebt = newest(bq, "LongTermDebt");
    const QString balanceTip = bqPeriod.isEmpty() ? QString() : tr("Balance sheet of %1 (Yahoo Finance)").arg(bqPeriod);
    const QString evTip = tr("Market cap + total debt − cash and short-term investments (%1)").arg(balanceTip);

    StatList *s = card(tr("Company Info"));
    s->add(tr("Name"), name);
    s->add(tr("Sector"), text(profile, "sector"));
    s->add(tr("Industry"), text(profile, "industry"));
    s->add(tr("Country"), country);
    s->add(tr("CEO"), ceo.isEmpty() ? "N/A" : ceo);
    s->add(tr("Employees"), compact(employees));
    s->add(tr("First trade"), dateText(ipo));

    s = card(tr("Key Stats"));
    s->add(tr("Market Cap"), money(mcap, cur));
    s->add(tr("Enterprise Value"), money(ev, cur), {}, evTip);
    s->add(tr("P/E (TTM)"), plain(sameCurrency ? raw(sd, "trailingPE") : ratio(mcap, inQuote(netIncome))));
    s->add(tr("Forward P/E"), plain(raw(sd, "forwardPE")));
    s->add(tr("Beta"), plain(raw(sd, "beta")));
    s->add(tr("52 Week High"), money(raw(sd, "fiftyTwoWeekHigh"), cur));
    s->add(tr("52 Week Low"), money(raw(sd, "fiftyTwoWeekLow"), cur));

    s = card(tr("Valuation Ratios"));
    s->add(tr("PEG"), plain(raw(ks, "pegRatio")));
    s->add(tr("P/S (Price to Sales)"), plain(ratio(mcap, inQuote(revenue))));
    s->add(tr("P/B (Price to Book)"), plain(ratio(mcap, inQuote(equity))), {}, balanceTip);
    s->add(tr("P/C (Price to Cash)"), plain(ratio(mcap, inQuote(cashSt))), {}, balanceTip);
    s->add(tr("P/CF (Price to Cash Flow)"), plain(ratio(mcap, inQuote(ocf))));
    s->add(tr("P/FCF (Price to FCF)"), plain(ratio(mcap, inQuote(fcf))));
    s->add(tr("EV/EBIT"), plain(ratio(ev, inQuote(opIncome))), {}, evTip);
    s->add(tr("EV/EBITDA"), plain(ratio(ev, inQuote(ebitda))), {}, evTip);
    s->add(tr("EV/Sales"), plain(ratio(ev, inQuote(revenue))), {}, evTip);

    s = card(tr("Earnings (TTM)"));
    s->add(tr("Next Earnings"), dateText(earningsDate));
    s->add(tr("Revenue/Sales"), money(revenue, fcur));
    s->add(tr("Net Income"), money(netIncome, fcur), changeColor(netIncome));
    s->add(tr("Gross Profit"), money(raw(fd, "grossProfits"), fcur));
    s->add(tr("Operating Income"), money(opIncome, fcur));
    s->add(tr("EBITDA"), money(ebitda, fcur));
    s->add(tr("Free Cash Flow"), money(fcf, fcur), changeColor(fcf));

    s = card(tr("Per Share"));
    s->add(tr("EPS (TTM)"), money(raw(ks, "trailingEps"), cur), changeColor(raw(ks, "trailingEps")));
    s->add(tr("EPS Forward"), money(raw(ks, "forwardEps"), cur));
    s->add(tr("Book Value per Share"), money(ratio(inQuote(equity), shares), cur), {}, balanceTip);
    s->add(tr("Cash per Share"), money(ratio(inQuote(cashSt), shares), cur), {}, balanceTip);
    s->add(tr("FCF per Share"), money(ratio(inQuote(fcf), shares), cur));
    s->add(tr("Revenue per Share"), money(ratio(inQuote(revenue), shares), cur));
    s->add(tr("TBVPS"), money(ratio(inQuote(tangible), shares), cur), {}, tr("Tangible book value per share. %1").arg(balanceTip));

    s = card(bqPeriod.isEmpty() ? tr("Balance Sheet") : tr("Balance Sheet · %1").arg(bqPeriod));
    s->add(tr("Total Assets"), money(assets, fcur));
    s->add(tr("Total Liabilities"), money(liabilities, fcur));
    s->add(tr("Equity"), money(equity, fcur));
    s->add(tr("Total Debt"), money(debt, fcur));
    s->add(tr("Working Capital"), money(newest(bq, "WorkingCapital"), fcur));
    s->add(tr("Cash & ST Investments"), money(cashSt, fcur));
    s->add(tr("Cash Only"), money(cashOnly, fcur));
    s->add(tr("Levered FCF (TTM)"), money(fcf, fcur));

    s = card(tr("Margins"));
    s->add(tr("Gross Margin"), percent(raw(fd, "grossMargins")));
    s->add(tr("Operating Margin"), percent(raw(fd, "operatingMargins")));
    s->add(tr("Net/Profit Margin"), percent(raw(fd, "profitMargins")));
    s->add(tr("EBITDA Margin"), percent(raw(fd, "ebitdaMargins")));

    // Ratios of yearly flows to yearly balances: every figure from the same fiscal year.
    auto fyTip = [](const QDate &end, const QString &what) {
        return end.isValid() ? tr("%1 · fiscal year ending %2").arg(what, end.toString(Qt::ISODate))
                             : tr("%1 · no fiscal year has all the figures").arg(what);
    };
    QVector<double> v;
    const QDate roicYear = sameYear({{&m_income, "EBIT"}, {&m_income, "TaxRateForCalcs"}, {&m_balance, "InvestedCapital"}}, &v);
    const double roic = ratio(v[0] * (1 - v[1]), v[2]);
    const QDate roceYear = sameYear({{&m_income, "EBIT"}, {&m_balance, "TotalAssets"}, {&m_balance, "CurrentLiabilities"}}, &v);
    const double roce = ratio(v[0], v[1] - v[2]);
    s = card(tr("Profitability"));
    s->add(tr("ROE"), percent(raw(fd, "returnOnEquity")));
    s->add(tr("ROA"), percent(raw(fd, "returnOnAssets")));
    s->add(tr("ROIC"), percent(roic), {}, fyTip(roicYear, tr("EBIT after tax / invested capital")));
    s->add(tr("ROCE"), percent(roce), {}, fyTip(roceYear, tr("EBIT / (total assets − current liabilities)")));
    s->add(tr("Return on Sales"), percent(raw(fd, "operatingMargins")));

    s = card(tr("Debt/Leverage"));
    s->add(tr("Debt to Equity"), plain(ratio(debt, equity)), {}, balanceTip);
    s->add(tr("LT Debt to Equity"), plain(ratio(ltDebt, equity)), {}, balanceTip);
    s->add(tr("Net Debt/EBITDA"), plain(ratio(ok(debt) && ok(cashSt) ? debt - cashSt : NaN, ebitda)), {},
           tr("Debt − cash and short-term investments (%1) / EBITDA over the last twelve months").arg(bqPeriod));
    s->add(tr("Debt/Assets"), plain(ratio(debt, assets)), {}, balanceTip);
    s->add(tr("LT Debt/Capital"), plain(ratio(ltDebt, ok(ltDebt) && ok(equity) ? ltDebt + equity : NaN)), {}, balanceTip);

    const Series dps = m_fund.annual.value("dividendsPerShare");
    s = card(tr("Dividends"));
    s->add(tr("Dividend (TTM)"), money(raw(sd, "trailingAnnualDividendRate"), cur));
    s->add(tr("Dividend Yield"), percent(raw(sd, "dividendYield")));
    s->add(tr("Dividend Rate (fwd)"), money(raw(sd, "dividendRate"), cur));
    s->add(tr("Payout Ratio"), percent(raw(sd, "payoutRatio")));
    s->add(tr("Ex-Dividend Date"), dateText(epochDate(raw(sd, "exDividendDate"))));
    s->add(tr("Dividend Growth 3Y/5Y"), percent(cagr(dps, 3)) + " / " + percent(cagr(dps, 5)));

    s = card(tr("Shares & Ownership"));
    s->add(tr("Shares Outstanding"), compact(raw(ks, "sharesOutstanding")));
    s->add(tr("Shares Float"), compact(raw(ks, "floatShares")));
    s->add(tr("Short % of Float"), percent(raw(ks, "shortPercentOfFloat")));
    s->add(tr("Insider Own %"), percent(raw(ks, "heldPercentInsiders")));
    s->add(tr("Institutional Own %"), percent(raw(ks, "heldPercentInstitutions")));

    const Series eps = m_fund.annual.value("epsDiluted"), rev = m_fund.annual.value("revenue");
    const QString source = m_fund.source.isEmpty() ? QString()
                           : tr("From the yearly figures (%1); per-share figures adjusted for stock splits").arg(m_fund.source);
    s = card(tr("Growth (CAGR)"));
    s->add(tr("EPS CAGR 3Y"), percent(cagr(eps, 3)), changeColor(cagr(eps, 3)), source);
    s->add(tr("EPS CAGR 5Y"), percent(cagr(eps, 5)), changeColor(cagr(eps, 5)), source);
    s->add(tr("Sales CAGR 3Y"), percent(cagr(rev, 3)), changeColor(cagr(rev, 3)), source);
    s->add(tr("Sales CAGR 5Y"), percent(cagr(rev, 5)), changeColor(cagr(rev, 5)), source);
    s->add(tr("Dividend CAGR 3Y"), percent(cagr(dps, 3)), changeColor(cagr(dps, 3)), source);
    s->add(tr("Dividend CAGR 5Y"), percent(cagr(dps, 5)), changeColor(cagr(dps, 5)), source);

    const Series qrev = m_fund.quarterly.value("revenue"), qeps = m_fund.quarterly.value("epsDiluted");
    auto qoq = [](const Series &q, int back) {
        return q.size() > back + 1 ? ratio(q[q.size() - 1 - back].value - q[q.size() - 2 - back].value,
                                           std::abs(q[q.size() - 2 - back].value)) : NaN;
    };
    auto qoqAvg = [&](const Series &q) {
        double sum = 0;
        int n = 0;
        for (int i = 0; i < 4; ++i)
            if (ok(qoq(q, i))) {
                sum += qoq(q, i);
                ++n;
            }
        return n ? sum / n : NaN;
    };
    s = card(tr("Growth YoY/QoQ"));
    s->add(tr("Sales Y/Y"), percent(raw(fd, "revenueGrowth")), changeColor(raw(fd, "revenueGrowth")),
           tr("Last quarter against the same quarter a year before"));
    s->add(tr("EPS Y/Y"), percent(raw(fd, "earningsGrowth")), changeColor(raw(fd, "earningsGrowth")));
    s->add(tr("Sales Q/Q"), percent(qoq(qrev, 0)), changeColor(qoq(qrev, 0)));
    s->add(tr("EPS Q/Q"), percent(qoq(qeps, 0)), changeColor(qoq(qeps, 0)),
           tr("Fourth quarters are the year less the first nine months: approximate for EPS"));
    s->add(tr("Sales 3Y/5Y Growth"), percent(totalGrowth(rev, 3)) + " / " + percent(totalGrowth(rev, 5)));
    s->add(tr("EPS 3Y/5Y Growth"), percent(totalGrowth(eps, 3)) + " / " + percent(totalGrowth(eps, 5)), {}, source);
    s->add(tr("Sales QoQ Avg"), percent(qoqAvg(qrev)), {}, tr("Mean of the last four quarter-on-quarter changes"));
    s->add(tr("EPS QoQ Avg"), percent(qoqAvg(qeps)));

    const QDate atYear = sameYear({{&m_income, "TotalRevenue"}, {&m_balance, "TotalAssets"}}, &v);
    const double assetTurnover = ratio(v[0], v[1]);
    const QDate invYear = sameYear({{&m_income, "CostOfRevenue"}, {&m_balance, "Inventory"}}, &v);
    const double invTurnover = ratio(v[0], v[1]);
    const QDate recYear = sameYear({{&m_income, "TotalRevenue"}, {&m_balance, "Receivables"}}, &v);
    const double recTurnover = ratio(v[0], v[1]);
    const QDate covYear = sameYear({{&m_income, "EBIT"}, {&m_income, "InterestExpense"}}, &v);
    const double coverage = ratio(v[0], v[1]);
    s = card(tr("Efficiency"));
    s->add(tr("Asset Turnover"), plain(assetTurnover), {}, fyTip(atYear, tr("Revenue / total assets")));
    s->add(tr("Inventory Turnover"), plain(invTurnover), {}, fyTip(invYear, tr("Cost of revenue / inventory")));
    s->add(tr("Receivables Turnover"), plain(recTurnover), {}, fyTip(recYear, tr("Revenue / receivables")));
    s->add(tr("Interest Coverage"), plain(coverage) + (covYear.isValid() && !m_income.ends.isEmpty() && covYear != m_income.ends.first()
                                                          ? QString(" (FY%1)").arg(covYear.year()) : QString()),
           {}, fyTip(covYear, tr("EBIT / interest expense")));

    s = card(tr("Liquidity"));
    s->add(tr("Current Ratio"), plain(ratio(curAssets, curLiab)), {}, balanceTip);
    s->add(tr("Quick Ratio"), plain(ratio(ok(cashSt) && ok(receivables) ? cashSt + receivables : NaN, curLiab)), {},
           tr("(Cash and short-term investments + receivables) / current liabilities. %1").arg(balanceTip));
    s->add(tr("Cash Ratio"), plain(ratio(cashSt, curLiab)), {}, balanceTip);
    s->add(tr("Op. CF Ratio"), plain(ratio(ocf, curLiab)), {},
           tr("Operating cash flow over the last twelve months / current liabilities (%1)").arg(bqPeriod));
}

// ---------------------------------------------------------------- Financials

FinancialsView::FinancialsView(QWidget *parent)
    : View(parent)
{
    auto *bar = new QHBoxLayout;
    bar->addWidget(segments({tr("Income Statement"), tr("Balance Sheet"), tr("Cash Flow")}, &m_kind,
                            QSettings().value("research/financialsKind", 0).toInt()));
    bar->addStretch(1);
    bar->addWidget(segments({tr("Annual"), tr("Quarterly")}, &m_period, QSettings().value("research/financialsPeriod", 0).toInt()));
    content()->addLayout(bar);
    QVBoxLayout *b;
    auto *p = panel({}, &b);
    m_table = table({tr("Metric")});
    b->addWidget(m_table);
    content()->addWidget(p);
    content()->addStretch(1);
    auto reload = [this] {
        QSettings().setValue("research/financialsKind", m_kind->checkedId());
        QSettings().setValue("research/financialsPeriod", m_period->checkedId());
        load();
    };
    connect(m_kind, &QButtonGroup::idClicked, this, reload);
    connect(m_period, &QButtonGroup::idClicked, this, reload);
}

void FinancialsView::load()
{
    const QString sym = m_symbol;
    const QString kind = QStringList {"income", "balance", "cash"}.value(m_kind->checkedId());
    setStatus(tr("Loading…"));
    Client::instance()->statement(sym, kind, m_period->checkedId() == 1, this,
                                  [this, sym, kind](const Statement &s, const QString &err) {
        if (sym != m_symbol || kind != QStringList {"income", "balance", "cash"}.value(m_kind->checkedId()))
            return;
        setStatus(s.ends.isEmpty() ? tr("No statements: %1").arg(err)
                                   : tr("Source: Yahoo Finance (as reported, %1)").arg(s.currency));
        fill(s);
    });
}

void FinancialsView::fill(const Statement &s)
{
    const Palette &c = Theme::current();
    const QString kind = QStringList {"income", "balance", "cash"}.value(m_kind->checkedId());
    const bool quarterly = m_period->checkedId() == 1;
    QStringList headers {tr("METRIC")};
    for (const QDate &d : s.ends)
        headers << (quarterly ? periodLabel(d, true) : QString::number(d.year()));
    m_table->clear();
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < headers.size(); ++i) {
        m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
        m_table->setColumnWidth(i, 220);
        m_table->horizontalHeaderItem(i)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    m_table->setRowCount(0);
    for (const StatementLine &line : statementLayout(kind)) {
        const QVector<double> values = s.lines.value(line.key);
        if (std::none_of(values.begin(), values.end(), [](double v) { return ok(v); }))
            continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto *name = cell(QString(line.depth * 3, ' ') + line.label, c.Text, Qt::AlignLeft, line.group);
        m_table->setItem(r, 0, name);
        const bool perShare = line.key.contains("EPS");
        const bool count = line.key.contains("Shares") || line.key == "ShareIssued";
        const bool rate = line.key == "TaxRateForCalcs";
        for (int i = 0; i < s.ends.size(); ++i) {
            const double v = values.value(i, NaN);
            QString t = !ok(v) ? "–" : rate ? percent(v, 2, false) : count ? QLocale().toString(v, 'f', 0)
                                     : perShare ? full(v, s.currency) : full(v, s.currency);
            m_table->setItem(r, i + 1, cell(t, ok(v) && v < 0 ? c.Down : c.Text, Qt::AlignRight, line.group));
        }
        if (line.group)
            for (int col = 0; col < m_table->columnCount(); ++col)
                m_table->item(r, col)->setBackground(c.Active);
    }
    fitHeight(m_table);
}

// ---------------------------------------------------------------- Financial charts

FinancialChartsView::FinancialChartsView(QWidget *parent)
    : View(parent)
{
    QVBoxLayout *b;
    QHBoxLayout *h;
    auto *bar = panel({}, &b, &h);
    h->insertWidget(0, segments({tr("Annual"), tr("Quarterly")}, &m_period));
    h->addWidget(segments({"5Y", "10Y", "15Y", "MAX"}, &m_range, 1));
    content()->addWidget(bar);
    auto addChart = [this](const QString &title, BarChart **chart) {
        QVBoxLayout *cb;
        auto *p = panel(title, &cb);
        *chart = new BarChart;
        (*chart)->setMinimumHeight(340);
        cb->addWidget(*chart);
        content()->addWidget(p);
    };
    addChart(tr("Income Statement"), &m_income);
    addChart(tr("Margin (%)"), &m_margin);
    addChart(tr("Earnings per Share (EPS)"), &m_eps);
    addChart(tr("Cash Flow"), &m_cash);
    addChart(tr("Balance Sheet"), &m_balance);
    m_source = label({}, "resNote");
    content()->addWidget(m_source);
    content()->addStretch(1);
    connect(m_period, &QButtonGroup::idClicked, this, [this] { fill(); });
    connect(m_range, &QButtonGroup::idClicked, this, [this] { fill(); });
}

void FinancialChartsView::load()
{
    const QString sym = m_symbol;
    m_fund = {};
    fill();
    setStatus(tr("Loading the reported figures…"));
    Client::instance()->fundamentals(sym, this, [this, sym](const Fundamentals &f, const QString &err) {
        if (sym != m_symbol)
            return;
        m_fund = f;
        setStatus(f.isEmpty() ? tr("No figures: %1").arg(err) : QString());
        fill();
    });
}

void FinancialChartsView::fill()
{
    const bool quarterly = m_period->checkedId() == 1;
    const int years = QVector<int> {5, 10, 15, 100}.value(m_range->checkedId());
    const int count = quarterly ? years * 4 : years;
    const auto &set = quarterly ? m_fund.quarterly : m_fund.annual;
    // The periods: those revenue (else net income) has, the latest `count`.
    Series spine = set.value("revenue");
    if (spine.isEmpty())
        spine = set.value("netIncome");
    if (spine.size() > count)
        spine = spine.mid(spine.size() - count);
    QStringList cats;
    for (const Point &p : spine)
        cats << periodLabel(p.date, quarterly);
    auto values = [&](const char *id) {
        QVector<double> out;
        const Series s = set.value(id);
        for (const Point &p : spine) {
            double v = NaN;
            for (const Point &q : s)
                if (q.date == p.date)
                    v = q.value;
            out << v;
        }
        return out;
    };
    const QString cur = m_fund.currency;
    auto moneyFmt = [cur](double v) { return money(v, cur); };
    m_income->setData(cats, {{tr("Revenue"), palette::blue(), values("revenue")},
                             {tr("Gross Profit"), palette::yellow(), values("grossProfit")},
                             {tr("Operating Income"), palette::red(), values("operatingIncome")},
                             {tr("Net Income"), palette::green(), values("netIncome")}}, moneyFmt);
    auto pct = [](QVector<double> v) {
        return v;
    };
    m_margin->setData(cats, {{tr("Gross Margin"), palette::blue(), pct(values("grossMargin"))},
                             {tr("Operating Margin"), palette::yellow(), pct(values("operatingMargin"))},
                             {tr("Net Margin"), palette::red(), pct(values("netMargin"))}},
                      [](double v) { return percent(v, 0, false); }, true, true);
    QVector<double> eps = values("epsDiluted");
    if (std::none_of(eps.begin(), eps.end(), [](double v) { return ok(v); }))
        eps = values("eps");
    m_eps->setData(cats, {{tr("EPS (diluted)"), palette::blue(), eps}}, [cur](double v) { return money(v, cur); });
    m_cash->setData(cats, {{tr("Operating Cash Flow"), palette::blue(), values("ocf")},
                           {tr("CapEx"), palette::red(), values("capex")},
                           {tr("Free Cash Flow"), palette::green(), values("fcf")}}, moneyFmt);
    m_balance->setData(cats, {{tr("Total Assets"), palette::blue(), values("assets")},
                              {tr("Total Liabilities"), palette::red(), values("liabilities")},
                              {tr("Total Equity"), palette::green(), values("equity")},
                              {tr("Total Debt"), palette::yellow(), values("totalDebt")}}, moneyFmt);
    m_source->setText(m_fund.isEmpty() ? QString()
                      : m_fund.source == "SEC EDGAR"
                          ? tr("Source: SEC EDGAR, XBRL figures as filed in 10-K / 10-Q reports (fourth quarters are the year less "
                               "the first nine months). Years are fiscal years, named after the year they end in; "
                               "quarters by the calendar quarter they end in.")
                          : tr("Source: Yahoo Finance (the last four years and five quarters only)."));
}

// ---------------------------------------------------------------- Ownership

namespace {

// Shares sold and bought by insiders, per quarter.
class QuarterVolume : public QWidget
{
public:
    struct Row {
        QString label;
        double sold = 0, bought = 0;
    };
    void setRows(const QVector<Row> &rows)
    {
        m_rows = rows;
        setFixedHeight(std::max<int>(40, rows.size() * 62));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const Palette &c = Theme::current();
        double max = 1;
        for (const Row &r : m_rows)
            max = std::max({max, r.sold, r.bought});
        if (m_rows.isEmpty()) {
            p.setPen(c.TextFaint);
            p.setFont(mono(11));
            p.drawText(rect(), Qt::AlignCenter, QObject::tr("No insider trades reported"));
            return;
        }
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row &r = m_rows[i];
            const QRectF box(0, i * 62, width(), 54);
            p.fillRect(box, c.Background);
            p.setPen(QPen(c.PanelLine, 1));
            p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
            p.setFont(mono(10, true));
            p.setPen(c.TextDim);
            p.drawText(QRectF(box.left(), box.top(), 80, box.height()), Qt::AlignCenter, r.label);
            const double left = box.left() + 84, w = box.width() - 84 - 8;
            const double sw = std::max(r.sold > 0 ? 8.0 : 0.0, w * r.sold / max);
            const double bw = std::max(r.bought > 0 ? 8.0 : 0.0, w * r.bought / max);
            QLinearGradient gs(left, 0, left + w, 0);
            gs.setColorAt(0, palette::red());
            gs.setColorAt(1, palette::red().darker(115));
            p.fillRect(QRectF(left, box.top() + 9, sw, 17), gs);
            p.fillRect(QRectF(left, box.top() + 30, bw, 17), palette::green());
            p.setFont(mono(9, true));
            p.setPen(Qt::white);
            if (r.sold > 0)
                p.drawText(QRectF(left + 5, box.top() + 9, std::max(sw, 120.0), 17), Qt::AlignVCenter,
                           QObject::tr("%1 sold").arg(compact(r.sold)));
            p.setPen(r.bought > 0 ? c.Text : c.TextFaint);
            p.drawText(QRectF(left + bw + 6, box.top() + 30, 200, 17), Qt::AlignVCenter,
                       r.bought > 0 ? QObject::tr("%1 bought").arg(compact(r.bought)) : QObject::tr("none bought"));
        }
    }

private:
    QVector<Row> m_rows;
};

} // namespace

OwnershipView::OwnershipView(QWidget *parent)
    : View(parent)
{
    QVBoxLayout *b;
    auto *breakdown = panel(tr("Ownership Breakdown"), &b);
    m_bar = new ProportionBar;
    b->addWidget(m_bar);
    auto *parts = new QHBoxLayout;
    parts->setSpacing(12);
    const QStringList names {tr("Institutions"), tr("Individual Insiders"), tr("General Public")};
    const QColor colors[3] {palette::purple(), palette::orange(), palette::green()};
    for (int i = 0; i < 3; ++i) {
        auto *box = new QFrame;
        box->setObjectName("resInset");
        auto *v = new QVBoxLayout(box);
        v->setContentsMargins(12, 8, 12, 8);
        v->setSpacing(2);
        v->addWidget(label(names[i].toUpper(), "resStatLabel"));
        m_parts[i] = label({}, "resMidValue");
        m_parts[i]->setStyleSheet(QString("border-left: 3px solid %1; padding-left: 6px;").arg(colors[i].name()));
        v->addWidget(m_parts[i]);
        m_partShares[i] = label({}, "resNote");
        v->addWidget(m_partShares[i]);
        parts->addWidget(box, 1);
    }
    b->addLayout(parts);
    content()->addWidget(breakdown);

    auto *holders = panel(tr("Top 25 Shareholders"), &b);
    m_holdersNote = label({}, "resNote");
    b->addWidget(m_holdersNote);
    m_holders = table({tr("% Own"), tr("Holder"), tr("Reported"), tr("Shares"), tr("Value"), tr("Change")});
    b->addWidget(m_holders);
    content()->addWidget(holders);

    auto *volume = panel(tr("Insider Trading Volume"), &b);
    m_volume = new QuarterVolume;
    b->addWidget(m_volume);
    content()->addWidget(volume);

    auto *trades = panel(tr("Recent Transactions"), &b);
    m_tradesNote = label({}, "resNote");
    b->addWidget(m_tradesNote);
    m_trades = table({tr("Date"), tr("Name"), tr("Relationship"), tr("Action"), tr("Shares"), tr("Price"), tr("Value")});
    b->addWidget(m_trades);
    content()->addWidget(trades);
    content()->addStretch(1);
}

void OwnershipView::load()
{
    const QString sym = m_symbol;
    setStatus(tr("Loading…"));
    m_sharesOut = NaN;
    Client::instance()->summary(sym, this, [this, sym](const QJsonObject &s, const QString &err) {
        if (sym != m_symbol)
            return;
        setStatus(s.isEmpty() ? err : QString());
        fillBreakdown(s);
        Client::instance()->holders(sym, this, [this, sym](const Holders &h, const QString &err2) {
            if (sym != m_symbol)
                return;
            fillHolders(h, m_sharesOut);
            if (h.top.isEmpty())
                m_holdersNote->setText(err2);
        });
    });
    Client::instance()->insiders(sym, this, [this, sym](const QVector<InsiderTrade> &t, const QString &err) {
        if (sym != m_symbol)
            return;
        fillInsiders(t);
        if (t.isEmpty())
            m_tradesNote->setText(err);
    });
}

void OwnershipView::fillBreakdown(const QJsonObject &summary)
{
    const QJsonObject mh = summary.value("majorHoldersBreakdown").toObject();
    const QJsonObject ks = summary.value("defaultKeyStatistics").toObject();
    m_sharesOut = raw(ks, "sharesOutstanding");
    m_price = raw(summary.value("price").toObject(), "regularMarketPrice");
    double inst = raw(mh, "institutionsPercentHeld"), ins = raw(mh, "insidersPercentHeld");
    if (!ok(inst)) inst = 0;
    if (!ok(ins)) ins = 0;
    const double pub = std::max(0.0, 1 - inst - ins);
    m_bar->setParts({{inst, palette::purple()}, {ins, palette::orange()}, {pub, palette::green()}});
    const double v[3] {inst, ins, pub};
    for (int i = 0; i < 3; ++i) {
        m_parts[i]->setText(percent(v[i], 2, false));
        m_partShares[i]->setText(ok(m_sharesOut) ? tr("%1 shares").arg(compact(v[i] * m_sharesOut)) : QString());
    }
}

void OwnershipView::fillHolders(const Holders &h, double sharesOut)
{
    const Palette &c = Theme::current();
    m_holders->setRowCount(0);
    double sum = 0;
    for (const Holder &x : h.top) {
        const int r = m_holders->rowCount();
        m_holders->insertRow(r);
        const double own = ratio(x.shares, sharesOut);
        if (ok(own))
            sum += own;
        m_holders->setItem(r, 0, cell(percent(own, 2, false)));
        m_holders->setItem(r, 1, cell(x.name.toUpper()));
        m_holders->setItem(r, 2, cell(dateText(x.date), c.TextDim));
        m_holders->setItem(r, 3, cell(compact(x.shares), {}, Qt::AlignRight));
        m_holders->setItem(r, 4, cell(money(ok(m_price) ? x.shares * m_price : x.value), {}, Qt::AlignRight));
        m_holders->setItem(r, 5, cell(percent(x.changePct), changeColor(x.changePct), Qt::AlignRight));
    }
    auto *hh = m_holders->horizontalHeader();
    hh->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int col : {0, 2, 3, 4, 5}) {
        hh->setSectionResizeMode(col, QHeaderView::Fixed);
        m_holders->setColumnWidth(col, col == 0 ? 110 : 150);
    }
    for (int col : {3, 4, 5})
        m_holders->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fitHeight(m_holders);
    QDate firstReport, lastReport;
    for (const Holder &x : h.top)
        if (x.date.isValid()) {
            if (!firstReport.isValid() || x.date < firstReport) firstReport = x.date;
            if (!lastReport.isValid() || x.date > lastReport) lastReport = x.date;
        }
    const QString dates = firstReport == lastReport ? tr("reported %1").arg(dateText(lastReport))
                                                    : tr("reported between %1 and %2 (the total mixes these dates)")
                                                          .arg(dateText(firstReport), dateText(lastReport));
    m_holdersNote->setText(h.top.isEmpty() ? QString()
                           : tr("Top %1 institutional holders own %2 of the company, %3 · %4 institutions file a 13F for it · "
                                "value at today's price · source: Nasdaq (13F filings)")
                                 .arg(h.top.size()).arg(percent(sum, 2, false)).arg(dates).arg(QLocale().toString(h.count)));
}

void OwnershipView::fillInsiders(const QVector<InsiderTrade> &trades)
{
    const Palette &c = Theme::current();
    // Open-market sales and purchases per quarter (gifts, tax withholdings and option exercises left out), the
    // last six. Nasdaq returns the latest 200 trades: when the list is full, its oldest quarter is incomplete
    // and isn't shown.
    std::map<std::pair<int, int>, QuarterVolume::Row> byQuarter;
    QDate oldest;
    for (const InsiderTrade &t : trades) {
        if (!t.date.isValid() || !ok(t.shares))
            continue;
        if (!oldest.isValid() || t.date < oldest)
            oldest = t.date;
        auto &row = byQuarter[{t.date.year(), (t.date.month() - 1) / 3 + 1}];
        row.label = QString("Q%1 %2").arg((t.date.month() - 1) / 3 + 1).arg(t.date.year());
        if (t.openMarket && t.sell)
            row.sold += t.shares;
        else if (t.openMarket && t.buy)
            row.bought += t.shares;
    }
    if (trades.size() >= 200 && oldest.isValid())
        byQuarter.erase({oldest.year(), (oldest.month() - 1) / 3 + 1});
    QVector<QuarterVolume::Row> rows;
    for (const auto &[k, r] : byQuarter)
        rows.push_back(r);
    if (rows.size() > 6)
        rows = rows.mid(rows.size() - 6);
    static_cast<QuarterVolume *>(m_volume)->setRows(rows);

    m_trades->setRowCount(0);
    const int shown = std::min<int>(50, trades.size());
    for (int i = 0; i < shown; ++i) {
        const InsiderTrade &t = trades[i];
        const int r = m_trades->rowCount();
        m_trades->insertRow(r);
        m_trades->setItem(r, 0, cell(dateText(t.date)));
        m_trades->setItem(r, 1, cell(t.name));
        m_trades->setItem(r, 2, cell(t.relation, c.TextDim));
        const QString action = !t.openMarket ? t.type.toUpper() : t.sell ? tr("SELL") : tr("BUY");
        m_trades->setItem(r, 3, cell(action, !t.openMarket ? c.TextDim : t.sell ? c.Down : c.Up, Qt::AlignLeft, true));
        m_trades->item(r, 3)->setToolTip(t.type);
        m_trades->setItem(r, 4, cell(compact(t.shares), {}, Qt::AlignRight));
        m_trades->setItem(r, 5, cell(money(t.price), {}, Qt::AlignRight));
        m_trades->setItem(r, 6, cell(ok(t.price) && ok(t.shares) ? full(t.price * t.shares) : "–", {}, Qt::AlignRight));
    }
    auto *hh = m_trades->horizontalHeader();
    hh->setSectionResizeMode(2, QHeaderView::Stretch);
    const int widths[7] {110, 230, 0, 160, 110, 110, 160};
    for (int col = 0; col < 7; ++col)
        if (col != 2) {
            hh->setSectionResizeMode(col, QHeaderView::Fixed);
            m_trades->setColumnWidth(col, widths[col]);
        }
    for (int col : {4, 5, 6})
        m_trades->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fitHeight(m_trades);
    m_tradesNote->setText(trades.isEmpty() ? QString()
                          : tr("Showing %1 transactions · source: Nasdaq (SEC Form 4 filings) · the quarterly volumes above "
                               "count open-market sales and purchases only").arg(shown));
}

// ---------------------------------------------------------------- Ratings

namespace {

QLabel *badge(const QString &text, const QColor &bg, const QColor &fg = QColor())
{
    auto *l = new QLabel(text);
    l->setObjectName("resBadge");
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QString("background: %1; color: %2;").arg(bg.name(), (fg.isValid() ? fg : QColor(20, 20, 20)).name()));
    return l;
}

QColor gradeColor(const QString &g)
{
    if (g.startsWith('A')) return palette::darkGreen();
    if (g.startsWith('B')) return palette::green();
    if (g.startsWith('C')) return palette::yellow();
    if (g.startsWith('D')) return palette::orange();
    return palette::red();
}

QColor ratingColor(double score)
{
    if (!ok(score)) return Theme::current().PanelLine;
    if (score >= 3.5) return palette::green();
    if (score >= 2.5) return palette::yellow();
    return palette::red();
}

void styleBadge(QLabel *l, const QString &text, const QColor &bg)
{
    l->setText(text);
    l->setStyleSheet(QString("background: %1; color: #141414;").arg(bg.name()));
}

} // namespace

RatingsView::RatingsView(QWidget *parent)
    : View(parent)
{
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(14);
    QVBoxLayout *b;
    auto *rec = panel(tr("Analyst Recommendation"), &b);
    m_basedOn = label({}, "resNote");
    b->addWidget(m_basedOn);
    auto *recRow = new QHBoxLayout;
    m_gauge = new Gauge;
    recRow->addWidget(m_gauge);
    m_bars = new HBars;
    recRow->addWidget(m_bars, 1, Qt::AlignTop);
    b->addLayout(recRow);
    b->addWidget(label(tr("Average Analyst Rating"), "resStatLabel"));
    m_average = label({}, "resBigValue");
    b->addWidget(m_average);
    m_scale = new RatingScale;
    b->addWidget(m_scale);
    b->addStretch(1);
    topRow->addWidget(rec, 1);

    auto *val = panel(tr("Valuation & Ratings"), &b);
    auto *iv = new QFrame;
    iv->setObjectName("resInset");
    auto *ivl = new QHBoxLayout(iv);
    auto *ivText = new QVBoxLayout;
    ivText->addWidget(experimentalTag(tr("Intrinsic value: a two-stage discounted cash flow (see the value's tooltip).")),
                      0, Qt::AlignLeft);
    m_valuation[0] = label({}, "resStatValue");
    m_valuation[1] = label({}, "resStatValue");
    ivText->addWidget(m_valuation[0]);
    ivText->addWidget(m_valuation[1]);
    ivl->addLayout(ivText, 1);
    m_badge = badge({}, palette::red());
    m_badge->setMinimumWidth(150);
    ivl->addWidget(m_badge);
    b->addWidget(iv);
    auto *tgt = new QFrame;
    tgt->setObjectName("resInset");
    auto *tl = new QVBoxLayout(tgt);
    auto *tt = label(tr("Analysts Target Price"), "resInsetTitle");
    tt->setAlignment(Qt::AlignCenter);
    tl->addWidget(tt);
    auto *ts = label(tr("12 Month Consensus"), "resNote");
    ts->setAlignment(Qt::AlignCenter);
    tl->addWidget(ts);
    m_target = label({}, "resBigValue");
    m_target->setAlignment(Qt::AlignCenter);
    tl->addWidget(m_target);
    b->addWidget(tgt);
    auto *gr = new QFrame;
    gr->setObjectName("resInset");
    auto *gl = new QVBoxLayout(gr);
    auto *gtRow = new QHBoxLayout;
    gtRow->addStretch(1);
    gtRow->addWidget(label(tr("Factor Grades"), "resInsetTitle"));
    gtRow->addSpacing(8);
    gtRow->addWidget(experimentalTag(tr("Factor grades: letters from fixed thresholds, the same for every sector "
                                        "(banks, utilities and real estate come out worse by construction).")));
    gtRow->addStretch(1);
    gl->addLayout(gtRow);
    auto *grow = new QHBoxLayout;
    grow->addStretch(1);
    const QStringList gnames {tr("Valuation"), tr("Profitability"), tr("Financial Health")};
    for (int i = 0; i < 3; ++i) {
        auto *col = new QVBoxLayout;
        m_grades[i] = badge("–", palette::yellow());
        m_grades[i]->setFixedSize(42, 42);
        m_grades[i]->setObjectName("resGrade");
        col->addWidget(m_grades[i], 0, Qt::AlignHCenter);
        auto *n = label(gnames[i], "resNote");
        n->setAlignment(Qt::AlignCenter);
        col->addWidget(n);
        grow->addLayout(col);
        grow->addSpacing(20);
    }
    grow->addStretch(1);
    gl->addLayout(grow);
    b->addWidget(gr);
    auto *qr = new QFrame;
    qr->setObjectName("resInset");
    auto *ql = new QHBoxLayout(qr);
    ql->addWidget(label(tr("Quant Rating"), "resStatValue"));
    ql->addSpacing(8);
    ql->addWidget(experimentalTag(tr("Quant rating: weights chosen by hand (valuation and profitability 30 % each, "
                                     "health 20 %, momentum and revisions 20 %), never tested on past prices.")));
    ql->addStretch(1);
    m_quant = badge("–", palette::yellow());
    m_quant->setMinimumWidth(110);
    ql->addWidget(m_quant);
    b->addWidget(qr);
    topRow->addWidget(val, 1);
    content()->addLayout(topRow);

    auto *fc = panel(tr("12 Month Price Forecast"), &b);
    auto *tr3 = new QHBoxLayout;
    const QStringList tnames {tr("Highest Target"), tr("Average Target"), tr("Lowest Target")};
    for (int i = 0; i < 3; ++i) {
        auto *box = new QFrame;
        box->setObjectName("resInset");
        auto *v = new QVBoxLayout(box);
        auto *n = label(tnames[i], "resNote");
        n->setAlignment(Qt::AlignCenter);
        v->addWidget(n);
        m_targets[i] = label({}, "resBigValue");
        m_targets[i]->setAlignment(Qt::AlignCenter);
        v->addWidget(m_targets[i]);
        tr3->addWidget(box, 1);
    }
    b->addLayout(tr3);
    m_forecast = new LineChart;
    m_forecast->setMinimumHeight(300);
    m_forecast->setLegend(true);
    m_forecast->setRotatedLabels(true);
    b->addWidget(m_forecast);
    content()->addWidget(fc);

    QHBoxLayout *hh;
    auto *hist = panel(tr("Analyst Recommendations History"), &b, &hh);
    hh->insertWidget(1, experimentalTag(tr("Rebuilt from each firm's rating changes, sorted into five buckets by "
                                           "Deepwater; it doesn't match Yahoo's own monthly counts exactly.")));
    hh->addWidget(segments({"6M", "1Y", "2Y", "5Y", tr("Max")}, &m_historyRange, 1));
    m_history = new StackedBarChart;
    m_history->setMinimumHeight(320);
    b->addWidget(m_history);
    m_historyNote = label({}, "resNote");
    m_historyNote->setAlignment(Qt::AlignCenter);
    m_historyNote->setWordWrap(true);
    b->addWidget(m_historyNote);
    content()->addWidget(hist);
    connect(m_historyRange, &QButtonGroup::idClicked, this, [this] { fillHistory(); });

    auto *changes = panel(tr("Recent Rating Changes"), &b);
    m_changes = table({tr("Date"), tr("Action"), tr("Analyst"), tr("Rating Change"), tr("Price Target")});
    m_changes->verticalHeader()->setDefaultSectionSize(28);
    b->addWidget(m_changes);
    content()->addWidget(changes);
    content()->addStretch(1);
}

void RatingsView::load()
{
    const QString sym = m_symbol;
    m_summary = {};
    m_prices.clear();
    setStatus(tr("Loading…"));
    Client::instance()->summary(sym, this, [this, sym](const QJsonObject &s, const QString &err) {
        if (sym != m_symbol)
            return;
        m_summary = s;
        setStatus(s.isEmpty() ? err : QString());
        fill();
    });
    Client::instance()->prices(sym, "1y", "1mo", this, [this, sym](const Series &s, const QString &) {
        if (sym != m_symbol)
            return;
        m_prices = s;
        fill();
    });
}

void RatingsView::fill()
{
    const QJsonObject fd = m_summary.value("financialData").toObject();
    const QJsonObject price = m_summary.value("price").toObject();
    const QString cur = text(price, "currency").isEmpty() ? QString("USD") : text(price, "currency");
    // Current month's counts.
    const QJsonArray trend = m_summary.value("recommendationTrend").toObject().value("trend").toArray();
    QJsonObject now;
    for (const QJsonValue &t : trend)
        if (t.toObject().value("period").toString() == "0m")
            now = t.toObject();
    const double sb = raw(now, "strongBuy"), bu = raw(now, "buy"), ho = raw(now, "hold"), se = raw(now, "sell"), ss = raw(now, "strongSell");
    const double total = (ok(sb) ? sb : 0) + (ok(bu) ? bu : 0) + (ok(ho) ? ho : 0) + (ok(se) ? se : 0) + (ok(ss) ? ss : 0);
    const double score = total > 0 ? (5 * sb + 4 * bu + 3 * ho + 2 * se + 1 * ss) / total : NaN;
    m_basedOn->setText(total > 0 ? tr("Based on %1 analysts this month · Yahoo Finance").arg(total) : tr("No analyst coverage"));
    m_gauge->setValue(score, ratingName(score));
    m_bars->setData({{tr("Strong Buy"), ok(sb) ? sb : 0, palette::darkGreen()}, {tr("Buy"), ok(bu) ? bu : 0, palette::green()},
                     {tr("Hold"), ok(ho) ? ho : 0, palette::yellow()}, {tr("Sell"), ok(se) ? se : 0, palette::orange()},
                     {tr("Strong Sell"), ok(ss) ? ss : 0, palette::red()}});
    // Yahoo's mean: 1 = strong buy … 5 = strong sell; shown the other way round.
    const double mean = raw(fd, "recommendationMean");
    const double avg = ok(mean) ? 6 - mean : NaN;
    m_average->setText(ok(avg) ? QString("%1 <span style='font-size:13px; color:%2'>/ 5</span>")
                                     .arg(plain(avg)).arg(Theme::current().TextFaint.name()) : "N/A");
    m_average->setToolTip(tr("%1 analysts' mean recommendation (Yahoo Finance)").arg(plain(raw(fd, "numberOfAnalystOpinions"), 0)));
    m_scale->setValue(avg);

    const Valuation v = intrinsicValue(m_summary);
    const Grades g = factorGrades(m_summary, v);
    m_valuation[0]->setText(tr("INTRINSIC VALUE  %1").arg(money(v.intrinsic, cur)));
    m_valuation[1]->setText(tr("CURRENT PRICE  %1").arg(money(v.price, cur)));
    for (QLabel *l : m_valuation)
        l->setToolTip(v.method);
    if (ok(v.upside))
        styleBadge(m_badge, v.upside < 0 ? tr("%1 OVERVALUED").arg(percent(ratio(v.price - v.intrinsic, v.intrinsic), 0, false))
                                         : tr("%1 UNDERVALUED").arg(percent(v.upside, 0, false)),
                   v.upside < 0 ? palette::red() : palette::green());
    else
        styleBadge(m_badge, tr("NOT MEANINGFUL"), Theme::current().PanelLine);
    m_badge->setToolTip(v.method);
    m_target->setText(money(raw(fd, "targetMeanPrice"), cur));
    const QString grades[3] {g.valuation, g.profitability, g.health};
    const QString gtips[3] {tr("Upside to the intrinsic value, forward P/E, PEG, EV/EBITDA"),
                            tr("Gross, operating and net margins, ROE, ROA"),
                            tr("Current and quick ratios, debt/equity, net cash against the market cap, free cash flow/debt")};
    for (int i = 0; i < 3; ++i) {
        styleBadge(m_grades[i], grades[i], gradeColor(grades[i]));
        m_grades[i]->setToolTip(gtips[i] + "\n" + tr("Deepwater's own scoring on fixed thresholds, from Yahoo Finance figures."));
    }
    styleBadge(m_quant, g.rating.toUpper(), ratingColor(g.score));
    m_quant->setToolTip(tr("Deepwater's quant score %1 / 5: valuation and profitability 30 % each, financial health 20 %, "
                           "momentum against the S&P 500 and earnings revisions 20 %.").arg(plain(g.score)));

    const double hi = raw(fd, "targetHighPrice"), lo = raw(fd, "targetLowPrice"), mid = raw(fd, "targetMeanPrice");
    m_targets[0]->setText(money(hi, cur));
    m_targets[0]->setStyleSheet(QString("color: %1;").arg(Theme::current().Up.name()));
    m_targets[1]->setText(money(mid, cur));
    m_targets[2]->setText(money(lo, cur));
    m_targets[2]->setStyleSheet(QString("color: %1;").arg(Theme::current().Down.name()));

    // Monthly closes over the last year, then straight lines to the three targets a year out.
    QStringList cats;
    QVector<double> past, high, avgLine, low;
    const QLocale loc;
    Series monthly;   // Yahoo adds today's price to the monthly bars: one point a month
    for (const Point &p : m_prices) {
        if (!monthly.isEmpty() && monthly.last().date.year() == p.date.year() && monthly.last().date.month() == p.date.month())
            monthly.last() = p;
        else
            monthly.push_back(p);
    }
    for (const Point &p : monthly) {
        cats << loc.toString(p.date, "MMM yyyy");
        past << p.value;
        high << NaN;
        avgLine << NaN;
        low << NaN;
    }
    const double lastPrice = ok(raw(price, "regularMarketPrice")) ? raw(price, "regularMarketPrice") : lastValue(m_prices);
    if (!past.isEmpty()) {
        past.last() = lastPrice;
        high.last() = avgLine.last() = low.last() = lastPrice;
        const QDate start = monthly.last().date;
        for (int m = 1; m <= 12; ++m) {
            cats << loc.toString(start.addMonths(m), "MMM yyyy");
            past << NaN;
            const double t = m / 12.0;
            high << lastPrice + (hi - lastPrice) * t;
            avgLine << lastPrice + (mid - lastPrice) * t;
            low << lastPrice + (lo - lastPrice) * t;
        }
    }
    m_forecast->setData(cats, {{tr("Price"), palette::cyan(), past, true, true, false},
                               {tr("High Forecast"), palette::green(), high, false, false, true},
                               {tr("Average Forecast"), Theme::current().TextDim, avgLine, false, false, true},
                               {tr("Low Forecast"), palette::red(), low, false, false, true}},
                        [cur](double x) { return money(x, cur, 0); });

    fillHistory();

    // Rating changes, latest first.
    const Palette &c = Theme::current();
    m_changes->setRowCount(0);
    const QJsonArray history = m_summary.value("upgradeDowngradeHistory").toObject().value("history").toArray();
    for (int i = 0; i < std::min<int>(40, history.size()); ++i) {
        const QJsonObject h = history[i].toObject();
        const int r = m_changes->rowCount();
        m_changes->insertRow(r);
        const QString act = h.value("action").toString();
        const QString name = act == "up" ? tr("UPGRADE") : act == "down" ? tr("DOWNGRADE") : act == "init" ? tr("INITIATE")
                             : act == "reit" ? tr("REITERATE") : tr("MAINTAIN");
        const QColor col = act == "up" ? c.Up : act == "down" ? c.Down : act == "init" ? palette::cyan() : palette::yellow();
        m_changes->setItem(r, 0, cell(dateText(epochDate(h.value("epochGradeDate").toDouble()))));
        auto *b = new QLabel(name);
        b->setObjectName("resActionBadge");
        b->setAlignment(Qt::AlignCenter);
        b->setStyleSheet(QString("color: %1; border: 1px solid %1;").arg(col.name()));
        auto *holder = new QWidget;
        auto *hl = new QHBoxLayout(holder);
        hl->setContentsMargins(0, 3, 0, 3);
        hl->addWidget(b);
        hl->addStretch(1);
        m_changes->setCellWidget(r, 1, holder);
        m_changes->setItem(r, 2, cell(h.value("firm").toString()));
        const QString from = h.value("fromGrade").toString(), to = h.value("toGrade").toString();
        m_changes->setItem(r, 3, cell(!from.isEmpty() && from != to ? from + " → " + to : to));
        const double pt = h.value("currentPriceTarget").toDouble(NaN), prior = h.value("priorPriceTarget").toDouble(NaN);
        QString target = ok(pt) && pt > 0 ? money(pt, cur) : QString("–");
        if (ok(prior) && prior > 0 && prior != pt)
            target += tr(" (from %1)").arg(money(prior, cur));
        m_changes->setItem(r, 4, cell(target, ok(prior) && ok(pt) && prior > 0 && pt > 0 ? changeColor(pt - prior) : QColor(),
                                      Qt::AlignRight));
    }
    auto *hh = m_changes->horizontalHeader();
    hh->setSectionResizeMode(2, QHeaderView::Stretch);
    hh->setSectionResizeMode(3, QHeaderView::Stretch);
    for (int col : {0, 1, 4}) {
        hh->setSectionResizeMode(col, QHeaderView::Fixed);
        m_changes->setColumnWidth(col, col == 4 ? 220 : 140);
    }
    m_changes->horizontalHeaderItem(4)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fitHeight(m_changes);
}

void RatingsView::fillHistory()
{
    // Each firm's grade at the end of each month: its last rating in the twelve months before, sorted into
    // five buckets. Yahoo only keeps four months of its own counts, so the history is rebuilt from the
    // rating changes it lists (every firm's actions since about 2012).
    const QJsonArray history = m_summary.value("upgradeDowngradeHistory").toObject().value("history").toArray();
    struct Rating {
        QDate date;
        QString firm;
        int bucket;
    };
    QVector<Rating> ratings;
    QDate first = QDate::currentDate();
    for (const QJsonValue &v : history) {
        const QJsonObject h = v.toObject();
        const int b = gradeBucket(h.value("toGrade").toString());
        const QDate d = epochDate(h.value("epochGradeDate").toDouble());
        if (b < 0 || !d.isValid())
            continue;
        ratings.push_back({d, h.value("firm").toString(), b});
        first = std::min(first, d);
    }
    std::sort(ratings.begin(), ratings.end(), [](const Rating &a, const Rating &b) { return a.date < b.date; });
    const int months = QVector<int> {6, 12, 24, 60, 1000}.value(m_historyRange->checkedId());
    const QDate today = QDate::currentDate();
    QDate start(today.year(), today.month(), 1);
    start = start.addMonths(-(months - 1));
    const QDate earliest = QDate(first.year(), first.month(), 1).addMonths(12);
    if (start < earliest)
        start = earliest;
    QStringList cats;
    QVector<double> buckets[5];
    QVector<double> avg;
    const QLocale loc;
    int firmsNow = 0;
    for (QDate m = start; m <= today; m = m.addMonths(1)) {
        const QDate end = std::min(m.addMonths(1).addDays(-1), today);
        QHash<QString, int> latest;
        for (const Rating &r : ratings) {
            if (r.date > end)
                break;
            if (r.date > end.addYears(-1))
                latest[r.firm] = r.bucket;
            else
                latest.remove(r.firm);
        }
        double counts[5] {0, 0, 0, 0, 0};
        for (int b : latest)
            counts[b] += 1;
        const double n = latest.size();
        cats << loc.toString(m, months > 24 ? "MMM yy" : "MMM yyyy");
        for (int b = 0; b < 5; ++b)
            buckets[b] << counts[b];
        avg << (n > 0 ? (counts[0] * 1 + counts[1] * 2 + counts[2] * 3 + counts[3] * 4 + counts[4] * 5) / n : NaN);
        firmsNow = int(n);
    }
    m_history->setData(cats,
                       {{tr("Strong Sell"), palette::red().darker(130), buckets[0]}, {tr("Sell"), palette::red(), buckets[1]},
                        {tr("Hold"), palette::yellow(), buckets[2]}, {tr("Buy"), palette::green(), buckets[3]},
                        {tr("Strong Buy"), palette::darkGreen(), buckets[4]}},
                       avg, tr("Avg Rating"), {tr("Strong Sell"), tr("Sell"), tr("Hold"), tr("Buy"), tr("Strong Buy")}, 1, 5,
                       [](double v) { return QString("%1 (%2)").arg(plain(v), ratingName(v)); });
    m_historyNote->setText(tr("Based on %1 firms rating the stock in the last 12 months. Each month counts every firm's last "
                              "rating of the twelve months before it, from the rating changes Yahoo Finance lists.")
                               .arg(firmsNow));
}

} // namespace research
