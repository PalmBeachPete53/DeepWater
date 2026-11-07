#pragma once

// What the MetaTrader page knows of the accounts, and what it decides before an order leaves: pure logic, tested by
// tests/quant/broker. An order is built for one account at a time from that account's balance, symbol and prices; anything
// missing or doubtful refuses the order for that account rather than guessing.

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace broker {

enum class Environment { Unknown, Demo, Live };   // as the service reports it; Unknown counts as real money

struct Account {
    QString id, alias, server, login, region, status, typeName;
    int typeId = 0;   // MetaCopier's platform: 1 MT5, 2 MT4, 3 DXtrade, 4 cTrader, 5 TradeLocker, 6 Match-Trader, 7+ exchanges and brokers
    bool propFirm = false;
    bool tradingDisabled = false;
    // Its information, synced by the service every minute.
    bool connected = false;
    bool wrongCredentials = false;
    Environment environment = Environment::Unknown;
    double balance = 0, equity = 0, unrealizedProfit = 0;
    QString currency;
    int leverage = 0;
    int openPositions = 0;
};

struct SymbolSpec {
    QString name, base, quote;
    int digits = 5;
    double contractSize = 0;    // units in one lot
    double minVolume = 0, maxVolume = 0, stepVolume = 0;
    bool disabled = false;
};

struct Quote {
    QString symbol;
    double bid = 0, ask = 0;
};

struct Position {
    QString id, symbol, type, comment, requestId;
    double volume = 0, openPrice = 0, stopLoss = 0, takeProfit = 0, profit = 0;
};

Account accountFromJson(const QJsonObject &o);
// The platforms an order is sized for here: those of CFD brokers and prop firms, where MetaCopier takes a volume in lots of
// a contract size it gives. Its exchanges (Binance, Bybit…), Tradovate, TradeStation and Alpaca use the same call, but
// their lots haven't been checked: an account there is listed and refused, not guessed.
bool sizedPlatform(int typeId);
void applyInformation(Account *account, const QJsonObject &information);
SymbolSpec symbolFromJson(const QJsonObject &o);
Quote quoteFromJson(const QJsonObject &o);
Position positionFromJson(const QJsonObject &o);

// The account's own name for a symbol: the same name, or the only one that is that name plus a broker's suffix (XAUUSD.f,
// EURUSDm). Several candidates: no answer, and `ambiguous` lists them.
QString resolveSymbol(const QString &wanted, const QVector<SymbolSpec> &symbols, QStringList *ambiguous = nullptr);

// What the trader asks for, the same on every account.
struct Intent {
    QString symbol;
    bool buy = true;
    double riskPercent = 0;   // of each account's balance
    double stop = 0;
    double target = 0;        // 0: none
};

constexpr double MaxRiskPercent = 5;   // a typing error shouldn't be able to empty an account

enum class Refusal {
    None,
    NoSymbol,            // the account has no such symbol
    AmbiguousSymbol,     // several of its symbols could be it
    SymbolDisabled,
    SymbolIncomplete,    // the service gave no contract size or volume steps
    NoQuote,
    NoConversion,        // no price to turn the quote currency into the account's
    NoBalance,
    RiskInvalid,
    RiskTooHigh,
    NoStop,
    StopWrongSide,
    TargetWrongSide,
    TooSmall,            // under the minimum volume at this risk: rounding up would risk more than asked
    NotConnected,
    WrongCredentials,
    TradingDisabled,
    RealMoneyLocked,
    NotChecked,          // a computed risk over the one asked for: never sent
    PlatformNotChecked,  // a platform whose lots aren't checked yet (sizedPlatform)
};

struct PlanLine {
    Account account;
    QString symbol;       // on this account
    bool buy = true;
    QStringList candidates;   // AmbiguousSymbol
    Refusal refusal = Refusal::None;
    int digits = 5;       // of the symbol's prices
    double price = 0;     // the side an order would fill at
    double stop = 0, target = 0;
    double lots = 0;
    double riskAmount = 0, riskPercent = 0;   // of what these lots really risk
    bool cappedAtMaximum = false;             // the symbol's maximum volume: less risk than asked
    bool ok() const { return refusal == Refusal::None; }
};

struct PlanInput {
    Account account;
    Intent intent;
    QVector<SymbolSpec> symbols;
    Quote quote;                 // of the resolved symbol
    double quoteToAccount = 0;   // one unit of the quote currency in the account's; 0: unknown
    bool allowRealMoney = false;
};
PlanLine buildPlanLine(const PlanInput &in);

// The other symbol needed to value a position, when its quote currency isn't the account's, on this account.
// Empty when there is none. `inverted`: its price is account per quote the wrong way round.
QString conversionSymbol(const QString &quote, const QString &accountCurrency, const QVector<SymbolSpec> &symbols, bool *inverted);

double roundToDigits(double price, int digits);
double lotsDownToStep(double lots, double step);

} // namespace broker
