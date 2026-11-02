#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

// Tiny on-disk cache for downloaded data sets: one file per data set in the
// app data folder, with the download time stored alongside the raw body.
namespace HttpCache {

bool load(const QString &name, QByteArray *body, QDateTime *fetchedAt);
// Written on a background thread, one file after the other in the order asked (a later copy is never
// overwritten by an earlier one): the UI thread doesn't wait on the disk.
void save(const QString &name, const QByteArray &body, const QDateTime &fetchedAt);
// Blocks until every save asked so far is on disk (before the app exits).
void waitForWrites();

} // namespace HttpCache
