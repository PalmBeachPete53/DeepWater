#pragma once

#include "NewsData.h"
#include "desk/CentralBanks.h"
#include "desk/Market.h"

#include <QDate>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <functional>

// The Desk's downloads, all through the Research client: its cache, its pace, its per-host switches in Sources.
namespace deskdata {

using Candles = QHash<QString, QVector<desk::Candle>>;
// Every symbol's candles, then `done` once; `progress` after each answer. A symbol that fails is absent.
void loadCandles(const QStringList &symbols, const QString &range, const QString &interval, QObject *ctx,
                 std::function<void(const Candles &, const QString &firstError)> done,
                 std::function<void(int finished, int total)> progress = {});

// A feed read as news items. `group` is the Sources group ("investinglive", "ing", "fed", "ecb", "boe", "bis").
void loadFeed(const QString &group, const QString &url, qint64 maxAgeSecs, QObject *ctx,
              std::function<void(const QVector<news::Item> &, const QString &error)> done);

// The dates of a central bank's decisions, by currency, sorted; nothing when its page changed. The RBA's and the RBNZ's
// are kept in the app: their sites refuse programs.
void loadMeetings(const QString &currency, QObject *ctx, std::function<void(const QVector<QDate> &)> done);

// What the futures price for the Fed's meetings, with the meetings of its calendar. A path without meetings: see `error`.
void loadFedPath(QObject *ctx, std::function<void(const desk::FedPath &, const QVector<QDate> &meetings, const QString &error)> done);
// The Bank of England's daily OIS curve; no points when unavailable.
void loadBoeOis(QObject *ctx, std::function<void(const desk::OisCurve &)> done);

// Binance's public share of accounts long on a USDT perpetual, in %, a point a day over 30 days, oldest first.
void loadLongShare(const QString &pair, QObject *ctx, std::function<void(const QVector<double> &, const QString &error)> done);

} // namespace deskdata
