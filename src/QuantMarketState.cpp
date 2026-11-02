#include "QuantMarketState.h"

#include "QuantJob.h"
#include "QuantMarkovJobs.h"
#include "QuantMarkovViews.h"
#include "QuantViews.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QCoreApplication>
#include <QSettings>

namespace quant {

namespace {

QJsonArray toArray(const QVector<double> &v)
{
    QJsonArray a;
    for (double x : v)
        a.push_back(x);
    return a;
}

QVector<double> fromArray(const QJsonArray &a)
{
    QVector<double> v;
    for (const QJsonValue &x : a)
        v.push_back(x.toDouble());
    return v;
}

QJsonObject toJson(const RegimeInfo &r)
{
    return {{"symbol", r.symbol}, {"source", r.source}, {"states", r.states}, {"regime", r.regime}, {"label", r.label},
            {"probability", r.probability}, {"dailyVol", r.dailyVol}, {"vols", toArray(r.vols)}, {"probabilities", toArray(r.probabilities)},
            {"transition", toArray(r.transition)}, {"asOf", r.asOf.toString(Qt::ISODate)}, {"observations", r.observations},
            {"computedAt", r.computedAt.toString(Qt::ISODate)}, {"spec", r.spec}};
}

RegimeInfo regimeFrom(const QJsonObject &o)
{
    RegimeInfo r;
    r.symbol = o.value("symbol").toString();
    r.source = o.value("source").toString();
    r.states = o.value("states").toInt();
    r.regime = o.value("regime").toInt();
    r.label = o.value("label").toString();
    r.probability = o.value("probability").toDouble();
    r.dailyVol = o.value("dailyVol").toDouble();
    r.vols = fromArray(o.value("vols").toArray());
    r.probabilities = fromArray(o.value("probabilities").toArray());
    r.transition = fromArray(o.value("transition").toArray());
    r.asOf = QDate::fromString(o.value("asOf").toString(), Qt::ISODate);
    r.observations = o.value("observations").toInt();
    r.computedAt = QDateTime::fromString(o.value("computedAt").toString(), Qt::ISODate);
    r.spec = o.value("spec").toObject();
    return r;
}

QJsonObject toJson(const VolInfo &v)
{
    return {{"symbol", v.symbol}, {"model", v.model}, {"dailyVol", v.dailyVol}, {"annualVol", v.annualVol},
            {"horizonDays", v.horizonDays}, {"horizonVol", v.horizonVol}, {"var95", v.var95}, {"var99", v.var99},
            {"es95", v.es95}, {"es99", v.es99}, {"asOf", v.asOf.toString(Qt::ISODate)}, {"observations", v.observations},
            {"computedAt", v.computedAt.toString(Qt::ISODate)}, {"spec", v.spec}};
}

VolInfo volFrom(const QJsonObject &o)
{
    VolInfo v;
    v.symbol = o.value("symbol").toString();
    v.model = o.value("model").toString();
    v.dailyVol = o.value("dailyVol").toDouble();
    v.annualVol = o.value("annualVol").toDouble();
    v.horizonDays = o.value("horizonDays").toInt();
    v.horizonVol = o.value("horizonVol").toDouble();
    v.var95 = o.value("var95").toDouble();
    v.var99 = o.value("var99").toDouble();
    v.es95 = o.value("es95").toDouble();
    v.es99 = o.value("es99").toDouble();
    v.asOf = QDate::fromString(o.value("asOf").toString(), Qt::ISODate);
    v.observations = o.value("observations").toInt();
    v.computedAt = QDateTime::fromString(o.value("computedAt").toString(), Qt::ISODate);
    v.spec = o.value("spec").toObject();
    return v;
}

QString age(const QDate &asOf)
{
    if (!asOf.isValid())
        return {};
    const int days = asOf.daysTo(QDate::currentDate());
    return days <= 0 ? QObject::tr("today") : days == 1 ? QObject::tr("yesterday") : QObject::tr("%1 days ago").arg(days);
}

// The last weekday before today: a record as of that day or later is current.
QDate lastTradingDay()
{
    QDate d = QDate::currentDate().addDays(-1);
    while (d.dayOfWeek() > 5)
        d = d.addDays(-1);
    return d;
}

} // namespace

// ---------------------------------------------------------------- MarketState

MarketState *MarketState::instance()
{
    static MarketState *state = new MarketState;
    return state;
}

MarketState::MarketState(QObject *parent)
    : QObject(parent)
{
    load();
    // A save still waiting when the app quits is written then.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        if (m_savePending) {
            m_savePending = false;
            save();
        }
    });
}

