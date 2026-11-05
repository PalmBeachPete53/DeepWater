#pragma once

#include "NewsData.h"
#include "desk/Market.h"

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

} // namespace deskdata
