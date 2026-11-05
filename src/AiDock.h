#pragma once

#include <QWidget>

#include <functional>

class ChatTranscript;
class LocalChat;
class QComboBox;
class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QVariantAnimation;
class SideFlap;

// The AI side panel: a flap on the right edge of the page area (under the developer tools' one) that slides a panel
// over the right of the page, to ask a model of the slots (a local one, opencode) from anywhere in the app. It is a second view of AI › Local's
// conversation (LocalChat: one model in memory, one conversation, one journal): the model among those the bound slots
// offer, a prompt with the answers under it. Each question goes with a line saying where the user is (the page and
// what it shows), so "this chart" or "this country" mean something to the model.
class QCheckBox;
class QVBoxLayout;

class AiDock : public QWidget
{
    Q_OBJECT
public:
    // host: the widget whose right edge the dock hangs on; topOffset: the header's height. `chat` gives the
    // conversation (it creates the AI page the first time); `context` says where the user is.
    AiDock(QWidget *host, int topOffset, std::function<LocalChat *()> chat, std::function<QString()> context);

    void setOpen(bool open, bool animate = true);
    bool isOpen() const { return m_open; }
    // The page changed: the context line follows.
    void refreshContext();
    // The developer tools' flap is shown: this flap goes under it and the panel opens to its left. Hidden: this flap
    // takes its place at the top.
    void setDevFlapShown(bool shown);
    // Dev flag --ai-dock [prompt]: opens the panel and asks (loading the chosen model first).
    void ask(const QString &prompt);

signals:
    void openChanged(bool open);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void build();
    void relayout();
    void refreshModels();
    void updateState();
    void send();
    void buildVoice(QVBoxLayout *layout);
    void speakLastAnswer();

    QWidget *m_host;
    int m_topOffset;
    std::function<LocalChat *()> m_chatFor;
    std::function<QString()> m_context;
    LocalChat *m_chat = nullptr;
    SideFlap *m_flap;
    QFrame *m_panel;
    QComboBox *m_model = nullptr;
    QPushButton *m_load = nullptr;
    QLabel *m_engineState = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QLabel *m_contextLine = nullptr;
    QPushButton *m_send = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_clear = nullptr;
    QPushButton *m_talk = nullptr;       // push-to-talk, while held
    QCheckBox *m_speak = nullptr;        // read the answers aloud
    QLabel *m_voiceNote = nullptr;
    bool m_wasBusy = false;
    QLabel *m_status = nullptr;
    ChatTranscript *m_transcript = nullptr;
    QVariantAnimation *m_anim;
    double m_shown = 0;   // 0 closed .. 1 open
    bool m_open = false;
    QString m_pendingPrompt;   // ask(): sent once the model is ready
    bool m_devFlapShown = false;
};
