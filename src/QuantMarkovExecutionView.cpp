#include "QuantMarkovViews.h"

#include "QuantMarkovJobs.h"
#include "QuantMarketState.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
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

QString num(double v, int decimals = 2)
{
    return ok(v) ? QString::number(v, 'f', decimals) : QString("—");
}

QString bps(double v)
{
    return ok(v) ? QString::number(v, 'f', 1) + " bp" : QString("—");
}

QString shares(double v)
{
    return ok(v) ? QLocale(QLocale::English).toString(v, 'f', 0) : QString("—");
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

QDoubleSpinBox *spin(double lo, double hi, double value, int decimals, double step, int width = 90)
{
    auto *s = new QDoubleSpinBox;
    s->setObjectName("qtSpin");
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setSingleStep(step);
    s->setValue(value);
    s->setFixedWidth(width);
    return s;
}

QColor strategyColor(const QString &name)
{
    if (name == "TWAP") return Theme::current().TextDim;
    if (name == "VWAP") return research::palette::cyan();
    if (name == "Almgren-Chriss") return research::palette::yellow();
    if (name.startsWith("MDP")) return research::palette::pink();
    if (name.startsWith("Q-learning")) return research::palette::purple();
    return research::palette::green();
}

} // namespace

ExecutionView::ExecutionView(QWidget *parent)
    : View(parent)
{
    QSettings settings;
    const Palette &c = Theme::current();
    auto v = [&](const char *key, double d) { return settings.value(QString("quant/markov/exec/") + key, d).toDouble(); };
    QHBoxLayout *h;
    auto *top = bar(&h);
    m_symbol = symbolEdit(settings.value("quant/markov/exec/symbol", "AAPL").toString(), c.Accent);
    h->addWidget(m_symbol);
    h->addWidget(label(tr("Quantity:"), "qtFieldLabel"));
    m_quantity = spin(1, 1e9, v("quantity", 500000), 0, 10000, 110);
    m_quantity->setGroupSeparatorShown(true);
    h->addWidget(m_quantity);
    h->addWidget(label(tr("Horizon (days):"), "qtFieldLabel"));
    m_horizon = spin(0.05, 60, v("horizon", 1), 2, 0.5, 70);
    h->addWidget(m_horizon);
    h->addWidget(label(tr("Intervals:"), "qtFieldLabel"));
    m_intervals = new QSpinBox;
    m_intervals->setObjectName("qtSpin");
    m_intervals->setRange(2, 200);
    m_intervals->setValue(settings.value("quant/markov/exec/intervals", 13).toInt());
    m_intervals->setFixedWidth(60);
    h->addWidget(m_intervals);
    h->addWidget(label(tr("λ (×1e-6):"), "qtFieldLabel"));
    m_lambda = spin(0, 1000, v("lambda", 2), 2, 0.5, 70);
    m_lambda->setToolTip(tr("Risk aversion of the mean-variance objective E + λ V, in 1 / currency: 0 is TWAP, more sells sooner."));
    h->addWidget(m_lambda);
    h->addWidget(label(tr("Temp. impact:"), "qtFieldLabel"));
    m_temporary = spin(0, 1000, v("temporary", 10), 1, 1, 70);
    m_temporary->setToolTip(tr("Temporary impact in basis points of the price for trading at 1% of the average daily volume per day."));
    h->addWidget(m_temporary);
    h->addWidget(label(tr("Perm. impact:"), "qtFieldLabel"));
    m_permanent = spin(0, 1000, v("permanent", 2.5), 1, 0.5, 70);
    m_permanent->setToolTip(tr("Permanent impact in basis points per 1% of the average daily volume traded."));
    h->addWidget(m_permanent);
    h->addWidget(label(tr("½ spread (bp):"), "qtFieldLabel"));
    m_spread = spin(0, 500, v("spread", 1), 1, 0.5, 60);
    h->addWidget(m_spread);
    h->addStretch(1);
    content()->addWidget(top);
    auto *second = bar(&h);
    h->addWidget(label(tr("Paths:"), "qtFieldLabel"));
    m_paths = new QSpinBox;
    m_paths->setObjectName("qtSpin");
    m_paths->setRange(100, 100000);
    m_paths->setValue(settings.value("quant/markov/exec/paths", 2000).toInt());
    m_paths->setFixedWidth(70);
    h->addWidget(m_paths);
    h->addWidget(label(tr("Impact:"), "qtFieldLabel"));
    h->addWidget(segments({tr("Linear"), tr("√"), }, &m_impact, settings.value("quant/markov/exec/impact", 0).toInt()));
    m_impact->button(1)->setToolTip(tr("Square-root impact: η (n / V)^½, concave in the participation; Almgren-Chriss is then linearized at the TWAP pace."));
    h->addWidget(label(tr("Regimes:"), "qtFieldLabel"));
    h->addWidget(segments({tr("None"), "HMM", "MS-GARCH"}, &m_regimes, std::clamp(settings.value("quant/markov/exec/regimes", 1).toInt(), 0, 2)));
    m_regimes->button(1)->setToolTip(tr("Two volatility regimes from the HMM (module 1), read from the shared state when it is current, else "
                                        "estimated on two years of daily returns and published."));
    m_regimes->button(2)->setToolTip(tr("Two regimes from the MS-GARCH (module 2): each regime's next-day volatility and the transition matrix."));
    m_agent = new QCheckBox(tr("Q-learning"));
    m_agent->setObjectName("qtCheck");
    m_agent->setMinimumWidth(100);
    m_agent->setChecked(settings.value("quant/markov/exec/agent", false).toBool());
    m_agent->setToolTip(tr("Train a tabular Q-learning agent on the simulator (model-free) and compare it."));
    h->addWidget(m_agent);
    m_episodes = new QSpinBox;
    m_episodes->setObjectName("qtSpin");
    m_episodes->setRange(1000, 2000000);
    m_episodes->setSingleStep(10000);
    m_episodes->setValue(settings.value("quant/markov/exec/episodes", 100000).toInt());
    m_episodes->setFixedWidth(90);
    m_episodes->setToolTip(tr("Training episodes"));
    h->addWidget(m_episodes);
    m_run = new QPushButton(tr("Run"));
    m_run->setObjectName("qtSeg");
    m_run->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_run);
    h->addStretch(1);
    content()->addWidget(second);
    m_state = new MarketStateStrip;
    content()->addWidget(m_state);

    m_runner = new JobRunner(this);
    content()->addWidget(new JobBar(m_runner));

    auto *cards = new QHBoxLayout;
    cards->setSpacing(10);
    QVBoxLayout *body;
    auto addStats = [&](QVBoxLayout *b, const QStringList &keys, QLabel **values) {
        auto *g = new QGridLayout;
        g->setHorizontalSpacing(8);
        g->setVerticalSpacing(3);
        for (int i = 0; i < keys.size(); ++i) {
            g->addWidget(label(keys[i], "qtCardLabel"), i, 0);
            values[i] = label("—", "qtStatValue");
            values[i]->setAlignment(Qt::AlignRight);
            g->addWidget(values[i], i, 1);
        }
        b->addLayout(g);
    };
    auto *market = card(tr("Market"), &body);
    addStats(body, {tr("Decision price"), tr("Average daily volume"), tr("Daily volatility"), tr("Order / ADV")}, m_marketValues);
    body->addStretch(1);
    cards->addWidget(market, 1);
    auto *ac = card(tr("Almgren-Chriss"), &body);
    addStats(body, {tr("κ (urgency, per day)"), tr("Half-life of the schedule"), tr("Expected cost"), tr("Risk (√V)"), tr("Objective E + λV")}, m_acValues);
    body->addStretch(1);
    cards->addWidget(ac, 1);
    auto *mdp = card(tr("MDP"), &body);
    addStats(body, {tr("Grid"), tr("Objective at the start"), tr("vs Almgren-Chriss"), tr("Regimes")}, m_mdpValues);
    body->addWidget(label(tr("Dynamic programming on (intervals left × shares left × regime); same costs as Almgren-Chriss."), "qtNote"));
    body->addStretch(1);
    cards->addWidget(mdp, 1);
    content()->addLayout(cards);

    QVBoxLayout *tb;
    auto *summary = card(tr("Monte Carlo · implementation shortfall"), &tb);
    m_summary = grid({tr("Strategy"), tr("Mean cost"), tr("Std dev"), tr("P95"), tr("Analytic E"), tr("Mean cost (currency)")});
    tb->addWidget(m_summary);
    tb->addWidget(label(tr("Shortfall against the decision price, in basis points of the order's value; every strategy on the same random paths."), "qtNote"));
    content()->addWidget(summary);

    auto chartCard = [&](Chart *chart, int height) {
        QVBoxLayout *b;
        auto *f = card({}, &b);
        b->setContentsMargins(4, 4, 4, 4);
        b->setSpacing(0);
        chart->setFixedHeight(height);
        b->addWidget(chart);
        return f;
    };
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    m_trajectories = new MultiLineChart;
    m_trajectories->setZeroLine(false);
    m_trajectories->setEndLabels(false);
    m_trajectories->setLegend(true);
    m_trajectories->setTitle({{tr("Liquidation trajectories · shares left, % of the order"), c.Text}});
    row->addWidget(chartCard(m_trajectories, 300), 1);
    m_histogram = new HistogramChart;
    m_histogram->setTitle({{tr("Simulated costs · basis points"), c.Text}});
    row->addWidget(chartCard(m_histogram, 300), 1);
    content()->addLayout(row);
    content()->addStretch(1);

    connect(m_run, &QPushButton::clicked, this, &ExecutionView::run);
    connect(m_symbol, &QLineEdit::returnPressed, m_run, &QPushButton::click);
}

