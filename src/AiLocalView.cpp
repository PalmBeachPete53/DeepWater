#include "AiLocalView.h"

#include "LocalChat.h"
#include "McpServer.h"
#include "LocalEngine.h"
#include "ModelSlots.h"
#include "OpencodeRun.h"
#include "Theme.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStyle>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *dimLabel(const QString &text = {})
{
    auto *l = new QLabel(text);
    l->setObjectName("dim");
    l->setWordWrap(true);
    return l;
}

QPushButton *segButton(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setObjectName("segButton");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

} // namespace

AiLocalView::AiLocalView(LocalChat *chat, QWidget *parent)
    : QWidget(parent)
    , m_chat(chat)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(32, 22, 32, 24);
    v->setSpacing(8);
    auto *title = new QLabel(tr("Local models"));
    title->setObjectName("title");
    v->addWidget(title);
    v->addWidget(dimLabel(tr("A conversation with a model of this computer, run by llama.cpp. Only the models in "
                             "a bound slot (Models › Slots) can run. The engine runs in a sandbox: it reads its own model "
                             "file and nothing else of yours, has no network, writes nowhere and starts no other program. "
                             "opencode, when a slot holds it, can be chosen too: it runs outside the sandbox, with its own "
                             "tools turned off.")));

    auto *controls = new QHBoxLayout;
    controls->setSpacing(8);
    m_model = new QComboBox;
    m_model->setObjectName("sourceCombo");
    m_model->setMinimumWidth(420);
    m_context = new QComboBox;
    m_context->setObjectName("sourceCombo");
    for (int n : {4096, 8192, 16384, 32768})
        m_context->addItem(tr("%1 tokens").arg(QLocale().toString(n)), n);
    m_load = new QPushButton(tr("Load"));
    m_load->setObjectName("primaryButton");
    m_load->setCursor(Qt::PointingHandCursor);
    controls->addWidget(new QLabel(tr("Model")));
    controls->addWidget(m_model, 1);
    controls->addWidget(new OpencodeModelBox(m_chat));
    controls->addWidget(new QLabel(tr("Context")));
    controls->addWidget(m_context);
    controls->addWidget(m_load);
    v->addLayout(controls);
    m_engineState = dimLabel();
    m_engineState->setObjectName("slotState");
    v->addWidget(m_engineState);
    m_skills = dimLabel();
    v->addWidget(m_skills);

    m_system = new QLineEdit;
    m_system->setObjectName("keyEdit");
    m_system->setPlaceholderText(tr("System prompt (optional): who the model is for this conversation"));
    m_system->setClearButtonEnabled(true);
    v->addWidget(m_system);

    m_transcript = new ChatTranscript(m_chat);
    v->addWidget(m_transcript, 1);

    auto *inputRow = new QHBoxLayout;
    inputRow->setSpacing(8);
    m_input = new QPlainTextEdit;
    m_input->setObjectName("aiInput");
    m_input->setPlaceholderText(tr("Your message · Cmd+Enter sends"));
    m_input->setFixedHeight(84);
    inputRow->addWidget(m_input, 1);
    auto *buttons = new QVBoxLayout;
    m_send = new QPushButton(tr("Send"));
    m_send->setObjectName("primaryButton");
    m_send->setCursor(Qt::PointingHandCursor);
    m_stop = segButton(tr("Stop"));
    m_clear = segButton(tr("New conversation"));
    buttons->addWidget(m_send);
    buttons->addWidget(m_stop);
    buttons->addWidget(m_clear);
    inputRow->addLayout(buttons);
    v->addLayout(inputRow);
    m_status = dimLabel();
    v->addWidget(m_status);

    // The journal: every tool call of a local model, run or refused (newest first).
    auto *journalHead = new QHBoxLayout;
    auto *journalTitle = new QLabel(tr("Tool calls · journal").toUpper());
    journalTitle->setObjectName("sectionLabel");
    journalHead->addWidget(journalTitle);
    journalHead->addStretch(1);
    auto *openJournal = segButton(tr("Show the file"));
    journalHead->addWidget(openJournal);
    v->addLayout(journalHead);
    m_journal = new QTableWidget(0, 6);
    m_journal->setObjectName("aiTable");
    m_journal->setHorizontalHeaderLabels({tr("Time"), tr("Slot"), tr("Tool"), tr("Arguments"), tr("Outcome"), tr("Result")});
    m_journal->verticalHeader()->setVisible(false);
    m_journal->verticalHeader()->setDefaultSectionSize(24);
    m_journal->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_journal->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_journal->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_journal->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_journal->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_journal->setFixedHeight(150);
    v->addWidget(m_journal);
    for (const tooljournal::Entry &e : tooljournal::recent(50)) addJournalRow(e);
    connect(openJournal, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(tooljournal::path()).path()));
    });

    QSettings settings;
    m_system->setText(settings.value("ai/localSystem").toString());
    if (const int i = m_context->findData(settings.value("ai/localContext", 8192).toInt()); i >= 0)
        m_context->setCurrentIndex(i);

    connect(m_system, &QLineEdit::editingFinished, this, [this] { QSettings().setValue("ai/localSystem", m_system->text()); });
    connect(m_context, &QComboBox::currentIndexChanged, this, [this] {
        QSettings().setValue("ai/localContext", m_context->currentData().toInt());
    });
    connect(m_model, &QComboBox::currentIndexChanged, this, [this] {
        if (m_model->currentIndex() >= 0) m_chat->select(m_model->currentData().toString());
    });
    connect(m_load, &QPushButton::clicked, this, [this] {
        if (m_chat->engine()->state() == LocalEngine::State::Off) m_chat->load();
        else m_chat->unload();
    });
    connect(m_send, &QPushButton::clicked, this, &AiLocalView::send);
    auto *shortcut = new QShortcut(QKeySequence("Ctrl+Return"), m_input);
    connect(shortcut, &QShortcut::activated, this, &AiLocalView::send);
    connect(m_stop, &QPushButton::clicked, m_chat, &LocalChat::stop);
    connect(m_clear, &QPushButton::clicked, m_chat, &LocalChat::clear);
    connect(m_chat, &LocalChat::choicesChanged, this, &AiLocalView::refreshModels);
    connect(m_chat, &LocalChat::selectionChanged, this, &AiLocalView::refreshModels);
    connect(m_chat, &LocalChat::stateChanged, this, &AiLocalView::updateState);
    connect(m_chat, &LocalChat::statusChanged, this, [this] { m_status->setText(m_chat->status()); });
    connect(m_chat, &LocalChat::journalAdded, this, &AiLocalView::addJournalRow);
    // opencode's calls (over MCP) go in the same journal; its "slot" column reads "opencode".
    if (McpServer *mcp = McpServer::instance())
        connect(mcp, &McpServer::journalAdded, this, &AiLocalView::addJournalRow);
    // The skills shown follow the slots.
    connect(m_chat->slotsView(), &ModelSlotsView::slotsChanged, this, &AiLocalView::updateState);
    refreshModels();
    m_status->setText(m_chat->status());
}

