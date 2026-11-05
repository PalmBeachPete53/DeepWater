#include "ToolJournal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

namespace tooljournal {

namespace {
constexpr qint64 MaxBytes = 2 * 1024 * 1024;
}

QString path()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ai/tool-journal.jsonl";
}

void append(const Entry &e)
{
    const QString p = path();
    QDir().mkpath(QFileInfo(p).path());
    if (QFileInfo(p).size() > MaxBytes) {
        QFile::remove(p + ".1");
        QFile::rename(p, p + ".1");
    }
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    const QJsonObject o {{"time", e.time.toUTC().toString(Qt::ISODateWithMs)}, {"model", e.model}, {"slot", e.slot + 1},
                         {"tool", e.tool}, {"arguments", e.arguments}, {"outcome", e.outcome}, {"detail", e.detail},
                         {"result_chars", e.resultChars}, {"ms", double(e.ms)}};
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n');
}

QVector<Entry> recent(int count)
{
    QVector<Entry> out;
    QFile f(path());
    if (!f.open(QIODevice::ReadOnly)) return out;
    // The end of the file is enough: a line is a few hundred bytes.
    const qint64 tail = qint64(count) * 1024;
    if (f.size() > tail) f.seek(f.size() - tail);
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &line : lines) {
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        if (o.isEmpty()) continue;   // also the first, cut line of the tail
        Entry e;
        e.time = QDateTime::fromString(o.value("time").toString(), Qt::ISODateWithMs);
        e.model = o.value("model").toString();
        e.slot = o.value("slot").toInt() - 1;
        e.tool = o.value("tool").toString();
        e.arguments = o.value("arguments").toObject();
        e.outcome = o.value("outcome").toString();
        e.detail = o.value("detail").toString();
        e.resultChars = o.value("result_chars").toInt();
        e.ms = qint64(o.value("ms").toDouble());
        out << e;
    }
    if (out.size() > count) out = out.mid(out.size() - count);
    return out;
}

} // namespace tooljournal
