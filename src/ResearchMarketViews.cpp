// The Research page's market sections: compare, compare charts, sectors, ICS, SPLC.

#include "ResearchViews.h"

#include "Theme.h"

#include <QCoreApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>

namespace research {

namespace {

QLabel *label(const QString &text, const char *name)
{
    auto *l = new QLabel(text);
    l->setObjectName(name);
    return l;
}

QTableWidgetItem *cell(const QString &text, const QColor &color = QColor(), Qt::Alignment align = Qt::AlignLeft,
                       bool bold = false)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(align | Qt::AlignVCenter);
    if (color.isValid())
        it->setForeground(color);
    if (bold)
        it->setFont(mono(11, true));
    return it;
}

void fitHeight(QTableWidget *t)
{
    int h = t->horizontalHeader()->height();
    for (int r = 0; r < t->rowCount(); ++r)
        h += t->rowHeight(r);
    t->setFixedHeight(h + 4);
}

double ratio(double a, double b)
{
    return ok(a) && ok(b) && b != 0 ? a / b : NaN;
}

// "AAPL" over "Apple Inc." in one cell.
QWidget *companyCell(const QString &symbol, const QString &name)
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(6, 4, 6, 4);
    v->setSpacing(0);
    v->addWidget(label(symbol, "resCellSymbol"));
    auto *n = label(name, "resCellName");
    n->setMaximumWidth(260);
    n->setText(QFontMetrics(mono(11)).elidedText(name, Qt::ElideRight, 250));
    n->setToolTip(name);
    v->addWidget(n);
    return w;
}

QString periodKeyLabel(const QDate &end, bool quarterly)
{
    return quarterly ? QString("Q%1 %2").arg((end.month() - 1) / 3 + 1).arg(end.year()) : QString::number(end.year());
}

int periodKey(const QDate &end, bool quarterly)
{
    return quarterly ? end.year() * 10 + (end.month() - 1) / 3 + 1 : end.year() * 10;
}

} // namespace

// ---------------------------------------------------------------- Compare

CompareView::CompareView(QWidget *parent)
    : View(parent)
{
    QVBoxLayout *b;
    auto *bar = panel({}, &b);
    auto *row1 = new QHBoxLayout;
    row1->addWidget(segments({tr("Peers"), tr("Sector"), tr("All")}, &m_mode, QSettings().value("research/compareMode", 0).toInt()));
    row1->addStretch(1);
    m_sector = new QComboBox;
    m_sector->setObjectName("resCombo");
    m_industry = new QComboBox;
    m_industry->setObjectName("resCombo");
    row1->addWidget(m_sector);
    row1->addWidget(m_industry);
    b->addLayout(row1);
    auto *row2 = new QHBoxLayout;
    row2->addWidget(segments({tr("General"), tr("Valuation"), tr("Earning"), tr("Growth"), tr("Margins"), tr("Ownership")}, &m_tab,
                             QSettings().value("research/compareTab", 0).toInt()));
    row2->addStretch(1);
    m_experimental = new QWidget;
    auto *eh = new QHBoxLayout(m_experimental);
    eh->setContentsMargins(0, 0, 0, 0);
    eh->addWidget(experimentalTag(tr("Fair value, valuation and quant score are Deepwater's own estimates (see Ratings).")));
    eh->addWidget(label(tr("Fair Value · Valuation · Quant Score"), "resNote"));
    row2->addWidget(m_experimental);
    b->addLayout(row2);
    content()->addWidget(bar);
    m_table = table({});
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->setCursor(Qt::PointingHandCursor);
    content()->addWidget(m_table);
    auto *note = label(tr("* Experimental. Fair value: Deepwater's discounted cash flow estimate; quant score: Deepwater's "
                          "rating from valuation, profitability, financial health and momentum (see Ratings). Peers: Yahoo Finance's "
                          "related symbols, then the largest companies of the same industry. Sectors and industries: "
                          "Nasdaq's classification."),
                       "resNote");
    note->setWordWrap(true);
    content()->addWidget(note);
    content()->addStretch(1);
    connect(m_mode, &QButtonGroup::idClicked, this, [this](int id) {
        QSettings().setValue("research/compareMode", id);
        rebuildSymbols();
    });
    connect(m_tab, &QButtonGroup::idClicked, this, [this](int id) {
        QSettings().setValue("research/compareTab", id);
        fillTable();
    });
    connect(m_sector, &QComboBox::activated, this, [this] {
        // The industries of that sector.
        const QString sector = m_sector->currentData().toString();
        QStringList industries;
        for (const Listing &l : std::as_const(m_listings))
            if ((sector.isEmpty() || l.sector == sector) && !l.industry.isEmpty())
                industries << l.industry;
        industries.removeDuplicates();
        industries.sort();
        m_industry->clear();
        m_industry->addItem(tr("All Industries"), QString());
        for (const QString &i : industries)
            m_industry->addItem(i, i);
        if (m_mode->checkedId() == 0)
            m_mode->button(1)->setChecked(true);
        rebuildSymbols();
    });
    connect(m_industry, &QComboBox::activated, this, [this] {
        if (m_mode->checkedId() == 0)
            m_mode->button(1)->setChecked(true);
        rebuildSymbols();
    });
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row >= 0 && row < m_symbols.size() && m_symbols[row] != m_symbol)
            emit symbolRequested(m_symbols[row]);
    });
}

void CompareView::load()
{
    const QString sym = m_symbol;
    setStatus(tr("Loading the listings…"));
    m_peers.clear();
    // Combos back to "all" for a new symbol.
    m_sector->clear();
    m_industry->clear();
    Client::instance()->listings(this, [this, sym](const QVector<Listing> &list, const QString &err) {
        if (sym != m_symbol)
            return;
        m_listings = list;
        setStatus(list.isEmpty() ? err : QString());
        QStringList sectors;
        for (const Listing &l : list)
            if (!l.sector.isEmpty())
                sectors << l.sector;
        sectors.removeDuplicates();
        sectors.sort();
        m_sector->clear();
        m_sector->addItem(tr("All Sectors"), QString());
        for (const QString &s : sectors)
            m_sector->addItem(s, s);
        m_industry->clear();
        m_industry->addItem(tr("All Industries"), QString());
        Client::instance()->peers(sym, this, [this, sym](const QStringList &peers, const QString &) {
            if (sym != m_symbol)
                return;
            m_peers = peers;
            rebuildSymbols();
        });
    });
}

