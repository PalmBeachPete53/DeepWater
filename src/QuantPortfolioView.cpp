#include "QuantMarkovViews.h"

#include "QuantMarketState.h"
#include "ResearchCharts.h"
#include "Theme.h"
#include "markov/LinAlg.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <random>

namespace quant {

namespace {

constexpr double Annual = 15.874507866387544;
constexpr double Z95 = 1.6448536269514722, Z99 = 2.3263478740408408;

QString money(double v)
{
    return ok(v) ? QLocale(QLocale::English).toString(v, 'f', 0) : QString("—");
}

QString pct(double v, int decimals = 2)
{
    return ok(v) ? QString::number(v, 'f', decimals) + "%" : QString("—");
}

QTableWidgetItem *cell(const QString &text, const QColor &color = QColor(), bool bold = false, Qt::Alignment align = Qt::AlignCenter)
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

PortfolioView::PortfolioView(QWidget *parent)
    : View(parent)
{
    const Palette &c = Theme::current();
    QHBoxLayout *h;
    auto *top = bar(&h);
    m_symbol = symbolEdit({}, c.Accent);
    m_symbol->setPlaceholderText(tr("Symbol"));
    h->addWidget(m_symbol);
    h->addWidget(label(tr("Quantity:"), "qtFieldLabel"));
    m_quantity = new QDoubleSpinBox;
    m_quantity->setObjectName("qtSpin");
    m_quantity->setRange(-1e9, 1e9);
    m_quantity->setDecimals(0);
    m_quantity->setValue(100);
    m_quantity->setGroupSeparatorShown(true);
    m_quantity->setFixedWidth(110);
    m_quantity->setToolTip(tr("Shares; negative for a short position."));
    h->addWidget(m_quantity);
    m_add = new QPushButton(tr("Add"));
    m_add->setObjectName("qtSeg");
    m_add->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_add);
    h->addStretch(1);
    m_estimate = new QPushButton(tr("Estimate missing"));
    m_estimate->setObjectName("qtSeg");
    m_estimate->setCursor(Qt::PointingHandCursor);
    m_estimate->setToolTip(tr("Fit the HMM and the MS-GARCH in the background for every position without a regime or a volatility forecast."));
    h->addWidget(m_estimate);
    m_refresh = new QPushButton(tr("Refresh prices"));
    m_refresh->setObjectName("qtSeg");
    m_refresh->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_refresh);
    content()->addWidget(top);

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    QVBoxLayout *body;
    auto *totals = card(tr("Portfolio risk · 1 day"), &body);
    auto *g = new QGridLayout;
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(4);
    const QStringList keys {tr("Value"), tr("Volatility (daily / annualized)"), tr("Sum of standalone VaR 95%"), tr("Diversification")};
    for (int i = 0; i < keys.size(); ++i) {
        g->addWidget(label(keys[i], "qtCardLabel"), i, 0);
        m_totals[i] = label("—", i == 0 ? "qtMidValue" : "qtStatValue");
        m_totals[i]->setAlignment(Qt::AlignRight);
        g->addWidget(m_totals[i], i, 1);
    }
    body->addLayout(g);
    m_methods = new QTableWidget(0, 5);
    m_methods->setObjectName("qtTable");
    m_methods->setHorizontalHeaderLabels({tr("Method"), tr("VaR 95%"), tr("VaR 99%"), tr("ES 95%"), tr("ES 99%")});
    m_methods->verticalHeader()->hide();
    m_methods->verticalHeader()->setDefaultSectionSize(24);
    m_methods->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_methods->horizontalHeader()->setHighlightSections(false);
    m_methods->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_methods->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_methods->setSelectionMode(QAbstractItemView::NoSelection);
    m_methods->setFocusPolicy(Qt::NoFocus);
    m_methods->setShowGrid(false);
    m_methods->setFrameShape(QFrame::NoFrame);
    m_methods->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_methods->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int col = 0; col < 5; ++col)
        m_methods->horizontalHeader()->setSectionResizeMode(col, col == 0 ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
    body->addWidget(m_methods);
    auto *note = label(tr("Parametric: σ_p = √(wᵀ Σ w), Σ from the correlations of a year of daily returns and each symbol's forecast volatility "
                          "(MS-GARCH; realized when there is none), normal quantiles. Historical: the portfolio's daily P&L over the year "
                          "with today's positions; filtered: the same returns rescaled to the forecast volatilities (Hull & White). "
                          "Regime Monte Carlo: 20,000 days where each symbol draws its regime with the current probabilities and its "
                          "volatility in it, shocks correlated as the history. Losses in currency (% of value)."), "qtNote");
    note->setWordWrap(true);
    body->addWidget(note);
    body->addStretch(1);
    row->addWidget(totals, 3);
    auto *mix = card(tr("Regimes"), &body);
    m_donut = new research::DonutChart;
    m_donut->setFixedHeight(200);
    m_donut->setLeaderLabels(true);
    body->addWidget(m_donut);
    m_mix = label({}, "qtCardLabel");
    m_mix->setWordWrap(true);
    body->addWidget(m_mix);
    row->addWidget(mix, 2);
    content()->addLayout(row);
    {
        QVBoxLayout *b;
        auto *f = card({}, &b);
        b->setContentsMargins(4, 4, 4, 4);
        b->setSpacing(0);
        m_histogram = new HistogramChart;
        m_histogram->setFixedHeight(230);
        m_histogram->setTitle({{tr("Daily P&L · a year of history against the regime Monte Carlo, % of value"), c.Text}});
        b->addWidget(m_histogram);
        content()->addWidget(f);
    }

    QVBoxLayout *tb;
    auto *tableCard = card(tr("Positions"), &tb);
    m_table = new QTableWidget(0, 10);
    m_table->setObjectName("qtTable");
    m_table->setHorizontalHeaderLabels({tr("Symbol"), tr("Quantity"), tr("Price"), tr("Value"), tr("Weight"), tr("Regime"),
                                        tr("Vol (daily)"), tr("VaR 95%"), tr("Source"), {}});
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(28);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int col = 0; col < 9; ++col)
        m_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Fixed);
    m_table->setColumnWidth(9, 40);
    tb->addWidget(m_table);
    tb->addWidget(label(tr("Regime and volatility come from Quant › Markov's shared state, re-estimated in the background when older than "
                           "the last trading day; prices from Yahoo quotes."), "qtNote"));
    content()->addWidget(tableCard);
    content()->addStretch(1);

    // Positions kept in the settings.
    const QJsonArray saved = QJsonDocument::fromJson(QSettings().value("quant/portfolio/positions").toByteArray()).array();
    for (const QJsonValue &v : saved) {
        const QJsonObject o = v.toObject();
        if (!o.value("symbol").toString().isEmpty())
            m_positions.push_back({o.value("symbol").toString(), o.value("quantity").toDouble()});
    }
    if (m_positions.isEmpty())
        m_positions = {{"SPY", 100}, {"AAPL", 50}, {"MSFT", 30}};

    connect(m_add, &QPushButton::clicked, this, &PortfolioView::addPosition);
    connect(m_symbol, &QLineEdit::returnPressed, this, &PortfolioView::addPosition);
    connect(m_refresh, &QPushButton::clicked, this, &PortfolioView::fetch);
    connect(m_estimate, &QPushButton::clicked, this, [this] {
        for (const Position &p : std::as_const(m_positions))
            if (!MarketState::instance()->regime(p.symbol) || !MarketState::instance()->volatility(p.symbol))
                StateRefresher::instance()->refresh(p.symbol);
    });
    // Only the positions' own regimes and forecasts matter; the refresher's state shows next to them.
    auto held = [this](const QString &symbol) {
        return std::any_of(m_positions.cbegin(), m_positions.cend(), [&](const Position &p) { return p.symbol == symbol; });
    };
    connect(MarketState::instance(), &MarketState::regimeChanged, this, [this, held](const QString &s) { if (held(s)) scheduleCompute(); });
    connect(MarketState::instance(), &MarketState::volatilityChanged, this, [this, held](const QString &s) { if (held(s)) scheduleCompute(); });
    connect(StateRefresher::instance(), &StateRefresher::changed, this, [this] { if (!m_positions.isEmpty()) scheduleCompute(); });
}

