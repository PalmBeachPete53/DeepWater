#pragma once

#include <QString>

// Position sizing: how big a trade can be for the risk you accept. Plain arithmetic, no market data: the caller
// gives the price of one unit of the quote currency in the account's currency.
namespace desk {

struct RiskInput {
    double balance = 0;           // account balance, in the account's currency
    double risk = 0;              // what you accept to lose: a percentage of the balance, or an amount
    bool riskIsPercent = true;
    double entry = 0, stop = 0;
    double target = 0;            // optional: 0 or less means none
    double contractSize = 100000; // units in one lot
    double quoteToAccount = 1;    // one unit of the quote currency, in the account's currency
};

struct RiskResult {
    bool valid = false;
    QString error;                // why not, untranslated key: "balance", "risk", "stop", "conversion", "target"
    bool isLong = true;
    double stopDistance = 0;
    double riskAmount = 0;        // in the account's currency
    double riskPercent = 0;
    double units = 0, lots = 0;
    bool hasTarget = false;
    double rewardAmount = 0;
    double rewardRisk = 0;        // the R in 1 : R
    double breakevenWinRate = 0;  // % of wins needed to break even at this ratio
};

RiskResult computeRisk(const RiskInput &in);

// A plain reading of a reward to risk ratio.
enum class RiskGrade { Poor, Thin, Acceptable, Strong };
RiskGrade gradeOf(double rewardRisk);

} // namespace desk