void MarketState::load()
{
    QSettings s;
    const QJsonObject regimes = QJsonDocument::fromJson(s.value("quant/marketState/regimes").toByteArray()).object();
    for (auto it = regimes.begin(); it != regimes.end(); ++it) {
        // Either one record per symbol (the first version) or one per source.
        const QJsonObject o = it.value().toObject();
        if (o.contains("symbol")) {
            const RegimeInfo r = regimeFrom(o);
            m_regimes[it.key()].insert(r.source, r);
        } else {
            for (auto src = o.begin(); src != o.end(); ++src)
                m_regimes[it.key()].insert(src.key(), regimeFrom(src.value().toObject()));
        }
    }
    const QJsonObject vols = QJsonDocument::fromJson(s.value("quant/marketState/volatility").toByteArray()).object();
    for (auto it = vols.begin(); it != vols.end(); ++it)
        m_vols.insert(it.key(), volFrom(it.value().toObject()));
}

void MarketState::save() const
{
    QJsonObject regimes, vols;
    for (auto it = m_regimes.begin(); it != m_regimes.end(); ++it) {
        QJsonObject bySource;
        for (auto src = it.value().begin(); src != it.value().end(); ++src)
            bySource.insert(src.key(), toJson(src.value()));
        regimes.insert(it.key(), bySource);
    }
    for (auto it = m_vols.begin(); it != m_vols.end(); ++it)
        vols.insert(it.key(), toJson(it.value()));
    QSettings s;
    s.setValue("quant/marketState/regimes", QJsonDocument(regimes).toJson(QJsonDocument::Compact));
    s.setValue("quant/marketState/volatility", QJsonDocument(vols).toJson(QJsonDocument::Compact));
}

void MarketState::scheduleSave()
{
    if (m_savePending)
        return;
    m_savePending = true;
    QTimer::singleShot(0, this, [this] {
        if (!m_savePending)
            return;
        m_savePending = false;
        save();
    });
}

void MarketState::publishRegime(const RegimeInfo &info)
{
    if (info.symbol.isEmpty() || !info.isValid())
        return;
    m_regimes[info.symbol].insert(info.source, info);
    scheduleSave();
    emit regimeChanged(info.symbol);
}

void MarketState::publishVolatility(const VolInfo &info)
{
    if (info.symbol.isEmpty() || !info.isValid())
        return;
    m_vols.insert(info.symbol, info);
    scheduleSave();
    emit volatilityChanged(info.symbol);
}

std::optional<RegimeInfo> MarketState::regime(const QString &symbol, const QString &source) const
{
    auto it = m_regimes.constFind(symbol);
    if (it == m_regimes.constEnd() || it->isEmpty())
        return std::nullopt;
    if (!source.isEmpty()) {
        auto s = it->constFind(source);
        return s == it->constEnd() ? std::nullopt : std::optional<RegimeInfo>(*s);
    }
    const RegimeInfo *best = nullptr;
    for (const RegimeInfo &r : *it)
        if (!best || r.computedAt > best->computedAt)
            best = &r;
    return *best;
}

std::optional<VolInfo> MarketState::volatility(const QString &symbol) const
{
    auto it = m_vols.constFind(symbol);
    return it == m_vols.constEnd() ? std::nullopt : std::optional<VolInfo>(*it);
}

QStringList MarketState::symbols() const
{
    QStringList out = m_regimes.keys() + m_vols.keys();
    out.removeDuplicates();
    out.sort();
    return out;
}

// ---------------------------------------------------------------- StateRefresher

StateRefresher *StateRefresher::instance()
{
    static StateRefresher *r = new StateRefresher;
    return r;
}

StateRefresher::StateRefresher(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(30 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &StateRefresher::tick);
}

void StateRefresher::start()
{
    if (m_runner)
        return;
    m_runner = new JobRunner(this);
    // First pass a minute after launch, so the app's own start-up requests go first.
    QTimer::singleShot(60 * 1000, this, &StateRefresher::tick);
    m_timer.start();
}

void StateRefresher::setEnabled(bool on)
{
    m_enabled = on;
    emit changed();
}

void StateRefresher::refreshAll()
{
    if (!m_runner)
        start();
    for (const QString &sym : MarketState::instance()->symbols())
        if (!m_queue.contains(sym) && sym != m_current)
            m_queue << sym;
    emit changed();
    if (!m_running)
        next();
}

