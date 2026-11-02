#include "QuantViews.h"

#include "QuantMarketState.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace quant {

namespace {

QColor yColor() { return research::palette::pink(); }
QColor xColor() { return research::palette::cyan(); }

QString num(double v, int decimals = 3)
{
    return ok(v) ? QString::number(v, 'f', decimals) : QString("—");
}

QString pct(double v, int decimals = 3)
{
    return ok(v) ? QString::number(v, 'f', decimals) + "%" : QString("—");
}

// "Label ............ value" inside a card.
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

QString equation(double alpha, double beta)
{
    if (!ok(alpha) || !ok(beta))
        return {};
    return QString("y = %1x %2 %3").arg(QString::number(beta, 'f', 3), alpha < 0 ? "-" : "+", QString::number(std::fabs(alpha), 'f', 3));
}

} // namespace

// ---------------------------------------------------------------- CorrelationView

CorrelationView::CorrelationView(QWidget *parent)
    : View(parent)
{
    QSettings settings;
    QHBoxLayout *h;
    auto *top = bar(&h);
    h->addWidget(segments({tr("1 Year"), tr("6 Month"), tr("3 Month"), tr("1 Month")}, &m_range));
    h->addSpacing(6);
    h->addWidget(label(tr("Window (days):"), "qtFieldLabel"));
    m_window = new QSpinBox;
    m_window->setObjectName("qtSpin");
    m_window->setRange(5, 250);
    m_window->setValue(std::clamp(settings.value("quant/correlation/window", 20).toInt(), 5, 250));
    m_window->setFixedWidth(64);
    h->addWidget(m_window);
    auto *set = new QPushButton(tr("Set"));
    set->setObjectName("qtSeg");
    set->setCursor(Qt::PointingHandCursor);
    h->addWidget(set);
    h->addSpacing(6);
    h->addWidget(label(tr("Y (buy):"), "qtFieldLabel"));
    m_y = symbolEdit(settings.value("quant/correlation/y", "AAPL").toString(), yColor());
    h->addWidget(m_y);
    h->addWidget(label(tr("X (sell):"), "qtFieldLabel"));
    m_x = symbolEdit(settings.value("quant/correlation/x", "SPY").toString(), xColor());
    h->addWidget(m_x);
    h->addSpacing(6);
    m_minMax = new QCheckBox(tr("Min/Max"));
    m_minMax->setObjectName("qtCheck");
    m_minMax->setChecked(settings.value("quant/correlation/minmax", true).toBool());
    m_minMax->setMinimumWidth(90);
    h->addWidget(m_minMax);
    m_zToggle = new QCheckBox(tr("Z-Score"));
    m_zToggle->setObjectName("qtCheck");
    m_zToggle->setChecked(settings.value("quant/correlation/zscore", true).toBool());
    m_zToggle->setMinimumWidth(90);
    h->addWidget(m_zToggle);
    h->addStretch(1);
    content()->addWidget(top);
    m_state = new MarketStateStrip;
    content()->addWidget(m_state);

    const Palette &c = Theme::current();
    auto *cards = new QHBoxLayout;
    cards->setSpacing(10);
    QVBoxLayout *body;
    QHBoxLayout *head;
    auto *coint = card(tr("Cointegration"), &body, &head);
    m_stationary = badge("—", c.TextDim);
    head->addWidget(m_stationary);
    m_adfP = label("—", "qtCardValue");
    statRow(body, tr("ADF p-value"), m_adfP);
    m_adfStat = label("—", "qtCardValue");
    statRow(body, tr("Test Statistic"), m_adfStat);
    body->addStretch(1);
    cards->addWidget(coint, 1);

    auto *mr = card(tr("Mean Reversion"), &body);
    body->addWidget(label(tr("Half-Life"), "qtCardLabel"));
    auto *hl = new QHBoxLayout;
    hl->setSpacing(6);
    m_halfLife = label("—", "qtBigValue");
    m_halfLife->setStyleSheet(QString("color: %1").arg(research::palette::green().name()));
    hl->addWidget(m_halfLife);
    hl->addWidget(label(tr("days"), "qtCardLabel"), 0, Qt::AlignBottom);
    hl->addStretch(1);
    body->addLayout(hl);
    body->addStretch(1);
    cards->addWidget(mr, 1);

    auto *hedge = card(tr("Hedge Ratio"), &body);
    body->addWidget(label(tr("Beta (β)"), "qtCardLabel"));
    m_beta = label("—", "qtBigValue");
    m_beta->setStyleSheet(QString("color: %1").arg(research::palette::blue().name()));
    body->addWidget(m_beta);
    m_formula = label({}, "qtNote");
    body->addWidget(m_formula);
    body->addStretch(1);
    cards->addWidget(hedge, 1);

    auto *signal = card(tr("Signal"), &body);
    m_z = label("—", "qtMidValue");
    statRow(body, tr("Current Z-Score"), m_z);
    m_signal = badge("—", c.TextDim);
    m_signal->setFixedHeight(26);
    body->addWidget(m_signal);
    body->addStretch(1);
    cards->addWidget(signal, 1);
    content()->addLayout(cards);

    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    auto chartCard = [&](Chart *chart) {
        QVBoxLayout *b;
        auto *f = card({}, &b);
        b->setContentsMargins(4, 4, 4, 4);
        b->setSpacing(0);
        chart->setFixedHeight(280);
        b->addWidget(chart);
        return f;
    };
    m_overlay = new OverlayChart;
    m_spread = new SpreadChart;
    m_zChart = new ZScoreChart;
    m_scatter = new ScatterChart;
    m_scatter->setPointColor(yColor());
    grid->addWidget(chartCard(m_overlay), 0, 0);
    grid->addWidget(chartCard(m_spread), 0, 1);
    m_zPanel = chartCard(m_zChart);
    grid->addWidget(m_zPanel, 1, 0);
    grid->addWidget(chartCard(m_scatter), 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    content()->addLayout(grid);
    content()->addStretch(1);

    m_range->button(std::clamp(settings.value("quant/correlation/range", 0).toInt(), 0, 3))->setChecked(true);
    m_overlay->setMinMax(m_minMax->isChecked());
    m_zPanel->setVisible(m_zToggle->isChecked());

    connect(m_range, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/correlation/range", m_range->checkedId());
        fetch();
    });
    auto applyWindow = [this] {
        QSettings().setValue("quant/correlation/window", m_window->value());
        compute();
    };
    connect(set, &QPushButton::clicked, this, applyWindow);
    connect(m_window, &QSpinBox::editingFinished, this, applyWindow);
    auto symbolsChanged = [this] {
        if (cleanSymbol(m_y->text()) != m_symY || cleanSymbol(m_x->text()) != m_symX)
            fetch();
    };
    connect(m_y, &QLineEdit::editingFinished, this, symbolsChanged);
    connect(m_x, &QLineEdit::editingFinished, this, symbolsChanged);
    connect(m_minMax, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue("quant/correlation/minmax", on);
        m_overlay->setMinMax(on);
    });
    connect(m_zToggle, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue("quant/correlation/zscore", on);
        m_zPanel->setVisible(on);
    });
}