void PortfolioView::load()
{
    fetch();
    // Positions without a regime or a forecast get theirs in the background.
    for (const Position &p : std::as_const(m_positions))
        if (!MarketState::instance()->regime(p.symbol) || !MarketState::instance()->volatility(p.symbol))
            StateRefresher::instance()->refresh(p.symbol);
}

void PortfolioView::showEvent(QShowEvent *e)
{
    View::showEvent(e);
    if (m_computeDirty)
        scheduleCompute();
}

void PortfolioView::scheduleCompute()
{
    // Each quote, price series, regime and refresher step asks for it: once per event loop pass, and only
    // while the section is shown (the rest of the time, when it is shown again).
    if (!isVisible()) {
        m_computeDirty = true;
        return;
    }
    if (m_computePending)
        return;
    m_computePending = true;
    QTimer::singleShot(0, this, [this] {
        m_computePending = false;
        m_computeDirty = false;
        compute();
    });
}

void PortfolioView::addPosition()
{
    const QString sym = cleanSymbol(m_symbol->text());
    if (sym.isEmpty())
        return;
    bool found = false;
    for (Position &p : m_positions)
        if (p.symbol == sym) {
            p.quantity = m_quantity->value();
            found = true;
        }
    if (!found)
        m_positions.push_back({sym, m_quantity->value()});
    m_symbol->clear();
    save();
    if (!MarketState::instance()->regime(sym) || !MarketState::instance()->volatility(sym))
        StateRefresher::instance()->refresh(sym);
    fetch();
}

