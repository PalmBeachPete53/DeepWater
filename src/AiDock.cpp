#include "AiDock.h"

#include "AiLocalView.h"
#include "LocalChat.h"
#include "LocalEngine.h"
#include "SideFlap.h"
#include "Voice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QRegion>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace {
constexpr int PanelWidth = 440;
constexpr int FlapGap = 10;   // between the developer tools' flap and this one
}

AiDock::AiDock(QWidget *host, int topOffset, std::function<LocalChat *()> chat, std::function<QString()> context)
    : QWidget(host)
    , m_host(host)
    , m_topOffset(topOffset)
    , m_chatFor(std::move(chat))
    , m_context(std::move(context))
{
    setObjectName("aiDock");
    m_flap = new SideFlap(tr("AI"), this);
    connect(m_flap, &SideFlap::clicked, this, [this] { setOpen(!m_open); });

    m_panel = new QFrame(this);
    m_panel->setObjectName("aiDockPanel");
    m_panel->hide();

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_shown = v.toDouble();
        relayout();
    });
    connect(m_anim, &QVariantAnimation::finished, this, [this] {
        if (m_open && m_input) m_input->setFocus();
    });
    host->installEventFilter(this);
    relayout();
}

// The panel's contents, the first time it opens: the conversation needs the AI page (its slots).
void AiDock::build()
{
    if (m_chat) return;
    m_chat = m_chatFor();
    if (!m_chat) return;

    auto *layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *bar = new QWidget;
    bar->setObjectName("devTabBar");
    bar->setFixedHeight(36);
    auto *barRow = new QHBoxLayout(bar);
    barRow->setContentsMargins(14, 0, 6, 0);
    auto *title = new QLabel(tr("Ask the AI").toUpper());
    title->setObjectName("sectionLabel");
    barRow->addWidget(title);
    barRow->addStretch(1);
    auto *close = sidePanelCloseButton();
    close->setToolTip(tr("Close"));
    connect(close, &QToolButton::clicked, this, [this] { setOpen(false); });
    barRow->addWidget(close);
    layout->addWidget(bar);

    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);
    auto *modelRow = new QHBoxLayout;
    modelRow->setSpacing(6);
    m_model = new QComboBox;
    m_model->setObjectName("sourceCombo");
    m_model->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_model->setMinimumContentsLength(12);
    modelRow->addWidget(m_model, 1);
    m_load = new QPushButton(tr("Load"));
    m_load->setObjectName("segButton");
    m_load->setCursor(Qt::PointingHandCursor);
    modelRow->addWidget(m_load);
    v->addLayout(modelRow);
    v->addWidget(new OpencodeModelBox(m_chat));
    m_engineState = new QLabel;
    m_engineState->setObjectName("slotState");
    m_engineState->setWordWrap(true);
    v->addWidget(m_engineState);

    m_input = new QPlainTextEdit;
    m_input->setObjectName("aiInput");
    m_input->setPlaceholderText(tr("Ask from anywhere in the app · Cmd+Enter sends"));
    m_input->setFixedHeight(92);
    v->addWidget(m_input);
    m_contextLine = new QLabel;
    m_contextLine->setObjectName("dim");
    m_contextLine->setWordWrap(true);
    m_contextLine->setToolTip(tr("Sent with the question, so the model knows what \"this\" is"));
    v->addWidget(m_contextLine);
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    m_send = new QPushButton(tr("Send"));
    m_send->setObjectName("primaryButton");
    m_send->setCursor(Qt::PointingHandCursor);
    m_stop = new QPushButton(tr("Stop"));
    m_stop->setObjectName("segButton");
    m_stop->setCursor(Qt::PointingHandCursor);
    m_clear = new QPushButton(tr("New conversation"));
    m_clear->setObjectName("segButton");
    m_clear->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_send);
    buttons->addWidget(m_stop);
    buttons->addStretch(1);
    buttons->addWidget(m_clear);
    v->addLayout(buttons);
    if (Voice::available())
        buildVoice(v);
    m_status = new QLabel;
    m_status->setObjectName("dim");
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::RichText);
    v->addWidget(m_status);
    m_transcript = new ChatTranscript(m_chat);
    v->addWidget(m_transcript, 1);
    layout->addWidget(body, 1);

    connect(m_model, &QComboBox::currentIndexChanged, this, [this] {
        if (m_model->currentIndex() >= 0) m_chat->select(m_model->currentData().toString());
    });
    connect(m_load, &QPushButton::clicked, this, [this] {
        if (m_chat->engine()->state() == LocalEngine::State::Off) m_chat->load();
        else m_chat->unload();
    });
    connect(m_send, &QPushButton::clicked, this, &AiDock::send);
    auto *shortcut = new QShortcut(QKeySequence("Ctrl+Return"), m_input);
    connect(shortcut, &QShortcut::activated, this, &AiDock::send);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), m_panel);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, [this] { setOpen(false); });
    connect(m_stop, &QPushButton::clicked, m_chat, &LocalChat::stop);
    connect(m_clear, &QPushButton::clicked, m_chat, &LocalChat::clear);
    connect(m_chat, &LocalChat::choicesChanged, this, &AiDock::refreshModels);
    connect(m_chat, &LocalChat::selectionChanged, this, &AiDock::refreshModels);
    connect(m_chat, &LocalChat::stateChanged, this, &AiDock::updateState);
    connect(m_chat, &LocalChat::statusChanged, this, [this] { m_status->setText(m_chat->status()); });
    refreshModels();
    m_status->setText(m_chat->status());
    refreshContext();
}

