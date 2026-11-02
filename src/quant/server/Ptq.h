#pragma once

// Port of lib/ptq.js, the PTQ connector. If you run PTQ (part-timequant.com) on this computer,
// Quant Terminal talks to it directly: it sends a strategy tester strategy to PTQ for its own
// backtest and luck checks, and shows what PTQ holds and has armed. PTQ keeps the licence, the
// engine and your exchange keys. The terminal never places a live order, never arms a strategy and
// never sees a key: it can only make PTQ do LESS (engage its stop, disarm a strategy).

#include "../core/Error.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace qtm {

// fail(message, status, code), plus the problems/notes check() attaches.
class PtqError : public Error
{
public:
    PtqError(const QString &message, int status, const QString &code, QStringList problems = {}, QStringList notes = {})
        : Error(message, status), m_code(code), m_problems(std::move(problems)), m_notes(std::move(notes))
    {
    }
    QString code() const { return m_code; }
    QStringList problems() const { return m_problems; }
    QStringList notes() const { return m_notes; }

private:
    QString m_code;
    QStringList m_problems;
    QStringList m_notes;
};

struct PtqTranslation {
    QJsonObject manifest;
    QStringList problems;
    QStringList notes;
};

struct PtqTerm {
    QJsonObject term;
    QString error;          // empty = none
    QStringList notes;
};

class Ptq
{
public:
    // url: PTQ_URL (default http://127.0.0.1:8787); home: PTQ_HOME ("" = not set).
    Ptq(const QString &url, const QString &home);
    static Ptq fromEnv();

    static const QString SITE;
    QString url() const { return m_url; }

    struct Lease {
        QString lease;      // "" = null
        QJsonValue exp;     // number or null
        bool activated = false;
    };
    Lease leaseInfo() const;

    // call(method, path, body, timeoutMs): throws PtqError.
    QJsonValue call(const QByteArray &method, const QString &p, const QJsonValue &body = QJsonValue(QJsonValue::Undefined),
                    int timeoutMs = 20000) const;

    QJsonObject status() const;
    static PtqTerm toTerm(const QJsonValue &operand, const QJsonObject &indicators = {});
    static PtqTranslation translate(const QJsonObject &strategy, const QJsonValue &tf, const QJsonValue &name);
    QJsonObject resolveInstrument(const QString &symbolId) const;
    QJsonObject check(const QJsonObject &body) const;
    QJsonObject overview() const;
    QJsonValue engageKillSwitch() const;
    QJsonValue disarm(const QString &id) const;

private:
    QString m_url;
    QString m_home;
};

} // namespace qtm
