#include "DeskData.h"

#include "ResearchData.h"
#include "desk/CentralBanks.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <memory>

namespace deskdata {

void loadCandles(const QStringList &symbols, const QString &range, const QString &interval, QObject *ctx,
                 std::function<void(const Candles &, const QString &)> done, std::function<void(int, int)> progress)
{
    struct State {
        Candles out;
        QString error;
        int finished = 0, total = 0;
    };
    auto state = std::make_shared<State>();
    state->total = symbols.size();
    if (symbols.isEmpty()) {
        done({}, {});
        return;
    }
    for (const QString &symbol : symbols) {
        research::Client::instance()->candles(symbol, range, interval, ctx, [state, symbol, done, progress](const QVector<research::Bar> &bars, const QString &error) {
            QVector<desk::Candle> list;
            list.reserve(bars.size());
            for (const research::Bar &b : bars)
                if (research::ok(b.close))
                    list.push_back({b.time.toSecsSinceEpoch(), research::ok(b.open) ? b.open : b.close, research::ok(b.high) ? b.high : b.close,
                                    research::ok(b.low) ? b.low : b.close, b.close, research::ok(b.volume) ? b.volume : 0});
            if (!list.isEmpty())
                state->out.insert(symbol, list);
            else if (state->error.isEmpty())
                state->error = error;
            ++state->finished;
            if (progress)
                progress(state->finished, state->total);
            if (state->finished == state->total)
                done(state->out, state->error);
        });
    }
}

void loadFeed(const QString &group, const QString &url, qint64 maxAgeSecs, QObject *ctx,
              std::function<void(const QVector<news::Item> &, const QString &)> done)
{
    research::Client::instance()->raw(group + ":feed:" + url, QUrl(url), maxAgeSecs, false, ctx, [done](const QByteArray &body, const QString &error) {
        done(news::Wire::parseFeed(body, news::Category::General), error);
    });
}

void loadMeetings(const QString &currency, QObject *ctx, std::function<void(const QVector<QDate> &)> done)
{
    if (currency == "AUD" || currency == "NZD") {
        done(desk::bundledMeetings(currency));
        return;
    }
    struct Page { const char *key, *url; QVector<QDate> (*parse)(const QString &); };
    static const QHash<QString, Page> pages {
        {"USD", {"fed:fomc", "https://www.federalreserve.gov/monetarypolicy/fomccalendars.htm", desk::parseFomcCalendar}},
        {"EUR", {"ecb:mgcgc", "https://www.ecb.europa.eu/press/calendars/mgcgc/html/index.en.html", desk::parseEcbMeetings}},
        {"GBP", {"boe:mpc-dates", "https://www.bankofengland.co.uk/monetary-policy/upcoming-mpc-dates", desk::parseBoeMeetings}},
        {"JPY", {"boj:mpm", "https://www.boj.or.jp/en/mopo/mpmsche_minu/index.htm", desk::parseBojMeetings}},
        {"CHF", {"snb:schedule", "https://www.snb.ch/en/services-events/digital-services/event-schedule", desk::parseSnbMeetings}},
        {"CAD", {"boc:schedule", "https://www.bankofcanada.ca/core-functions/monetary-policy/key-interest-rate/", desk::parseBocMeetings}},
    };
    const auto it = pages.constFind(currency);
    if (it == pages.constEnd()) {
        done({});
        return;
    }
    research::Client::instance()->raw(it->key, QUrl(it->url), 24 * 3600, false, ctx, [parse = it->parse, done](const QByteArray &body, const QString &) {
        done(parse(QString::fromUtf8(body)));
    });
}

void loadLongShare(const QString &pair, QObject *ctx, std::function<void(const QVector<double> &, const QString &error)> done)
{
    const QString url = QString("https://fapi.binance.com/futures/data/globalLongShortAccountRatio?symbol=%1&period=1d&limit=30").arg(pair);
    research::Client::instance()->raw("binance:ratio:" + pair, QUrl(url), 3600, false, ctx, [done](const QByteArray &body, const QString &error) {
        QVector<double> longs;
        for (const QJsonValue &v : QJsonDocument::fromJson(body).array())
            longs << v.toObject().value("longAccount").toString().toDouble() * 100.0;
        done(longs, error);
    });
}

} // namespace deskdata
