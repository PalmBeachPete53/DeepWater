#pragma once

#include "fusion/Fusion.h"

#include <QJsonObject>

#include <optional>

// The golden records on disk: one file per country and indicator, <dir>/<ISO3>/<indicator>.json, and the sources
// fusion read beside it (<indicator>.raw.json: each source's series as extracted, with when). Written whole and
// atomically; read back by the tables, the exports and the tools.
namespace fusion {

struct Record {
    QString country;            // ISO3
    QString indicator;          // the catalogue's id
    QDateTime fused;            // UTC
    QVector<Golden> golden;     // the finest frequency first
    QVector<Input> inputs;      // the sources as read (saved in the .raw.json; not loaded back by load())
};

QString recordPath(const QString &dir, const QString &country, const QString &indicator);
QString rawPath(const QString &dir, const QString &country, const QString &indicator);

QJsonObject toJson(const Golden &g);
Golden goldenFromJson(const QJsonObject &o);

// What a record brought, kept in <dir>/index.json (one entry per country and indicator, updated by save()) so the
// app can tell what fusion does without reading every record.
struct Summary {
    QString country, indicator;
    QDateTime fused;
    int records = 0;           // golden records of two sources or more
    int points = 0;            // their points
    int added = 0;             // of which from a source after the most trusted one
    int spliced = 0, unverified = 0, conflicts = 0;
    QStringList providers;     // the providers ("eurostat", "oecd": the part of a source id before ':') that gave
    QStringList labels;        // points to such a record, and their labels, in the same order
};
Summary summarize(const Record &r);
QString indexPath(const QString &dir);
// The index, written first from the records on disk when it is missing (records saved before it existed).
QVector<Summary> loadIndex(const QString &dir);

bool save(const QString &dir, const Record &r, QString *error = nullptr);
// The golden records only (not the inputs); nothing when there is no file or it can't be read.
std::optional<Record> load(const QString &dir, const QString &country, const QString &indicator);

} // namespace fusion
