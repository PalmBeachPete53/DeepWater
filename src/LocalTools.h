#pragma once

#include "AiRead.h"

#include "MlData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <functional>

// The tools a local model may call (AI › Local), and the broker that runs them for it. Each tool belongs to a skill
// (ModelSlots: modelslots::skills()); a model is told only of the tools of the skills its slot grants, and every call
// is checked again against the skills the slot grants at that moment: a tool of another skill, or a call once the
// slot has let the model go, is refused. The tools run in the app, never in the model's process (which can reach
// nothing), take their arguments as JSON, check them, and answer JSON kept short enough for a small context.
//
// The calls follow the Hermes convention most open models were trained on (Qwen2.5/3, Hermes, many fine-tunes): the
// tools are listed in the system prompt inside <tools></tools>, the model writes
// <tool_call>{"name": …, "arguments": {…}}</tool_call>, and the answers go back as <tool_response>…</tool_response>.
class LocalTools : public QObject
{
    Q_OBJECT
public:
    struct Tool {
        QString name;
        QString skill;          // the skill that grants it
        QString description;
        QJsonObject parameters; // JSON Schema
    };
    struct Call {
        QString name;
        QJsonObject arguments;
        QString error;          // the call couldn't be read
        QString question;       // what the user asked, when the app knows it (not through MCP)
    };
    using Done = std::function<void(const QJsonObject &result)>;   // {"error": …} when it failed or was refused

    explicit LocalTools(const MlDataExporter::Sources &sources, QObject *parent = nullptr);

    static const QVector<Tool> &tools();
    static QVector<Tool> toolsFor(const QStringList &skills);
    // The system prompt that tells the model where it is and what it can call (nothing about tools without any).
    // `local`: a model run here; else one outside the app (opencode), told that only these tools exist.
    // read_page and open_page, from MainWindow: reading never moves the user and refuses Sources and AI; only through these
    // tools does an AI reach the app's tables and charts, never in a prompt (AiRead.h).
    using PageReader = std::function<void(const AiPageRequest &request, std::function<void(const AiPageRead &)> done)>;
    using PageOpener = std::function<QString(const AiPageRequest &request, QString *error)>;   // where the user now is
    static void setPageAccess(PageReader reader, PageOpener opener);
    // The Organize skill's tools, from MainWindow: Desk workspaces and the Journal.
    using Organizer = std::function<void(const QString &tool, const QJsonObject &arguments, const QString &question, Done done)>;
    static void setOrganizer(Organizer organizer);
    static QString systemPrompt(const QStringList &skills, const QString &userPrompt, bool local = true);
    // The tool calls written in an answer (without its thinking), in order.
    static QVector<Call> parseCalls(const QString &answer);
    // The answer without its tool calls, as shown.
    static QString withoutCalls(const QString &answer);
    // The message that hands the results back.
    static QString responses(const QVector<QJsonObject> &results);

    // Runs a call when one of `skills` grants its tool; `done` gets the result (now, or later: prices, backtests, orders).
    void call(const Call &call, const QStringList &skills, const Done &done);
    // Whether a tool places, closes or cancels paper orders (counted against MaxOrders per question).
    static bool isOrder(const QString &tool);

    static constexpr int MaxResultChars = 6000;
    static constexpr int MaxOrders = 5;
    // Where backtest_trades_csv writes its exports.
    static QString exportsDir();

private:
    QJsonObject listData() const;
    void readPage(const QJsonObject &a, const Done &done) const;
    QJsonObject openPage(const QJsonObject &a) const;
    AiPageRequest pageRequest(const QJsonObject &a) const;
    QJsonObject countrySeries(const QJsonObject &a) const;
    QJsonObject weo(const QJsonObject &a) const;
    QJsonObject policyRate(const QJsonObject &a) const;
    QJsonObject fxRate(const QJsonObject &a) const;
    QJsonObject cot(const QJsonObject &a) const;
    QJsonObject golden(const QJsonObject &a) const;
    void prices(const QJsonObject &a, const Done &done);
    void trade(const Call &c, const Done &done);
    // ISO3 for a code (ISO3 or ISO2) or an English name; empty when unknown.
    QString country(const QString &text) const;

    MlDataExporter::Sources m_s;
};