void ExecutionView::load()
{
    run();
}

void ExecutionView::run()
{
    const QString sym = cleanSymbol(m_symbol->text());
    if (sym.isEmpty()) {
        setStatus(tr("Enter a symbol."));
        return;
    }
    m_sym = sym;
    m_symbol->setText(sym);
    m_state->setSymbol(sym);
    QSettings settings;
    settings.setValue("quant/markov/exec/symbol", sym);
    settings.setValue("quant/markov/exec/quantity", m_quantity->value());
    settings.setValue("quant/markov/exec/horizon", m_horizon->value());
    settings.setValue("quant/markov/exec/intervals", m_intervals->value());
    settings.setValue("quant/markov/exec/lambda", m_lambda->value());
    settings.setValue("quant/markov/exec/temporary", m_temporary->value());
    settings.setValue("quant/markov/exec/permanent", m_permanent->value());
    settings.setValue("quant/markov/exec/spread", m_spread->value());
    settings.setValue("quant/markov/exec/paths", m_paths->value());
    settings.setValue("quant/markov/exec/regimes", m_regimes->checkedId());
    settings.setValue("quant/markov/exec/impact", m_impact->checkedId());
    settings.setValue("quant/markov/exec/agent", m_agent->isChecked());
    settings.setValue("quant/markov/exec/episodes", m_episodes->value());
    const int gen = ++m_generation;
    setStatus(tr("Loading prices…"));
    // Three months of daily candles for the price, the volume and the volatility; two years of closes for the
    // regimes when the shared state has none; a month of 30-minute candles for the intraday volume profile.
    struct Data {
        QVector<research::Bar> daily;
        research::Series closes;
        QVector<research::Bar> intraday;
        int left = 3;
    };
    auto data = std::make_shared<Data>();
    const int regimeMode = m_regimes->checkedId();   // 0 none, 1 HMM, 2 MS-GARCH
    const QString source = regimeMode == 1 ? "HMM" : regimeMode == 2 ? "MS-GARCH" : QString();
    // A current record of the shared state (as of the last trading day or later) is reused instead of a refit.
    std::optional<RegimeInfo> stored;
    if (regimeMode > 0)
        if (auto r = MarketState::instance()->regime(sym, source); r && r->states == 2 && r->transition.size() == 4
                                                                    && r->asOf.daysTo(QDate::currentDate()) <= 4)
            stored = r;
    auto ready = [this, gen, sym, data, regimeMode, source, stored] {
        if (gen != m_generation || --data->left > 0)
            return;
        if (data->daily.size() < 20) {
            setStatus(QString("%1: %2").arg(sym, tr("not enough prices")));
            return;
        }
        setStatus({});
        const double quantity = m_quantity->value(), horizon = m_horizon->value(), lambda = m_lambda->value() * 1e-6,
                     tempBps = m_temporary->value(), permBps = m_permanent->value(), spreadBps = m_spread->value();
        const int intervals = m_intervals->value(), paths = m_paths->value(), episodes = m_episodes->value();
        const bool sqrtImpact = m_impact->checkedId() == 1, agent = m_agent->isChecked();
        const Data d = *data;
        m_runner->start<ExecResult>(
            [=](const markov::Progress &progress) {
                ExecResult r;
                // Market figures from the last three months.
                const double price = d.daily.last().close;
                double adv = 0;
                int nv = 0;
                QVector<double> closes;
                for (const research::Bar &b : d.daily) {
                    if (ok(b.volume) && b.volume > 0) {
                        adv += b.volume;
                        ++nv;
                    }
                    closes.push_back(b.close);
                }
                adv = nv > 0 ? adv / nv : NaN;
                const QVector<double> ret = returns(closes);
                r.dailyVol = stdev(ret);
                r.adv = adv;
                r.market = tr("Yahoo daily candles, last %1 days").arg(d.daily.size());
                markov::ExecutionParams &p = r.params;
                p.quantity = quantity;
                p.price = price;
                p.horizon = horizon;
                p.intervals = intervals;
                p.sigma = price * r.dailyVol / 100;
                p.lambda = lambda;
                p.adv = adv;
                p.exponent = sqrtImpact ? 0.5 : 1.0;
                p.volumeNoise = 0.35;
                // Impact coefficients: basis points of the price at a participation of 1% of ADV (per day of trading at
                // that pace for the temporary one, per 1% of ADV traded for the permanent one), in Almgren-Chriss units.
                if (!ok(adv) || adv <= 0) {
                    r.error = tr("No volume data for this symbol: the impact can't be scaled.");
                    return r;
                }
                const double perShare = price * 1e-4 / (0.01 * adv);
                p.temporary = tempBps * perShare / (sqrtImpact ? std::sqrt(0.01) / 0.01 : 1.0);
                p.permanent = permBps * perShare;
                p.spread = spreadBps * 1e-4 * price;
                // The expected volume profile over the horizon: the average intraday shape repeated each day.
                QVector<double> shape(13, 1.0 / 13);
                if (!d.intraday.isEmpty()) {
                    QHash<int, double> bySlot;
                    QHash<QDate, double> byDay;
                    for (const research::Bar &b : d.intraday)
                        if (ok(b.volume))
                            byDay[b.time.toLocalTime().date()] += b.volume;
                    for (const research::Bar &b : d.intraday) {
                        const QDateTime lt = b.time.toLocalTime();
                        const double dayVol = byDay.value(lt.date());
                        if (ok(b.volume) && dayVol > 0)
                            bySlot[lt.time().hour() * 2 + lt.time().minute() / 30] += b.volume / dayVol;
                    }
                    QList<int> bins = bySlot.keys();
                    std::sort(bins.begin(), bins.end());
                    if (bins.size() >= 5) {
                        shape = QVector<double>(bins.size());
                        double total = 0;
                        for (int i = 0; i < bins.size(); ++i)
                            total += bySlot[bins[i]];
                        for (int i = 0; i < bins.size(); ++i)
                            shape[i] = bySlot[bins[i]] / total;
                        r.vwapProfile = true;
                    }
                }
                // Each interval's expected share: the shape sampled along the horizon in days, the same volume every day.
                p.profile = markov::Vec(intervals, 0.0);
                for (int k = 0; k < intervals; ++k) {
                    double share = 0;
                    const int samples = 40;
                    for (int j = 0; j < samples; ++j) {
                        const double t = horizon * (k + (j + 0.5) / samples) / intervals;   // in days
                        const double u = t - std::floor(t);                                   // within the day
                        share += shape[std::min(int(shape.size()) - 1, int(u * shape.size()))] * shape.size();
                    }
                    p.profile[k] = share / samples;
                }
                {
                    double sum = 0;
                    for (double v : p.profile) sum += v;
                    for (double &v : p.profile) v /= sum;
                }
                if (!p.isValid()) {
                    r.error = tr("Invalid parameters: the temporary impact must exceed half the permanent impact in every interval.");
                    return r;
                }
                // Regimes: the shared state's record, or a fresh estimation (published afterwards by the view).
                if (regimeMode > 0) {
                    RegimeInfo info;
                    if (stored) {
                        info = *stored;
                        r.regimeFromState = true;
                    } else if (d.closes.size() > 250) {
                        Prices pr;
                        for (const research::Point &pt : d.closes) {
                            pr.dates.push_back(pt.date);
                            pr.close.push_back(pt.value);
                        }
                        markov::Progress sub;
                        sub.cancel = progress.cancel;
                        sub.report = [&](int pct, const QString &text) { progress(pct / 4, QString("%1 · %2").arg(source, text)); };
                        if (regimeMode == 1) {
                            HmmSpec hs;
                            hs.range = "2y";
                            hs.states = 2;
                            hs.compareK = false;
                            // One estimation: the fit, and the filtered probabilities regimeInfo needs.
                            const HmmResult full = runHmm(pr, hs, sub);
                            r.hmm = full.fit;
                            info = regimeInfo(sym, full, hs);
                        } else {
                            GarchSpec gs;
                            gs.range = "2y";
                            gs.regimes = 2;
                            gs.benchmark = false;
                            info = regimeInfo(sym, runGarch(pr, gs, sub), gs);
                        }
                    }
                    if (info.isValid() && info.states == 2 && info.transition.size() == 4) {
                        r.regimes = true;
                        r.regimeInfo = info;
                        r.regimeSource = info.source;
                        r.regimeModel.sigma = {price * info.vols[0] / 100, price * info.vols[1] / 100};
                        markov::Mat daily(2, 2);
                        daily.a = info.transition;
                        r.regimeModel.transition = markov::RegimeModel::perInterval(daily, p.tau());
                        r.regimeModel.initial = {info.probabilities.value(0, 0.5), info.probabilities.value(1, 0.5)};
                        r.currentRegime = info.regime;
                    }
                }
                if (progress.cancelled())
                    return r;
                // Schedules.
                r.schedules.push_back(markov::twap(p));
                r.schedules.push_back(markov::vwap(p, p.profile));
                r.schedules.push_back(markov::almgrenChriss(p));
                r.levels = 300;
                markov::Progress sub;
                sub.cancel = progress.cancel;
                sub.report = [&](int pct, const QString &text) { progress(25 + pct / 8, text); };
                const auto mdp = markov::MdpPolicy::solve(p, r.levels, r.regimes ? &r.regimeModel : nullptr, sub);
                if (mdp->cancelled)
                    return r;
                r.mdpValue = mdp->value(intervals, quantity, r.currentRegime);
                const int R = r.regimes ? 2 : 1;
                for (int k = 0; k < R; ++k) {
                    markov::Trajectory t = mdp->trajectory(k);
                    if (R > 1)
                        t.name = QString("MDP · %1").arg(regimeLabels(2)[k]);
                    r.schedules.push_back(t);
                }
                QVector<std::shared_ptr<markov::Policy>> policies;
                for (const markov::Trajectory &t : std::as_const(r.schedules))
                    if (!t.name.startsWith("MDP"))
                        policies.push_back(std::make_shared<markov::SchedulePolicy>(t));
                policies.push_back(mdp);
                if (agent) {
                    markov::QLearningPolicy::Options o;
                    o.episodes = episodes;
                    sub.report = [&](int pct, const QString &text) { progress(38 + pct / 4, text); };
                    const auto q = markov::QLearningPolicy::train(p, r.regimes ? &r.regimeModel : nullptr, o, sub);
                    if (q->cancelled)
                        return r;
                    r.learningCurve = q->learningCurve();
                    r.episodes = q->episodes;
                    for (int k = 0; k < R; ++k) {
                        markov::Trajectory t = q->trajectory(k);
                        if (R > 1)
                            t.name = QString("Q-learning · %1").arg(regimeLabels(2)[k]);
                        r.schedules.push_back(t);
                    }
                    policies.push_back(q);
                }
                sub.report = [&](int pct, const QString &text) { progress(63 + pct * 37 / 100, text); };
                r.paths = paths;
                r.results = markov::simulate(p, policies, paths, 7, r.regimes ? &r.regimeModel : nullptr, sub);
                return r;
            },
            [this, sym](const ExecResult &r) {
                if (sym != m_sym)
                    return;
                if (!r.error.isEmpty()) {
                    setStatus(r.error);
                    return;
                }
                if (r.results.isEmpty())
                    return;
                if (r.regimes && !r.regimeFromState)
                    MarketState::instance()->publishRegime(r.regimeInfo);
                show(r);
            });
    };
    research::Client::instance()->candles(sym, "3mo", "1d", this, [data, ready](const QVector<research::Bar> &bars, const QString &) {
        data->daily = bars;
        ready();
    });
    if (regimeMode > 0 && !stored)
        research::Client::instance()->prices(sym, "2y", "1d", this, [data, ready](const research::Series &s, const QString &) {
            data->closes = s;
            ready();
        });
    else
        ready();
    research::Client::instance()->candles(sym, "1mo", "30m", this, [data, ready](const QVector<research::Bar> &bars, const QString &) {
        data->intraday = bars;
        ready();
    });
}

