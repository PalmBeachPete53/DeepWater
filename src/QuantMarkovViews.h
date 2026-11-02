#pragma once

// The Markov section of the Quant page: three models behind one set of segments. HMM regimes
// (contexte de marché), Markov-switching GARCH (volatility forecast, risk) and the execution MDP
// (order slicing). The engines live in src/markov (no UI); these views fetch the prices, run the
// estimation off the UI thread (QuantJob.h) and draw the results.

#include "QuantJob.h"
#include "QuantMarketState.h"
#include "QuantViews.h"
#include "markov/Hmm.h"
#include "markov/Execution.h"
#include "markov/MsGarch.h"

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace research { class DonutChart; }

namespace quant {

class MarketStateStrip;

// Names and colours of the regimes of a K-state model, calm first.
QStringList regimeLabels(int states);
// The same names untranslated, as they are stored in the shared state (QuantMarketState) and on disk, so that
// a state fitted in one language reads correctly in the other; regimeLabelText() translates a stored key.
QStringList regimeKeys(int states);
QString regimeLabelText(const QString &key);
// A VolInfo::model stored untranslated ("MS-GARCH K=3 · Student-t"), translated for display.
QString modelText(const QString &model);
QVector<QColor> regimeColors(int states);

// What the HMM job computes for one symbol.
struct HmmResult {
    QVector<QDate> dates;
    QVector<double> price;
    markov::Mat obs;               // returns (%), and the realized volatility (%) when asked
    markov::HmmFit fit;            // the model shown
    QVector<markov::HmmFit> others;   // K = 2..4 for the comparison table (the shown one included)
    markov::Mat filtered, smoothed;
    QVector<int> viterbi;
    int window = 0;                // observations the model was estimated on (0: all)
};

class HmmView : public View
{
    Q_OBJECT
public:
    explicit HmmView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fetch();
    void estimate();
    void show(const HmmResult &r);

    QLineEdit *m_symbol;
    QButtonGroup *m_range, *m_states, *m_inputs;
    QSpinBox *m_window;
    QPushButton *m_fit;
    QCheckBox *m_posterior;
    JobRunner *m_runner;
    QLabel *m_regimeBadge, *m_regimeProb, *m_regimeNote;
    QLabel *m_modelValues[6];
    QTableWidget *m_compare, *m_transition, *m_statesTable;
    RegimeChart *m_regimeChart;
    StackedAreaChart *m_probChart;
    Prices m_prices;
    QString m_sym;
    HmmResult m_result;
    bool m_haveResult = false;
    int m_generation = 0;
};

// What the MS-GARCH job computes for one symbol.
struct GarchResult {
    QVector<QDate> dates;          // of the returns
    QVector<double> returns;       // %
    QVector<double> realized;      // rolling 20-day realized volatility, annualized %
    markov::MsGarchFit fit;        // the model shown (K regimes)
    markov::MsGarchFit single;     // GARCH(1,1) with the same innovations, the benchmark
    markov::VolForecast forecast;
    markov::Risk oneDay, horizon;
    int horizonDays = 10;
};

class MsGarchView : public View
{
    Q_OBJECT
public:
    explicit MsGarchView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fetch();
    void estimate();
    void show(const GarchResult &r);
    void sizePosition();