void CorrelationView::load()
{
    fetch();
}

void CorrelationView::fetch()
{
    const QString y = cleanSymbol(m_y->text()), x = cleanSymbol(m_x->text());
    if (y.isEmpty() || x.isEmpty()) {
        setStatus(tr("Enter two symbols."));
        return;
    }
    m_symY = y;
    m_symX = x;
    m_state->setSymbol(y);
    m_y->setText(y);
    m_x->setText(x);
    QSettings().setValue("quant/correlation/y", y);
    QSettings().setValue("quant/correlation/x", x);
    const int gen = ++m_generation;
    m_haveY = m_haveX = false;
    setStatus(tr("Loading…"));
    const QString range = yahooRange(m_range->button(m_range->checkedId())->text());
    auto fetchOne = [this, gen, range](const QString &sym, Prices *into, bool *flag) {
        research::Client::instance()->prices(sym, range, "1d", this, [this, gen, sym, into, flag](const research::Series &s, const QString &err) {
            if (gen != m_generation)
                return;
            into->dates.clear();
            into->close.clear();
            for (const research::Point &p : s) {
                into->dates.push_back(p.date);
                into->close.push_back(p.value);
            }
            *flag = true;
            if (s.isEmpty())
                setStatus(QString("%1: %2").arg(sym, err.isEmpty() ? tr("no prices") : err));
            else if (m_haveY && m_haveX)
                compute();
        });
    };
    fetchOne(y, &m_py, &m_haveY);
    fetchOne(x, &m_px, &m_haveX);
}

