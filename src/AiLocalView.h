#pragma once

#include "ToolJournal.h"

#include <QWidget>

class LocalChat;
class ChatTranscript;
class QTableWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// AI › Local: the conversation with a model of this computer or opencode (LocalChat, shared with the AI side panel), with the
// engine's controls: the model among those the bound slots offer, the context size, load and unload, the system
// prompt, the skills of the slot, and the journal of every tool call (ToolJournal).
class AiLocalView : public QWidget
{
    Q_OBJECT
public:
    explicit AiLocalView(LocalChat *chat, QWidget *parent = nullptr);

    // A model's description and state, shared with the side panel: "✓ name · 6 billion parameters · Metal GPU…",
    // and its tone ("good", "bad", "dim") for the slotState label style.
    static QString engineState(LocalChat *chat, QString *tone);

private:
    void refreshModels();
    void updateState();
    void send();
    void addJournalRow(const tooljournal::Entry &e);

    LocalChat *m_chat;
    QComboBox *m_model;
    QComboBox *m_context;
    QPushButton *m_load;
    QLabel *m_engineState;
    QLabel *m_skills;
    QLineEdit *m_system;
    ChatTranscript *m_transcript;
    QPlainTextEdit *m_input;
    QPushButton *m_send;
    QPushButton *m_stop;
    QPushButton *m_clear;
    QLabel *m_status;
    QTableWidget *m_journal;
};
