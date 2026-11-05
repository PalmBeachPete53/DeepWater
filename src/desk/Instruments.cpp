#include "desk/Instruments.h"

#include <QHash>
#include <QRegularExpression>

namespace desk {

namespace {

QVector<Instrument> build()
{
    QVector<Instrument> out;
    const char *majors[] = {"EUR", "USD", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD"};
    // Market convention for which currency comes first.
    const QStringList rank {"EUR", "GBP", "AUD", "NZD", "USD", "CAD", "CHF", "JPY"};
    QVector<std::pair<QString, QString>> pairs;
    for (int i = 0; i < int(std::size(majors)); ++i)
        for (int j = i + 1; j < int(std::size(majors)); ++j) {
            QString a = majors[i], b = majors[j];
            if (rank.indexOf(a) > rank.indexOf(b))
                std::swap(a, b);
            pairs.push_back({a, b});
        }
    std::sort(pairs.begin(), pairs.end(), [&](const auto &x, const auto &y) {
        const auto key = [&](const auto &p) { return (p.first == "USD" || p.second == "USD" ? 0 : 1000) + rank.indexOf(p.first) * 10 + rank.indexOf(p.second); };
        return key(x) < key(y);
    });
    const QHash<QString, QString> fund {{"EUR", "FXE"}, {"GBP", "FXB"}, {"JPY", "FXY"}, {"CHF", "FXF"}, {"CAD", "FXC"}, {"AUD", "FXA"}};
    for (const auto &[a, b] : pairs) {
        // Yahoo writes the dollar pairs with the dollar first as "JPY=X".
        const QString symbol = a == "USD" ? b + "=X" : a + b + "=X";
        QString proxy = a == "USD" ? fund.value(b) : fund.value(a);
        out.push_back({a + b, a + "/" + b, symbol, AssetClass::Fx, 100000, b, proxy});
    }
    const auto add = [&](const char *id, const char *label, const char *symbol, AssetClass cls, double size, const char *quote, const char *proxy) {
        out.push_back({id, label, symbol, cls, size, quote, proxy});
    };
    add("US500", "S&P 500", "^GSPC", AssetClass::Index, 1, "USD", "SPY");
    add("USTEC", "Nasdaq 100", "^NDX", AssetClass::Index, 1, "USD", "QQQ");
    add("US30", "Dow Jones", "^DJI", AssetClass::Index, 1, "USD", "DIA");
    add("US2000", "Russell 2000", "^RUT", AssetClass::Index, 1, "USD", "IWM");
    add("IXIC", "Nasdaq Composite", "^IXIC", AssetClass::Index, 1, "USD", "");
    add("VIX", "VIX", "^VIX", AssetClass::Index, 1, "USD", "");
    add("DXY", "US Dollar Index", "DX-Y.NYB", AssetClass::Index, 1, "USD", "UUP");
    add("DE40", "DAX", "^GDAXI", AssetClass::Index, 1, "EUR", "EWG");
    add("UK100", "FTSE 100", "^FTSE", AssetClass::Index, 1, "GBP", "EWU");
    add("FR40", "CAC 40", "^FCHI", AssetClass::Index, 1, "EUR", "EWQ");
    add("EU50", "Euro Stoxx 50", "^STOXX50E", AssetClass::Index, 1, "EUR", "FEZ");
    add("AEX", "AEX", "^AEX", AssetClass::Index, 1, "EUR", "");
    add("SMI", "SMI", "^SSMI", AssetClass::Index, 1, "CHF", "EWL");
    add("ES35", "IBEX 35", "^IBEX", AssetClass::Index, 1, "EUR", "EWP");
    add("JP225", "Nikkei 225", "^N225", AssetClass::Index, 1, "JPY", "EWJ");
    add("HK50", "Hang Seng", "^HSI", AssetClass::Index, 1, "HKD", "EWH");
    add("CN", "Shanghai Composite", "000001.SS", AssetClass::Index, 1, "CNY", "");
    add("KS11", "KOSPI", "^KS11", AssetClass::Index, 1, "KRW", "EWY");
    add("AU200", "ASX 200", "^AXJO", AssetClass::Index, 1, "AUD", "EWA");
    add("NIFTY", "Nifty 50", "^NSEI", AssetClass::Index, 1, "INR", "INDA");
    add("TSX", "S&P/TSX", "^GSPTSE", AssetClass::Index, 1, "CAD", "EWC");
    add("BVSP", "Bovespa", "^BVSP", AssetClass::Index, 1, "BRL", "EWZ");
    add("MXX", "IPC Mexico", "^MXX", AssetClass::Index, 1, "MXN", "EWW");

    add("XAUUSD", "Gold", "GC=F", AssetClass::Commodity, 100, "USD", "GLD");
    add("XAGUSD", "Silver", "SI=F", AssetClass::Commodity, 5000, "USD", "SLV");
    add("USOIL", "WTI Crude", "CL=F", AssetClass::Commodity, 1000, "USD", "USO");
    add("UKOIL", "Brent Crude", "BZ=F", AssetClass::Commodity, 1000, "USD", "BNO");
    add("NATGAS", "Natural Gas", "NG=F", AssetClass::Commodity, 10000, "USD", "UNG");
    add("COPPER", "Copper", "HG=F", AssetClass::Commodity, 25000, "USD", "CPER");
    add("PLAT", "Platinum", "PL=F", AssetClass::Commodity, 50, "USD", "PPLT");
    add("PALL", "Palladium", "PA=F", AssetClass::Commodity, 100, "USD", "PALL");
    add("CORN", "Corn", "ZC=F", AssetClass::Commodity, 50, "USD", "CORN");
    add("WHEAT", "Wheat", "ZW=F", AssetClass::Commodity, 50, "USD", "WEAT");
    add("SOY", "Soybeans", "ZS=F", AssetClass::Commodity, 50, "USD", "SOYB");
    add("SUGAR", "Sugar", "SB=F", AssetClass::Commodity, 112000, "USD", "");
    add("COFFEE", "Coffee", "KC=F", AssetClass::Commodity, 375, "USD", "");
    add("COCOA", "Cocoa", "CC=F", AssetClass::Commodity, 10, "USD", "");
    add("HEAT", "Heating Oil", "HO=F", AssetClass::Commodity, 42000, "USD", "");
    add("GASOLINE", "Gasoline (RBOB)", "RB=F", AssetClass::Commodity, 42000, "USD", "UGA");

    const char *stocks[][2] = {{"AAPL", "Apple"}, {"MSFT", "Microsoft"}, {"GOOGL", "Alphabet"}, {"AMZN", "Amazon"}, {"NVDA", "Nvidia"},
                               {"META", "Meta Platforms"}, {"TSLA", "Tesla"}, {"BRK-B", "Berkshire Hathaway"}, {"JPM", "JPMorgan Chase"},
                               {"V", "Visa"}, {"MA", "Mastercard"}, {"UNH", "UnitedHealth"}, {"XOM", "Exxon Mobil"}, {"CVX", "Chevron"},
                               {"JNJ", "Johnson & Johnson"}, {"WMT", "Walmart"}, {"PG", "Procter & Gamble"}, {"HD", "Home Depot"},
                               {"KO", "Coca-Cola"}, {"PEP", "PepsiCo"}, {"COST", "Costco"}, {"AVGO", "Broadcom"}, {"ORCL", "Oracle"},
                               {"CRM", "Salesforce"}, {"AMD", "AMD"}, {"NFLX", "Netflix"}, {"INTC", "Intel"}, {"GS", "Goldman Sachs"},
                               {"BA", "Boeing"}, {"DIS", "Walt Disney"}};
    for (const auto &s : stocks)
        out.push_back({s[0], s[1], s[0], AssetClass::Stock, 1, "USD", ""});

    const char *coins[][2] = {{"BTC", "BTC/USD"}, {"ETH", "ETH/USD"}, {"SOL", "SOL/USD"}, {"XRP", "XRP/USD"}, {"BNB", "BNB/USD"},
                              {"ADA", "ADA/USD"}, {"DOGE", "DOGE/USD"}, {"DOT", "DOT/USD"}, {"LINK", "LINK/USD"}, {"AVAX", "AVAX/USD"},
                              {"LTC", "LTC/USD"}, {"TRX", "TRX/USD"}};
    for (const auto &c : coins)
        out.push_back({QString(c[0]) + "USD", c[1], QString(c[0]) + "-USD", AssetClass::Crypto, 1, "USD", ""});
    return out;
}

QString key(const QString &text)
{
    QString k = text.toUpper();
    k.remove(QRegularExpression("[^A-Z0-9]"));
    return k;
}

} // namespace

const QVector<Instrument> &instruments()
{
    static const QVector<Instrument> list = build();
    return list;
}

const Instrument *findInstrument(const QString &text)
{
    static const QHash<QString, const Instrument *> index = [] {
        QHash<QString, const Instrument *> h;
        const QHash<QString, QString> aliases {
            {"GOLD", "XAUUSD"}, {"SILVER", "XAGUSD"}, {"WTI", "USOIL"}, {"XTIUSD", "USOIL"}, {"BRENT", "UKOIL"}, {"XBRUSD", "UKOIL"},
            {"SPX500", "US500"}, {"SP500", "US500"}, {"SPX", "US500"}, {"NAS100", "USTEC"}, {"NDX", "USTEC"}, {"NASDAQ", "USTEC"},
            {"DJ30", "US30"}, {"DOW", "US30"}, {"WS30", "US30"}, {"GER40", "DE40"}, {"DAX", "DE40"}, {"GER30", "DE40"},
            {"FTSE", "UK100"}, {"CAC", "FR40"}, {"NIKKEI", "JP225"}, {"XBTUSD", "BTCUSD"}, {"BITCOIN", "BTCUSD"}, {"ETHEREUM", "ETHUSD"},
        };
        for (const Instrument &i : instruments()) {
            h.insert(key(i.id), &i);
            h.insert(key(i.symbol), &i);
            h.insert(key(i.label), &i);
        }
        for (auto it = aliases.begin(); it != aliases.end(); ++it)
            if (const Instrument *target = h.value(it.value()))
                h.insert(it.key(), target);
        return h;
    }();
    const QString k = key(text);
    if (const Instrument *i = index.value(k))
        return i;
    // Broker suffixes: "EURUSDm", "EURUSD.pro", "XAUUSD_i".
    QString head = text.section('.', 0, 0).section('_', 0, 0).section('#', 0, 0);
    if (const Instrument *i = index.value(key(head)))
        return i;
    for (int cut : {1, 2, 3})
        if (k.size() > cut + 2)
            if (const Instrument *i = index.value(k.left(k.size() - cut)))
                return i;
    return nullptr;
}

QString assetClassName(AssetClass c)
{
    switch (c) {
    case AssetClass::Fx: return "FX";
    case AssetClass::Index: return "Indices";
    case AssetClass::Commodity: return "Commodities";
    case AssetClass::Stock: return "Stocks";
    case AssetClass::Crypto: return "Crypto";
    }
    return {};
}

const QStringList &majorCurrencies()
{
    static const QStringList list {"USD", "EUR", "JPY", "GBP", "CHF", "CAD", "AUD", "NZD"};
    return list;
}

QString dollarPair(const QString &currency, bool *direct)
{
    if (currency == "USD")
        return {};
    const bool d = currency == "EUR" || currency == "GBP" || currency == "AUD" || currency == "NZD";
    if (direct)
        *direct = d;
    return d ? currency + "USD=X" : currency + "=X";
}

} // namespace desk