void CorrelationView::compute()
{
    if (!m_haveY || !m_haveX)
        return;
    const Palette &c = Theme::current();
    Prices y = m_py, x = m_px;
    align(&y, &x);
    const int n = y.dates.size();
    if (n < 30) {
        setStatus(tr("Not enough overlapping prices (%1 days).").arg(n));
        return;
    }
    setStatus({});
    const int window = m_window->value();
    const Ols hedge = ols(x.close, y.close);
    const double beta = hedge.beta;
    QVector<double> spread(n);
    for (int i = 0; i < n; ++i)
        spread[i] = y.close[i] - beta * x.close[i];
    const Adf a = adf(spread, 1);
    const double hl = halfLife(spread);
    const QVector<double> z = zscore(spread, window), m = sma(spread, window), sd = rollingStd(spread, window);
    QVector<double> upper(n, NaN), lower(n, NaN);
    for (int i = 0; i < n; ++i)
        if (ok(m[i]) && ok(sd[i])) {
            upper[i] = m[i] + 2 * sd[i];
            lower[i] = m[i] - 2 * sd[i];
        }
    const QVector<double> rx = returns(x.close), ry = returns(y.close);
    const Ols rr = ols(rx, ry);
    double zLast = NaN;
    for (int i = n - 1; i >= 0; --i)
        if (ok(z[i])) {
            zLast = z[i];
            break;
        }

    // Cards.
    const bool stationary = ok(a.p) && a.p < 0.05;
    setBadge(m_stationary, ok(a.p) ? (stationary ? tr("STATIONARY") : tr("NON-STATIONARY")) : "—", stationary ? c.Up : c.Down);
    m_adfP->setText(num(a.p));
    m_adfP->setStyleSheet(QString("color: %1").arg(ok(a.p) ? (stationary ? c.Up : c.Down).name() : c.TextDim.name()));
    m_adfP->setToolTip(tr("Augmented Dickey-Fuller test of the spread (constant, %1 lag): below 0.05 the spread is stationary, "
                          "so the pair mean-reverts. MacKinnon's approximate p-value.").arg(a.lags));
    m_adfStat->setText(num(a.stat, 2));
    m_halfLife->setText(ok(hl) ? QString::number(hl, 'f', 1) : tr("∞"));
    m_halfLife->setToolTip(tr("Time for a deviation of the spread to halve, from Δspread regressed on the lagged spread "
                              "(-ln 2 / λ); ∞ when the spread doesn't revert."));
    m_beta->setText(num(beta, 4));
    m_formula->setText(QString("%1 = %2 - %3 × %4").arg(tr("Spread"), m_symY, QString::number(beta, 'f', 2), m_symX));
    m_beta->setToolTip(tr("Least-squares slope of %1 on %2 prices over the range: units of %2 to sell per unit of %1 bought.")
                           .arg(m_symY, m_symX));
    m_z->setText(num(zLast, 2));
    QString sig = "—";
    QColor sigColor = c.TextDim;
    if (ok(zLast)) {
        if (zLast >= 2) { sig = tr("SELL %1 / BUY %2").arg(m_symY, m_symX); sigColor = c.Down; }
        else if (zLast <= -2) { sig = tr("BUY %1 / SELL %2").arg(m_symY, m_symX); sigColor = c.Up; }
        else if (std::fabs(zLast) >= 0.5) { sig = tr("WATCH"); sigColor = research::palette::yellow(); }
        else { sig = tr("NEUTRAL"); sigColor = c.TextDim; }
    }
    setBadge(m_signal, sig, sigColor);
    m_signal->setToolTip(tr("Z-score of the spread against its %1-day mean: beyond ±2σ the spread is stretched (sell it above, "
                            "buy it below), beyond ±0.5σ it is worth watching.").arg(window));

    // Charts.
    m_overlay->setTitle({{QString("%1: %2").arg(m_symY, QString::number(y.close.last(), 'f', 2)), yColor()}, {"   ", {}},
                         {QString("%1: %2").arg(m_symX, QString::number(x.close.last(), 'f', 2)), xColor()}});
    m_overlay->setData(y.dates, m_symY, yColor(), y.close, m_symX, xColor(), x.close);
    m_spread->setTitle({{QString("%1 (%2 - β×%3): %4").arg(tr("Spread"), m_symY, m_symX, QString::number(spread.last(), 'f', 2)),
                         research::palette::green()}});
    m_spread->setData(y.dates, spread, m, upper, lower, window);
    m_zChart->setTitle({{QString("%1: %2").arg(tr("Z-Score"), num(zLast, 2)), research::palette::yellow()}});
    m_zChart->setData(y.dates, z);
    m_scatter->setTitle({{equation(rr.alpha, rr.beta), research::palette::yellow()}});
    m_scatter->setLabels(QString("%1 Returns (%)").arg(m_symX), QString("%1 Returns (%)").arg(m_symY), xColor(), yColor());
    m_scatter->setData(y.dates.mid(1), rx, ry, rr.alpha, rr.beta);
    m_scatter->setStats({{tr("Beta (β)"), num(rr.beta)}, {tr("Alpha (α)"), num(rr.alpha)}, {tr("Pearson R"), num(rr.r)},
                         {tr("R-squared"), num(rr.r2)}, {tr("Std Dev Error"), num(rr.seResid)},
                         {tr("Std Error (α)"), num(rr.seAlpha)}, {tr("Std Error (β)"), num(rr.seBeta)}});
}