void AiDock::refreshModels()
{
    m_model->blockSignals(true);
    m_model->clear();
    for (const LocalChat::Choice &c : m_chat->choices()) {
        m_model->addItem(c.label, c.file);
        m_model->setItemData(m_model->count() - 1, c.label, Qt::ToolTipRole);
    }
    m_model->setCurrentIndex(m_model->findData(m_chat->selected()));
    m_model->blockSignals(false);
    updateState();
}

void AiDock::updateState()
{
    if (!m_chat) return;
    const LocalEngine::State s = m_chat->state();
    const bool off = m_chat->engine()->state() == LocalEngine::State::Off;
    m_model->setEnabled(off && s != LocalEngine::State::Busy);
    m_load->setText(off ? tr("Load") : tr("Unload"));
    m_load->setVisible(!m_chat->isOpencode());   // nothing to load
    m_load->setEnabled(LocalEngine::available() && (!off || m_model->count() > 0));
    m_send->setEnabled(s == LocalEngine::State::Ready);
    m_stop->setEnabled(s == LocalEngine::State::Busy);
    // A question asked before the model was ready (--ai-dock): sent now, with where the user is now.
    if (s == LocalEngine::State::Ready && !m_pendingPrompt.isEmpty()) {
        const QString prompt = m_pendingPrompt;
        m_pendingPrompt.clear();
        refreshContext();
        m_chat->send(prompt, m_context ? m_context() : QString());
    }
    QString tone;
    m_engineState->setText(AiLocalView::engineState(m_chat, &tone));
    m_engineState->setProperty("tone", tone);
    m_engineState->style()->unpolish(m_engineState);
    m_engineState->style()->polish(m_engineState);
}

void AiDock::refreshContext()
{
    if (!m_contextLine) return;
    const QString context = m_context ? m_context() : QString();
    m_contextLine->setText(context.isEmpty() ? QString() : tr("Context: %1").arg(context));
}

// Push-to-talk: nothing listens unless the button is held; recognition and voices stay on this Mac (Voice).
void AiDock::buildVoice(QVBoxLayout *layout)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    m_talk = new QPushButton(tr("Hold to talk"));
    m_talk->setObjectName("segButton");
    m_talk->setCursor(Qt::PointingHandCursor);
    m_talk->setToolTip(tr("Listens only while held. Recognition runs on this Mac: no sound or text leaves it."));
    row->addWidget(m_talk);
    m_speak = new QCheckBox(tr("Read the answers aloud"));
    m_speak->setObjectName("qtCheck");
    m_speak->setChecked(QSettings().value("ai/speakAnswers", false).toBool());
    row->addWidget(m_speak);
    row->addStretch(1);
    layout->addLayout(row);
    m_voiceNote = new QLabel;
    m_voiceNote->setObjectName("dim");
    m_voiceNote->setWordWrap(true);
    m_voiceNote->hide();
    layout->addWidget(m_voiceNote);

    Voice *voice = Voice::instance();
    connect(m_talk, &QPushButton::pressed, this, [this, voice] {
        m_voiceNote->hide();
        voice->stopSpeaking();
        voice->startListening();
    });
    connect(m_talk, &QPushButton::released, voice, &Voice::stopListening);
    connect(voice, &Voice::listeningChanged, this, [this](bool on) { m_talk->setText(on ? tr("Listening…") : tr("Hold to talk")); });
    connect(voice, &Voice::partial, this, [this](const QString &text) { m_input->setPlainText(text); });
    connect(voice, &Voice::heard, this, [this](const QString &text) {
        if (text.trimmed().isEmpty()) {
            m_voiceNote->setText(tr("Nothing was understood."));
            m_voiceNote->show();
            return;
        }
        m_input->setPlainText(text);
        send();   // the text stays in the box when the model isn't ready
    });
    connect(voice, &Voice::failed, this, [this](Voice::Failure why, const QString &detail) {
        QString text;
        switch (why) {
        case Voice::Failure::Denied:
            text = tr("Deepwater may not use the microphone or speech recognition: allow them in System Settings › Privacy & Security.");
            break;
        case Voice::Failure::NotOnDevice:
            text = tr("This Mac has no on-device recognition for %1 yet: turn on Dictation for that language in System Settings › Keyboard. Nothing is sent to a server instead.").arg(detail);
            break;
        case Voice::Failure::NoMicrophone:
            text = tr("No microphone is available. %1").arg(detail);
            break;
        case Voice::Failure::Other:
            text = tr("Recognition failed: %1").arg(detail);
            break;
        }
        m_voiceNote->setText(text);
        m_voiceNote->show();
    });
    connect(m_speak, &QCheckBox::toggled, this, [voice](bool on) {
        QSettings().setValue("ai/speakAnswers", on);
        if (!on)
            voice->stopSpeaking();
    });
    connect(m_chat, &LocalChat::stateChanged, this, [this] {
        const bool busy = m_chat->state() == LocalEngine::State::Busy;
        if (m_wasBusy && !busy && m_speak->isChecked())
            speakLastAnswer();
        m_wasBusy = busy;
    });
}