void StateRefresher::tick()
{
    if (!m_enabled)
        return;
    const QDate current = lastTradingDay();
    for (const QString &sym : MarketState::instance()->symbols()) {
        bool stale = false;
        for (const char *src : {"HMM", "MS-GARCH"})
            if (auto r = MarketState::instance()->regime(sym, src); r && r->asOf < current)
                stale = true;
        if (auto v = MarketState::instance()->volatility(sym); v && v->asOf < current)
            stale = true;
        if (stale && !m_queue.contains(sym) && sym != m_current)
            m_queue << sym;
    }
    emit changed();
    if (!m_running)
        next();
}

void StateRefresher::refresh(const QString &symbol)
{
    if (symbol.isEmpty() || m_queue.contains(symbol) || symbol == m_current)
        return;
    if (!m_runner)
        start();
    m_queue << symbol;
    emit changed();
    if (!m_running)
        next();
}

void StateRefresher::next()
{
    if (m_queue.isEmpty()) {
        m_running = false;
        m_current.clear();
        emit changed();
        return;
    }
    m_running = true;
    m_current = m_queue.takeFirst();
    emit changed();
    const QString sym = m_current;
    // What to re-estimate: the records' own specs, or the defaults for a new symbol.
    const auto hmm = MarketState::instance()->regime(sym, "HMM");
    const auto garch = MarketState::instance()->regime(sym, "MS-GARCH");
    const auto vol = MarketState::instance()->volatility(sym);
    const HmmSpec hs = HmmSpec::fromJson(hmm ? hmm->spec : QJsonObject());
    const GarchSpec gs = GarchSpec::fromJson(vol ? vol->spec : garch ? garch->spec : QJsonObject());
    const bool doHmm = hmm || (!garch && !vol), doGarch = true;
    struct Data {
        Prices hmmPrices, garchPrices;
        int left = 2;
    };
    auto data = std::make_shared<Data>();
    auto toPrices = [](const research::Series &s) {
        Prices p;
        for (const research::Point &pt : s) {
            p.dates.push_back(pt.date);
            p.close.push_back(pt.value);
        }
        return p;
    };
    auto *runner = static_cast<JobRunner *>(m_runner);
    auto go = [this, runner, sym, hs, gs, doHmm, doGarch, data, hmm, garch, vol] {
        if (--data->left > 0)
            return;
        const Prices hp = data->hmmPrices, gp = data->garchPrices;
        // The same days as the records were estimated on (a market holiday, a feed a day behind): the
        // estimation would give them again, so it is skipped until a new close comes in.
        auto same = [](const auto &record, const Prices &p) {
            return record->observations > 0 && record->observations == p.dates.size() - 1 && !p.dates.isEmpty()
                   && record->asOf == p.dates.last();
        };
        const bool any = hmm || garch || vol;
        if (any && (!hmm || same(hmm, hp)) && (!garch || same(garch, gp)) && (!vol || same(vol, gp))) {
            ++m_done;
            next();
            return;
        }
        runner->start<QPair<RegimeInfo, QPair<RegimeInfo, VolInfo>>>(
            [=](const markov::Progress &progress) {
                QPair<RegimeInfo, QPair<RegimeInfo, VolInfo>> out;
                HmmSpec h = hs;
                h.compareK = false;
                GarchSpec g = gs;
                g.benchmark = false;
                if (doHmm && hp.dates.size() > 60)
                    out.first = regimeInfo(sym, runHmm(hp, h, progress), hs);
                if (doGarch && gp.dates.size() > 120 && !progress.cancelled()) {
                    const GarchResult r = runGarch(gp, g, progress);
                    out.second.first = regimeInfo(sym, r, gs);
                    out.second.second = volInfo(sym, r, gs);
                }
                return out;
            },
            [this, sym](const QPair<RegimeInfo, QPair<RegimeInfo, VolInfo>> &out) {
                MarketState::instance()->publishRegime(out.first);
                MarketState::instance()->publishRegime(out.second.first);
                MarketState::instance()->publishVolatility(out.second.second);
                m_lastRun = QDateTime::currentDateTime();
                m_lastError = out.second.second.isValid() || out.first.isValid() ? QString()
                                                                                   : tr("%1: not enough prices").arg(sym);
                ++m_done;
                next();
            });
    };
    research::Client::instance()->prices(sym, hs.range, "1d", this, [data, go, toPrices](const research::Series &s, const QString &) {
        data->hmmPrices = toPrices(s);
        go();
    });
    research::Client::instance()->prices(sym, gs.range, "1d", this, [data, go, toPrices](const research::Series &s, const QString &) {
        data->garchPrices = toPrices(s);
        go();
    });
}