// ---------------------------------------------------------------- RiskBetaView

RiskBetaView::RiskBetaView(QWidget *parent)
    : View(parent)
{
    QSettings settings;
    QHBoxLayout *h;
    auto *top = bar(&h);
    h->addWidget(segments({tr("1 Year"), tr("2 Years"), tr("3 Years"), tr("5 Years")}, &m_range, 1));
    h->addSpacing(6);
    h->addWidget(label(tr("Period:"), "qtFieldLabel"));
    h->addWidget(segments({tr("Daily"), tr("Weekly")}, &m_period, 1));
    h->addStretch(1);
    h->addWidget(label(tr("Y (Stock):"), "qtFieldLabel"));
    m_y = symbolEdit(settings.value("quant/beta/y", "AAPL").toString(), yColor());
    h->addWidget(m_y);
    h->addWidget(label(tr("X (Index):"), "qtFieldLabel"));
    m_x = symbolEdit(settings.value("quant/beta/x", "SPY").toString(), xColor());
    h->addWidget(m_x);
    content()->addWidget(top);
    m_state = new MarketStateStrip;
    content()->addWidget(m_state);

    auto *main = new QHBoxLayout;
    main->setSpacing(10);
    QVBoxLayout *body;
    auto *scatterCard = card({}, &body);
    body->setContentsMargins(4, 4, 4, 4);
    body->setSpacing(0);
    m_scatter = new ScatterChart;
    m_scatter->setMarginals(true);
    m_scatter->setPointColor(yColor());
    m_scatter->setFixedHeight(470);
    body->addWidget(m_scatter);
    main->addWidget(scatterCard, 1);

    auto *statsCard = card(tr("Linear Beta Analysis"), &body);
    statsCard->setFixedWidth(260);
    m_stats = new QGridLayout;
    m_stats->setHorizontalSpacing(8);
    m_stats->setVerticalSpacing(0);
    const QStringList keys {tr("Y ="), tr("X ="), {}, tr("Raw BETA"), tr("Adjusted BETA"), tr("ALPHA (Intercept)"), {},
                            tr("R^2 (Correlation^2)"), tr("R (Correlation)"), {}, tr("Std Dev of Y"), tr("Std Dev of X"), {},
                            tr("Std Error of ALPHA"), tr("Std Error of BETA"), {}, tr("t-Test"), tr("Significance"), {},
                            tr("Number of Points")};
    const QStringList tips {{}, {},{},
                            tr("Slope of the stock's returns on the index's: how much it moves for a 1% move of the index."),
                            tr("Blume's adjustment toward 1: 0.67 × raw beta + 0.33, a better forecast of the next period's beta."),
                            tr("Average return of the stock per period when the index is flat."), {},
                            tr("Share of the stock's variance explained by the index."), tr("Pearson correlation of the returns."), {},
                            tr("Standard deviation of the stock's returns per period."), tr("Standard deviation of the index's returns per period."), {},
                            tr("Standard error of the intercept."), tr("Standard error of the slope."), {},
                            tr("Beta over its standard error: above 2 the beta is significantly different from zero."),
                            tr("Two-sided p-value of the t-test: the chance of a slope this large if the true beta were zero."), {},
                            tr("Returns in the regression.")};
    for (int i = 0; i < keys.size(); ++i) {
        if (keys[i].isEmpty()) {
            m_stats->setRowMinimumHeight(i, 8);
            continue;
        }
        auto *k = label(keys[i], "qtCardLabel");
        auto *v = label("—", "qtStatValue");
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!tips.value(i).isEmpty()) {
            k->setToolTip(tips[i]);
            v->setToolTip(tips[i]);
        }
        if (i == 3) {
            QColor tint = Theme::current().Accent;
            tint.setAlpha(35);
            const QString ss = QString("background: %1; padding: 2px 4px;").arg(tint.name(QColor::HexArgb));
            k->setStyleSheet(ss);
            v->setStyleSheet(ss);
        } else {
            k->setStyleSheet("padding: 2px 4px;");
            v->setStyleSheet("padding: 2px 4px;");
        }
        m_stats->addWidget(k, i, 0);
        m_stats->addWidget(v, i, 1);
        m_stats->setRowMinimumHeight(i, 22);
        m_statValues.push_back(v);
    }
    m_statY = m_statValues[0];
    m_statX = m_statValues[1];
    m_statY->setStyleSheet(QString("color: %1; padding: 2px 4px;").arg(yColor().name()));
    m_statX->setStyleSheet(QString("color: %1; padding: 2px 4px;").arg(xColor().name()));
    body->addLayout(m_stats);
    body->addStretch(1);
    main->addWidget(statsCard);
    content()->addLayout(main);

    QHBoxLayout *head;
    auto *perfCard = card(tr("Performance Comparison"), &body, &head);
    head->addWidget(segments({tr("Performance"), tr("Spread")}, &m_perfMode));
    m_perf = new MultiLineChart;
    m_perf->setFillBetween(true);
    m_perf->setFixedHeight(300);
    body->addWidget(m_perf);
    content()->addWidget(perfCard);
    content()->addStretch(1);

    m_range->button(std::clamp(settings.value("quant/beta/range", 1).toInt(), 0, 3))->setChecked(true);
    m_period->button(std::clamp(settings.value("quant/beta/period", 1).toInt(), 0, 1))->setChecked(true);

    connect(m_range, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/beta/range", m_range->checkedId());
        fetch();
    });
    connect(m_period, &QButtonGroup::idClicked, this, [this] {
        QSettings().setValue("quant/beta/period", m_period->checkedId());
        compute();
    });
    connect(m_perfMode, &QButtonGroup::idClicked, this, [this] { compute(); });
    auto symbolsChanged = [this] {
        if (cleanSymbol(m_y->text()) != m_symY || cleanSymbol(m_x->text()) != m_symX)
            fetch();
    };
    connect(m_y, &QLineEdit::editingFinished, this, symbolsChanged);
    connect(m_x, &QLineEdit::editingFinished, this, symbolsChanged);
}