void CompareView::rebuildSymbols()
{
    const int mode = m_mode->checkedId();
    m_sector->setEnabled(mode != 0);
    m_industry->setEnabled(mode != 0);
    const Listing *self = nullptr;
    for (const Listing &l : std::as_const(m_listings))
        if (l.symbol == m_symbol)
            self = &l;
    QVector<const Listing *> byCap;
    for (const Listing &l : std::as_const(m_listings))
        if (ok(l.marketCap) && l.marketCap > 0)
            byCap.push_back(&l);
    std::sort(byCap.begin(), byCap.end(), [](const Listing *a, const Listing *b) { return a->marketCap > b->marketCap; });
    // Share classes of one company (GOOGL / GOOG) count once.
    QMultiHash<QString, const Listing *> bySymbol;
    for (const Listing &l : std::as_const(m_listings))
        bySymbol.insert(l.symbol, &l);
    auto sameCompany = [&bySymbol](const Listing *a, const QStringList &taken, const QVector<Listing> &) {
        for (const QString &t : taken)
            for (auto it = bySymbol.constFind(t); it != bySymbol.constEnd() && it.key() == t; ++it) {
                const Listing &l = **it;
                if (ok(l.marketCap) && l.marketCap == a->marketCap && l.name.left(8) == a->name.left(8))
                    return true;
            }
        return false;
    };
    QStringList symbols {m_symbol};
    if (mode == 0) {
        for (const QString &p : std::as_const(m_peers))
            if (!symbols.contains(p))
                symbols << p;
        if (self)
            for (const Listing *l : byCap) {
                if (symbols.size() >= 10)
                    break;
                if (l->industry == self->industry && !symbols.contains(l->symbol) && !sameCompany(l, symbols, m_listings))
                    symbols << l->symbol;
            }
    } else {
        QString sector = m_sector->currentData().toString();
        const QString industry = m_industry->currentData().toString();
        if (mode == 1 && sector.isEmpty() && industry.isEmpty() && self)
            sector = self->sector;
        for (const Listing *l : byCap) {
            if (symbols.size() >= 20)
                break;
            if ((sector.isEmpty() || l->sector == sector) && (industry.isEmpty() || l->industry == industry)
                && !symbols.contains(l->symbol) && !sameCompany(l, symbols, m_listings))
                symbols << l->symbol;
        }
    }
    m_symbols = symbols;
    const int generation = ++m_generation;
    fillTable();
    for (const QString &s : symbols) {
        if (m_summaries.contains(s))
            continue;
        Client::instance()->summary(s, this, [this, s, generation](const QJsonObject &summary, const QString &) {
            m_summaries.insert(s, summary);
            if (generation == m_generation && !m_fillPending) {   // the summaries come one by one: one fill per pass
                m_fillPending = true;
                QTimer::singleShot(0, this, [this, generation] {
                    m_fillPending = false;
                    if (generation == m_generation)
                        fillTable();
                });
            }
        });
    }
}

void CompareView::fillTable()
{
    const Palette &c = Theme::current();
    const int tab = m_tab->checkedId();
    QStringList headers {"#", tr("Company")};
    switch (tab) {
    case 0: headers << tr("Market Cap") << tr("Price") << tr("Fair Value") + " *" << tr("Valuation") + " *" << tr("Analyst Target")
                    << tr("Quant Score") + " *"; break;
    case 1: headers << tr("P/E") << tr("Fwd P/E") << tr("PEG") << tr("P/S") << tr("P/B") << tr("EV/EBITDA") << tr("EV/Sales"); break;
    case 2: headers << tr("Revenue") << tr("Gross Profit") << tr("EBITDA") << tr("Net Income") << tr("EPS (TTM)") << tr("EPS Fwd") << tr("Next Earnings"); break;
    case 3: headers << tr("Sales Y/Y") << tr("EPS Y/Y") << tr("EPS This Yr") << tr("EPS Next Yr") << tr("52W Change") << tr("vs S&P 500"); break;
    case 4: headers << tr("Gross") << tr("Operating") << tr("Net") << tr("EBITDA") << tr("ROE") << tr("ROA"); break;
    default: headers << tr("Insiders") << tr("Institutions") << tr("Short % Float") << tr("Shares Out") << tr("Float"); break;
    }
    QStringList upper;
    for (const QString &h : headers)
        upper << h.toUpper();
    m_table->clear();
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(upper);
    m_experimental->setVisible(tab == 0);
    if (tab == 0)
        for (int col : {4, 5, 7}) {
            m_table->horizontalHeaderItem(col)->setToolTip(experimentalReason());
            m_table->horizontalHeaderItem(col)->setForeground(QColor(0xf0, 0xb4, 0x4c));
        }
    m_table->setRowCount(m_symbols.size());
    for (int r = 0; r < m_symbols.size(); ++r) {
        const QString sym = m_symbols[r];
        const QJsonObject s = m_summaries.value(sym);
        const QJsonObject price = s.value("price").toObject(), sd = s.value("summaryDetail").toObject(),
                          ks = s.value("defaultKeyStatistics").toObject(), fd = s.value("financialData").toObject();
        const QString cur = text(price, "currency").isEmpty() ? QString("USD") : text(price, "currency");
        const QString name = text(price, "longName").isEmpty() ? text(price, "shortName") : text(price, "longName");
        m_table->setItem(r, 0, cell(QString::number(r + 1), c.TextDim, Qt::AlignCenter));
        m_table->setCellWidget(r, 1, companyCell(sym, s.isEmpty() ? tr("loading…") : name));
        QVector<std::pair<QString, QColor>> values;
        QString valuationMethod;   // tab 0: the method, or why there's no value
        auto pctCol = [&](double v) { values.push_back({percent(v), changeColor(v)}); };
        if (!s.isEmpty()) {
            switch (tab) {
            case 0: {
                const Valuation v = intrinsicValue(s);
                valuationMethod = v.method;
                const Grades g = factorGrades(s, v);
                values = {{money(raw(sd, "marketCap"), cur), {}}, {money(raw(price, "regularMarketPrice"), cur), {}},
                          {ok(v.intrinsic) ? money(v.intrinsic, cur) : tr("N/M"), ok(v.intrinsic) ? QColor() : c.TextFaint},
                          {ok(v.upside) ? (v.upside < 0 ? tr("%1 OVERVALUED").arg(percent(ratio(v.price - v.intrinsic, v.intrinsic), 0, false))
                                                        : tr("%1 UNDERVALUED").arg(percent(v.upside, 0, false)))
                                        : tr("N/M"),
                           ok(v.upside) ? (v.upside < 0 ? c.Down : c.Up) : c.TextFaint},
                          {money(raw(fd, "targetMeanPrice"), cur), {}},
                          {g.rating.toUpper(), g.score >= 3.5 ? palette::green() : g.score >= 2.5 ? palette::yellow() : palette::red()}};
                break;
            }
            case 1:
                values = {{plain(raw(sd, "trailingPE")), {}}, {plain(raw(sd, "forwardPE")), {}}, {plain(raw(ks, "pegRatio")), {}},
                          {plain(raw(sd, "priceToSalesTrailing12Months")), {}}, {plain(raw(ks, "priceToBook")), {}},
                          {plain(raw(ks, "enterpriseToEbitda")), {}}, {plain(raw(ks, "enterpriseToRevenue")), {}}};
                break;
            case 2: {
                const QString fcur = text(fd, "financialCurrency").isEmpty() ? cur : text(fd, "financialCurrency");
                const QJsonArray d = s.value("calendarEvents").toObject().value("earnings").toObject().value("earningsDate").toArray();
                const QString next = d.isEmpty() ? QString("N/A") : d.first().toObject().value("fmt").toString();
                values = {{money(raw(fd, "totalRevenue"), fcur), {}}, {money(raw(fd, "grossProfits"), fcur), {}},
                          {money(raw(fd, "ebitda"), fcur), {}},
                          {money(raw(ks, "netIncomeToCommon"), fcur), changeColor(raw(ks, "netIncomeToCommon"))},
                          {money(raw(ks, "trailingEps"), fcur), {}}, {money(raw(ks, "forwardEps"), fcur), {}}, {next, {}}};
                break;
            }
            case 3: {
                double thisYear = NaN, nextYear = NaN;
                for (const QJsonValue &t : s.value("earningsTrend").toObject().value("trend").toArray()) {
                    const QString p = t.toObject().value("period").toString();
                    if (p == "0y") thisYear = raw(t.toObject(), "growth");
                    if (p == "+1y") nextYear = raw(t.toObject(), "growth");
                }
                pctCol(raw(fd, "revenueGrowth"));
                pctCol(raw(fd, "earningsGrowth"));
                pctCol(thisYear);
                pctCol(nextYear);
                pctCol(raw(ks, "52WeekChange"));
                pctCol(raw(ks, "52WeekChange") - raw(ks, "SandP52WeekChange"));
                break;
            }
            case 4:
                for (const char *k : {"grossMargins", "operatingMargins", "profitMargins", "ebitdaMargins", "returnOnEquity", "returnOnAssets"})
                    values.push_back({percent(raw(fd, k), 2, false), raw(fd, k) < 0 ? c.Down : QColor()});
                break;
            default:
                values = {{percent(raw(ks, "heldPercentInsiders"), 2, false), {}}, {percent(raw(ks, "heldPercentInstitutions"), 2, false), {}},
                          {percent(raw(ks, "shortPercentOfFloat"), 2, false), {}}, {compact(raw(ks, "sharesOutstanding")), {}},
                          {compact(raw(ks, "floatShares")), {}}};
            }
        }
        for (int col = 2; col < headers.size(); ++col) {
            const auto v = values.value(col - 2, {QString("…"), c.TextFaint});
            m_table->setItem(r, col, cell(v.first, v.second, Qt::AlignRight, tab == 0 && col >= 5));
        }
        if (tab == 0 && !s.isEmpty())   // the method, or why there's no value (no positive free cash flow…)
            for (int col : {4, 5})
                m_table->item(r, col)->setToolTip(valuationMethod);
        if (sym == m_symbol)
            for (int col = 0; col < headers.size(); ++col)
                if (auto *it = m_table->item(r, col))
                    it->setBackground(c.Active);
        if (sym == m_symbol)
            m_table->setItem(r, 1, cell({}));
        if (sym == m_symbol)
            m_table->item(r, 1)->setBackground(c.Active);
    }
    auto *hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 44);
    hh->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int col = 2; col < headers.size(); ++col) {
        hh->setSectionResizeMode(col, QHeaderView::Fixed);
        m_table->setColumnWidth(col, tab == 0 && col == 5 ? 190 : 140);
        m_table->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    fitHeight(m_table);
}