void AiLocalView::refreshModels()
{
    m_model->blockSignals(true);
    m_model->clear();
    for (const LocalChat::Choice &c : m_chat->choices()) m_model->addItem(c.label, c.file);
    m_model->setCurrentIndex(m_model->findData(m_chat->selected()));
    m_model->blockSignals(false);
    updateState();
}

void AiLocalView::send()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_chat->state() != LocalEngine::State::Ready) return;
    m_input->clear();
    m_chat->send(text);
}

void AiLocalView::addJournalRow(const tooljournal::Entry &e)
{
    m_journal->insertRow(0);
    const QString args = QString::fromUtf8(QJsonDocument(e.arguments).toJson(QJsonDocument::Compact));
    const QString outcome = e.outcome == "ok" ? tr("run") : e.outcome == "refused" ? tr("refused") : tr("error");
    const QStringList cells {QLocale().toString(e.time.toLocalTime(), QLocale::ShortFormat),
                             e.slot >= 0 ? QString::number(e.slot + 1) : e.model, e.tool, args, outcome,
                             e.outcome == "ok" ? tr("%n character(s)", nullptr, e.resultChars) + " · " + e.detail : e.detail};
    for (int c = 0; c < cells.size(); ++c) {
        auto *item = new QTableWidgetItem(cells[c]);
        item->setToolTip(c == 3 ? args : c == 5 ? e.detail : QString());
        if (c == 4 && e.outcome != "ok") item->setForeground(Theme::current().Down);
        m_journal->setItem(0, c, item);
    }
    m_journal->setToolTip(e.model);
    while (m_journal->rowCount() > 200) m_journal->removeRow(m_journal->rowCount() - 1);
}

