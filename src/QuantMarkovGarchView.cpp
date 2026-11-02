#include "QuantMarkovViews.h"

#include "QuantMarkovJobs.h"
#include "QuantMarketState.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace quant {

namespace {

constexpr int RealizedWindow = 20;   // days behind the realized volatility line (as in runGarch)
constexpr int ConeHistory = 60;      // days of conditional volatility drawn before the forecast
constexpr double Annual = 15.874507866387544;   // √252

QString num(double v, int decimals = 3)
{
    return ok(v) ? QString::number(v, 'f', decimals) : QString("—");
}

QString pct(double v, int decimals = 2)
{
    return ok(v) ? QString::number(v, 'f', decimals) + "%" : QString("—");
}

QTableWidget *grid(const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size());
    t->setObjectName("qtTable");
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(24);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->setFrameShape(QFrame::NoFrame);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int col = 0; col < headers.size(); ++col)
        t->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    return t;
}

QTableWidgetItem *cell(const QString &text, const QColor &color = QColor(), bool bold = false)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(Qt::AlignCenter);
    if (color.isValid())
        it->setForeground(color);
    if (bold)
        it->setFont(mono(11, true));
    return it;
}

void fitRows(QTableWidget *t)
{
    int h = t->horizontalHeader()->height();
    for (int r = 0; r < t->rowCount(); ++r)
        h += t->rowHeight(r);
    t->setFixedHeight(h + 4);
}

QHBoxLayout *statRow(QVBoxLayout *body, const QString &key, QLabel *value)
{
    auto *h = new QHBoxLayout;
    h->setSpacing(8);
    h->addWidget(label(key, "qtCardLabel"));
    h->addStretch(1);
    h->addWidget(value);
    body->addLayout(h);
    return h;
}

// The next business days after `from`.
QVector<QDate> businessDays(QDate from, int count)
{
    QVector<QDate> out;
    while (out.size() < count) {
        from = from.addDays(1);
        if (from.dayOfWeek() <= 5)
            out.push_back(from);
    }
    return out;
}

} // namespace