// ---------------------------------------------------------------- Compare charts

CompareChartsView::CompareChartsView(QWidget *parent)
    : View(parent)
{
    QVBoxLayout *b;
    QHBoxLayout *h;
    auto *bar = panel({}, &b, &h);
    int i = 0;
    h->insertWidget(i++, segments({tr("Quarterly"), tr("Annual")}, &m_period));
    h->insertWidget(i++, label(tr("Metric:"), "resStatLabel"));
    m_metric = new QComboBox;
    m_metric->setObjectName("resCombo");
    for (const Metric &m : fundamentalMetrics())
        m_metric->addItem(m.label, m.id);
    m_metric->setCurrentIndex(std::max(0, m_metric->findData(QSettings().value("research/compareMetric", "revenue"))));
    h->insertWidget(i++, m_metric);
    h->insertSpacing(i++, 12);
    h->insertWidget(i++, label(tr("Symbols:"), "resStatLabel"));
    m_a = new QLineEdit;
    m_a->setObjectName("resSymbolA");
    m_a->setFixedWidth(90);
    m_b = new QLineEdit;
    m_b->setObjectName("resSymbolB");
    m_b->setFixedWidth(90);
    m_b->setText(QSettings().value("research/compareWith").toString());
    h->insertWidget(i++, m_a);
    h->insertWidget(i++, label("vs", "resStatLabel"));
    h->insertWidget(i++, m_b);
    h->addWidget(segments({tr("Values"), tr("% Growth")}, &m_mode));
    content()->addWidget(bar);
    m_chart = new LineChart;
    m_chart->setLegend(true);
    m_chart->setMinimumHeight(520);
    content()->addWidget(m_chart, 1);
    m_source = label({}, "resNote");
    m_source->setWordWrap(true);
    content()->addWidget(m_source);
    connect(m_period, &QButtonGroup::idClicked, this, [this] { fill(); });
    connect(m_mode, &QButtonGroup::idClicked, this, [this] { fill(); });
    connect(m_metric, &QComboBox::activated, this, [this] {
        QSettings().setValue("research/compareMetric", m_metric->currentData());
        fill();
    });
    for (QLineEdit *e : {m_a, m_b})
        connect(e, &QLineEdit::editingFinished, this, [this, e] {
            e->setText(e->text().trimmed().toUpper());
            if (e == m_b)
                QSettings().setValue("research/compareWith", m_b->text());
            fetch();
        });
}

void CompareChartsView::load()
{
    m_a->setText(m_symbol);
    if (m_b->text().isEmpty() || m_b->text() == m_symbol) {
        const QString sym = m_symbol;
        Client::instance()->peers(sym, this, [this, sym](const QStringList &peers, const QString &) {
            if (sym != m_symbol)
                return;
            QString other = "MSFT";
            for (const QString &p : peers)
                if (p != sym) {
                    other = p;
                    break;
                }
            if (other == sym)
                other = "AAPL";
            m_b->setText(other);
            fetch();
        });
    }
    fetch();
}

void CompareChartsView::fetch()
{
    for (const QString &s : {m_a->text(), m_b->text()}) {
        if (s.isEmpty() || m_fund.contains(s))
            continue;
        setStatus(tr("Loading %1…").arg(s));
        Client::instance()->fundamentals(s, this, [this, s](const Fundamentals &f, const QString &err) {
            m_fund.insert(s, f);
            setStatus(f.isEmpty() ? tr("No figures for %1: %2").arg(s, err) : QString());
            fill();
        });
    }
    fill();
}

void CompareChartsView::fill()
{
    const bool quarterly = m_period->checkedId() == 0;
    const bool growth = m_mode->checkedId() == 1;
    const QString id = m_metric->currentData().toString();
    const Metric *metric = findMetric(id);
    const QStringList syms {m_a->text(), m_b->text()};
    // Periods by calendar quarter (or fiscal year): the last 12 quarters or 10 years either company has.
    std::map<int, QString> labels;
    QHash<QString, QHash<int, double>> byKey;
    QStringList sources, currencies;
    for (const QString &s : syms) {
        const Fundamentals f = m_fund.value(s);
        if (!f.source.isEmpty() && !sources.contains(f.source))
            sources << f.source;
        if (!f.currency.isEmpty() && !currencies.contains(f.currency))
            currencies << f.currency;
        for (const Point &p : (quarterly ? f.quarterly : f.annual).value(id)) {
            const int k = periodKey(p.date, quarterly);
            byKey[s][k] = p.value;
            labels[k] = periodKeyLabel(p.date, quarterly);
        }
    }
    const int count = quarterly ? 12 : 10;
    QVector<int> keys;
    for (const auto &[k, l] : labels)
        keys << k;
    if (keys.size() > count)
        keys = keys.mid(keys.size() - count);
    QStringList cats;
    for (int k : keys)
        cats << labels[k];
    const QColor colors[2] {palette::pink(), palette::cyan()};
    QVector<LineChart::Series> series;
    for (int i = 0; i < 2; ++i) {
        QVector<double> v;
        for (int k : keys) {
            double x = byKey[syms[i]].value(k, NaN);
            if (growth) {
                const int prevKey = quarterly ? k - 10 : k - 10;   // same quarter a year before / the year before
                const double prev = byKey[syms[i]].value(prevKey, NaN);
                x = ok(x) && ok(prev) && prev != 0 ? (x - prev) / std::abs(prev) : NaN;
            }
            v << x;
        }
        series.push_back({syms[i], colors[i], v, true, false, false});
    }
    const QString cur = currencies.value(0, "USD");
    Format fmt;
    if (growth || (metric && metric->kind == Metric::Percent))
        fmt = [](double v) { return percent(v, 1, false); };
    else
        fmt = [cur, metric](double v) { return money(v, cur, metric && metric->kind == Metric::PerShare ? 2 : 2); };
    m_chart->setTitleForTooltip(m_metric->currentText());
    m_chart->setData(cats, series, fmt);
    m_source->setText(tr("Source: %1. %2 Currencies: %3.")
                          .arg(sources.isEmpty() ? QString("–") : sources.join(", "),
                               growth ? tr("Growth is against the same period a year before.") : QString(),
                               currencies.isEmpty() ? QString("–") : currencies.join(", ")));
}

