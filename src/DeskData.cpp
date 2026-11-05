#include "DeskData.h"

#include "ResearchData.h"

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

} // namespace deskdata