void RiskBetaView::load()
{
    fetch();
}

void RiskBetaView::fetch()
{
    const QString y = cleanSymbol(m_y->text()), x = cleanSymbol(m_x->text());
    if (y.isEmpty() || x.isEmpty()) {
        setStatus(tr("Enter two symbols."));
        return;
    }
    m_symY = y;
    m_symX = x;
    m_state->setSymbol(y);
    m_y->setText(y);
    m_x->setText(x);
    QSettings().setValue("quant/beta/y", y);
    QSettings().setValue("quant/beta/x", x);
    const int gen = ++m_generation;
    m_haveY = m_haveX = false;
    setStatus(tr("Loading…"));
    const QString range = yahooRange(m_range->button(m_range->checkedId())->text());
    auto fetchOne = [this, gen, range](const QString &sym, Prices *into, bool *flag) {
        research::Client::instance()->prices(sym, range, "1d", this, [this, gen, sym, into, flag](const research::Series &s, const QString &err) {
            if (gen != m_generation)
                return;
            into->dates.clear();
            into->close.clear();
            for (const research::Point &p : s) {
                into->dates.push_back(p.date);
                into->close.push_back(p.value);
            }
            *flag = true;
            if (s.isEmpty())
                setStatus(QString("%1: %2").arg(sym, err.isEmpty() ? tr("no prices") : err));
            else if (m_haveY && m_haveX)
                compute();
        });
    };
    fetchOne(y, &m_py, &m_haveY);
    fetchOne(x, &m_px, &m_haveX);
}