// ---------------------------------------------------------------- Sectors

namespace {

QString gicsFromNasdaq(const QString &s)
{
    static const QHash<QString, QString> map {
        {"Technology", "Information Technology"}, {"Finance", "Financials"}, {"Health Care", "Health Care"},
        {"Consumer Discretionary", "Consumer Discretionary"}, {"Consumer Staples", "Consumer Staples"},
        {"Industrials", "Industrials"}, {"Energy", "Energy"}, {"Basic Materials", "Materials"},
        {"Telecommunications", "Communication Services"}, {"Utilities", "Utilities"}, {"Real Estate", "Real Estate"},
    };
    return map.value(s, s.isEmpty() ? QString("Other") : s);
}

const Palette &pal0() { return Theme::current(); }

// The eleven GICS sectors, as read from Wikipedia and Nasdaq (English), shown translated.
QString sectorLabel(const QString &s)
{
    static const QHash<QString, const char *> names {
        {"Information Technology", QT_TRANSLATE_NOOP("research::SectorsView", "Information Technology")},
        {"Financials", QT_TRANSLATE_NOOP("research::SectorsView", "Financials")},
        {"Health Care", QT_TRANSLATE_NOOP("research::SectorsView", "Health Care")},
        {"Consumer Discretionary", QT_TRANSLATE_NOOP("research::SectorsView", "Consumer Discretionary")},
        {"Consumer Staples", QT_TRANSLATE_NOOP("research::SectorsView", "Consumer Staples")},
        {"Industrials", QT_TRANSLATE_NOOP("research::SectorsView", "Industrials")},
        {"Energy", QT_TRANSLATE_NOOP("research::SectorsView", "Energy")},
        {"Materials", QT_TRANSLATE_NOOP("research::SectorsView", "Materials")},
        {"Communication Services", QT_TRANSLATE_NOOP("research::SectorsView", "Communication Services")},
        {"Utilities", QT_TRANSLATE_NOOP("research::SectorsView", "Utilities")},
        {"Real Estate", QT_TRANSLATE_NOOP("research::SectorsView", "Real Estate")},
        {"Other", QT_TRANSLATE_NOOP("research::SectorsView", "Other")},
    };
    const char *name = names.value(s);
    return name ? QCoreApplication::translate("research::SectorsView", name) : s;
}

// Short labels for the donut's slices.
QString shortSector(const QString &s)
{
    static const QHash<QString, const char *> map {
        {"Information Technology", QT_TRANSLATE_NOOP("research::SectorsView", "Technology")},
        {"Consumer Discretionary", QT_TRANSLATE_NOOP("research::SectorsView", "Cons Discr")},
        {"Consumer Staples", QT_TRANSLATE_NOOP("research::SectorsView", "Cons Staples")},
        {"Communication Services", QT_TRANSLATE_NOOP("research::SectorsView", "Comm Svcs")},
    };
    const char *name = map.value(s);
    return name ? QCoreApplication::translate("research::SectorsView", name) : sectorLabel(s);
}

// Change of a member over the period, all from Yahoo's daily closes: the last price (five-day series, fresh)
// against the previous close (today) or the close before the period's start (the year's series). Nasdaq's
// screener isn't used for prices: it can be a session behind.
double periodChange(int period, const Series &recent, const Series &closes)
{
    if (recent.size() < 2)
        return NaN;
    const double now = recent.last().value;
    if (period == 0)
        return recent[recent.size() - 2].value > 0 ? now / recent[recent.size() - 2].value - 1 : NaN;
    if (closes.size() < 2)
        return NaN;
    const QDate last = recent.last().date;
    QDate from;
    switch (period) {
    case 1: from = last.addDays(-7); break;
    case 2: from = last.addMonths(-1); break;
    case 3: from = last.addMonths(-3); break;
    default: from = QDate(last.year() - 1, 12, 31); break;
    }
    double base = NaN;
    for (const Point &p : closes) {
        if (p.date > from)
            break;
        base = p.value;
    }
    return ok(base) && base > 0 ? now / base - 1 : NaN;
}

// The change bar of a sector row, painted in its column.
class BarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    double max = 0.01;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(p, opt, QModelIndex());
        const QVariant v = index.data(Qt::UserRole);
        if (!v.isValid())
            return;
        const double x = v.toDouble();
        const QRectF r = opt.rect.adjusted(4, 6, -4, -6);
        const double mid = r.center().x();
        const double w = std::min(1.0, std::abs(x) / max) * r.width() / 2;
        p->fillRect(x >= 0 ? QRectF(mid, r.top(), std::max(2.0, w), r.height()) : QRectF(mid - std::max(2.0, w), r.top(), std::max(2.0, w), r.height()),
                    x >= 0 ? QColor(0x3f, 0xb3, 0x8a) : palette::red());
    }
};

} // namespace

SectorsView::SectorsView(QWidget *parent)
    : View(parent)
{
    auto *head = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    m_kicker = label({}, "resKicker");
    m_kicker->setTextFormat(Qt::RichText);
    titles->addWidget(m_kicker);
    titles->addWidget(label(tr("Percent Price Change by GICS Sector"), "resTitle"));
    head->addLayout(titles);
    head->addStretch(1);
    head->addWidget(segments({tr("Today"), "1W", "1M", "3M", "YTD"}, &m_period, QSettings().value("research/sectorsPeriod", 0).toInt()));
    content()->addLayout(head);
    auto *tabs = new QWidget;
    auto *th = new QHBoxLayout(tabs);
    th->setContentsMargins(0, 0, 0, 0);
    th->setSpacing(0);
    m_index = new QButtonGroup(this);
    const QStringList names {"S&&P 500", "NASDAQ-100", "DJIA"};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QPushButton(names[i]);
        b->setObjectName("resIndexTab");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_index->addButton(b, i);
        th->addWidget(b);
    }
    th->addStretch(1);
    m_index->button(std::clamp(QSettings().value("research/sectorsIndex", 0).toInt(), 0, 2))->setChecked(true);
    content()->addWidget(tabs);
    auto *row = new QHBoxLayout;
    row->setSpacing(14);
    QVBoxLayout *lb;
    auto *left = panel({}, &lb);
    m_donut = new DonutChart;
    m_donut->setLeaderLabels(true);
    m_donut->setMinimumSize(520, 520);
    m_donut->setThickness(0.55);
    lb->addWidget(m_donut, 1);
    row->addWidget(left, 1);
    QVBoxLayout *rb;
    auto *right = panel({}, &rb);
    m_tree = new QTreeWidget;
    m_tree->setObjectName("resTree");
    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels({"#", tr("NAME"), {}, tr("%CHG"), tr("WEIGHT")});
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setItemDelegateForColumn(2, new BarDelegate(m_tree));
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c : {0, 2, 3, 4})
        m_tree->header()->setSectionResizeMode(c, QHeaderView::Fixed);
    m_tree->setColumnWidth(0, 70);
    m_tree->setColumnWidth(2, 120);
    m_tree->setColumnWidth(3, 90);
    m_tree->setColumnWidth(4, 80);
    m_tree->setMinimumHeight(520);
    rb->addWidget(m_tree, 1);
    row->addWidget(right, 1);
    content()->addLayout(row, 1);
    m_note = label({}, "resNote");
    m_note->setWordWrap(true);
    content()->addWidget(m_note);
    connect(m_index, &QButtonGroup::idClicked, this, [this](int id) {
        QSettings().setValue("research/sectorsIndex", id);
        fill();
    });
    connect(m_period, &QButtonGroup::idClicked, this, [this](int id) {
        QSettings().setValue("research/sectorsPeriod", id);
        fill();
    });
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *it) {
        const QString sym = it->data(1, Qt::UserRole).toString();
        if (!sym.isEmpty())
            emit symbolRequested(sym);
        else
            it->setExpanded(!it->isExpanded());
    });
}

