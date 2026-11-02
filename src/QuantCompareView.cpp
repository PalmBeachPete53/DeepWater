#include "QuantViews.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace quant {

// ---------------------------------------------------------------- shared blocks

QFont mono(int px, bool bold)
{
    return chartFont(px, bold);
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
        auto *b = new QPushButton(labels[i]);
        b->setObjectName("qtSeg");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(i == checked);
        g->addButton(b, i);
        h->addWidget(b);
    }
    *group = g;
    return w;
}

QFrame *bar(QHBoxLayout **layout)
{
    auto *f = new QFrame;
    f->setObjectName("qtBar");
    auto *h = new QHBoxLayout(f);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(10);
    *layout = h;
    return f;
}

QFrame *card(const QString &title, QVBoxLayout **body, QHBoxLayout **head)
{
    auto *f = new QFrame;
    f->setObjectName("qtCard");
    auto *v = new QVBoxLayout(f);
    v->setContentsMargins(14, 10, 14, 12);
    v->setSpacing(6);
    auto *h = new QHBoxLayout;
    h->setSpacing(8);
    auto *t = label(title.toUpper(), "qtCardTitle");
    h->addWidget(t);
    h->addStretch(1);
    v->addLayout(h);
    t->setVisible(!title.isEmpty());   // once in the card: showing a parentless label opens a window
    *body = v;
    if (head)
        *head = h;
    return f;
}

QLabel *label(const QString &text, const char *objectName)
{
    auto *l = new QLabel(text);
    l->setObjectName(objectName);
    return l;
}

void setBadge(QLabel *l, const QString &text, const QColor &color, bool filled)
{
    l->setText(text);
    QColor bg = color;
    bg.setAlpha(filled ? 255 : 40);
    l->setStyleSheet(QString("QLabel { color: %1; background: %2; border: 1px solid %3; border-radius: 2px; "
                             "font-size: 9px; font-weight: 800; letter-spacing: 0.5px; padding: 2px 8px; }")
                         .arg(filled ? Theme::current().Background.name() : color.name(), bg.name(QColor::HexArgb),
                              color.name()));
}

QLabel *badge(const QString &text, const QColor &color, bool filled)
{
    auto *l = new QLabel;
    l->setObjectName("qtBadge");
    l->setAlignment(Qt::AlignCenter);
    setBadge(l, text, color, filled);
    return l;
}

QLineEdit *symbolEdit(const QString &text, const QColor &color)
{
    auto *e = new QLineEdit(text);
    e->setObjectName("qtSymbol");
    e->setFixedWidth(84);
    e->setAlignment(Qt::AlignCenter);
    e->setStyleSheet(QString("QLineEdit#qtSymbol { color: %1; border-color: %1; }").arg(color.name()));
    return e;
}

QString cleanSymbol(const QString &text)
{
    const QString s = text.trimmed().toUpper();
    static const QRegularExpression re("^[A-Z0-9.\\-=^]{1,12}$");
    return re.match(s).hasMatch() ? s : QString();
}

QString yahooRange(const QString &label)
{
    static const QHash<QString, QString> map {{"1 Month", "1mo"}, {"3 Month", "3mo"}, {"6 Month", "6mo"}, {"1 Year", "1y"},
                                              {"2 Years", "2y"}, {"3 Years", "3y"}, {"5 Years", "5y"}};
    return map.value(label, "1y");
}

QColor seriesColor(int i)
{
    static const QVector<QColor> colors {research::palette::red(), research::palette::blue(), research::palette::green(),
                                         research::palette::yellow(), research::palette::purple(), research::palette::cyan(),
                                         research::palette::orange(), research::palette::pink()};
    return colors[((i % colors.size()) + colors.size()) % colors.size()];
}

QColor changeColor(double v)
{
    const Palette &c = Theme::current();
    return !ok(v) ? c.TextFaint : v > 0 ? c.Up : v < 0 ? c.Down : c.Text;
}

// ---------------------------------------------------------------- View

View::View(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_status = label({}, "qtStatus");
    m_status->hide();
    outer->addWidget(m_status);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("qtScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    inner->setObjectName("qtContent");
    m_content = new QVBoxLayout(inner);
    m_content->setContentsMargins(12, 10, 12, 16);
    m_content->setSpacing(10);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
}

void View::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!m_loaded) {
        m_loaded = true;
        load();
    }
}