QString AiLocalView::engineState(LocalChat *chat, QString *tone)
{
    const LocalEngine *engine = chat->engine();
    const LocalEngine::State s = engine->state();
    *tone = "dim";
    if (chat->isOpencode()) {
        if (!OpencodeRun::available()) {
            *tone = "bad";
            return tr("opencode isn't on this computer (opencode.ai to install it).");
        }
        *tone = "good";
        return QString::fromUtf8("✓ ") + tr("opencode answers with the model chosen here, usually at a provider, which gets "
                                           "your questions and what the tools read. Its own tools (shell, files, web) are off here.");
    }
    if (!LocalEngine::available()) {
        *tone = "bad";
        return tr("The local engine isn't part of this build.");
    }
    if (s == LocalEngine::State::Off && chat->choices().isEmpty())
        return tr("No model can run: drag a GGUF model or opencode into a slot (Models › Slots); it shows here once the "
                  "slot has it.");
    if (s == LocalEngine::State::Off) return tr("Not loaded.");
    if (s == LocalEngine::State::Loading) return tr("Loading %1 in the sandbox…").arg(engine->model());
    const QJsonObject i = engine->info();
    bool gpu = false;
    for (const QJsonValue &d : i.value("devices").toArray()) gpu = gpu || d.toString().startsWith("MTL");
    *tone = "good";
    return QString::fromUtf8("✓ ") + tr("%1 · %2 billion parameters · %3 · context %4 tokens · loaded in %5 s")
               .arg(i.value("model").toString().remove(" ?B"))   // llama.cpp's "?B" when it can't name the size
               .arg(QLocale().toString(i.value("params").toDouble() / 1e9, 'f', 1))
               .arg(gpu ? tr("Metal GPU") : tr("CPU"))
               .arg(QLocale().toString(i.value("ctx").toInt()))
               .arg(QLocale().toString(i.value("load_ms").toDouble() / 1000.0, 'f', 1));
}

void AiLocalView::updateState()
{
    const LocalEngine::State s = m_chat->state();
    const bool off = m_chat->engine()->state() == LocalEngine::State::Off;
    m_model->setEnabled(off && s != LocalEngine::State::Busy);
    m_context->setEnabled(off && !m_chat->isOpencode());
    m_load->setText(off ? tr("Load") : tr("Unload"));
    m_load->setVisible(!m_chat->isOpencode());   // nothing to load
    m_load->setEnabled(LocalEngine::available() && (!off || m_model->count() > 0));
    m_send->setEnabled(s == LocalEngine::State::Ready);
    m_stop->setEnabled(s == LocalEngine::State::Busy);

    QString tone;
    m_engineState->setText(engineState(m_chat, &tone));
    m_engineState->setProperty("tone", tone);
    m_engineState->style()->unpolish(m_engineState);
    m_engineState->style()->polish(m_engineState);

    // The skills of the slot of the model chosen (or loaded).
    int slot = -1;
    for (const LocalChat::Choice &c : m_chat->choices())
        if (c.file == m_chat->selected()) slot = c.slot;
    QStringList names, skillIds;
    for (const modelslots::Skill &k : modelslots::skills())
        if (m_chat->slotsView()->skillsOf(slot).contains(k.id)) {
            names << QCoreApplication::translate("ModelSlotsView", k.name);
            skillIds << k.id;
        }
    if (slot < 0) m_skills->clear();
    else if (names.isEmpty())
        m_skills->setText(tr("Slot %1 grants no skill: the model sees this conversation and nothing else.").arg(slot + 1));
    else {
        QStringList tools;
        for (const LocalTools::Tool &t : LocalTools::toolsFor(skillIds)) tools << t.name;
        m_skills->setText(tr("Skills of slot %1: %2. Its tools: %3. Each call is checked against the slot and written "
                             "to the journal below.").arg(slot + 1).arg(names.join(", "), tools.join(", ")));
    }
}
