#include "Voice.h"

// Not macOS: no local recognition to rely on, and nothing goes to a server, so the AI panel hides the button.
struct Voice::Impl {};

Voice::Voice(QObject *parent) : QObject(parent) {}
Voice::~Voice() = default;
Voice *Voice::instance() { static Voice v; return &v; }
bool Voice::available() { return false; }
void Voice::startListening() {}
void Voice::stopListening() {}
void Voice::speak(const QString &) {}
void Voice::stopSpeaking() {}
void Voice::beginCapture() {}
void Voice::endCapture() {}