void View::setStatus(const QString &text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

// ---------------------------------------------------------------- CompareView

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

using research::raw;

// A row of the comparison table: where the figure comes from, how it reads and which way is better.
struct MetricDef {
    QString section;
    QString label;
    enum Kind { Compact, Ratio, Percent } kind;
    int better;   // +1 higher is better, -1 lower is better, 0 neither
    std::function<double(const QJsonObject &)> value;
};

const QVector<MetricDef> &metricDefs()
{
    auto sd = [](const QJsonObject &s) { return s.value("summaryDetail").toObject(); };
    auto ks = [](const QJsonObject &s) { return s.value("defaultKeyStatistics").toObject(); };
    auto fd = [](const QJsonObject &s) { return s.value("financialData").toObject(); };
    // Statements in another currency than the quote (ADRs): the rate Client::summary adds.
    auto fx = [](const QJsonObject &s) { return s.contains("fxToQuote") ? s.value("fxToQuote").toDouble() : 1.0; };
    static const QVector<MetricDef> defs {
        {"Key Stats", "Market Cap", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(sd(s), "marketCap"); }},
        {"Key Stats", "Enterprise Value", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(ks(s), "enterpriseValue"); }},
        {"Key Stats", "Beta", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(sd(s), "beta"); }},
        {"Valuation Ratios", "P/E (TTM)", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(sd(s), "trailingPE"); }},
        {"Valuation Ratios", "Forward P/E", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(sd(s), "forwardPE"); }},
        {"Valuation Ratios", "PEG", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(ks(s), "pegRatio"); }},
        {"Valuation Ratios", "P/S (Price to Sales)", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(sd(s), "priceToSalesTrailing12Months"); }},
        {"Valuation Ratios", "P/B (Price to Book)", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(ks(s), "priceToBook"); }},
        {"Valuation Ratios", "EV/EBITDA", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(ks(s), "enterpriseToEbitda"); }},
        {"Valuation Ratios", "P/FCF (Price to FCF)", MetricDef::Ratio, -1, [=](const QJsonObject &s) {
             const double fcf = raw(fd(s), "freeCashflow") * fx(s);
             return fcf > 0 ? raw(sd(s), "marketCap") / fcf : NaN;
         }},
        {"Profitability & Margins", "Gross Margin", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "grossMargins"); }},
        {"Profitability & Margins", "Operating Margin", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "operatingMargins"); }},
        {"Profitability & Margins", "Net/Profit Margin", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "profitMargins"); }},
        {"Profitability & Margins", "ROE", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "returnOnEquity"); }},
        {"Profitability & Margins", "ROA", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "returnOnAssets"); }},
        {"Profitability & Margins", "ROI/ROIC", MetricDef::Percent, 1, [=](const QJsonObject &s) {
             // Net income over invested capital (debt plus book equity, the latter from the market cap and P/B).
             const double equity = raw(sd(s), "marketCap") / raw(ks(s), "priceToBook");
             const double capital = raw(fd(s), "totalDebt") * fx(s) + equity;
             return capital > 0 ? raw(ks(s), "netIncomeToCommon") * fx(s) / capital : NaN;
         }},
        {"Earnings & Financials", "Revenue (TTM)", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(fd(s), "totalRevenue") * fx(s); }},
        {"Earnings & Financials", "Gross Profit", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(fd(s), "grossProfits") * fx(s); }},
        {"Earnings & Financials", "EBITDA", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(fd(s), "ebitda") * fx(s); }},
        {"Earnings & Financials", "Net Income", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(ks(s), "netIncomeToCommon") * fx(s); }},
        {"Earnings & Financials", "EPS (TTM)", MetricDef::Ratio, 1, [=](const QJsonObject &s) { return raw(ks(s), "trailingEps"); }},
        {"Earnings & Financials", "Forward EPS", MetricDef::Ratio, 1, [=](const QJsonObject &s) { return raw(ks(s), "forwardEps"); }},
        {"Earnings & Financials", "Operating Cash Flow", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(fd(s), "operatingCashflow") * fx(s); }},
        {"Earnings & Financials", "Free Cash Flow", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(fd(s), "freeCashflow") * fx(s); }},
        {"Growth", "Revenue Growth (Y/Y)", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "revenueGrowth"); }},
        {"Growth", "Earnings Growth (Y/Y)", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(fd(s), "earningsGrowth"); }},
        {"Growth", "52 Week Change", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(ks(s), "52WeekChange"); }},
        {"Balance Sheet", "Total Cash", MetricDef::Compact, 1, [=](const QJsonObject &s) { return raw(fd(s), "totalCash") * fx(s); }},
        {"Balance Sheet", "Total Debt", MetricDef::Compact, -1, [=](const QJsonObject &s) { return raw(fd(s), "totalDebt") * fx(s); }},
        {"Balance Sheet", "Debt/Equity", MetricDef::Ratio, -1, [=](const QJsonObject &s) { return raw(fd(s), "debtToEquity"); }},
        {"Balance Sheet", "Current Ratio", MetricDef::Ratio, 1, [=](const QJsonObject &s) { return raw(fd(s), "currentRatio"); }},
        {"Balance Sheet", "Quick Ratio", MetricDef::Ratio, 1, [=](const QJsonObject &s) { return raw(fd(s), "quickRatio"); }},
        {"Dividends", "Dividend Yield", MetricDef::Percent, 1, [=](const QJsonObject &s) { return raw(sd(s), "dividendYield"); }},
        {"Dividends", "Payout Ratio", MetricDef::Percent, 0, [=](const QJsonObject &s) { return raw(sd(s), "payoutRatio"); }},
        {"Ownership", "Insiders", MetricDef::Percent, 0, [=](const QJsonObject &s) { return raw(ks(s), "heldPercentInsiders"); }},
        {"Ownership", "Institutions", MetricDef::Percent, 0, [=](const QJsonObject &s) { return raw(ks(s), "heldPercentInstitutions"); }},
        {"Ownership", "Short % of Float", MetricDef::Percent, -1, [=](const QJsonObject &s) { return raw(ks(s), "shortPercentOfFloat"); }},
    };
    return defs;
}