void SectorsView::scheduleFill()
{
    if (m_fillPending)
        return;
    m_fillPending = true;
    QTimer::singleShot(0, this, [this] {
        m_fillPending = false;
        fill();
    });
}

void SectorsView::load()
{
    setStatus(tr("Loading index members and prices…"));
    Client *c = Client::instance();
    c->listings(this, [this](const QVector<Listing> &list, const QString &) {
        m_listings.clear();
        for (const Listing &l : list)
            m_listings.insert(l.symbol, l);
        m_haveListings = true;
        scheduleFill();
    });
    c->indexes(this, [this](const Indexes &idx, const QString &err) {
        m_indexes = idx;
        m_haveIndexes = true;
        if (idx.sp500.isEmpty())
            setStatus(tr("Couldn't load the index members: %1").arg(err));
        QStringList symbols;
        for (const auto *list : {&idx.sp500, &idx.nasdaq100, &idx.dow})
            for (const IndexMember &m : *list)
                symbols << m.symbol;
        scheduleFill();
        Client::instance()->spark(symbols + QStringList {"^GSPC", "^NDX", "^DJI"}, "5d", this,
                                  [this](const QHash<QString, Series> &h, const QString &) {
            m_recent = h;
            m_haveRecent = true;
            scheduleFill();
        });
        Client::instance()->spark(symbols + QStringList {"^GSPC", "^NDX", "^DJI"}, "1y", this, [this](const QHash<QString, Series> &h, const QString &) {
            m_history = h;
            m_haveHistory = true;
            scheduleFill();
        });
    });
}

void SectorsView::fill()
{
    const int index = m_index->checkedId(), period = m_period->checkedId();
    const QString indexName = QStringList {"S&P 500", "NASDAQ-100", "DJIA"}.value(index);
    m_kicker->setText(indexName + "  ›  GICS");
    if (!m_haveIndexes || !m_haveListings) {
        m_tree->clear();
        return;
    }
    if (!m_haveRecent || (period > 0 && !m_haveHistory))
        setStatus(tr("Loading the daily closes of %1 stocks…").arg(m_indexes.sp500.size() + m_indexes.nasdaq100.size()));
    else
        setStatus({});
    const QVector<IndexMember> &members = index == 0 ? m_indexes.sp500 : index == 1 ? m_indexes.nasdaq100 : m_indexes.dow;
    struct Member {
        QString symbol, name;
        double weight, change;
    };
    QMap<QString, QVector<Member>> bySector;
    double totalWeight = 0;
    int missing = 0;
    // Share classes of one company (GOOGL and GOOG) each carry the company's whole market cap: split it.
    QVector<const Listing *> inIndex;
    for (const IndexMember &m : members) {
        const auto it = m_listings.constFind(m.symbol);
        if (it != m_listings.constEnd() && ok(it->marketCap) && it->marketCap > 0)
            inIndex.push_back(&*it);
    }
    // The members' market caps by the first word of their name, to find the share classes of a company.
    QHash<QString, QVector<double>> capsByFirstWord;
    for (const Listing *o : std::as_const(inIndex))
        capsByFirstWord[o->name.section(' ', 0, 0)].push_back(o->marketCap);
    auto classCount = [&capsByFirstWord](const Listing *l) {
        int n = 0;
        for (double cap : capsByFirstWord.value(l->name.section(' ', 0, 0)))
            if (std::abs(cap / l->marketCap - 1) < 0.03)
                ++n;
        return std::max(1, n);
    };
    for (const IndexMember &m : members) {
        const auto it = m_listings.constFind(m.symbol);
        const Listing *l = it == m_listings.constEnd() ? nullptr : &*it;
        const double w = index == 2 ? (l ? l->price : NaN)
                                    : (l && ok(l->marketCap) ? l->marketCap / classCount(l) : NaN);
        const double ch = periodChange(period, m_recent.value(m.symbol), m_history.value(m.symbol));
        if (!ok(w) || w <= 0) {
            ++missing;
            continue;
        }
        const QString sector = !m.sector.isEmpty() ? m.sector : gicsFromNasdaq(l ? l->sector : QString());
        bySector[sector].push_back({m.symbol, m.name, w, ch});
        totalWeight += w;
    }
    struct Row {
        QString sector;
        double weight = 0, change = NaN;
        QVector<Member> members;
    };
    QVector<Row> rows;
    double indexSum = 0, indexWeight = 0, maxAbs = 0.001;
    for (auto it = bySector.begin(); it != bySector.end(); ++it) {
        Row r;
        r.sector = it.key();
        double sum = 0, wsum = 0;
        for (const Member &m : it.value()) {
            r.weight += m.weight;
            if (ok(m.change)) {
                sum += m.weight * m.change;
                wsum += m.weight;
            }
        }
        r.change = wsum > 0 ? sum / wsum : NaN;
        r.members = it.value();
        std::sort(r.members.begin(), r.members.end(), [](const Member &a, const Member &b) { return a.weight > b.weight; });
        if (ok(r.change)) {
            indexSum += sum;
            indexWeight += wsum;
            maxAbs = std::max(maxAbs, std::abs(r.change));
        }
        rows.push_back(r);
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        return (ok(a.change) ? a.change : -1e9) > (ok(b.change) ? b.change : -1e9);
    });
    const double indexChange = indexWeight > 0 ? indexSum / indexWeight : NaN;
    // The index itself, to check the recomputation against.
    const QString indexSymbol = QStringList {"^GSPC", "^NDX", "^DJI"}.value(index);
    const double official = periodChange(period, m_recent.value(indexSymbol), m_history.value(indexSymbol));
    m_kicker->setText(QString("%1  ›  GICS     <span style='color:%2'>%3 %4: <span style='color:%5'>%6</span>"
                              " · %7: <span style='color:%8'>%9</span></span>")
                          .arg(indexName.toHtmlEscaped(), pal0().TextDim.name(), tr("official index"), indexSymbol,
                               changeColor(official).name(), percent(official), tr("recomputed from members"),
                               changeColor(indexChange).name(), percent(indexChange)));

    // Donut: slices by weight, coloured by change.
    QVector<DonutChart::Slice> slices;
    const Palette &pal = Theme::current();
    for (const Row &r : rows) {
        const double t = ok(r.change) ? std::min(1.0, std::abs(r.change) / maxAbs) : 0;
        QColor base = !ok(r.change) ? pal.PanelLine : r.change >= 0 ? QColor(0x3f, 0xb3, 0x8a) : QColor(0xd9, 0x55, 0x55);
        if (ok(r.change))
            base = QColor::fromRgbF(base.redF() * (0.55 + 0.45 * t), base.greenF() * (0.55 + 0.45 * t), base.blueF() * (0.55 + 0.45 * t));
        slices.push_back({shortSector(r.sector), r.weight, base, percent(r.change), changeColor(r.change)});
    }
    // Biggest sector at the top left, like the original.
    m_donut->setData(slices, index == 1 ? "NASDAQ100" : indexName, tr("BY GICS"), percent(indexChange), changeColor(indexChange));

    m_tree->clear();
    auto *delegate = static_cast<BarDelegate *>(m_tree->itemDelegateForColumn(2));
    delegate->max = maxAbs;
    int rank = 0;
    for (const Row &r : rows) {
        auto *it = new QTreeWidgetItem(m_tree);
        it->setText(0, QString("%1)").arg(++rank));
        it->setText(1, sectorLabel(r.sector));
        it->setFont(1, mono(12, true));
        it->setData(2, Qt::UserRole, ok(r.change) ? QVariant(r.change) : QVariant());
        it->setText(3, percent(r.change));
        it->setForeground(3, changeColor(r.change));
        it->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        it->setText(4, percent(r.weight / totalWeight, 1, false));
        it->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
        it->setForeground(4, pal.TextDim);
        for (const Member &m : r.members) {
            auto *child = new QTreeWidgetItem(it);
            child->setText(1, m.symbol + "  " + m.name);
            child->setData(1, Qt::UserRole, m.symbol);
            child->setForeground(1, pal.TextDim);
            child->setData(2, Qt::UserRole, ok(m.change) ? QVariant(m.change) : QVariant());
            child->setText(3, percent(m.change));
            child->setForeground(3, changeColor(m.change));
            child->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
            child->setText(4, percent(m.weight / totalWeight, 2, false));
            child->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
            child->setForeground(4, pal.TextFaint);
        }
    }
    const QString periodName = QStringList {tr("today"), tr("over a week"), tr("over a month"), tr("over three months"),
                                            tr("since the start of the year")}.value(period);
    m_note->setText(tr("%1 members (%2), weighted by %3; change %4. Members: Wikipedia (S&P 500 and Dow, with their GICS "
                       "sector) and Nasdaq (Nasdaq-100; members outside the S&P 500 get the nearest GICS sector from "
                       "Nasdaq's classification). Prices: Yahoo Finance daily closes; market caps: Nasdaq's screener. "
                       "The index's official change differs a little: it weighs by free float. "
                       "Click a sector to list its members, a member to open it.")
                        .arg(members.size() - missing)
                        .arg(indexName, index == 2 ? tr("price, like the index") : tr("market capitalisation, share classes splitting their company's"),
                             periodName));
}

