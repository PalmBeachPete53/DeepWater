#include "Voice.h"

#include <QLocale>
#include <QMetaObject>
#include <QPointer>

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <Speech/Speech.h>

struct Voice::Impl {
    SFSpeechRecognizer *recognizer = nil;
    SFSpeechAudioBufferRecognitionRequest *request = nil;
    SFSpeechRecognitionTask *task = nil;
    AVAudioEngine *engine = nil;
    AVSpeechSynthesizer *synth = nil;
    QString last;      // the best transcription so far
    bool sent = false; // heard() already went out for this capture
};

namespace {

NSString *languageTag()
{
    return QLocale().language() == QLocale::French ? @"fr-FR" : @"en-US";
}

} // namespace

Voice::Voice(QObject *parent) : QObject(parent), d(new Impl) {}
Voice::~Voice() { delete d; }

Voice *Voice::instance()
{
    static Voice v;
    return &v;
}

bool Voice::available() { return true; }

void Voice::startListening()
{
    if (m_wanted)
        return;
    m_wanted = true;
    // Both answers come on other queues, the first time after the system's dialogs.
    [SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus speech) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL mic) {
            const bool granted = speech == SFSpeechRecognizerAuthorizationStatusAuthorized && mic;
            QMetaObject::invokeMethod(Voice::instance(), [granted] {
                Voice *v = Voice::instance();
                if (!granted) {
                    v->m_wanted = false;
                    emit v->failed(Failure::Denied, {});
                } else if (v->m_wanted) {
                    v->beginCapture();
                }
            }, Qt::QueuedConnection);
        }];
    }];
}

void Voice::beginCapture()
{
    d->recognizer = [[SFSpeechRecognizer alloc] initWithLocale:[NSLocale localeWithLocaleIdentifier:languageTag()]];
    if (!d->recognizer || !d->recognizer.available || !d->recognizer.supportsOnDeviceRecognition) {
        m_wanted = false;
        d->recognizer = nil;
        emit failed(Failure::NotOnDevice, QString::fromNSString(languageTag()));
        return;
    }
    d->request = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
    d->request.requiresOnDeviceRecognition = YES;   // never Apple's servers
    d->request.shouldReportPartialResults = YES;
    d->engine = [[AVAudioEngine alloc] init];
    AVAudioInputNode *input = d->engine.inputNode;
    AVAudioFormat *format = [input outputFormatForBus:0];
    if (format.channelCount == 0 || format.sampleRate <= 0) {
        m_wanted = false;
        d->engine = nil;
        d->request = nil;
        emit failed(Failure::NoMicrophone, {});
        return;
    }
    d->last.clear();
    d->sent = false;
    SFSpeechAudioBufferRecognitionRequest *request = d->request;
    [input installTapOnBus:0 bufferSize:4096 format:format block:^(AVAudioPCMBuffer *buffer, AVAudioTime *) {
        [request appendAudioPCMBuffer:buffer];
    }];
    d->task = [d->recognizer recognitionTaskWithRequest:d->request resultHandler:^(SFSpeechRecognitionResult *result, NSError *error) {
        const QString text = result ? QString::fromNSString(result.bestTranscription.formattedString) : QString();
        const bool final = result.isFinal;
        const bool failedNow = error != nil;
        const QString detail = error ? QString::fromNSString(error.localizedDescription) : QString();
        QMetaObject::invokeMethod(Voice::instance(), [text, final, failedNow, detail] {
            Voice *v = Voice::instance();
            if (!text.isEmpty()) {
                v->d->last = text;
                emit v->partial(text);
            }
            if ((final || failedNow) && !v->d->sent) {
                v->d->sent = true;
                v->endCapture();
                // Silence ends in an error too: what was understood still counts.
                if (failedNow && v->d->last.isEmpty() && !detail.isEmpty())
                    emit v->failed(Failure::Other, detail);
                else
                    emit v->heard(v->d->last);
            }
        }, Qt::QueuedConnection);
    }];
    NSError *startError = nil;
    [d->engine prepare];
    if (![d->engine startAndReturnError:&startError]) {
        m_wanted = false;
        d->sent = true;
        endCapture();
        emit failed(Failure::NoMicrophone, startError ? QString::fromNSString(startError.localizedDescription) : QString());
        return;
    }
    m_listening = true;
    emit listeningChanged(true);
    if (!m_wanted)   // released during the start
        stopListening();
}

void Voice::stopListening()
{
    m_wanted = false;
    if (!d->engine)
        return;
    // The microphone closes at once; the recogniser then gives its final text.
    [d->engine stop];
    [d->engine.inputNode removeTapOnBus:0];
    [d->request endAudio];
    if (m_listening) {
        m_listening = false;
        emit listeningChanged(false);
    }
}

void Voice::endCapture()
{
    if (d->engine) {
        if (d->engine.running)
            [d->engine stop];
        [d->engine.inputNode removeTapOnBus:0];
    }
    d->engine = nil;
    d->request = nil;
    d->task = nil;
    d->recognizer = nil;
    m_wanted = false;
    if (m_listening) {
        m_listening = false;
        emit listeningChanged(false);
    }
}

void Voice::speak(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    if (!d->synth)
        d->synth = [[AVSpeechSynthesizer alloc] init];
    [d->synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    AVSpeechUtterance *utterance = [AVSpeechUtterance speechUtteranceWithString:text.toNSString()];
    utterance.voice = [AVSpeechSynthesisVoice voiceWithLanguage:languageTag()];
    [d->synth speakUtterance:utterance];
}

void Voice::stopSpeaking()
{
    if (d->synth)
        [d->synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
}