void PortfolioView::removePosition(const QString &symbol)
{
    m_positions.erase(std::remove_if(m_positions.begin(), m_positions.end(), [&](const Position &p) { return p.symbol == symbol; }),
                      m_positions.end());
    save();
    compute();
}

void PortfolioView::save() const
{
    QJsonArray a;
    for (const Position &p : m_positions)
        a.push_back(QJsonObject {{"symbol", p.symbol}, {"quantity", p.quantity}});
    QSettings().setValue("quant/portfolio/positions", QJsonDocument(a).toJson(QJsonDocument::Compact));
}

void PortfolioView::fetch()
{
    QStringList symbols;
    for (const Position &p : std::as_const(m_positions))
        symbols << p.symbol;
    if (symbols.isEmpty()) {
        setStatus(tr("Add a position."));
        compute();
        return;
    }
    const int gen = ++m_generation;
    setStatus(tr("Loading quotes and a year of prices…"));
    m_pending = 1 + symbols.size();
    auto done = [this, gen] {
        if (gen != m_generation)
            return;
        if (--m_pending == 0)
            setStatus({});
        scheduleCompute();
    };
    research::Client::instance()->quotes(symbols, this, [this, gen, done](const QHash<QString, QJsonObject> &q, const QString &) {
        if (gen != m_generation)
            return;
        for (auto it = q.begin(); it != q.end(); ++it)
            m_quotes.insert(it.key(), it.value());
        done();
    });
    for (const QString &sym : symbols)
        research::Client::instance()->prices(sym, "1y", "1d", this, [this, gen, sym, done](const research::Series &s, const QString &) {
            if (gen != m_generation)
                return;
            Prices p;
            for (const research::Point &pt : s) {
                p.dates.push_back(pt.date);
                p.close.push_back(pt.value);
            }
            m_history.insert(sym, p);
            done();
        });
}

