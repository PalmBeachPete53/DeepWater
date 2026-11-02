#include "QuantMarkovViews.h"

#include "QuantMarkovJobs.h"
#include "QuantMarketState.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
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


QString num(double v, int decimals = 3)
{
    return ok(v) ? QString::number(v, 'f', decimals) : QString("—");
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

} // namespace

HmmView::HmmView(QWidget *parent)
    : View(parent)
{
    QSettings settings;
    const Palette &c = Theme::current();
    QHBoxLayout *h;
    auto *top = bar(&h);
    m_symbol = symbolEdit(settings.value("quant/markov/hmm/symbol", "SPY").toString(), c.Accent);
    h->addWidget(m_symbol);
    h->addWidget(segments({tr("1 Year"), tr("2 Years"), tr("5 Years"), tr("10 Years"), tr("Max")}, &m_range, 2));
    h->addSpacing(6);
    h->addWidget(label(tr("States:"), "qtFieldLabel"));
    h->addWidget(segments({"2", "3", "4"}, &m_states, 1));
    h->addSpacing(6);
    h->addWidget(label(tr("Inputs:"), "qtFieldLabel"));
    h->addWidget(segments({tr("Returns"), tr("Returns + Vol")}, &m_inputs, 0));
    m_inputs->button(1)->setToolTip(tr("Two variables per day: the return and the realized volatility (standard deviation of the "
                                       "last 10 returns), both in %."));
    h->addSpacing(6);
    h->addWidget(label(tr("Window (days):"), "qtFieldLabel"));
    m_window = new QSpinBox;
    m_window->setObjectName("qtSpin");
    m_window->setRange(0, 5000);
    m_window->setSpecialValueText(tr("all"));
    m_window->setFixedWidth(70);
    m_window->setValue(settings.value("quant/markov/hmm/window", 0).toInt());
    m_window->setToolTip(tr("Rolling estimation: the model is estimated on the last N days only, then applied to the whole "
                            "range. 0 (all) estimates on the whole range."));
    h->addWidget(m_window);
    m_fit = new QPushButton(tr("Fit"));
    m_fit->setObjectName("qtSeg");
    m_fit->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_fit);
    h->addStretch(1);
    m_posterior = new QCheckBox(tr("A posteriori (smoothed)"));
    m_posterior->setObjectName("qtCheck");
    m_posterior->setMinimumWidth(170);
    m_posterior->setChecked(settings.value("quant/markov/hmm/posterior", false).toBool());
    m_posterior->setToolTip(tr("Off: filtered probabilities P(state | data up to that day), usable as a signal. On: smoothed "
                               "probabilities and the Viterbi path, which use the whole sample (look-ahead): for reading "
                               "history only."));
    h->addWidget(m_posterior);
    content()->addWidget(top);

    m_runner = new JobRunner(this);
    content()->addWidget(new JobBar(m_runner));

    // Cards: the current regime, the model, the comparison of K.
    auto *cards = new QHBoxLayout;
    cards->setSpacing(10);
    QVBoxLayout *body;
    QHBoxLayout *head;
    auto *regime = card(tr("Current regime"), &body, &head);
    m_regimeBadge = badge("—", c.TextDim);
    m_regimeBadge->setFixedHeight(26);
    body->addWidget(m_regimeBadge);
    m_regimeProb = label("—", "qtMidValue");
    auto *pr = new QHBoxLayout;
    pr->addWidget(label(tr("Filtered probability"), "qtCardLabel"));
    pr->addStretch(1);
    pr->addWidget(m_regimeProb);
    body->addLayout(pr);
    m_regimeNote = label({}, "qtNote");
    m_regimeNote->setWordWrap(true);
    body->addWidget(m_regimeNote);
    body->addStretch(1);
    cards->addWidget(regime, 1);

    auto *model = card(tr("Model"), &body);
    const QStringList keys {tr("Log-likelihood"), tr("AIC"), tr("BIC"), tr("Parameters"), tr("EM iterations"), tr("Observations")};
    auto *g = new QGridLayout;
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(3);
    for (int i = 0; i < keys.size(); ++i) {
        g->addWidget(label(keys[i], "qtCardLabel"), i, 0);
        m_modelValues[i] = label("—", "qtStatValue");
        m_modelValues[i]->setAlignment(Qt::AlignRight);
        g->addWidget(m_modelValues[i], i, 1);
    }
    body->addLayout(g);
    body->addStretch(1);
    cards->addWidget(model, 1);

    auto *compare = card(tr("Number of states"), &body);
    m_compare = grid({tr("K"), tr("Log-likelihood"), tr("AIC"), tr("BIC")});
    body->addWidget(m_compare);
    body->addWidget(label(tr("Lowest BIC in bold; the shown model is highlighted."), "qtNote"));
    body->addStretch(1);
    cards->addWidget(compare, 2);
    content()->addLayout(cards);

    auto chartCard = [&](Chart *chart, int height) {
        QVBoxLayout *b;
        auto *f = card({}, &b);
        b->setContentsMargins(4, 4, 4, 4);
        b->setSpacing(0);
        chart->setFixedHeight(height);
        b->addWidget(chart);
        return f;
    };
    m_regimeChart = new RegimeChart;
    content()->addWidget(chartCard(m_regimeChart, 320));
    m_probChart = new StackedAreaChart;
    content()->addWidget(chartCard(m_probChart, 220));

    auto *tables = new QHBoxLayout;
    tables->setSpacing(10);
    auto *tr1 = card(tr("Transition matrix"), &body);
    m_transition = grid({tr("From \\ To")});
    body->addWidget(m_transition);
    body->addWidget(label(tr("P(next regime | current regime), one row per current regime."), "qtNote"));
    body->addStretch(1);
    tables->addWidget(tr1, 2);
    auto *st = card(tr("Regimes"), &body);
    m_statesTable = grid({tr("Regime"), tr("Mean (daily %)"), tr("Vol (daily %)"), tr("Vol (ann. %)"), tr("Expected duration"), tr("Share of time")});
    body->addWidget(m_statesTable);
    body->addWidget(label(tr("Expected duration = 1 / (1 − p_kk) days; share of time from the filtered probabilities."), "qtNote"));
    body->addStretch(1);
    tables->addWidget(st, 3);
    content()->addLayout(tables);
    content()->addStretch(1);

    m_range->button(std::clamp(settings.value("quant/markov/hmm/range", 2).toInt(), 0, 4))->setChecked(true);
    m_states->button(std::clamp(settings.value("quant/markov/hmm/states", 1).toInt(), 0, 2))->setChecked(true);
    m_inputs->button(std::clamp(settings.value("quant/markov/hmm/inputs", 0).toInt(), 0, 1))->setChecked(true);

    connect(m_range, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/markov/hmm/range", m_range->checkedId());
        fetch();
    });
    connect(m_states, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/markov/hmm/states", m_states->checkedId());
        estimate();
    });
    connect(m_inputs, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/markov/hmm/inputs", m_inputs->checkedId());
        estimate();
    });
    connect(m_fit, &QPushButton::clicked, this, [this] {
        QSettings().setValue("quant/markov/hmm/window", m_window->value());
        if (cleanSymbol(m_symbol->text()) != m_sym)
            fetch();
        else
            estimate();
    });
    connect(m_symbol, &QLineEdit::returnPressed, m_fit, &QPushButton::click);
    connect(m_posterior, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue("quant/markov/hmm/posterior", on);
        if (m_haveResult)
            show(m_result);
    });
}