QString formatMetric(const MetricDef &m, double v)
{
    if (!ok(v))
        return "—";
    switch (m.kind) {
    case MetricDef::Compact: return research::compact(v);
    case MetricDef::Ratio: return QString::number(v, 'f', 2);
    case MetricDef::Percent: return QString::number(100 * v, 'f', 2) + "%";
    }
    return {};
}

} // namespace

CompareView::CompareView(QWidget *parent)
    : View(parent)
{
    QHBoxLayout *h;
    auto *top = bar(&h);
    h->addWidget(segments({tr("1 Year"), tr("6 Month"), tr("3 Month"), tr("1 Month")}, &m_range));
    m_changeMode = new QPushButton(tr("Change %"));
    m_changeMode->setObjectName("qtSeg");
    m_changeMode->setCheckable(true);
    m_changeMode->setChecked(true);
    m_changeMode->setToolTip(tr("Checked: the change since the start of the range. Unchecked: prices rebased to 100."));
    h->addWidget(m_changeMode);
    h->addStretch(1);
    m_add = new QLineEdit;
    m_add->setObjectName("qtInput");
    m_add->setPlaceholderText(tr("Add a ticker"));
    m_add->setFixedWidth(150);
    h->addWidget(m_add);
    content()->addWidget(top);

    auto *chipsRow = new QWidget;
    m_chips = new QHBoxLayout(chipsRow);
    m_chips->setContentsMargins(2, 0, 2, 0);
    m_chips->setSpacing(6);
    m_chips->addStretch(1);
    content()->addWidget(chipsRow);

    QVBoxLayout *body;
    auto *chartCard = card({}, &body);
    body->setContentsMargins(4, 4, 4, 4);
    m_chart = new MultiLineChart;
    m_chart->setFixedHeight(340);
    body->addWidget(m_chart);
    content()->addWidget(chartCard);

    m_table = new QTableWidget;
    m_table->setObjectName("qtTable");
    m_table->setItemDelegate(new RowBackground(m_table));
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    QVBoxLayout *tbody;
    auto *tableCard = card({}, &tbody);
    tbody->setContentsMargins(0, 0, 0, 0);
    tbody->setSpacing(0);
    tbody->addWidget(m_table);
    content()->addWidget(tableCard);
    content()->addStretch(1);

    connect(m_range, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/compare/range", m_range->checkedId());
        fetch();
    });
    connect(m_changeMode, &QPushButton::toggled, this, [this] { fillChart(); });
    connect(m_add, &QLineEdit::returnPressed, this, [this] {
        addTicker(m_add->text());
        m_add->clear();
    });
    m_range->button(std::clamp(QSettings().value("quant/compare/range", 0).toInt(), 0, 3))->setChecked(true);
    m_tickers = QSettings().value("quant/compare/tickers", QStringList {"AAPL", "AMD", "ASML"}).toStringList();
}