void PortfolioView::compute()
{
    const Palette &c = Theme::current();
    const int n = m_positions.size();
    m_table->setRowCount(n);
    QVector<double> value(n, NaN), vol(n, NaN);
    QVector<QString> source(n);
    double total = 0;
    for (int i = 0; i < n; ++i) {
        const Position &p = m_positions[i];
        const QJsonObject q = m_quotes.value(p.symbol);
        const double price = q.value("regularMarketPrice").toDouble(NaN);
        value[i] = ok(price) ? price * p.quantity : NaN;
        if (ok(value[i]))
            total += value[i];
        if (auto v = MarketState::instance()->volatility(p.symbol)) {
            vol[i] = v->dailyVol;
            source[i] = modelText(v->model);
        } else if (const Prices h = m_history.value(p.symbol); h.close.size() > 30) {
            vol[i] = stdev(returns(h.close));
            source[i] = tr("realized, 1 year");
        }
    }
    // Correlations of daily returns on the dates each pair shares.
    QVector<QVector<double>> rho(n, QVector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        rho[i][i] = 1;
        for (int j = i + 1; j < n; ++j) {
            Prices a = m_history.value(m_positions[i].symbol), b = m_history.value(m_positions[j].symbol);
            align(&a, &b);
            if (a.close.size() > 30) {
                const Ols o = ols(returns(a.close), returns(b.close));
                rho[i][j] = rho[j][i] = ok(o.r) ? o.r : 0;
            }
        }
    }
    double variance = 0, standalone = 0;
    for (int i = 0; i < n; ++i) {
        if (!ok(value[i]) || !ok(vol[i]))
            continue;
        standalone += Z95 * vol[i] / 100 * std::fabs(value[i]);
        for (int j = 0; j < n; ++j)
            if (ok(value[j]) && ok(vol[j]))
                variance += value[i] * value[j] * rho[i][j] * vol[i] * vol[j] / 1e4;
    }
    const double sigmaP = total != 0 ? std::sqrt(std::max(0.0, variance)) / std::fabs(total) * 100 : NaN;   // daily %
    const double sdP = std::sqrt(std::max(0.0, variance));
    m_totals[0]->setText(money(total));
    m_totals[1]->setText(ok(sigmaP) ? QString("%1 / %2").arg(pct(sigmaP), pct(sigmaP * Annual, 1)) : "—");
    m_totals[2]->setText(ok(sigmaP) ? money(standalone) : "—");
    m_totals[3]->setText(ok(sigmaP) && sdP > 0 ? tr("%1× (standalone / portfolio VaR)").arg(QString::number(standalone / (Z95 * sdP), 'f', 2)) : "—");

    // The methods. Losses are positive numbers in currency.
    struct Method {
        QString name;
        double var95 = NaN, var99 = NaN, es95 = NaN, es99 = NaN;
    };
    QVector<Method> methods;
    auto tail = [](QVector<double> pnl, Method *m) {
        if (pnl.size() < 50)
            return;
        std::sort(pnl.begin(), pnl.end());
        auto at = [&](double level) {
            const int k = std::clamp(int((1 - level) * pnl.size()), 0, int(pnl.size()) - 1);
            double sum = 0;
            for (int i = 0; i <= k; ++i)
                sum += pnl[i];
            return std::make_pair(-pnl[k], -sum / (k + 1));
        };
        const auto [v95, e95] = at(0.95);
        const auto [v99, e99] = at(0.99);
        m->var95 = v95;
        m->es95 = e95;
        m->var99 = v99;
        m->es99 = e99;
    };
    {
        Method m {tr("Parametric (normal)")};
        if (ok(sigmaP)) {
            m.var95 = Z95 * sdP;
            m.var99 = Z99 * sdP;
            m.es95 = sdP * std::exp(-0.5 * Z95 * Z95) / std::sqrt(2 * M_PI) / 0.05;
            m.es99 = sdP * std::exp(-0.5 * Z99 * Z99) / std::sqrt(2 * M_PI) / 0.01;
        }
        methods.push_back(m);
    }
    // Historical: the portfolio's daily P&L on the dates every valued position has.
    QVector<double> historical, filtered;
    {
        QVector<int> valued;
        for (int i = 0; i < n; ++i)
            if (ok(value[i]) && m_history.value(m_positions[i].symbol).close.size() > 30)
                valued.push_back(i);
        if (!valued.isEmpty()) {
            // Common dates.
            QSet<QDate> common;
            for (int k = 0; k < valued.size(); ++k) {
                const Prices &h = m_history.value(m_positions[valued[k]].symbol);
                QSet<QDate> dates(h.dates.begin(), h.dates.end());
                common = k == 0 ? dates : common.intersect(dates);
            }
            QVector<QDate> dates(common.begin(), common.end());
            std::sort(dates.begin(), dates.end());
            QVector<QVector<double>> rets(valued.size());
            for (int k = 0; k < valued.size(); ++k) {
                const Prices &h = m_history.value(m_positions[valued[k]].symbol);
                QHash<QDate, double> closeOf;
                for (int t = 0; t < h.dates.size(); ++t)
                    closeOf.insert(h.dates[t], h.close[t]);
                QVector<double> closes;
                for (const QDate &d : dates)
                    closes.push_back(closeOf.value(d));
                rets[k] = returns(closes);
            }
            const int T = dates.size() - 1;
            QVector<double> scales(valued.size());   // forecast over realized volatility, per position
            for (int k = 0; k < valued.size(); ++k) {
                const int i = valued[k];
                const double realized = stdev(rets[k]);
                scales[k] = ok(vol[i]) && realized > 0 ? vol[i] / realized : 1.0;
            }
            for (int t = 0; t < T; ++t) {
                double pnl = 0, scaled = 0;
                for (int k = 0; k < valued.size(); ++k) {
                    const int i = valued[k];
                    const double scale = scales[k];
                    pnl += value[i] * rets[k][t] / 100;
                    scaled += value[i] * rets[k][t] * scale / 100;
                }
                historical.push_back(pnl);
                filtered.push_back(scaled);
            }
        }
        Method h {tr("Historical (1 year)")}, f {tr("Filtered historical")};
        tail(historical, &h);
        tail(filtered, &f);
        methods.push_back(h);
        methods.push_back(f);
    }
    // Regime Monte Carlo: correlated shocks, each symbol in a regime drawn with its current probabilities.
    QVector<double> simulated;
    {
        Method m {tr("Regime Monte Carlo")};
        QVector<int> valued;
        for (int i = 0; i < n; ++i)
            if (ok(value[i]) && ok(vol[i]))
                valued.push_back(i);
        const int k = valued.size();
        if (k > 0) {
            markov::Mat corr(k, k);
            for (int a = 0; a < k; ++a)
                for (int b = 0; b < k; ++b)
                    corr(a, b) = a == b ? 1.0 + 1e-9 : rho[valued[a]][valued[b]];
            markov::Mat L;
            if (!markov::cholesky(corr, &L))
                L = markov::Mat::identity(k);
            QVector<QVector<double>> regimeVols(k), regimeProbs(k);
            for (int a = 0; a < k; ++a) {
                const auto r = MarketState::instance()->regime(m_positions[valued[a]].symbol);
                if (r && !r->vols.isEmpty() && r->vols.size() == r->probabilities.size()) {
                    regimeVols[a] = r->vols;
                    regimeProbs[a] = r->probabilities;
                    // The regime volatilities around the forecast: keep their mix at the forecast level.
                    double mix = 0;
                    for (int j = 0; j < r->vols.size(); ++j)
                        mix += r->probabilities[j] * r->vols[j] * r->vols[j];
                    const double adjust = mix > 0 ? vol[valued[a]] / std::sqrt(mix) : 1.0;
                    for (double &v : regimeVols[a])
                        v *= adjust;
                } else {
                    regimeVols[a] = {vol[valued[a]]};
                    regimeProbs[a] = {1.0};
                }
            }
            std::mt19937 rng(17);
            std::normal_distribution<double> normal(0, 1);
            std::uniform_real_distribution<double> u(0, 1);
            const int paths = 20000;
            simulated.reserve(paths);
            QVector<double> z(k), e(k);
            for (int p = 0; p < paths; ++p) {
                for (int a = 0; a < k; ++a)
                    z[a] = normal(rng);
                double pnl = 0;
                for (int a = 0; a < k; ++a) {
                    e[a] = 0;
                    for (int b = 0; b <= a; ++b)
                        e[a] += L(a, b) * z[b];
                    double x = u(rng), sigma = regimeVols[a].last();
                    for (int j = 0; j < regimeProbs[a].size(); ++j) {
                        x -= regimeProbs[a][j];
                        if (x <= 0) {
                            sigma = regimeVols[a][j];
                            break;
                        }
                    }
                    pnl += value[valued[a]] * sigma * e[a] / 100;
                }
                simulated.push_back(pnl);
            }
            tail(simulated, &m);
        }
        methods.push_back(m);
    }
    m_methods->setRowCount(methods.size());
    for (int i = 0; i < methods.size(); ++i) {
        const Method &m = methods[i];
        m_methods->setItem(i, 0, cell(m.name, c.Text, false, Qt::AlignLeft));
        const double values[4] {m.var95, m.var99, m.es95, m.es99};
        for (int j = 0; j < 4; ++j)
            m_methods->setItem(i, j + 1, cell(ok(values[j]) && total != 0 ? QString("%1 (%2)").arg(money(values[j]), pct(100 * values[j] / std::fabs(total)))
                                                                           : "—", c.Down, j == 0));
    }
    {
        int hgt = m_methods->horizontalHeader()->height();
        for (int r = 0; r < m_methods->rowCount(); ++r)
            hgt += m_methods->rowHeight(r);
        m_methods->setFixedHeight(hgt + 4);
    }
    {
        QVector<HistogramChart::Sample> samples;
        auto toPct = [total](const QVector<double> &v) {
            QVector<double> out;
            for (double x : v)
                out.push_back(total != 0 ? 100 * x / std::fabs(total) : NaN);
            return out;
        };
        if (!historical.isEmpty())
            samples.push_back({tr("Historical"), c.TextDim, toPct(historical)});
        if (!simulated.isEmpty()) {
            // The same count as the history, so the two heights compare: a random subset of the simulation.
            QVector<double> sub;
            const int step = std::max(1, int(simulated.size() / std::max(1, int(historical.size()))));
            for (int i = 0; i < simulated.size(); i += step)
                sub.push_back(simulated[i]);
            samples.push_back({tr("Regime Monte Carlo"), c.Accent, toPct(sub)});
        }
        m_histogram->setData(samples, 60, [](double v) { return QString::number(v, 'f', 1) + "%"; });
    }

    // Positions.
    QVector<research::DonutChart::Slice> slices;
    QHash<QString, double> byRegime;
    for (int i = 0; i < n; ++i) {
        const Position &p = m_positions[i];
        const auto r = MarketState::instance()->regime(p.symbol);
        const QJsonObject q = m_quotes.value(p.symbol);
        const double price = q.value("regularMarketPrice").toDouble(NaN);
        const double weight = total != 0 && ok(value[i]) ? 100 * value[i] / total : NaN;
        m_table->setItem(i, 0, cell(p.symbol, c.Text, true));
        m_table->setItem(i, 1, cell(money(p.quantity)));
        m_table->setItem(i, 2, cell(ok(price) ? QString::number(price, 'f', 2) : "…"));
        m_table->setItem(i, 3, cell(money(value[i])));
        m_table->setItem(i, 4, cell(pct(weight, 1)));
        const QColor regimeColor = r ? regimeColors(r->states).value(r->regime, c.TextDim) : c.TextFaint;
        const bool busy = StateRefresher::instance()->current() == p.symbol;
        m_table->setItem(i, 5, cell(r ? QString("%1 %2%").arg(regimeLabelText(r->label).toUpper(), QString::number(100 * r->probability, 'f', 0))
                                      : busy ? tr("estimating…") : tr("—"), regimeColor, r != std::nullopt));
        m_table->setItem(i, 6, cell(pct(vol[i]), source[i].startsWith(tr("realized")) ? c.TextDim : c.Text));
        m_table->setItem(i, 7, cell(ok(vol[i]) && ok(value[i]) ? money(Z95 * vol[i] / 100 * std::fabs(value[i])) : "—", c.Down));
        m_table->setItem(i, 8, cell(source[i].isEmpty() ? "—" : source[i], c.TextDim));
        auto *remove = new QPushButton("✕");
        remove->setObjectName("qtIconButton");
        remove->setCursor(Qt::PointingHandCursor);
        const QString sym = p.symbol;
        connect(remove, &QPushButton::clicked, this, [this, sym] { removePosition(sym); });
        m_table->setCellWidget(i, 9, remove);
        if (ok(value[i]) && value[i] > 0) {
            slices.push_back({p.symbol, value[i], regimeColor, r ? regimeLabelText(r->label) : tr("no regime"), regimeColor});
            byRegime[r ? regimeLabelText(r->label) : tr("no regime")] += value[i];
        }
    }
    int h = m_table->horizontalHeader()->height();
    for (int r = 0; r < m_table->rowCount(); ++r)
        h += m_table->rowHeight(r);
    m_table->setFixedHeight(h + 4);
    m_donut->setData(slices, tr("VALUE"), {}, money(total), c.Text);
    QStringList parts;
    QList<QString> names = byRegime.keys();
    std::sort(names.begin(), names.end(), [&](const QString &a, const QString &b) { return byRegime[a] > byRegime[b]; });
    for (const QString &name : names)
        if (total > 0)
            parts << QString("%1 %2%").arg(name, QString::number(100 * byRegime[name] / total, 'f', 0));
    m_mix->setText(parts.isEmpty() ? tr("No valued position.") : tr("Share of the value by regime: %1").arg(parts.join(" · ")));
}

} // namespace quant
