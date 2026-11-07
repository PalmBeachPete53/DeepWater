#include "broker/Broker.h"

#include "desk/Risk.h"

#include <QJsonValue>
#include <QVariant>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace broker {

Account accountFromJson(const QJsonObject &o)
{
    Account a;
    a.id = o.value("id").toString();
    a.alias = o.value("alias").toString();
    a.server = o.value("loginServer").toString();
    a.login = o.value("loginAccountNumber").toString();
    a.region = o.value("region").toObject().value("name").toString();
    a.status = o.value("status").toObject().value("name").toString();
    a.typeName = o.value("type").toObject().value("name").toString();
    a.typeId = o.value("type").toObject().value("id").toInt();
    a.propFirm = o.value("propFirm").toBool();
    a.tradingDisabled = o.value("tradingDisabled").toBool();
    if (o.value("accountInformation").isObject())
        applyInformation(&a, o.value("accountInformation").toObject());
    return a;
}

void applyInformation(Account *a, const QJsonObject &o)
{
    a->connected = o.value("connected").toBool();
    a->wrongCredentials = o.value("wrongCredentials").toBool();
    const QString env = o.value("environment").toString();
    a->environment = env == "DEMO" ? Environment::Demo : env == "LIVE" ? Environment::Live : Environment::Unknown;
    a->balance = o.value("balance").toDouble();
    a->equity = o.value("equity").toDouble();
    a->unrealizedProfit = o.value("unrealizedProfit").toDouble();
    a->currency = o.value("currency").toString();
    a->leverage = o.value("leverage").toInt();
    a->openPositions = o.value("openPositionsCount").toInt();
}

SymbolSpec symbolFromJson(const QJsonObject &o)
{
    SymbolSpec s;
    s.name = o.value("name").toString();
    s.base = o.value("baseCurrency").toString();
    s.quote = o.value("quoteCurrency").toString();
    s.digits = o.value("digits").toInt(5);
    s.contractSize = o.value("lotSize").toDouble();
    s.minVolume = o.value("minimalVolume").toDouble();
    s.maxVolume = o.value("maximalVolume").toDouble();
    s.stepVolume = o.value("stepVolume").toDouble();
    s.disabled = o.value("disabled").toBool();
    return s;
}

Quote quoteFromJson(const QJsonObject &o)
{
    return {o.value("symbol").toString(), o.value("bid").toDouble(), o.value("ask").toDouble()};
}

Position positionFromJson(const QJsonObject &o)
{
    Position p;
    p.id = o.value("id").toVariant().toString();
    p.symbol = o.value("symbol").toString();
    p.type = o.value("orderType").toString();
    p.comment = o.value("comment").toString();
    p.requestId = o.value("requestId").toVariant().toString();
    p.volume = o.value("volume").toDouble();
    p.openPrice = o.value("openPrice").toDouble();
    p.stopLoss = o.value("stopLoss").toDouble();
    p.takeProfit = o.value("takeProfit").toDouble();
    p.profit = o.value("profit").toDouble();
    return p;
}

QString resolveSymbol(const QString &wanted, const QVector<SymbolSpec> &symbols, QStringList *ambiguous)
{
    const QString name = wanted.trimmed().toUpper();
    if (name.isEmpty())
        return {};
    QStringList suffixed;
    for (const SymbolSpec &s : symbols) {
        const QString upper = s.name.toUpper();
        if (upper == name)
            return s.name;
        // The name, then a short suffix that isn't a word of its own: EURUSD.f, EURUSDm, EURUSD_i, not EURUSDT.
        if (upper.startsWith(name)) {
            const QString rest = upper.mid(name.size());
            static const QRegularExpression suffix(QStringLiteral("^([._#+-][A-Z0-9]{0,6}|[MCRZ])$"));
            if (suffix.match(rest).hasMatch())
                suffixed << s.name;
        }
    }
    if (suffixed.size() == 1)
        return suffixed.first();
    if (ambiguous)
        *ambiguous = suffixed;
    return {};
}

QString conversionSymbol(const QString &quote, const QString &accountCurrency, const QVector<SymbolSpec> &symbols, bool *inverted)
{
    const QString direct = resolveSymbol(quote + accountCurrency, symbols);
    if (!direct.isEmpty()) {
        *inverted = false;
        return direct;
    }
    const QString other = resolveSymbol(accountCurrency + quote, symbols);
    *inverted = !other.isEmpty();
    return other;
}