// ---------------------------------------------------------------- strip

MarketStateStrip::MarketStateStrip(QWidget *parent, bool compact)
    : QFrame(parent)
    , m_compact(compact)
{
    setObjectName(compact ? "qtStateCompact" : "qtStateStrip");
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(compact ? 0 : 10, compact ? 0 : 5, compact ? 0 : 10, compact ? 0 : 5);
    h->setSpacing(10);
    m_badge = badge("—", Theme::current().TextDim);
    h->addWidget(m_badge);
    m_regime = label({}, "qtCardLabel");
    h->addWidget(m_regime);
    m_vol = label({}, "qtCardLabel");
    h->addWidget(m_vol);
    if (!compact)
        h->addStretch(1);
    connect(MarketState::instance(), &MarketState::regimeChanged, this, [this](const QString &s) { if (s == m_symbol) refresh(); });
    connect(MarketState::instance(), &MarketState::volatilityChanged, this, [this](const QString &s) { if (s == m_symbol) refresh(); });
    connect(StateRefresher::instance(), &StateRefresher::changed, this, &MarketStateStrip::refresh);
    refresh();
}

void MarketStateStrip::setSymbol(const QString &symbol)
{
    m_symbol = symbol;
    refresh();
}

void MarketStateStrip::refresh()
{
    const Palette &c = Theme::current();
    const auto r = MarketState::instance()->regime(m_symbol);
    const auto v = MarketState::instance()->volatility(m_symbol);
    const bool busy = StateRefresher::instance()->current() == m_symbol && !m_symbol.isEmpty();
    if (r) {
        const QVector<QColor> colors = regimeColors(r->states);
        const QColor color = colors.value(r->regime, c.TextDim);
        setBadge(m_badge, regimeLabelText(r->label).toUpper(), color);
        m_regime->setText(m_compact ? QString("%1% · %2").arg(QString::number(100 * r->probability, 'f', 0), age(r->asOf))
                                    : tr("%1% · %2, %3 states · as of %4 (%5)%6")
                                          .arg(QString::number(100 * r->probability, 'f', 0), r->source).arg(r->states)
                                          .arg(r->asOf.toString(Qt::ISODate), age(r->asOf), busy ? tr(" · refreshing…") : QString()));
        m_badge->setToolTip(tr("Regime of %1 from the %2 fitted in Quant › Markov on %3: %4 with probability %5%, regime volatility %6% a day. "
                               "Re-estimated in the background when older than the last trading day.")
                                .arg(m_symbol, r->source, r->computedAt.toString("yyyy-MM-dd HH:mm"), regimeLabelText(r->label),
                                     QString::number(100 * r->probability, 'f', 1), QString::number(r->dailyVol, 'f', 2)));
    } else {
        setBadge(m_badge, busy ? tr("ESTIMATING…") : tr("NO REGIME"), c.TextFaint);
        m_regime->setText(m_compact ? QString() : busy ? QString() : tr("fit %1 in Quant › Markov › HMM regimes, or add it to the Portfolio").arg(m_symbol.isEmpty() ? tr("a symbol") : m_symbol));
        m_badge->setToolTip({});
    }
    if (v) {
        m_vol->setText(m_compact ? tr("σ %1%/d · VaR95 %2%").arg(QString::number(v->dailyVol, 'f', 2), QString::number(v->var95, 'f', 2))
                                 : tr("Vol forecast %1%/day (%2% ann.) · %3 days: %4% · VaR 95%: %5% · ES 95%: %6% · %7 · as of %8")
                                       .arg(QString::number(v->dailyVol, 'f', 2), QString::number(v->annualVol, 'f', 1)).arg(v->horizonDays)
                                       .arg(QString::number(v->horizonVol, 'f', 2), QString::number(v->var95, 'f', 2),
                                            QString::number(v->es95, 'f', 2), modelText(v->model), v->asOf.toString(Qt::ISODate)));
        m_vol->setToolTip(tr("From the %1 fitted in Quant › Markov on %2.").arg(modelText(v->model), v->computedAt.toString("yyyy-MM-dd HH:mm")));
    } else {
        m_vol->setText(m_compact || busy ? QString() : tr("· no volatility forecast: fit it in Quant › Markov › MS-GARCH"));
        m_vol->setToolTip({});
    }
}

} // namespace quant