    QLineEdit *m_symbol;
    QButtonGroup *m_range, *m_regimes, *m_innovations;
    QSpinBox *m_horizon;
    QPushButton *m_fit;
    JobRunner *m_runner;
    QLabel *m_volToday, *m_volAnnual, *m_volHorizon, *m_regimeBadge, *m_regimeProb;
    QLabel *m_risk[8];             // VaR 95 / 99, ES 95 / 99: one day, then the horizon
    QLabel *m_riskHorizonTitle;
    QDoubleSpinBox *m_capital, *m_targetRisk;
    QLabel *m_position, *m_leverage, *m_positionNote;
    QTableWidget *m_models, *m_regimeTable;
    MultiLineChart *m_volChart;
    StackedAreaChart *m_probChart;
    QFrame *m_probPanel;
    SpreadChart *m_cone;
    Prices m_prices;
    QString m_sym;
    GarchResult m_result;
    bool m_haveResult = false;
    // What m_result was estimated from: a new horizon alone only recomputes the forecast and the risk.
    int m_pricesVersion = 0;
    struct {
        int pricesVersion = -1, regimes = 0;
        bool student = false;
    } m_resultFrom;
    int m_generation = 0;
};

// What the execution job computes.
struct ExecResult {
    markov::ExecutionParams params;
    double adv = NaN, dailyVol = NaN;          // shares a day; daily volatility in %
    QString market;                            // where the price, ADV and volatility come from
    bool regimes = false;
    markov::RegimeModel regimeModel;
    markov::HmmFit hmm;                        // the HMM, when estimated here
    RegimeInfo regimeInfo;                     // the regimes used (shared-state record or the fresh estimation)
    int currentRegime = 0;
    QString regimeSource;                      // "HMM" / "MS-GARCH", and whether it came from the shared state
    bool regimeFromState = false;
    QVector<markov::Trajectory> schedules;     // TWAP, VWAP, Almgren-Chriss, MDP (per regime), Q-learning (per regime)
    QVector<markov::SimulationResult> results;
    double mdpValue = NaN;                     // the MDP's objective at the start, in the current regime
    int levels = 0, paths = 0;
    bool vwapProfile = false;                  // a volume profile was found (else VWAP is flat)
    markov::Vec learningCurve;
    int episodes = 0;
    QString error;
};

// A portfolio of positions: prices, weights, each symbol's regime and volatility forecast from the shared state
// (re-estimated in the background), and the parametric portfolio risk from the correlations of daily returns.
class PortfolioView : public View
{
    Q_OBJECT
public:
    explicit PortfolioView(QWidget *parent = nullptr);

protected:
    void load() override;
    void showEvent(QShowEvent *) override;

private:
    struct Position {
        QString symbol;
        double quantity = 0;
    };
    void addPosition();
    void removePosition(const QString &symbol);
    void save() const;
    void fetch();
    void compute();
    void scheduleCompute();

    QLineEdit *m_symbol;
    QDoubleSpinBox *m_quantity;
    QPushButton *m_add, *m_refresh, *m_estimate;
    QTableWidget *m_table, *m_methods;
    QLabel *m_totals[4];
    QLabel *m_mix;
    research::DonutChart *m_donut;
    HistogramChart *m_histogram;
    QVector<Position> m_positions;
    QHash<QString, QJsonObject> m_quotes;
    QHash<QString, Prices> m_history;        // a year of closes, for the correlations
    int m_generation = 0;
    int m_pending = 0;
    bool m_computePending = false, m_computeDirty = false;
};

class ExecutionView : public View
{
    Q_OBJECT
public:
    explicit ExecutionView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void run();
    void show(const ExecResult &r);

    QLineEdit *m_symbol;
    QDoubleSpinBox *m_quantity, *m_horizon, *m_lambda, *m_temporary, *m_permanent, *m_spread;
    QSpinBox *m_intervals, *m_paths, *m_episodes;
    QButtonGroup *m_regimes, *m_impact;
    QCheckBox *m_agent;
    QPushButton *m_run;
    MarketStateStrip *m_state;
    JobRunner *m_runner;
    QLabel *m_marketValues[4], *m_acValues[5], *m_mdpValues[4];
    QTableWidget *m_summary;
    MultiLineChart *m_trajectories;
    HistogramChart *m_histogram;
    QString m_sym;
    int m_generation = 0;
};

// The Markov section: segments HMM / MS-GARCH / EXECUTION over a stack.
class MarkovView : public QWidget
{
    Q_OBJECT
public:
    explicit MarkovView(QWidget *parent = nullptr);

private:
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
};

} // namespace quant