void AiDock::speakLastAnswer()
{
    // The stretches of the last answer, tool calls taken out, without the markdown a voice would read out.
    QStringList parts;
    const auto &entries = m_chat->entries();
    for (int i = entries.size() - 1; i >= 0 && entries[i].kind != LocalChat::Entry::User; --i)
        if (entries[i].kind == LocalChat::Entry::Answer && !entries[i].error)
            parts.prepend(entries[i].text);
    static const QRegularExpression marks("[*_`#>|]+");
    Voice::instance()->speak(parts.join(' ').replace(marks, " "));
}

void AiDock::send()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_chat->state() != LocalEngine::State::Ready) return;
    refreshContext();
    m_input->clear();
    m_chat->send(text, m_context ? m_context() : QString());
}

void AiDock::ask(const QString &prompt)
{
    setOpen(true, false);
    if (!m_chat) return;
    m_pendingPrompt = prompt;
    if (m_chat->state() == LocalEngine::State::Off) m_chat->load();
    else updateState();
}

void AiDock::setOpen(bool open, bool animate)
{
    if (open) {
        build();
        refreshContext();
        m_panel->show();
    }
    if (open != m_open) {
        m_open = open;
        m_flap->setOpen(open);
        emit openChanged(open);
    }
    m_anim->stop();
    if (animate) {
        m_anim->setStartValue(m_shown);
        m_anim->setEndValue(open ? 1.0 : 0.0);
        m_anim->start();
    } else {
        m_shown = open ? 1.0 : 0.0;
        relayout();
        if (open && m_input) m_input->setFocus();
    }
}

bool AiDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host && event->type() == QEvent::Resize)
        relayout();
    return QWidget::eventFilter(watched, event);
}

void AiDock::setDevFlapShown(bool shown)
{
    m_devFlapShown = shown;
    relayout();
}

void AiDock::relayout()
{
    // The panel slides in from the right edge (its left part first, as a drawer); the flap rides outside its left edge,
    // under the developer tools' flap, or at the top when that one is hidden.
    const int width = std::min(PanelWidth, std::max(260, m_host->width() - 60));
    const int shown = int(width * m_shown + 0.5);
    const int h = m_host->height() - m_topOffset;
    // Closed, the flap sits on the right edge under the developer tools' one; open, the panel stops left of it.
    const int inset = m_devFlapShown ? SideFlap::Width : 0;
    const int right = m_host->width() - int(inset * m_shown + 0.5);
    setGeometry(right - SideFlap::Width - shown, m_topOffset, SideFlap::Width + shown, h);
    // The flap stays outside the panel, against its left edge, over the page: always in sight to close it again.
    m_panel->setGeometry(SideFlap::Width, 0, width, h);
    m_panel->setVisible(m_shown > 0);
    m_flap->move(0, m_devFlapShown ? SideFlap::Top + SideFlap::Height + FlapGap : SideFlap::Top);
    m_flap->raise();
    // Only the flap (and the panel while open) takes the mouse: the rest of the dock's column lets clicks through,
    // to the developer tools' flap above it or the page.
    QRegion region(m_flap->geometry());
    if (m_shown > 0) region += m_panel->geometry();
    setMask(region);
    raise();
}
