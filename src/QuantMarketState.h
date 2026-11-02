#pragma once

// What the Markov models know about a symbol, for the rest of the app: its regime (from the HMM or the
// MS-GARCH, with every regime's volatility and the transition matrix, so other models can reuse the
// chain) and its volatility forecast with VaR / ES (from the MS-GARCH). The views publish after each fit;
// any widget reads the last value and listens for changes. Values are kept in QSettings with their dates
// and the spec they were estimated with, so a tab can show them at the next launch and a background
// refresher (StateRefresher) can re-estimate them when they are older than the last session.

#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include <optional>

class QLabel;

namespace quant {

struct RegimeInfo {
    QString symbol;
    QString source;                 // "HMM" or "MS-GARCH"
    int states = 0;
    int regime = 0;                 // 0 = calm
    QString label;                  // "Normal"
    double probability = 0;         // current, 0..1
    double dailyVol = 0;            // of the current regime, %
    QVector<double> vols;           // every regime's daily volatility, %
    QVector<double> probabilities;  // current probability of each regime
    QVector<double> transition;     // states × states, row-major, per day
    QDate asOf;                     // last observation
    int observations = 0;           // days estimated on (0 in records saved before it was kept)
    QDateTime computedAt;
    QJsonObject spec;               // how it was estimated (HmmSpec / GarchSpec)
    bool isValid() const { return states > 0; }
};

struct VolInfo {
    QString symbol;
    QString model;                  // "MS-GARCH K=2 · normal"
    double dailyVol = 0;            // next day, %
    double annualVol = 0;           // %
    int horizonDays = 0;
    double horizonVol = 0;          // over the horizon, %
    double var95 = 0, var99 = 0, es95 = 0, es99 = 0;   // one day, % of the position
    QDate asOf;
    int observations = 0;
    QDateTime computedAt;
    QJsonObject spec;
    bool isValid() const { return !model.isEmpty(); }
};

class MarketState : public QObject
{
    Q_OBJECT
public:
    static MarketState *instance();

    void publishRegime(const RegimeInfo &info);
    void publishVolatility(const VolInfo &info);
    // The regime from a source, or the most recently computed one when none is given.
    std::optional<RegimeInfo> regime(const QString &symbol, const QString &source = {}) const;
    std::optional<VolInfo> volatility(const QString &symbol) const;
    QStringList symbols() const;

signals:
    void regimeChanged(const QString &symbol);
    void volatilityChanged(const QString &symbol);

private:
    explicit MarketState(QObject *parent = nullptr);
    void load();
    void save() const;
    // A refresh publishes three records in a row: written once, in the next event loop pass (and at exit).
    void scheduleSave();
    bool m_savePending = false;

    QHash<QString, QHash<QString, RegimeInfo>> m_regimes;   // symbol -> source -> info
    QHash<QString, VolInfo> m_vols;
};

// Re-estimates what the state holds when it is older than the last trading day: at launch, then every
// few hours, one symbol at a time on a worker thread, with the spec each record was estimated with.
// refresh(symbol) queues one symbol now (a symbol without records gets the default specs).
class StateRefresher : public QObject
{
    Q_OBJECT
public:
    static StateRefresher *instance();
    void start();                         // call once, from the main window
    void refresh(const QString &symbol);
    // Every symbol of the state now, current or not (the Sources page's Refresh).
    void refreshAll();
    bool isBusy() const { return !m_queue.isEmpty() || m_running; }
    QString current() const { return m_current; }
    int queued() const { return m_queue.size(); }
    // Sources page: off, nothing is re-estimated on its own (symbols asked for by a view still are).
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    QDateTime lastRun() const { return m_lastRun; }      // end of the last estimation
    QString lastError() const { return m_lastError; }
    int done() const { return m_done; }                  // estimations completed this session
    static constexpr int IntervalSecs = 30 * 60;

signals:
    void changed();                       // queue or running state changed

private:
    explicit StateRefresher(QObject *parent = nullptr);
    void tick();
    void next();

    QTimer m_timer;
    QStringList m_queue;
    QString m_current;
    bool m_running = false, m_enabled = true;
    QDateTime m_lastRun;
    QString m_lastError;
    int m_done = 0;
    QObject *m_runner = nullptr;          // quant::JobRunner, created on start()
};

// One line on a symbol: its regime badge and its volatility forecast, or where to get them.
class MarketStateStrip : public QFrame
{
    Q_OBJECT
public:
    explicit MarketStateStrip(QWidget *parent = nullptr, bool compact = false);
    void setSymbol(const QString &symbol);

private:
    void refresh();

    QString m_symbol;
    bool m_compact;
    QLabel *m_badge, *m_regime, *m_vol;
};

} // namespace quant
