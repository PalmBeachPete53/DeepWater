#pragma once

#include "LocalEngine.h"
#include "LocalTools.h"
#include "ToolJournal.h"

#include <QComboBox>
#include <QJsonArray>
#include <QTextEdit>

class ModelSlotsView;
class OpencodeRun;

// The conversation with a model of the slots, shared by AI › Local and the AI side panel (AiDock): one engine (one model in
// memory), one conversation, two views. It offers the models of the bound slots and opencode when a slot holds it, loads the
// model chosen in the sandboxed engine (LocalEngine; opencode needs no loading, OpencodeRun), sends the questions, runs the
// tools of the slot's skills the model calls (LocalTools: checked against the slot at every call, at most a few rounds per
// question) and writes every call to the journal (ToolJournal). The views draw its entries (ChatTranscript) and its status.
class LocalChat : public QObject
{
    Q_OBJECT
public:
    // What the transcript shows, in order: a question, a stretch of answer (several per question when the model calls
    // tools in between), a tool call.
    struct Entry {
        enum Kind { User, Answer, Tool };
        Kind kind = User;
        QString header;     // "You", the model's name; empty for the stretches after the first of an answer
        QString text;       // the question, the answer (tool calls taken out), or the tool line
        QString thinking;   // the model's thinking (between <think> and </think>)
        QString context;    // a question's context line (where the user was in the app), shown under it
        bool writing = false;   // the answer is still being written
        bool calling = false;   // ... and a tool call is being written
        bool error = false;     // a tool call that failed or was refused
    };
    // A model the bound slots offer.
    struct Choice {
        int slot = -1;
        QString model;   // "org/name"
        QString file;    // its GGUF (the first part of a split one), or modelslots::OpencodeId
        QString label;   // "Slot 1 · name · file.gguf"
    };

    LocalChat(ModelSlotsView *slotsView, const MlDataExporter::Sources &data, QObject *parent = nullptr);

    LocalEngine *engine() const { return m_engine; }
    bool isOpencode() const;           // opencode is the choice, not a model of this computer
    // Where the conversation stands, for the model chosen: Ready when a question can be sent.
    LocalEngine::State state() const;
    // The model opencode answers with (ai/opencodeModel; empty: the one it is set to use), and those it offers.
    QString opencodeModel() const;
    void setOpencodeModel(const QString &model);
    QStringList opencodeModels() const { return m_opencodeModels; }
    void refreshOpencodeModels();      // asks opencode again, at most every 30 s
    ModelSlotsView *slotsView() const { return m_slots; }
    QVector<Choice> choices() const { return m_choices; }
    QString selected() const { return m_selected; }   // the file of the chosen model (ai/localModel)
    void select(const QString &file);
    // Loads the chosen model in a context of ai/localContext tokens; a status says why not.
    void load();
    void unload();
    // Sends a question; `context` (where the user is in the app, no data: a model reads a table through the read_page tool
    // of its skill) goes to the model before it and shows under it.
    void send(const QString &text, const QString &context = {});
    void stop();
    void clear();   // a new conversation
    // Dev flag --ai-local: loads the chosen model (when none is) and sends the prompt once it is ready.
    void runPrompt(const QString &prompt, const QString &context = {});

    QStringList grantedSkills() const;   // the skills of the loaded model's slot, now (none when it may no longer run)
    int loadedSlot() const { return m_loadedSlot; }
    const QVector<Entry> &entries() const { return m_entries; }
    QString status() const { return m_status; }   // rich text
    bool statusIsError() const { return m_statusError; }
    // An entry as rich text for the transcript, its header and its body apart (each empty when there is nothing to
    // show yet).
    static QString renderHeader(const Entry &e);
    static QString renderBody(const Entry &e);

signals:
    void choicesChanged();
    void selectionChanged();
    void stateChanged();
    void entryAdded(int index);
    void entryChanged(int index);   // the last one: an answer being written
    void cleared();
    void statusChanged();
    void journalAdded(const tooljournal::Entry &e);
    void opencodeModelsChanged();

private:
    void refreshChoices();
    void ask();
    void fail(const QString &error);
    int activeSlot() const;
    QString activeModel() const;   // the name in the journal
    void onToken(const QString &text);
    void answered(const QJsonObject &stats);
    void runCalls(QVector<LocalTools::Call> calls, QVector<QJsonObject> results);
    void finishTurn(const QString &note);
    void setStatus(const QString &text, bool error = false);
    void addEntry(const Entry &e);
    void updateAnswer(bool writing);

    static constexpr int MaxRounds = 6;

    ModelSlotsView *m_slots;
    LocalEngine *m_engine;
    OpencodeRun *m_opencode;
    QStringList m_opencodeTold;    // the skills opencode's session was last told about
    bool m_opencodePrompted = false;
    QStringList m_opencodeModels;
    qint64 m_opencodeModelsAt = 0;
    LocalTools *m_tools;
    QVector<Choice> m_choices;
    QString m_selected;
    int m_loadedSlot = -1;
    QString m_loadedFile;
    QVector<Entry> m_entries;
    QString m_status;
    bool m_statusError = false;
    QJsonArray m_messages;   // the conversation, the answers without their thinking
    QString m_pending;       // the answer being written, as the model writes it
    int m_answer = -1;       // its entry
    QString m_devPrompt, m_devContext;
    int m_turnStart = 0;     // m_messages' size before the question being answered
    int m_round = 0;         // the rounds of tool calls of that question
    int m_toolCalls = 0;
    int m_orders = 0;        // paper orders placed, closed or cancelled for that question (LocalTools::MaxOrders)
    QString m_question;      // the user's last question: a tool checks what a model wrote against it
    bool m_cancelled = false;
    int m_failedRounds = 0;  // rounds in a row where every call failed or was refused
};

// The model opencode answers with, for the views of LocalChat: shown while opencode is the choice.
class OpencodeModelBox : public QComboBox
{
public:
    explicit OpencodeModelBox(LocalChat *chat, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;

private:
    void rebuild();

    LocalChat *m_chat;
};

// A conversation of LocalChat, drawn as it goes: the entries added at the end, the answer being written redrawn in
// place, the view kept at the bottom while it was there.
class ChatTranscript : public QTextEdit
{
    Q_OBJECT
public:
    explicit ChatTranscript(LocalChat *chat, QWidget *parent = nullptr);

private:
    void rebuild();
    void renderFrom(int index);

    LocalChat *m_chat;
    QVector<int> m_starts;   // where each entry starts in the document (its block separator included)
};