void HmmView::load()
{
    fetch();
}

void HmmView::fetch()
{
    const QString sym = cleanSymbol(m_symbol->text());
    if (sym.isEmpty()) {
        setStatus(tr("Enter a symbol."));
        return;
    }
    m_sym = sym;
    m_symbol->setText(sym);
    QSettings().setValue("quant/markov/hmm/symbol", sym);
    const int gen = ++m_generation;
    setStatus(tr("Loading prices…"));
    static const QStringList ranges {"1y", "2y", "5y", "10y", "max"};
    research::Client::instance()->prices(sym, ranges[m_range->checkedId()], "1d", this,
        [this, gen, sym](const research::Series &s, const QString &err) {
            if (gen != m_generation)
                return;
            if (s.size() < 60) {
                setStatus(QString("%1: %2").arg(sym, err.isEmpty() ? tr("not enough prices") : err));
                return;
            }
            m_prices.dates.clear();
            m_prices.close.clear();
            for (const research::Point &p : s) {
                m_prices.dates.push_back(p.date);
                m_prices.close.push_back(p.value);
            }
            estimate();
        });
}

void HmmView::estimate()
{
    if (m_prices.dates.size() < 60)
        return;
    setStatus({});
    const Prices prices = m_prices;
    HmmSpec spec;
    static const QStringList ranges {"1y", "2y", "5y", "10y", "max"};
    spec.range = ranges[m_range->checkedId()];
    spec.states = m_states->checkedId() + 2;
    spec.withVol = m_inputs->checkedId() == 1;
    spec.window = m_window->value();
    const QString sym = m_sym;
    m_runner->start<HmmResult>(
        [prices, spec](const markov::Progress &progress) { return runHmm(prices, spec, progress); },
        [this, sym, spec](const HmmResult &r) {
            if (sym != m_sym)
                return;
            if (!r.fit.params.isValid()) {
                setStatus(r.fit.error.isEmpty() ? tr("Estimation failed.") : r.fit.error);
                return;
            }
            m_result = r;
            m_haveResult = true;
            MarketState::instance()->publishRegime(regimeInfo(sym, r, spec));
            show(r);
        });
}

