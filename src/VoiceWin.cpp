#include "Voice.h"

#include <QLocale>
#include <QTimer>

#include <windows.h>
#include <sapi.h>
#include <sperror.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Windows' own speech engines through SAPI: recognition and voices installed on this PC, in this process. Nothing goes to
// a server. A language without an installed recognizer is refused rather than sent anywhere.
struct Voice::Impl {
    ComPtr<ISpRecognizer> recognizer;
    ComPtr<ISpRecoContext> context;
    ComPtr<ISpRecoGrammar> grammar;
    ComPtr<ISpVoice> voice;
    QTimer poll;
    QString finished;   // the phrases recognised so far
    QString hearing;    // the phrase under way
    bool ending = false;
};

namespace {

const wchar_t *languageAttribute()
{
    return QLocale().language() == QLocale::French ? L"Language=40c" : L"Language=409";
}

QString languageTag()
{
    return QLocale().language() == QLocale::French ? QStringLiteral("fr-FR") : QStringLiteral("en-US");
}

// The first token of a category, for a language if given: its engine or its device.
ComPtr<ISpObjectToken> firstToken(const wchar_t *category, const wchar_t *attributes)
{
    ComPtr<ISpObjectTokenCategory> tokens;
    if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&tokens)))
        || FAILED(tokens->SetId(category, FALSE)))
        return {};
    ComPtr<IEnumSpObjectTokens> list;
    if (FAILED(tokens->EnumTokens(attributes, nullptr, &list)) || !list)
        return {};
    ComPtr<ISpObjectToken> token;
    ULONG fetched = 0;
    if (FAILED(list->Next(1, &token, &fetched)) || fetched == 0)
        return {};
    return token;
}

ComPtr<ISpObjectToken> defaultMicrophone()
{
    ComPtr<ISpObjectTokenCategory> tokens;
    if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&tokens)))
        || FAILED(tokens->SetId(SPCAT_AUDIOIN, FALSE)))
        return {};
    wchar_t *id = nullptr;
    if (FAILED(tokens->GetDefaultTokenId(&id)) || !id)
        return {};
    ComPtr<ISpObjectToken> token;
    const bool ok = SUCCEEDED(CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&token)))
                    && SUCCEEDED(token->SetId(nullptr, id, FALSE));
    CoTaskMemFree(id);
    return ok ? token : ComPtr<ISpObjectToken>();
}

QString resultText(ISpRecoResult *result)
{
    wchar_t *text = nullptr;
    if (FAILED(result->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &text, nullptr)) || !text)
        return {};
    const QString out = QString::fromWCharArray(text);
    CoTaskMemFree(text);
    return out;
}

// SAPI's SpClearEvent, which lives in a header that needs ATL.
void clearEvent(SPEVENT &event)
{
    if (event.elParamType == SPET_LPARAM_IS_OBJECT || event.elParamType == SPET_LPARAM_IS_TOKEN) {
        if (event.lParam)
            reinterpret_cast<IUnknown *>(event.lParam)->Release();
    } else if (event.elParamType == SPET_LPARAM_IS_POINTER || event.elParamType == SPET_LPARAM_IS_STRING) {
        CoTaskMemFree(reinterpret_cast<void *>(event.lParam));
    }
}

} // namespace

Voice::Voice(QObject *parent) : QObject(parent), d(new Impl)
{
    d->poll.setInterval(100);
    connect(&d->poll, &QTimer::timeout, this, [this] {
        if (!d->context)
            return;
        SPEVENT event;
        ULONG fetched = 0;
        while (SUCCEEDED(d->context->GetEvents(1, &event, &fetched)) && fetched == 1) {
            if ((event.eEventId == SPEI_HYPOTHESIS || event.eEventId == SPEI_RECOGNITION)
                && event.elParamType == SPET_LPARAM_IS_OBJECT) {
                const QString text = resultText(reinterpret_cast<ISpRecoResult *>(event.lParam));
                if (event.eEventId == SPEI_RECOGNITION) {
                    d->finished = (d->finished + ' ' + text).trimmed();
                    d->hearing.clear();
                } else {
                    d->hearing = text;
                }
                emit partial((d->finished + ' ' + d->hearing).trimmed());
            }
            clearEvent(event);
        }
    });
}

Voice::~Voice()
{
    delete d;
}

Voice *Voice::instance()
{
    static Voice v;
    return &v;
}

bool Voice::available()
{
    return true;
}

void Voice::startListening()
{
    if (m_wanted)
        return;
    m_wanted = true;
    beginCapture();
}

void Voice::beginCapture()
{
    const ComPtr<ISpObjectToken> engine = firstToken(SPCAT_RECOGNIZERS, languageAttribute());
    if (!engine) {
        m_wanted = false;
        emit failed(Failure::NotOnDevice, languageTag());
        return;
    }
    const ComPtr<ISpObjectToken> microphone = defaultMicrophone();
    if (!microphone) {
        m_wanted = false;
        emit failed(Failure::NoMicrophone, {});
        return;
    }
    const auto refuse = [this](HRESULT hr) {
        endCapture();
        // E_ACCESSDENIED: Windows' privacy settings keep the microphone from desktop apps.
        emit failed(hr == E_ACCESSDENIED ? Failure::Denied : Failure::NoMicrophone, QString::number(uint(hr), 16));
    };
    HRESULT hr = CoCreateInstance(CLSID_SpInprocRecognizer, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&d->recognizer));
    if (SUCCEEDED(hr)) hr = d->recognizer->SetRecognizer(engine.Get());
    if (SUCCEEDED(hr)) hr = d->recognizer->SetInput(microphone.Get(), TRUE);
    if (SUCCEEDED(hr)) hr = d->recognizer->CreateRecoContext(&d->context);
    if (SUCCEEDED(hr)) {
        const ULONGLONG interest = SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS);
        hr = d->context->SetInterest(interest, interest);
    }
    if (SUCCEEDED(hr)) hr = d->context->CreateGrammar(1, &d->grammar);
    if (SUCCEEDED(hr)) hr = d->grammar->LoadDictation(nullptr, SPLO_STATIC);
    if (SUCCEEDED(hr)) hr = d->grammar->SetDictationState(SPRS_ACTIVE);
    if (FAILED(hr)) {
        refuse(hr);
        return;
    }
    d->finished.clear();
    d->hearing.clear();
    d->ending = false;
    d->poll.start();
    m_listening = true;
    emit listeningChanged(true);
}

void Voice::stopListening()
{
    m_wanted = false;
    if (!d->grammar || d->ending)
        return;
    // The microphone stops at once; the recogniser gets a moment to give what it was still working on.
    d->ending = true;
    d->grammar->SetDictationState(SPRS_INACTIVE);
    if (m_listening) {
        m_listening = false;
        emit listeningChanged(false);
    }
    QTimer::singleShot(500, this, [this] {
        const QString text = (d->finished + ' ' + d->hearing).trimmed();
        endCapture();
        emit heard(text);
    });
}

void Voice::endCapture()
{
    d->poll.stop();
    d->grammar.Reset();
    d->context.Reset();
    d->recognizer.Reset();
    d->ending = false;
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
    if (!d->voice && FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&d->voice))))
        return;
    if (const ComPtr<ISpObjectToken> voice = firstToken(SPCAT_VOICES, languageAttribute()))
        d->voice->SetVoice(voice.Get());
    d->voice->Speak(reinterpret_cast<const wchar_t *>(text.utf16()), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, nullptr);
}

void Voice::stopSpeaking()
{
    if (d->voice)
        d->voice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}