void ExecutionView::show(const ExecResult &r)
{
    const Palette &c = Theme::current();
    const markov::ExecutionParams &p = r.params;
    const double value = p.quantity * p.price;
    auto toBps = [value](double currency) { return 1e4 * currency / value; };
    const QStringList m {QString::number(p.price, 'f', 2), shares(r.adv), num(r.dailyVol) + "% " + tr("(%1% ann.)").arg(num(r.dailyVol * 15.87, 1)),
                         num(100 * p.quantity / r.adv, 1) + "%"};
    for (int i = 0; i < 4; ++i)
        m_marketValues[i]->setText(m[i]);
    m_marketValues[0]->setToolTip(r.market);
    const markov::Trajectory *ac = nullptr;
    for (const markov::Trajectory &t : r.schedules)
        if (t.name == "Almgren-Chriss")
            ac = &t;
    if (ac) {
        const double k = markov::kappa(p);
        const QStringList a {num(k, 3), ok(k) && k > 0 ? tr("%1 days").arg(num(std::log(2.0) / k, 2)) : "—", bps(toBps(ac->expectedCost)),
                             bps(toBps(std::sqrt(ac->variance))), bps(toBps(ac->objective))};
        for (int i = 0; i < 5; ++i)
            m_acValues[i]->setText(a[i]);
    }
    const QStringList md {tr("%1 intervals × %2 levels%3").arg(p.intervals).arg(r.levels).arg(r.regimes ? tr(" × 2 regimes") : QString()),
                          bps(toBps(r.mdpValue)), ac ? QString("%1 bp").arg(num(toBps(r.mdpValue - ac->objective), 2)) : "—",
                          r.regimes ? tr("%1 now (%2%) · vols %3% / %4% · P(stay) %5% / %6% · %7%8")
                                          .arg(regimeLabels(2)[r.currentRegime])
                                          .arg(num(100 * r.regimeModel.initial[r.currentRegime], 0))
                                          .arg(num(r.regimeInfo.vols.value(0)), num(r.regimeInfo.vols.value(1)),
                                               num(100 * r.regimeInfo.transition.value(0), 1), num(100 * r.regimeInfo.transition.value(3), 1),
                                               r.regimeSource, r.regimeFromState ? tr(" (shared state)") : tr(" (estimated now)"))
                                    : tr("none")};
    for (int i = 0; i < 4; ++i)
        m_mdpValues[i]->setText(md[i]);

    // Summary.
    m_summary->setRowCount(r.results.size());
    double bestMean = INFINITY;
    for (const markov::SimulationResult &s : r.results)
        bestMean = std::min(bestMean, s.meanBps);
    for (int i = 0; i < r.results.size(); ++i) {
        const markov::SimulationResult &s = r.results[i];
        const bool best = s.meanBps == bestMean;
        m_summary->setItem(i, 0, cell(s.name, strategyColor(s.name), true));
        m_summary->setItem(i, 1, cell(bps(s.meanBps), best ? c.Up : c.Text, best));
        m_summary->setItem(i, 2, cell(bps(s.stdBps)));
        m_summary->setItem(i, 3, cell(bps(s.p95Bps)));
        m_summary->setItem(i, 4, cell(ok(s.expectedCostBps) ? bps(s.expectedCostBps) : tr("path-dependent"), c.TextDim));
        if (s.name == "VWAP" && !r.vwapProfile)
            m_summary->item(i, 0)->setToolTip(tr("No intraday volume data: flat profile, same as TWAP."));
        if (s.name == "Q-learning")
            m_summary->item(i, 0)->setToolTip(tr("Trained on %1 episodes; mean cost per block fell from %2 to %3 bp.")
                                                  .arg(r.episodes).arg(num(r.learningCurve.value(0), 1), num(r.learningCurve.isEmpty() ? NaN : r.learningCurve.last(), 1)));
        m_summary->setItem(i, 5, cell(QLocale(QLocale::English).toString(s.meanBps * value / 1e4, 'f', 0)));
    }
    fitRows(m_summary);

    // Trajectories, as % of the order.
    QStringList labels;
    for (int k = 0; k <= p.intervals; ++k)
        labels << QString::number(k);
    QVector<MultiLineChart::Line> lines;
    for (const markov::Trajectory &t : r.schedules) {
        MultiLineChart::Line l {t.name, strategyColor(t.name), {}};
        for (double hld : t.holdings)
            l.values.push_back(100 * hld / p.quantity);
        if (t.name.endsWith(regimeLabels(2)[1]))
            l.color = t.name.startsWith("MDP") ? c.Down : research::palette::orange();
        lines.push_back(l);
    }
    m_trajectories->setXLabels(labels);
    m_trajectories->setData({}, lines, [](double v) { return QString::number(v, 'f', 0) + "%"; });
    QVector<HistogramChart::Sample> samples;
    for (const markov::SimulationResult &s : r.results)
        samples.push_back({s.name, strategyColor(s.name), s.costsBps});
    m_histogram->setData(samples, 50, [](double v) { return QString::number(v, 'f', 0); });
}

} // namespace quant