void CompareView::load()
{
    rebuildChips();
    fetch();
}

void CompareView::addTicker(const QString &symbol)
{
    const QString s = cleanSymbol(symbol);
    if (s.isEmpty() || m_tickers.contains(s) || m_tickers.size() >= 8)
        return;
    m_tickers << s;
    saveTickers();
    rebuildChips();
    fetch();
}

void CompareView::removeTicker(const QString &symbol)
{
    m_tickers.removeAll(symbol);
    saveTickers();
    rebuildChips();
    fillChart();
    fillTable();
}

void CompareView::saveTickers()
{
    QSettings().setValue("quant/compare/tickers", m_tickers);
}

void CompareView::rebuildChips()
{
    while (m_chips->count() > 1) {
        QLayoutItem *it = m_chips->takeAt(0);
        delete it->widget();
        delete it;
    }
    for (int i = 0; i < m_tickers.size(); ++i) {
        const QString sym = m_tickers[i];
        auto *b = new QPushButton(QString("— %1  ✕").arg(sym));
        b->setObjectName("qtChip");
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tr("Remove %1").arg(sym));
        b->setStyleSheet(QString("QPushButton#qtChip { color: %1; border-color: %1; }").arg(seriesColor(i).name()));
        connect(b, &QPushButton::clicked, this, [this, sym] { removeTicker(sym); });
        m_chips->insertWidget(i, b);
    }
}

void CompareView::fetch()
{
    const int gen = ++m_generation;
    m_prices.clear();
    m_errors.clear();
    fillChart();
    fillTable();
    if (m_tickers.isEmpty()) {
        setStatus(tr("Add a ticker to compare."));
        return;
    }
    setStatus(tr("Loading…"));
    const QString range = yahooRange(m_range->button(m_range->checkedId())->text());
    for (const QString &sym : std::as_const(m_tickers)) {
        research::Client::instance()->prices(sym, range, "1d", this, [this, gen, sym](const research::Series &s, const QString &err) {
            if (gen != m_generation)
                return;
            m_prices.insert(sym, s);
            if (!err.isEmpty())
                m_errors.insert(sym, err);
            fillChart();
        });
        if (!m_summaries.contains(sym))
            research::Client::instance()->summary(sym, this, [this, gen, sym](const QJsonObject &o, const QString &err) {
                if (gen != m_generation)
                    return;
                m_summaries.insert(sym, o);
                if (!err.isEmpty() && o.isEmpty())
                    m_errors.insert(sym, err);
                fillTable();
            });
    }
}

void CompareView::fillChart()
{
    const bool pct = m_changeMode->isChecked();
    // Every date any ticker has, then each ticker's change from its first close.
    QSet<QDate> set;
    for (const research::Series &s : std::as_const(m_prices))
        for (const research::Point &p : s)
            set.insert(p.date);
    QVector<QDate> dates(set.begin(), set.end());
    std::sort(dates.begin(), dates.end());
    QHash<QDate, int> index;
    for (int i = 0; i < dates.size(); ++i)
        index.insert(dates[i], i);
    QVector<MultiLineChart::Line> lines;
    for (int i = 0; i < m_tickers.size(); ++i) {
        const research::Series s = m_prices.value(m_tickers[i]);
        if (s.isEmpty())
            continue;
        MultiLineChart::Line l {m_tickers[i], seriesColor(i), QVector<double>(dates.size(), NaN)};
        const double base = s.first().value;
        for (const research::Point &p : s)
            if (base > 0)
                l.values[index.value(p.date)] = pct ? 100 * (p.value / base - 1) : 100 * p.value / base;
        lines.push_back(l);
    }
    m_chart->setZeroLine(pct);
    m_chart->setData(dates, lines, pct ? Format([](double v) { return (v > 0 ? "+" : "") + QString::number(v, 'f', 1) + "%"; })
                                       : Format([](double v) { return QString::number(v, 'f', 1); }));
    if (m_prices.size() == m_tickers.size() || !m_tickers.isEmpty() && m_prices.size() + m_errors.size() >= m_tickers.size()) {
        QStringList errs;
        for (auto it = m_errors.begin(); it != m_errors.end(); ++it)
            errs << QString("%1: %2").arg(it.key(), it.value());
        setStatus(errs.isEmpty() ? QString() : errs.join(" · "));
    }
}