MsGarchView::MsGarchView(QWidget *parent)
    : View(parent)
{
    QSettings settings;
    const Palette &c = Theme::current();
    QHBoxLayout *h;
    auto *top = bar(&h);
    m_symbol = symbolEdit(settings.value("quant/markov/garch/symbol", "SPY").toString(), c.Accent);
    h->addWidget(m_symbol);
    h->addWidget(segments({tr("1 Year"), tr("2 Years"), tr("5 Years"), tr("10 Years"), tr("Max")}, &m_range, 2));
    h->addSpacing(6);
    h->addWidget(label(tr("Regimes:"), "qtFieldLabel"));
    h->addWidget(segments({"1", "2", "3"}, &m_regimes, 1));
    m_regimes->button(0)->setToolTip(tr("One regime: a plain GARCH(1,1)."));
    h->addSpacing(6);
    h->addWidget(label(tr("Innovations:"), "qtFieldLabel"));
    h->addWidget(segments({tr("Normal"), tr("Student-t")}, &m_innovations, 0));
    h->addSpacing(6);
    h->addWidget(label(tr("Horizon (days):"), "qtFieldLabel"));
    m_horizon = new QSpinBox;
    m_horizon->setObjectName("qtSpin");
    m_horizon->setRange(1, 250);
    m_horizon->setValue(std::clamp(settings.value("quant/markov/garch/horizon", 10).toInt(), 1, 250));
    m_horizon->setFixedWidth(64);
    h->addWidget(m_horizon);
    m_fit = new QPushButton(tr("Fit"));
    m_fit->setObjectName("qtSeg");
    m_fit->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_fit);
    h->addStretch(1);
    content()->addWidget(top);

    m_runner = new JobRunner(this);
    content()->addWidget(new JobBar(m_runner));

    // Cards: forecast, one-day risk, horizon risk, position size.
    auto *cards = new QHBoxLayout;
    cards->setSpacing(10);
    QVBoxLayout *body;
    QHBoxLayout *head;
    auto *vol = card(tr("Volatility forecast"), &body, &head);
    m_regimeBadge = badge("—", c.TextDim);
    head->addWidget(m_regimeBadge);
    m_volToday = label("—", "qtBigValue");
    m_volToday->setStyleSheet(QString("color: %1").arg(c.Accent.name()));
    auto *vt = new QHBoxLayout;
    vt->setSpacing(6);
    vt->addWidget(m_volToday);
    vt->addWidget(label(tr("daily, next day"), "qtCardLabel"), 0, Qt::AlignBottom);
    vt->addStretch(1);
    body->addLayout(vt);
    m_volAnnual = label("—", "qtCardValue");
    statRow(body, tr("Annualized"), m_volAnnual);
    m_volHorizon = label("—", "qtCardValue");
    statRow(body, tr("Over the horizon"), m_volHorizon);
    m_regimeProb = label("—", "qtCardValue");
    statRow(body, tr("Regime probability (next day)"), m_regimeProb);
    body->addStretch(1);
    cards->addWidget(vol, 1);

    auto *risk1 = card(tr("Risk · 1 day"), &body);
    const QStringList riskKeys {tr("VaR 95%"), tr("VaR 99%"), tr("ES 95%"), tr("ES 99%")};
    for (int i = 0; i < 4; ++i) {
        m_risk[i] = label("—", "qtCardValue");
        statRow(body, riskKeys[i], m_risk[i]);
    }
    body->addWidget(label(tr("Parametric, on the regime mixture of the next day; losses in % of the position."), "qtNote"));
    body->addStretch(1);
    cards->addWidget(risk1, 1);

    auto *riskH = card(tr("Risk · horizon"), &body, &head);
    m_riskHorizonTitle = head->itemAt(0)->widget() ? qobject_cast<QLabel *>(head->itemAt(0)->widget()) : nullptr;
    for (int i = 4; i < 8; ++i) {
        m_risk[i] = label("—", "qtCardValue");
        statRow(body, riskKeys[i - 4], m_risk[i]);
    }
    body->addWidget(label(tr("Summed forecast variance as one normal or Student component."), "qtNote"));
    body->addStretch(1);
    cards->addWidget(riskH, 1);

    auto *size = card(tr("Position size"), &body);
    auto *g = new QGridLayout;
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(4);
    g->addWidget(label(tr("Capital"), "qtCardLabel"), 0, 0);
    m_capital = new QDoubleSpinBox;
    m_capital->setObjectName("qtSpin");
    m_capital->setRange(1, 1e12);
    m_capital->setDecimals(0);
    m_capital->setGroupSeparatorShown(true);
    m_capital->setValue(settings.value("quant/markov/garch/capital", 100000).toDouble());
    g->addWidget(m_capital, 0, 1);
    g->addWidget(label(tr("Target daily risk (%)"), "qtCardLabel"), 1, 0);
    m_targetRisk = new QDoubleSpinBox;
    m_targetRisk->setObjectName("qtSpin");
    m_targetRisk->setRange(0.01, 100);
    m_targetRisk->setDecimals(2);
    m_targetRisk->setSingleStep(0.1);
    m_targetRisk->setValue(settings.value("quant/markov/garch/targetRisk", 1.0).toDouble());
    g->addWidget(m_targetRisk, 1, 1);
    body->addLayout(g);
    m_position = label("—", "qtBigValue");
    body->addWidget(m_position);
    m_leverage = label("—", "qtCardValue");
    statRow(body, tr("Exposure / capital"), m_leverage);
    m_positionNote = label(tr("Size = capital × target risk ÷ forecast daily volatility."), "qtNote");
    m_positionNote->setWordWrap(true);
    body->addWidget(m_positionNote);
    body->addStretch(1);
    cards->addWidget(size, 1);
    content()->addLayout(cards);

    auto *tables = new QHBoxLayout;
    tables->setSpacing(10);
    auto *models = card(tr("Model vs GARCH(1,1)"), &body);
    m_models = grid({tr("Model"), tr("LogL"), tr("AIC"), tr("BIC"), tr("Params"), tr("Evals")});
    m_models->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_models->horizontalHeader()->setMinimumSectionSize(70);
    for (int col = 1; col < 6; ++col)
        m_models->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    m_models->setMinimumWidth(560);
    body->addWidget(m_models);
    body->addWidget(label(tr("Same sample and innovations; lowest BIC in bold."), "qtNote"));
    body->addWidget(label(tr("Evals: likelihood evaluations of the best start."), "qtNote"));
    body->addStretch(1);
    tables->addWidget(models, 3);
    auto *regimes = card(tr("Regimes"), &body);
    m_regimeTable = grid({tr("Regime"), "ω", "α", "β", tr("α + β"), tr("Long-run vol"), tr("P(stay)"), tr("Duration")});
    body->addWidget(m_regimeTable);
    auto *formula = label(tr("σ²_k,t = ω_k + α_k ε²_t−1 + β_k σ²_k,t−1 in each regime (Haas, Mittnik & Paolella); returns in %; "
                             "long-run vol = √(ω / (1 − α − β)) annualized. Bounds: α + β ≤ 0.99, P(stay) ≥ 80%, ω ≥ 0.1% of the sample variance."), "qtNote");
    formula->setWordWrap(true);
    body->addWidget(formula);
    body->addStretch(1);
    tables->addWidget(regimes, 4);
    content()->addLayout(tables);

    auto chartCard = [&](Chart *chart, int height) {
        QVBoxLayout *b;
        auto *f = card({}, &b);
        b->setContentsMargins(4, 4, 4, 4);
        b->setSpacing(0);
        chart->setFixedHeight(height);
        b->addWidget(chart);
        return f;
    };
    m_volChart = new MultiLineChart;
    m_volChart->setZeroLine(false);
    content()->addWidget(chartCard(m_volChart, 280));
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    m_probChart = new StackedAreaChart;
    m_probPanel = chartCard(m_probChart, 240);
    row->addWidget(m_probPanel, 1);
    m_cone = new SpreadChart;
    m_cone->setLegend({tr("Conditional vol"), tr("Forecast"), tr("Regime range")}, [](double v) { return QString::number(v, 'f', 1) + "%"; });
    row->addWidget(chartCard(m_cone, 240), 1);
    content()->addLayout(row);
    content()->addStretch(1);

    m_range->button(std::clamp(settings.value("quant/markov/garch/range", 2).toInt(), 0, 4))->setChecked(true);
    m_regimes->button(std::clamp(settings.value("quant/markov/garch/regimes", 1).toInt(), 0, 2))->setChecked(true);
    m_innovations->button(std::clamp(settings.value("quant/markov/garch/innovations", 0).toInt(), 0, 1))->setChecked(true);

    connect(m_range, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/markov/garch/range", m_range->checkedId());
        fetch();
    });
    connect(m_regimes, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/markov/garch/regimes", m_regimes->checkedId());
        estimate();
    });
    connect(m_innovations, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/markov/garch/innovations", m_innovations->checkedId());
        estimate();
    });
    connect(m_fit, &QPushButton::clicked, this, [this] {
        QSettings().setValue("quant/markov/garch/horizon", m_horizon->value());
        if (cleanSymbol(m_symbol->text()) != m_sym)
            fetch();
        else
            estimate();
    });
    connect(m_symbol, &QLineEdit::returnPressed, m_fit, &QPushButton::click);
    connect(m_horizon, &QSpinBox::editingFinished, m_fit, &QPushButton::click);
    connect(m_capital, &QDoubleSpinBox::valueChanged, this, [this] {
        QSettings().setValue("quant/markov/garch/capital", m_capital->value());
        sizePosition();
    });
    connect(m_targetRisk, &QDoubleSpinBox::valueChanged, this, [this] {
        QSettings().setValue("quant/markov/garch/targetRisk", m_targetRisk->value());
        sizePosition();
    });
}