// ---------------------------------------------------------------- ICS

IcsView::IcsView(QWidget *parent)
    : View(parent)
{
    // Not a scrolling page: the hierarchy on the left, the selection on the right.
    scroll()->setWidgetResizable(true);
    auto *row = new QHBoxLayout;
    row->setSpacing(0);
    auto *left = new QFrame;
    left->setObjectName("resPanel");
    left->setFixedWidth(330);
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(8, 8, 8, 8);
    auto *lh = new QHBoxLayout;
    lh->addWidget(label(tr("Classification Hierarchy"), "resPanelTitle"));
    lh->addStretch(1);
    lh->addWidget(label(tr("Count"), "resNote"));
    lv->addLayout(lh);
    m_search = new QLineEdit;
    m_search->setObjectName("resSearch");
    m_search->setPlaceholderText(tr("Search for an industry or a company…"));
    m_search->setClearButtonEnabled(true);
    lv->addWidget(m_search);
    m_tree = new QTreeWidget;
    m_tree->setObjectName("resIcsTree");
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tree->setColumnWidth(1, 58);
    m_tree->setMinimumHeight(640);
    lv->addWidget(m_tree, 1);
    row->addWidget(left);

    auto *right = new QFrame;
    right->setObjectName("resPanel");
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(12, 8, 12, 12);
    rv->setSpacing(12);
    auto *rh = new QHBoxLayout;
    rh->addWidget(label(tr("Industrial Classification System - ICS"), "resTitle"));
    rh->addStretch(1);
    m_filter = label({}, "resNote");
    m_filter->setTextFormat(Qt::RichText);
    rh->addWidget(m_filter);
    rv->addLayout(rh);
    m_overview = new QWidget;
    auto *oh = new QHBoxLayout(m_overview);
    oh->setContentsMargins(0, 0, 0, 0);
    const QStringList names {tr("Total Sectors"), tr("Total Companies"), tr("Total Market Cap")};
    for (int i = 0; i < 3; ++i) {
        auto *box = new QFrame;
        box->setObjectName("resInset");
        auto *v = new QVBoxLayout(box);
        auto *n = label(names[i].toUpper(), "resStatLabel");
        n->setAlignment(Qt::AlignCenter);
        v->addWidget(n);
        m_totals[i] = label({}, "resAccentValue");
        m_totals[i]->setAlignment(Qt::AlignCenter);
        v->addWidget(m_totals[i]);
        oh->addWidget(box, 1);
    }
    rv->addWidget(m_overview);
    m_hint = label(tr("Select an industry from the hierarchy to view member companies."), "resNote");
    m_hint->setAlignment(Qt::AlignCenter);
    rv->addWidget(m_hint);
    m_detail = new QWidget;
    auto *dv = new QVBoxLayout(m_detail);
    dv->setContentsMargins(0, 0, 0, 0);
    dv->setSpacing(12);
    auto *agg = new QFrame;
    agg->setObjectName("resInset");
    auto *av = new QVBoxLayout(agg);
    av->addWidget(label(tr("Aggregate Statistics"), "resInsetTitle"));
    m_aggregate = new StatList;
    av->addWidget(m_aggregate);
    dv->addWidget(agg);
    dv->addWidget(label(tr("Member Companies"), "resInsetTitle"));
    m_members = table({"#", tr("Symbol"), tr("Company Name"), tr("Country"), tr("Mkt Cap"), tr("Price"), tr("Last Session"), tr("Volume")});
    m_members->setCursor(Qt::PointingHandCursor);
    m_members->setSelectionMode(QAbstractItemView::SingleSelection);   // the company the search found
    m_members->setSelectionBehavior(QAbstractItemView::SelectRows);
    dv->addWidget(m_members);
    rv->addWidget(m_detail);
    m_detail->hide();
    rv->addStretch(1);
    row->addWidget(right, 1);
    content()->setContentsMargins(16, 14, 16, 16);
    content()->addLayout(row, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this] { buildTree(); });
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this] { showSelection(); });
    // A company found by the search: a double click opens it (a click lists its industry, with it selected).
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        const QString key = it->data(0, Qt::UserRole).toString();
        if (key.startsWith("C:"))
            emit symbolRequested(key.section('|', 2));
    });
    connect(m_members, &QTableWidget::cellClicked, this, [this](int r, int) {
        if (auto *it = m_members->item(r, 1))
            emit symbolRequested(it->text());
    });
}

void IcsView::setSearch(const QString &text)
{
    m_search->setText(text);
}

void IcsView::load()
{
    setStatus(tr("Loading every US listing…"));
    Client::instance()->listings(this, [this](const QVector<Listing> &list, const QString &err) {
        m_listings = list;
        setStatus(list.isEmpty() ? err : tr("Classification, market caps and volumes: Nasdaq stock screener (every US listing, "
                                            "%1 stocks); prices and changes: Yahoo Finance's last two daily closes.")
                                             .arg(QLocale().toString(list.size())));
        buildTree();
    });
}