void HmmView::show(const HmmResult &r)
{
    const Palette &c = Theme::current();
    const markov::HmmParams &p = r.fit.params;
    const int K = p.states, n = r.dates.size();
    const QStringList labels = regimeLabels(K);
    const QVector<QColor> colors = regimeColors(K);
    const bool posterior = m_posterior->isChecked();
    const markov::Mat &probs = posterior ? r.smoothed : r.filtered;

    // Regime per day: the most probable one (filtered) or the Viterbi path (a posteriori).
    QVector<int> regime(n);
    QVector<double> prob(n);
    for (int t = 0; t < n; ++t) {
        int best = 0;
        for (int k = 1; k < K; ++k)
            if (probs(t, k) > probs(t, best))
                best = k;
        regime[t] = posterior ? r.viterbi[t] : best;
        prob[t] = probs(t, regime[t]);
    }
    const int last = regime.last();
    setBadge(m_regimeBadge, labels[last].toUpper(), colors[last]);
    m_regimeProb->setText(QString::number(100 * r.filtered(n - 1, last), 'f', 1) + "%");
    m_regimeProb->setStyleSheet(QString("color: %1").arg(colors[last].name()));
    // A one-line reading: how far into the visit, and what usually comes next.
    int run = 1;
    for (int t = n - 2; t >= 0 && regime[t] == last; --t)
        ++run;
    int nextK = -1;
    for (int k = 0; k < K; ++k)
        if (k != last && (nextK < 0 || p.transition(last, k) > p.transition(last, nextK)))
            nextK = k;
    m_regimeNote->setText(tr("%1 for %2 days (expected visit %3 days); P(stay) = %4%; most likely switch: %5 (%6%)%7")
                              .arg(labels[last]).arg(run).arg(num(p.expectedDuration(last), 1))
                              .arg(num(100 * p.transition(last, last), 1))
                              .arg(nextK >= 0 ? labels[nextK] : "—", nextK >= 0 ? num(100 * p.transition(last, nextK), 1) : "—")
                              .arg(r.window > 0 ? tr(" · estimated on the last %1 days").arg(r.window) : QString()));
    const QStringList values {num(r.fit.logLikelihood, 1), num(r.fit.aic, 1), num(r.fit.bic, 1), QString::number(p.parameterCount()),
                              QString("%1%2").arg(r.fit.iterations).arg(r.fit.converged ? "" : tr(" (not converged)")),
                              QString("%1%2").arg(r.fit.observations).arg(r.obs.cols == 2 ? tr(" × 2 variables") : QString())};
    for (int i = 0; i < values.size(); ++i)
        m_modelValues[i]->setText(values[i]);

    // K comparison.
    m_compare->setRowCount(r.others.size());
    double bestBic = INFINITY;
    for (const markov::HmmFit &f : r.others)
        if (ok(f.bic))
            bestBic = std::min(bestBic, f.bic);
    QColor tint = c.Accent;
    tint.setAlpha(35);
    for (int i = 0; i < r.others.size(); ++i) {
        const markov::HmmFit &f = r.others[i];
        const bool best = ok(f.bic) && f.bic == bestBic;
        m_compare->setItem(i, 0, cell(QString::number(f.params.states), c.Text, best));
        m_compare->setItem(i, 1, cell(num(f.logLikelihood, 1), c.Text, best));
        m_compare->setItem(i, 2, cell(num(f.aic, 1), c.Text, best));
        m_compare->setItem(i, 3, cell(num(f.bic, 1), best ? c.Up : c.Text, best));
        if (f.params.states == K)
            for (int col = 0; col < 4; ++col)
                m_compare->item(i, col)->setBackground(tint);
    }
    fitRows(m_compare);

    // Charts.
    m_regimeChart->setTitle({{QString("%1 · %2").arg(m_sym, posterior ? tr("price coloured by the Viterbi path (a posteriori)")
                                                                        : tr("price coloured by the filtered regime")),
                              c.Text}});
    m_regimeChart->setData(r.dates, r.price, regime, prob, labels, colors);
    QVector<QVector<double>> series(K, QVector<double>(n));
    for (int t = 0; t < n; ++t)
        for (int k = 0; k < K; ++k)
            series[k][t] = probs(t, k);
    m_probChart->setTitle({{posterior ? tr("Smoothed probabilities P(state | whole sample) — a posteriori")
                                      : tr("Filtered probabilities P(state | data up to the day)"), c.Text}});
    m_probChart->setData(r.dates, series, labels, colors);

    // Transition matrix.
    QStringList heads {tr("From \\ To")};
    heads << labels;
    m_transition->setColumnCount(heads.size());
    m_transition->setHorizontalHeaderLabels(heads);
    for (int col = 0; col < heads.size(); ++col)
        m_transition->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    m_transition->setRowCount(K);
    for (int i = 0; i < K; ++i) {
        m_transition->setItem(i, 0, cell(labels[i], colors[i], true));
        for (int j = 0; j < K; ++j) {
            auto *it = cell(QString::number(100 * p.transition(i, j), 'f', 1) + "%", i == j ? c.Text : c.TextDim, i == j);
            if (i == j) {
                QColor t = colors[i];
                t.setAlpha(35);
                it->setBackground(t);
            }
            m_transition->setItem(i, j + 1, it);
        }
    }
    fitRows(m_transition);

    // Regimes table.
    QVector<double> share(K, 0.0);
    for (int t = 0; t < n; ++t)
        for (int k = 0; k < K; ++k)
            share[k] += r.filtered(t, k);
    m_statesTable->setRowCount(K);
    for (int k = 0; k < K; ++k) {
        const double vol = p.volatility(k);
        m_statesTable->setItem(k, 0, cell(labels[k], colors[k], true));
        m_statesTable->setItem(k, 1, cell(num(p.means[k][0], 3) + "%", changeColor(p.means[k][0])));
        m_statesTable->setItem(k, 2, cell(num(vol, 3) + "%"));
        m_statesTable->setItem(k, 3, cell(num(vol * std::sqrt(252.0), 1) + "%"));
        const double d = p.expectedDuration(k);
        m_statesTable->setItem(k, 4, cell(std::isfinite(d) ? tr("%1 days").arg(num(d, 1)) : "∞"));
        m_statesTable->setItem(k, 5, cell(num(100 * share[k] / n, 1) + "%"));
    }
    fitRows(m_statesTable);
}

} // namespace quant
