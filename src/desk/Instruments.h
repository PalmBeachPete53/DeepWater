#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// The Desk and Journal tabs' universe: about a hundred liquid markets, each with its Yahoo Finance symbol and what a
// position in it means (contract size, quote currency). Plain data, no network.
namespace desk {

enum class AssetClass { Fx, Index, Commodity, Stock, Crypto };

struct Instrument {
    QString id;          // what brokers call it: "EURUSD", "XAUUSD", "US500", "AAPL", "BTCUSD"
    QString label;       // shown: "EUR/USD", "Gold", "S&P 500"
    QString symbol;      // Yahoo Finance: "EURUSD=X", "GC=F", "^GSPC"
    AssetClass cls = AssetClass::Fx;
    double contractSize = 1;   // units in one lot: 100,000 for FX, 100 oz for gold
    QString quote = "USD";     // the currency the price is in
    QString volumeProxy;       // the listed fund whose volume stands in when the market has none ("FXE"), or empty
};

const QVector<Instrument> &instruments();
// By id, by Yahoo symbol or as a broker writes it ("EURUSD.m", "EUR/USD", "GOLD", "XAUUSDm"); null when unknown.
const Instrument *findInstrument(const QString &text);
QString assetClassName(AssetClass c);   // "FX", "Indices", "Commodities", "Stocks", "Crypto": untranslated keys

// The eight currencies the page reads, strongest economies first.
const QStringList &majorCurrencies();
// The Yahoo pair quoting `currency` against the dollar ("EURUSD=X", "JPY=X"), and whether its price is
// dollars per unit of the currency (true: EUR, GBP, AUD, NZD) or the reverse (false: JPY, CHF, CAD). Empty for USD.
QString dollarPair(const QString &currency, bool *direct = nullptr);

} // namespace desk