void IcsView::buildTree()
{
    const QString filter = m_search->text().trimmed();
    const QString current = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
    QMap<QString, QMap<QString, int>> tree;
    QMap<QString, int> sectorCount;
    // Companies the search matches (a symbol it starts, or a name it is part of), by sector and industry: listed
    // under their industry, which is shown even when its own name doesn't match.
    QMap<QString, QMap<QString, QVector<const Listing *>>> found;
    QSet<QString> counted;   // share classes of one company carry the same market cap
    double totalCap = 0;
    for (const Listing &l : std::as_const(m_listings)) {
        const QString sector = l.sector.isEmpty() ? tr("Unclassified") : l.sector;
        const QString industry = l.industry.isEmpty() ? tr("Unclassified") : l.industry;
        tree[sector][industry]++;
        sectorCount[sector]++;
        if (!filter.isEmpty() && (l.symbol.startsWith(filter, Qt::CaseInsensitive) || l.name.contains(filter, Qt::CaseInsensitive)))
            found[sector][industry].push_back(&l);
        if (ok(l.marketCap) && !counted.contains(l.name.section(' ', 0, 1) + QString::number(qRound64(l.marketCap / 1e8))))
            totalCap += l.marketCap;
        counted.insert(l.name.section(' ', 0, 1) + QString::number(qRound64(l.marketCap / 1e8)));
    }
    m_totals[0]->setText(QString::number(tree.size()));
    m_totals[1]->setText(QLocale().toString(m_listings.size()));
    m_totals[2]->setText(compact(totalCap));
    m_tree->blockSignals(true);
    m_tree->clear();
    QStringList sectors = tree.keys();
    std::sort(sectors.begin(), sectors.end(), [&](const QString &a, const QString &b) { return sectorCount[a] > sectorCount[b]; });
    const Palette &c = Theme::current();
    QTreeWidgetItem *select = nullptr;
    QTreeWidgetItem *single = nullptr;   // the only company found: shown without a click
    int foundCount = 0;
    for (const QString &s : sectors) {
        QList<std::pair<QString, int>> industries;
        for (auto it = tree[s].begin(); it != tree[s].end(); ++it)
            if (filter.isEmpty() || it.key().contains(filter, Qt::CaseInsensitive) || s.contains(filter, Qt::CaseInsensitive)
                || !found[s][it.key()].isEmpty())
                industries.push_back({it.key(), it.value()});
        if (industries.isEmpty())
            continue;
        std::sort(industries.begin(), industries.end(), [](auto &a, auto &b) { return a.second > b.second; });
        auto *top = new QTreeWidgetItem(m_tree, {s, QString::number(sectorCount[s])});
        top->setData(0, Qt::UserRole, "S:" + s);
        top->setForeground(0, c.Accent);
        top->setFont(0, mono(12, true));
        top->setForeground(1, c.TextDim);
        top->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        if (current == "S:" + s)
            select = top;
        for (const auto &[name, count] : industries) {
            auto *child = new QTreeWidgetItem(top, {name, QString::number(count)});
            child->setData(0, Qt::UserRole, "I:" + s + "|" + name);
            child->setToolTip(0, name);
            child->setForeground(1, c.TextFaint);
            child->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            if (current == "I:" + s + "|" + name)
                select = child;
            QVector<const Listing *> companies = found[s][name];
            std::sort(companies.begin(), companies.end(), [](const Listing *a, const Listing *b) {
                return (ok(a->marketCap) ? a->marketCap : -1) > (ok(b->marketCap) ? b->marketCap : -1);
            });
            for (int i = 0; i < companies.size() && i < 30; ++i) {   // the 30 largest matches of the industry
                const Listing *l = companies[i];
                auto *leaf = new QTreeWidgetItem(child, {l->symbol + "  " + l->name, compact(l->marketCap, 1)});
                leaf->setData(0, Qt::UserRole, "C:" + s + "|" + name + "|" + l->symbol);
                leaf->setToolTip(0, l->name);
                leaf->setFont(0, mono(11));
                leaf->setForeground(1, c.TextFaint);
                leaf->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
                if (current == leaf->data(0, Qt::UserRole).toString())
                    select = leaf;
                single = leaf;
            }
            foundCount += companies.size();
            if (!companies.isEmpty())
                child->setExpanded(true);
        }
        if (!filter.isEmpty())
            top->setExpanded(true);
    }
    if (!select && foundCount == 1)
        select = single;
    if (select) {
        m_tree->setCurrentItem(select);
        for (QTreeWidgetItem *up = select->parent(); up; up = up->parent())
            up->setExpanded(true);
    }
    m_tree->blockSignals(false);
    showSelection();
}

void IcsView::showSelection()
{
    const QString key = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
    const Palette &c = Theme::current();
    if (key.isEmpty()) {
        m_filter->setText(tr("Filter By <span style='color:%1'>None</span>").arg(c.Accent.name()));
        m_overview->show();
        m_hint->show();
        m_detail->hide();
        return;
    }
    const bool isSector = key.startsWith("S:");
    const QString sector = isSector ? key.mid(2) : key.mid(2).section('|', 0, 0);
    const QString industry = isSector ? QString() : key.mid(2).section('|', 1, 1);
    const QString company = key.startsWith("C:") ? key.section('|', 2) : QString();   // a company found by the search
    m_filter->setText(tr("Filter By <span style='color:%1'>%2</span>").arg(c.Accent.name(), (isSector ? sector : industry).toHtmlEscaped()));
    QVector<const Listing *> members;
    for (const Listing &l : std::as_const(m_listings)) {
        const QString s = l.sector.isEmpty() ? tr("Unclassified") : l.sector;
        const QString i = l.industry.isEmpty() ? tr("Unclassified") : l.industry;
        if (s == sector && (isSector || i == industry))
            members.push_back(&l);
    }
    std::sort(members.begin(), members.end(), [](const Listing *a, const Listing *b) {
        return (ok(a->marketCap) ? a->marketCap : -1) > (ok(b->marketCap) ? b->marketCap : -1);
    });
    // Market caps and names from the screener; prices and changes from Yahoo's daily closes (the screener can be
    // a session behind), for the members shown.
    int shown = std::min<int>(members.size(), 300);
    for (int r = shown; r < members.size(); ++r)   // the company looked for is shown even beyond the 300 largest
        if (members[r]->symbol == company) {
            members.move(r, shown);
            ++shown;
            break;
        }
    QStringList symbols;
    for (int r = 0; r < shown; ++r)
        symbols << members[r]->symbol;
    double cap = 0;
    QVector<double> caps;
    for (const Listing *l : members)
        if (ok(l->marketCap) && l->marketCap > 0) {
            cap += l->marketCap;
            caps << l->marketCap;
        }
    std::sort(caps.begin(), caps.end());
    auto fillAggregate = [this, cap, caps, members](const QHash<QString, Series> &recent, bool loaded) {
        const Palette &c = Theme::current();
        double weighted = 0, wsum = 0;
        int up = 0, down = 0;
        for (const Listing *l : members) {
            const Series sr = recent.value(l->symbol);
            if (sr.size() < 2 || sr[sr.size() - 2].value <= 0)
                continue;
            const double ch = sr.last().value / sr[sr.size() - 2].value - 1;
            if (ok(l->marketCap) && l->marketCap > 0) {
                weighted += l->marketCap * ch;
                wsum += l->marketCap;
            }
            if (ch > 0) ++up;
            else if (ch < 0) ++down;
        }
        m_aggregate->clear();
        m_aggregate->add(tr("Total Market Cap"), compact(cap), c.Accent);
        m_aggregate->add(tr("Num Companies"), QString::number(members.size()), c.Accent);
        m_aggregate->add(tr("Median Market Cap"), caps.isEmpty() ? "N/A" : compact(caps[caps.size() / 2]), c.Accent);
        m_aggregate->add(tr("Last Session (cap-weighted)"), loaded ? percent(wsum > 0 ? weighted / wsum : NaN) : QString("…"),
                         changeColor(weighted), tr("Of the %1 largest members").arg(std::min<int>(members.size(), 300)));
        m_aggregate->add(tr("Advancing / Declining"), loaded ? QString("%1 / %2").arg(up).arg(down) : QString("…"));
    };
    fillAggregate({}, false);
    m_members->setRowCount(shown);
    for (int r = 0; r < shown; ++r) {
        const Listing *l = members[r];
        m_members->setItem(r, 0, cell(QString("%1)").arg(r + 1), c.Accent));
        m_members->setItem(r, 1, cell(l->symbol, c.Accent, Qt::AlignLeft, true));
        m_members->setItem(r, 2, cell(l->name));
        m_members->setItem(r, 3, cell(l->country, c.TextDim));
        m_members->setItem(r, 4, cell(compact(l->marketCap), {}, Qt::AlignRight));
        m_members->setItem(r, 5, cell("…", c.TextFaint, Qt::AlignRight));
        m_members->setItem(r, 6, cell("…", c.TextFaint, Qt::AlignRight));
        m_members->setItem(r, 7, cell(compact(l->volume, 1), c.TextDim, Qt::AlignRight));
    }
    Client::instance()->spark(symbols, "5d", this, [this, key, symbols, fillAggregate](const QHash<QString, Series> &recent, const QString &) {
        const QString now = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
        if (now != key)
            return;
        for (int r = 0; r < symbols.size() && r < m_members->rowCount(); ++r) {
            const Series sr = recent.value(symbols[r]);
            const double price = sr.isEmpty() ? NaN : sr.last().value;
            const double ch = sr.size() >= 2 && sr[sr.size() - 2].value > 0 ? price / sr[sr.size() - 2].value - 1 : NaN;
            m_members->setItem(r, 5, cell(money(price), {}, Qt::AlignRight));
            m_members->setItem(r, 6, cell(percent(ch), changeColor(ch), Qt::AlignRight));
        }
        fillAggregate(recent, true);
    });
    auto *hh = m_members->horizontalHeader();
    hh->setSectionResizeMode(2, QHeaderView::Stretch);
    const int widths[8] {60, 90, 0, 150, 100, 100, 90, 90};
    for (int col = 0; col < 8; ++col)
        if (col != 2) {
            hh->setSectionResizeMode(col, QHeaderView::Fixed);
            m_members->setColumnWidth(col, widths[col]);
        }
    for (int col : {4, 5, 6, 7})
        m_members->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fitHeight(m_members);
    m_overview->hide();
    m_hint->hide();
    m_detail->show();
    m_members->clearSelection();
    const int row = company.isEmpty() ? -1 : int(symbols.indexOf(company));
    if (row >= 0) {
        m_members->selectRow(row);
        QTimer::singleShot(0, this, [this, row] {   // once the page is laid out
            if (auto *it = m_members->item(row, 1)) {
                const QPoint at = m_members->viewport()->mapTo(scroll()->widget(), m_members->visualItemRect(it).center());
                scroll()->ensureVisible(at.x(), at.y(), 0, 200);
            }
        });
    }
}

