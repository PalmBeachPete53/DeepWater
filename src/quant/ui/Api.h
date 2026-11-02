#pragma once

// Port of public/js/api.js `api`: the UI talks to the backend with the same paths and JSON as the
// browser talked to server.js ("/api/alerts", "/api/search?q=..."). Calls run the backend router
// on a worker thread and deliver the result on the UI thread, to `context`'s thread, only if
// `context` still exists. A non-2xx answer arrives in `fail` with the server's error text
// (data.error, or the body text, or "HTTP <status>"), like the JS req() throws.

#include "../core/CandleSet.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QStringList>

#include <functional>

namespace qtm {

namespace api {

using Ok = std::function<void(const QJsonValue &data)>;
using Fail = std::function<void(const QString &message)>;

void get(QObject *context, const QString &url, Ok ok, Fail fail = {});
void post(QObject *context, const QString &url, const QJsonValue &body, Ok ok, Fail fail = {});
void put(QObject *context, const QString &url, const QJsonValue &body, Ok ok, Fail fail = {});
void patch(QObject *context, const QString &url, const QJsonValue &body, Ok ok, Fail fail = {});
void del(QObject *context, const QString &url, Ok ok, Fail fail = {});
// A raw text body (CSV imports: POST /api/import/bars?name=...).
void postText(QObject *context, const QString &url, const QByteArray &body, Ok ok, Fail fail = {});

// api.candles / api.quotes / api.search from api.js. candles() skips the JSON round trip.
void candles(QObject *context, const QString &symbol, const QString &tf, int limit,
             std::function<void(const CandleSet &)> ok, Fail fail = {});
void quotes(QObject *context, const QStringList &symbols, Ok ok, Fail fail = {});
void search(QObject *context, const QString &q, Ok ok, Fail fail = {});

// Long computations (backtests, quant checks) off the UI thread: `work` runs on a worker thread,
// its result arrives in `done` the same way as the calls above, and an Error or std::exception
// it throws arrives in `fail` with its message.
void run(QObject *context, std::function<QJsonObject()> work, std::function<void(const QJsonObject &)> done,
         Fail fail = {});

} // namespace api

// api.js download(): asks where to save (QFileDialog) and writes the file.
void download(QWidget *parent, const QString &name, const QByteArray &content);

} // namespace qtm