double roundToDigits(double price, int digits)
{
    const double scale = std::pow(10.0, std::clamp(digits, 0, 10));
    return std::round(price * scale) / scale;
}

double lotsDownToStep(double lots, double step)
{
    if (!(step > 0))
        return 0;
    // A hair over: 0.3 / 0.01 is 29.999999999999996, which is 30 steps.
    const double steps = std::floor(lots / step + 1e-9);
    return std::round(steps * step * 1e8) / 1e8;
}

bool sizedPlatform(int typeId)
{
    return typeId >= 1 && typeId <= 6;
}

PlanLine buildPlanLine(const PlanInput &in)
{
    PlanLine line;
    line.account = in.account;
    line.buy = in.intent.buy;
    const auto refuse = [&line](Refusal why) {
        line.refusal = why;
        line.lots = 0;
        return line;
    };
    const Account &a = in.account;
    const Intent &intent = in.intent;
    if (a.wrongCredentials)
        return refuse(Refusal::WrongCredentials);
    if (!a.connected)
        return refuse(Refusal::NotConnected);
    if (a.tradingDisabled)
        return refuse(Refusal::TradingDisabled);
    if (!sizedPlatform(a.typeId))
        return refuse(Refusal::PlatformNotChecked);
    if (a.environment != Environment::Demo && !in.allowRealMoney)
        return refuse(Refusal::RealMoneyLocked);
    if (!(intent.riskPercent > 0) || !std::isfinite(intent.riskPercent))
        return refuse(Refusal::RiskInvalid);
    if (intent.riskPercent > MaxRiskPercent)
        return refuse(Refusal::RiskTooHigh);
    if (!(intent.stop > 0))
        return refuse(Refusal::NoStop);
    if (!(a.balance > 0) || a.currency.isEmpty())
        return refuse(Refusal::NoBalance);

    line.symbol = resolveSymbol(intent.symbol, in.symbols, &line.candidates);
    if (line.symbol.isEmpty())
        return refuse(line.candidates.isEmpty() ? Refusal::NoSymbol : Refusal::AmbiguousSymbol);
    const auto spec = std::find_if(in.symbols.begin(), in.symbols.end(), [&line](const SymbolSpec &s) { return s.name == line.symbol; });
    if (spec->disabled)
        return refuse(Refusal::SymbolDisabled);
    if (!(spec->contractSize > 0) || !(spec->stepVolume > 0) || !(spec->minVolume > 0) || !(spec->maxVolume > 0))
        return refuse(Refusal::SymbolIncomplete);
    const double price = intent.buy ? in.quote.ask : in.quote.bid;
    if (!(price > 0))
        return refuse(Refusal::NoQuote);
    line.price = price;
    line.digits = spec->digits;
    line.stop = roundToDigits(intent.stop, spec->digits);
    line.target = intent.target > 0 ? roundToDigits(intent.target, spec->digits) : 0;
    if (intent.buy ? line.stop >= price : line.stop <= price)
        return refuse(Refusal::StopWrongSide);
    if (line.target > 0 && (intent.buy ? line.target <= price : line.target >= price))
        return refuse(Refusal::TargetWrongSide);
    if (!(in.quoteToAccount > 0))
        return refuse(Refusal::NoConversion);

    desk::RiskInput risk;
    risk.balance = a.balance;
    risk.risk = intent.riskPercent;
    risk.entry = price;
    risk.stop = line.stop;
    risk.contractSize = spec->contractSize;
    risk.quoteToAccount = in.quoteToAccount;
    const desk::RiskResult r = desk::computeRisk(risk);
    if (!r.valid)
        return refuse(Refusal::NotChecked);

    double lots = lotsDownToStep(r.lots, spec->stepVolume);
    if (lots < spec->minVolume)
        return refuse(Refusal::TooSmall);
    if (lots > spec->maxVolume) {
        lots = lotsDownToStep(spec->maxVolume, spec->stepVolume);
        line.cappedAtMaximum = true;
    }
    line.lots = lots;
    line.riskAmount = lots * spec->contractSize * r.stopDistance * in.quoteToAccount;
    line.riskPercent = line.riskAmount / a.balance * 100;
    // Worked out again from what would be sent: more than asked for doesn't leave.
    if (line.riskPercent > intent.riskPercent * 1.0001)
        return refuse(Refusal::NotChecked);
    return line;
}

} // namespace broker
