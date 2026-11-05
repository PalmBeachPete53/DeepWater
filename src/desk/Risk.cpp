#include "desk/Risk.h"

#include <cmath>

namespace desk {

RiskResult computeRisk(const RiskInput &in)
{
    RiskResult r;
    const auto fail = [&](const char *why) {
        r.error = why;
        return r;
    };
    if (!(in.balance > 0))
        return fail("balance");
    if (!(in.risk > 0))
        return fail("risk");
    if (!(in.entry > 0) || !(in.stop > 0) || in.entry == in.stop)
        return fail("stop");
    if (!(in.quoteToAccount > 0) || !(in.contractSize > 0))
        return fail("conversion");

    r.isLong = in.stop < in.entry;
    r.stopDistance = std::abs(in.entry - in.stop);
    r.riskAmount = in.riskIsPercent ? in.balance * in.risk / 100.0 : in.risk;
    r.riskPercent = r.riskAmount / in.balance * 100.0;
    r.units = r.riskAmount / (r.stopDistance * in.quoteToAccount);
    r.lots = r.units / in.contractSize;

    if (in.target > 0) {
        // A target on the stop's side of the entry is a mistake in the form, not a trade.
        if ((r.isLong && in.target <= in.entry) || (!r.isLong && in.target >= in.entry))
            return fail("target");
        const double distance = std::abs(in.target - in.entry);
        r.hasTarget = true;
        r.rewardAmount = distance * r.units * in.quoteToAccount;
        r.rewardRisk = distance / r.stopDistance;
        r.breakevenWinRate = 100.0 / (1.0 + r.rewardRisk);
    }
    r.valid = true;
    return r;
}

RiskGrade gradeOf(double rewardRisk)
{
    if (rewardRisk < 1)
        return RiskGrade::Poor;
    if (rewardRisk < 1.5)
        return RiskGrade::Thin;
    if (rewardRisk < 3)
        return RiskGrade::Acceptable;
    return RiskGrade::Strong;
}

} // namespace desk