void MsGarchView::load()
{
    fetch();
}

void MsGarchView::fetch()
{
    const QString sym = cleanSymbol(m_symbol->text());
    if (sym.isEmpty()) {
        setStatus(tr("Enter a symbol."));
        return;
    }
    m_sym = sym;
    m_symbol->setText(sym);
    QSettings().setValue("quant/markov/garch/symbol", sym);
    const int gen = ++m_generation;
    setStatus(tr("Loading prices…"));
    static const QStringList ranges {"1y", "2y", "5y", "10y", "max"};
    research::Client::instance()->prices(sym, ranges[m_range->checkedId()], "1d", this,
        [this, gen, sym](const research::Series &s, const QString &err) {
            if (gen != m_generation)
                return;
            if (s.size() < 120) {
                setStatus(QString("%1: %2").arg(sym, err.isEmpty() ? tr("not enough prices") : err));
                return;
            }
            m_prices.dates.clear();
            m_prices.close.clear();
            for (const research::Point &p : s) {
                m_prices.dates.push_back(p.date);
                m_prices.close.push_back(p.value);
            }
            ++m_pricesVersion;
            estimate();
        });
}

void MsGarchView::estimate()
{
    if (m_prices.dates.size() < 120)
        return;
    setStatus({});
    const Prices prices = m_prices;
    GarchSpec spec;
    static const QStringList ranges {"1y", "2y", "5y", "10y", "max"};
    spec.range = ranges[m_range->checkedId()];
    spec.regimes = m_regimes->checkedId() + 1;
    spec.student = m_innovations->checkedId() == 1;
    spec.horizon = m_horizon->value();
    const QString sym = m_sym;
    if (m_haveResult && !m_runner->isRunning() && m_resultFrom.pricesVersion == m_pricesVersion
        && m_resultFrom.regimes == spec.regimes && m_resultFrom.student == spec.student) {
        // Only the horizon changed: the fit stands (the same prices and model give the same one), the forecast
        // and the risk over the new horizon follow from it in a moment.
        GarchResult r = m_result;
        r.horizonDays = spec.horizon;
        r.forecast = markov::MsGarch::forecast(r.fit, spec.horizon);
        r.oneDay = markov::MsGarch::riskOneDay(r.fit, r.forecast);
        r.horizon = markov::MsGarch::riskHorizon(r.fit, r.forecast);
        m_result = r;
        MarketState::instance()->publishVolatility(volInfo(sym, r, spec));
        MarketState::instance()->publishRegime(regimeInfo(sym, r, spec));
        show(r);
        return;
    }
    const int pricesVersion = m_pricesVersion;
    m_runner->start<GarchResult>(
        [prices, spec](const markov::Progress &progress) { return runGarch(prices, spec, progress); },
        [this, sym, spec, pricesVersion](const GarchResult &r) {
            if (sym != m_sym)
                return;
            if (!r.fit.params.isValid()) {
                setStatus(r.fit.error.isEmpty() ? tr("Estimation failed.") : r.fit.error);
                return;
            }
            m_result = r;
            m_haveResult = true;
            m_resultFrom = {pricesVersion, spec.regimes, spec.student};
            MarketState::instance()->publishVolatility(volInfo(sym, r, spec));
            MarketState::instance()->publishRegime(regimeInfo(sym, r, spec));
            show(r);
        });
}