void RiskBetaView::compute()
{
    if (!m_haveY || !m_haveX)
        return;
    Prices y = m_py, x = m_px;
    align(&y, &x);
    if (y.dates.size() < 30) {
        setStatus(tr("Not enough overlapping prices (%1 days).").arg(y.dates.size()));
        return;
    }
    setStatus({});
    const bool weeklyPeriod = m_period->checkedId() == 1;
    const Prices py = weeklyPeriod ? weekly(y) : y, px = weeklyPeriod ? weekly(x) : x;
    const QVector<double> rx = returns(px.close), ry = returns(py.close);
    const Ols o = ols(rx, ry);
    m_statY->setText(m_symY);
    m_statX->setText(m_symX);
    const QStringList values {{}, {}, {}, num(o.beta), num(ok(o.beta) ? 0.67 * o.beta + 0.33 : NaN), pct(o.alpha), {},
                              num(o.r2), num(o.r), {}, pct(o.sdY), pct(o.sdX), {}, num(o.seAlpha), num(o.seBeta), {},
                              num(o.t), num(o.p), {}, QString::number(o.n)};
    int v = 1;   // m_statValues: Y, X, then the figures in the keys' order
    for (int i = 3; i < values.size(); ++i)
        if (!values[i].isEmpty())
            m_statValues[++v]->setText(values[i]);
    m_scatter->setLabels(QString("%1 Returns (%)").arg(m_symX), QString("%1 Returns (%)").arg(m_symY), xColor(), yColor());
    m_scatter->setData(py.dates.mid(1), rx, ry, o.alpha, o.beta);

    // Cumulative performance since the start of the range, daily.
    const int n = y.dates.size();
    QVector<double> perfY(n), perfX(n), diff(n);
    for (int i = 0; i < n; ++i) {
        perfY[i] = 100 * (y.close[i] / y.close[0] - 1);
        perfX[i] = 100 * (x.close[i] / x.close[0] - 1);
        diff[i] = perfY[i] - perfX[i];
    }
    const Format fmt = [](double v) { return QString::number(v, 'f', 1) + "%"; };
    if (m_perfMode->checkedId() == 0) {
        m_perf->setFillBetween(true);
        m_perf->setSignedArea(false);
        m_perf->setData(y.dates, {{m_symY, yColor(), perfY}, {m_symX, xColor(), perfX}}, fmt);
    } else {
        m_perf->setFillBetween(false);
        m_perf->setSignedArea(true);
        m_perf->setData(y.dates, {{QString("%1 − %2").arg(m_symY, m_symX), research::palette::yellow(), diff}}, fmt);
    }
}

} // namespace quant
