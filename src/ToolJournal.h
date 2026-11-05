#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

// The journal of the tool calls of local models (AI › Local): every call a model makes, run or refused, one JSON line
// per call in <app data>/ai/tool-journal.jsonl, kept by the app (the model's process can't write). Rotated at 2 MB
// (the previous file is kept as .1).
namespace tooljournal {

struct Entry {
    QDateTime time;
    QString model;          // "org/name"
    int slot = -1;          // 0-based
    QString tool;
    QJsonObject arguments;
    QString outcome;        // "ok", "refused", "error"
    QString detail;         // the error, or a short description of the result
    int resultChars = 0;    // what went back to the model
    qint64 ms = 0;
};

QString path();
void append(const Entry &e);
QVector<Entry> recent(int count);   // newest last

} // namespace tooljournal
