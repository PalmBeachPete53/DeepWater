#pragma once

#include "ResearchData.h"

#include <QColor>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// The Economy page's catalogue: its sections, the indicators they show and where each one comes from (FRED for
// most, the OECD, Yahoo Finance, CoinMarketCap and alternative.me), and the stock market Fear & Greed estimate.
namespace economy {

using research::Point;
using research::Series;

enum class Source {
    Fred,             // code: the FRED series id
    Oecd,             // code: an OECD KEI flow key
    Yahoo,            // code: a Yahoo symbol (daily closes)
    StockFearGreed,   // Deepwater's estimate, see stockFearGreed()
    CryptoFearGreed,  // alternative.me
    AltcoinSeason,    // CoinMarketCap
    CryptoCap,        // CoinMarketCap, total market cap
    BtcDominance,     // CoinMarketCap, bitcoin's share
};

enum class Unit { Number, Percent, Dollars, Count, Yen };

enum class Hue { Red, Green, Amber, Purple, Teal, Orange, Yellow, Pink, Mint };

// Bands drawn behind a 0…100 index: the Fear & Greed classes, or the bitcoin / altcoin seasons.
enum class Zones { None, FearGreed, AltSeason };

struct Indicator {
    const char *title = "";   // English; translated in the "Economy" context
    const char *help = "";    // the "?" tooltip, likewise
    Source source = Source::Fred;
    QString code;
    Unit unit = Unit::Number;
    double scale = 1;         // what one unit of the source is worth (FRED's "millions of dollars": 1e6)
    int decimals = 2;
    Hue hue = Hue::Amber;
    QStringList ranges;       // "1M" … "MAX"
    int range = 0;            // the one shown first
    bool zeroLine = false;    // filled against zero (spreads, stress indexes)
    Zones zones = Zones::None;
};

// One panel of a section: a chart, or a gauge (a dial of the indicator's last value).
struct Cell {
    Indicator indicator;
    bool gauge = false;
    int stretch = 1;
};

struct Section {
    const char *name;               // English; translated in the "Economy" context
    QVector<QVector<Cell>> rows;    // top to bottom, each left to right
};

const QVector<Section> &sections();

QString text(const char *english);   // translated in the "Economy" context
QColor hueColor(Hue hue);
// Days covered by a range label ("1M" 31, "5Y" 1827…); 0 for MAX.
int rangeDays(const QString &range);

// The series an indicator shows, in its unit (scale applied), oldest first.
using Done = std::function<void(const Series &series, const QString &error)>;
void load(const Indicator &indicator, QObject *ctx, Done done);

// What a 0…100 value is called: "Extreme fear" … "Extreme greed", "Bitcoin season" … "Altcoin season".
QString zoneLabel(Zones zones, double value);
QColor zoneColor(Zones zones, double value);

// The stock market Fear & Greed index, estimated the way CNN builds its own (CNN's figures can't be read by an
// app): each component is ranked against its last 252 trading days (0 = lowest of the year, 100 = highest) and
// the index is their average. Components: momentum (S&P 500 against its 125-day average), volatility (VIX
// against its 50-day average, inverted), safe haven demand (20-day return of stocks, SPY, less bonds, TLT),
// breadth (equal-weight S&P 500, RSP, against the cap-weighted one over 20 days) and junk bond demand
// (high-yield less investment-grade spread, inverted; FRED keeps three years of it).
struct FearGreedComponent {
    QString name;
    double score = research::NaN;
};
struct FearGreed {
    Series index;
    QVector<FearGreedComponent> components;   // on the last day
};
void stockFearGreed(QObject *ctx, std::function<void(const FearGreed &value, const QString &error)> done);

} // namespace economy