void MsGarchView::show(const GarchResult &r)
{
    const Palette &c = Theme::current();
    const markov::MsGarchParams &p = r.fit.params;
    const int K = p.regimes, n = r.dates.size();
    const QStringList labels = regimeLabels(K);
    const QVector<QColor> colors = regimeColors(K);
    const bool student = p.innovations == markov::Innovations::Student;

    // Forecast card.
    const double v1 = std::sqrt(r.forecast.variance.value(0, NaN));
    m_volToday->setText(pct(v1));
    m_volAnnual->setText(pct(v1 * Annual, 1));
    m_volHorizon->setText(tr("%1 (%2 days)").arg(pct(std::sqrt(r.forecast.totalVariance), 2)).arg(r.horizonDays));
    int next = 0;
    for (int k = 1; k < K; ++k)
        if (r.forecast.regimeProb(0, k) > r.forecast.regimeProb(0, next))
            next = k;
    setBadge(m_regimeBadge, labels[next].toUpper(), colors[next]);
    m_regimeProb->setText(K > 1 ? pct(100 * r.forecast.regimeProb(0, next), 1) : tr("single regime"));
    m_regimeProb->setStyleSheet(QString("color: %1").arg(colors[next].name()));
    // Risk cards.
    const double risks[8] {r.oneDay.var95, r.oneDay.var99, r.oneDay.es95, r.oneDay.es99,
                           r.horizon.var95, r.horizon.var99, r.horizon.es95, r.horizon.es99};
    for (int i = 0; i < 8; ++i) {
        m_risk[i]->setText(pct(risks[i]));
        m_risk[i]->setStyleSheet(QString("color: %1").arg(c.Down.name()));
    }
    if (m_riskHorizonTitle)
        m_riskHorizonTitle->setText(tr("RISK · %1 DAYS").arg(r.horizonDays));
    sizePosition();

    // Model comparison.
    m_models->setRowCount(K > 1 ? 2 : 1);
    const QString innName = student ? tr("Student-t") : tr("normal");
    auto modelRow = [&](int row, const QString &name, const markov::MsGarchFit &f, bool best) {
        m_models->setItem(row, 0, cell(name, c.Text, best));
        m_models->setItem(row, 1, cell(num(f.logLikelihood, 1), c.Text, best));
        m_models->setItem(row, 2, cell(num(f.aic, 1), c.Text, best));
        m_models->setItem(row, 3, cell(num(f.bic, 1), best ? c.Up : c.Text, best));
        m_models->setItem(row, 4, cell(QString::number(f.params.parameterCount()), c.Text, best));
        m_models->setItem(row, 5, cell(QString("%1%2").arg(f.evaluations).arg(f.converged ? "" : tr(" (not converged)")), c.TextDim));
    };
    if (K > 1) {
        const bool msBest = r.fit.bic <= r.single.bic;
        modelRow(0, tr("MS-GARCH K=%1 · %2").arg(K).arg(innName), r.fit, msBest);
        modelRow(1, tr("GARCH(1,1) · %1").arg(innName), r.single, !msBest);
    } else {
        modelRow(0, tr("GARCH(1,1) · %1").arg(innName), r.fit, true);
    }
    fitRows(m_models);

    // Regimes.
    m_regimeTable->setRowCount(K);
    for (int k = 0; k < K; ++k) {
        const markov::GarchRegime &g = p.garch[k];
        const double u = g.unconditionalVariance();
        m_regimeTable->setItem(k, 0, cell(labels[k], colors[k], true));
        m_regimeTable->setItem(k, 1, cell(num(g.omega, 4)));
        m_regimeTable->setItem(k, 2, cell(num(g.alpha)));
        m_regimeTable->setItem(k, 3, cell(num(g.beta)));
        m_regimeTable->setItem(k, 4, cell(num(g.persistence()), g.persistence() > 0.99 ? research::palette::yellow() : QColor()));
        m_regimeTable->setItem(k, 5, cell(ok(u) ? pct(std::sqrt(u) * Annual, 1) : tr("explosive")));
        m_regimeTable->setItem(k, 6, cell(K > 1 ? pct(100 * p.transition(k, k), 1) : "—"));
        const double d = p.expectedDuration(k);
        m_regimeTable->setItem(k, 7, cell(K > 1 ? (std::isfinite(d) ? tr("%1 days").arg(num(d, 1)) : "∞") : "—"));
    }
    fitRows(m_regimeTable);

    // Conditional against realized volatility, annualized.
    QVector<double> cond(n);
    for (int t = 0; t < n; ++t)
        cond[t] = std::sqrt(r.fit.variance[t]) * Annual;
    const Format pctFmt = [](double v) { return QString::number(v, 'f', 0) + "%"; };
    m_volChart->setTitle({{QString("%1 · %2").arg(m_sym, tr("conditional volatility (one day ahead) against realized (%1-day), annualized")
                                                             .arg(RealizedWindow)), c.Text}});
    m_volChart->setData(r.dates, {{tr("Conditional"), c.Accent, cond}, {tr("Realized"), c.TextDim, r.realized}}, pctFmt);
    if (student)
        m_volChart->setTitle({{QString("%1 · %2").arg(m_sym, tr("conditional volatility (one day ahead) against realized (%1-day), annualized · ν = %2")
                                                                 .arg(RealizedWindow).arg(num(p.nu, 1))), c.Text}});
    // Regime probabilities (filtered).
    m_probPanel->setVisible(K > 1);
    if (K > 1) {
        QVector<QVector<double>> series(K, QVector<double>(n));
        for (int t = 0; t < n; ++t)
            for (int k = 0; k < K; ++k)
                series[k][t] = r.fit.filtered(t, k);
        m_probChart->setTitle({{tr("Filtered regime probabilities P(regime | returns up to the day)"), c.Text}});
        m_probChart->setData(r.dates, series, labels, colors);
    }
    // The cone: the last days of conditional volatility, then the forecast between the lowest and highest regime.
    const int hist = std::min(ConeHistory, n), H = r.forecast.variance.size();
    QVector<QDate> dates = r.dates.mid(n - hist) + businessDays(r.dates.last(), H);
    QVector<double> past(hist + H, NaN), center(hist + H, NaN), upper(hist + H, NaN), lower(hist + H, NaN);
    for (int t = 0; t < hist; ++t)
        past[t] = cond[n - hist + t];
    center[hist - 1] = past[hist - 1];
    for (int j = 0; j < H; ++j) {
        center[hist + j] = std::sqrt(r.forecast.variance[j]) * Annual;
        double lo = INFINITY, hi = -INFINITY;
        for (int k = 0; k < K; ++k) {
            lo = std::min(lo, r.forecast.regimeVariance(j, k));
            hi = std::max(hi, r.forecast.regimeVariance(j, k));
        }
        lower[hist + j] = std::sqrt(lo) * Annual;
        upper[hist + j] = std::sqrt(hi) * Annual;
    }
    m_cone->setTitle({{tr("Volatility forecast · %1 days ahead, annualized").arg(H), c.Text}});
    m_cone->setData(dates, past, center, upper, lower, 0);
}

void MsGarchView::sizePosition()
{
    if (!m_haveResult || m_result.forecast.variance.isEmpty()) {
        m_position->setText("—");
        m_leverage->setText("—");
        return;
    }
    const double vol = std::sqrt(m_result.forecast.variance[0]);   // daily %
    const double exposure = m_capital->value() * m_targetRisk->value() / vol;
    m_position->setText(QLocale(QLocale::English).toString(exposure, 'f', 0));
    m_leverage->setText(QString("%1×").arg(QString::number(exposure / m_capital->value(), 'f', 2)));
    m_positionNote->setText(tr("Size = capital × target risk ÷ forecast daily volatility: %1 × %2% ÷ %3% · one-day VaR 95%: %4")
                                .arg(QLocale(QLocale::English).toString(m_capital->value(), 'f', 0), QString::number(m_targetRisk->value(), 'f', 2),
                                     QString::number(vol, 'f', 3), QLocale(QLocale::English).toString(exposure * m_result.oneDay.var95 / 100, 'f', 0)));
}

} // namespace quant