// ---------------------------------------------------------------- SPLC

SplcView::SplcView(QWidget *parent)
    : View(parent)
{
    QVBoxLayout *b;
    auto *p = panel({}, &b);
    m_graph = new SupplyChainGraph;
    m_graph->setMinimumHeight(620);
    b->addWidget(m_graph, 1);
    m_note = label({}, "resNote");
    m_note->setWordWrap(true);
    b->addWidget(m_note);
    content()->addWidget(p, 1);
    connect(m_graph, &SupplyChainGraph::symbolActivated, this, &View::symbolRequested);
}

void SplcView::load()
{
    const QString sym = m_symbol;
    auto center = std::make_shared<SupplyChainGraph::Node>();
    center->symbol = sym;
    m_graph->setMetricLabel(tr("PRODUCTS"));
    m_graph->setNote(tr("Loading the links recorded on Wikidata…"));
    m_graph->setData(*center, {}, {});
    m_note->setText({});
    Client::instance()->summary(sym, this, [this, sym, center](const QJsonObject &s, const QString &) {
        if (sym != m_symbol)
            return;
        const QJsonObject price = s.value("price").toObject();
        center->name = text(price, "longName").isEmpty() ? text(price, "shortName") : text(price, "longName");
        center->price = raw(price, "regularMarketPrice");
        center->changePct = raw(price, "regularMarketChangePercent");
    });
    Client::instance()->supplyChain(sym, this, [this, sym, center](const SupplyChain &chain, const QString &err) {
        if (sym != m_symbol)
            return;
        auto nodes = [](const QVector<Partner> &list) {
            QVector<SupplyChainGraph::Node> out;
            for (const Partner &p : list) {
                SupplyChainGraph::Node n;
                n.symbol = p.symbol;
                n.openable = !p.symbol.isEmpty();
                // US tickers speak for themselves; "2317.TW" doesn't: the name then.
                n.label = p.symbol.isEmpty() || p.symbol.contains('.') || p.symbol.front().isDigit() ? p.name : QString();
                n.name = p.name;
                n.sector = p.listing.isEmpty() ? tr("Not listed") : p.listing;
                QStringList shown = p.products.mid(0, 4);
                n.product = tr("Products: %1%2").arg(shown.join(", "), p.products.size() > 4 ? tr(" and %1 more").arg(p.products.size() - 4) : QString());
                n.shareText = QString::number(p.products.size());
                n.url = "https://www.wikidata.org/wiki/" + p.item;
                out.push_back(n);
            }
            return out;
        };
        auto suppliers = std::make_shared<QVector<SupplyChainGraph::Node>>(nodes(chain.suppliers));
        auto customers = std::make_shared<QVector<SupplyChainGraph::Node>>(nodes(chain.customers));
        QString note;
        if (chain.item.isEmpty() && chain.suppliers.isEmpty() && chain.customers.isEmpty())
            note = err.isEmpty() ? tr("No Wikidata item lists the ticker %1, or none of its products names a maker.").arg(sym)
                                 : tr("Wikidata couldn't be reached: %1").arg(err);
        else if (suppliers->isEmpty() && customers->isEmpty())
            note = tr("Wikidata records no manufacturer for this company's products, and no product it makes for others.");
        m_graph->setNote(note);
        m_graph->setData(*center, *suppliers, *customers);
        m_note->setText(tr("Source: Wikidata, through products: suppliers make the company's products (property P176, "
                           "manufacturer); customers sell products it makes (P1716 brand, P178 developer). The number is how "
                           "many products link them, not a share of revenue: Wikidata is edited by volunteers, its coverage is "
                           "uneven and no revenue figure is recorded. Prices: Yahoo Finance. Click a listed company to open it."));
        // Prices of the listed partners.
        QStringList symbols {sym};
        for (const auto *list : {suppliers.get(), customers.get()})
            for (const auto &n : *list)
                if (n.openable)
                    symbols << n.symbol;
        Client::instance()->quotes(symbols, this, [this, sym, center, suppliers, customers](const QHash<QString, QJsonObject> &q, const QString &) {
            if (sym != m_symbol)
                return;
            auto apply = [&q](SupplyChainGraph::Node &n) {
                const QJsonObject o = q.value(n.symbol);
                if (o.isEmpty())
                    return;
                n.price = o.value("regularMarketPrice").toDouble(NaN);
                n.currency = o.value("currency").toString("USD");
                n.changePct = o.value("regularMarketChangePercent").toDouble(NaN) / 100;
                if (n.name.isEmpty())
                    n.name = o.value("longName").toString();
            };
            apply(*center);
            for (auto *list : {suppliers.get(), customers.get()})
                for (auto &n : *list)
                    apply(n);
            m_graph->setData(*center, *suppliers, *customers);
        });
    });
}

} // namespace research