void CompareView::fillTable()
{
    const Palette &c = Theme::current();
    const QVector<MetricDef> &defs = metricDefs();
    m_table->clear();
    m_table->setColumnCount(1 + m_tickers.size());
    QStringList headers {tr("METRIC")};
    for (const QString &t : std::as_const(m_tickers))
        headers << t;
    m_table->setHorizontalHeaderLabels(headers);
    for (int i = 0; i < m_tickers.size(); ++i) {
        m_table->horizontalHeaderItem(i + 1)->setForeground(seriesColor(i));
        m_table->horizontalHeaderItem(i + 1)->setTextAlignment(Qt::AlignCenter);
    }
    int rows = 0;
    QString section;
    for (const MetricDef &m : defs)
        rows += (m.section != section ? (section = m.section, 2) : 1);
    m_table->setRowCount(rows);
    int r = 0;
    section.clear();
    QColor upTint = c.Up, downTint = c.Down;
    upTint.setAlpha(45);
    downTint.setAlpha(45);
    for (const MetricDef &m : defs) {
        if (m.section != section) {
            section = m.section;
            auto *it = new QTableWidgetItem(section.toUpper());
            it->setFont(mono(10, true));
            it->setForeground(c.Text);
            it->setBackground(c.Hover);
            m_table->setItem(r, 0, it);
            for (int col = 1; col <= m_tickers.size(); ++col) {
                auto *fill = new QTableWidgetItem;
                fill->setBackground(c.Hover);
                m_table->setItem(r, col, fill);
            }
            m_table->setRowHeight(r, 20);
            ++r;
        }
        auto *name = new QTableWidgetItem(m.label);
        name->setForeground(c.TextDim);
        m_table->setItem(r, 0, name);
        QVector<double> values;
        for (const QString &t : std::as_const(m_tickers)) {
            const QJsonObject s = m_summaries.value(t);
            values.push_back(s.isEmpty() ? NaN : m.value(s));
        }
        // The best and the worst of the row; for "lower is better" ratios a negative value is the worst.
        int best = -1, worst = -1, valid = 0;
        auto score = [&](double v) {
            if (!ok(v)) return NaN;
            if (m.better < 0) return v <= 0 ? -1e300 : -v;
            return v;
        };
        for (int i = 0; i < values.size(); ++i) {
            const double sc = score(values[i]);
            if (!ok(sc))
                continue;
            ++valid;
            if (best < 0 || sc > score(values[best])) best = i;
            if (worst < 0 || sc < score(values[worst])) worst = i;
        }
        for (int i = 0; i < values.size(); ++i) {
            const QJsonObject s = m_summaries.value(m_tickers[i]);
            auto *it = new QTableWidgetItem(s.isEmpty() ? QString("…") : formatMetric(m, values[i]));
            it->setTextAlignment(Qt::AlignCenter);
            it->setFont(mono(11, true));
            if (s.isEmpty() || !ok(values[i]))
                it->setForeground(c.TextFaint);
            else if (m.better != 0 && valid >= 2 && i == best && score(values[best]) != score(values[worst])) {
                it->setForeground(c.Up);
                it->setBackground(upTint);
            } else if (m.better != 0 && valid >= 2 && i == worst && score(values[best]) != score(values[worst])) {
                it->setForeground(c.Down);
                it->setBackground(downTint);
            } else {
                it->setForeground(c.Text);
            }
            m_table->setItem(r, i + 1, it);
        }
        ++r;
    }
    auto *hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 220);
    for (int col = 1; col <= m_tickers.size(); ++col)
        hh->setSectionResizeMode(col, QHeaderView::Stretch);
    int h = hh->height();
    for (int i = 0; i < m_table->rowCount(); ++i)
        h += m_table->rowHeight(i);
    m_table->setFixedHeight(h + 4);
}

} // namespace quant
