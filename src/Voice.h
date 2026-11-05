#pragma once

#include <QObject>
#include <QString>

// Talking to the local model, on this computer only (macOS: Apple's on-device recognition and voices; Windows: SAPI's): push-to-talk,
// nothing listens unless the button is held, no sound or text leaves the machine. When a language has no on-device
// recognition the capture refuses rather than fall back on Apple's servers. Elsewhere than macOS it isn't available.
class Voice : public QObject
{
    Q_OBJECT
public:
    enum class Failure { Denied, NotOnDevice, NoMicrophone, Other };

    static Voice *instance();
    static bool available();   // built for a platform that has it

    // The first use asks the system for the microphone and speech recognition permissions.
    void startListening();     // the button went down
    void stopListening();      // the button came up: what was heard ends with heard()
    bool isListening() const { return m_listening; }

    void speak(const QString &text);
    void stopSpeaking();

signals:
    void partial(const QString &text);
    void heard(const QString &text);    // final; empty when nothing was understood
    void failed(Voice::Failure why, const QString &detail);
    void listeningChanged(bool listening);

private:
    explicit Voice(QObject *parent = nullptr);
    ~Voice() override;
    void beginCapture();
    void endCapture();

    struct Impl;
    Impl *d = nullptr;
    bool m_wanted = false;     // still held: permissions answer later than the button may come up
    bool m_listening = false;
};
