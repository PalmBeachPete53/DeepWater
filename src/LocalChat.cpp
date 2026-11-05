#include "LocalChat.h"

#include <QRegularExpression>

#include "LocalEngine.h"
#include "ModelSlots.h"
#include "OpencodeRun.h"
#include "Theme.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocale>
#include <QPointer>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>

#include <algorithm>

#ifdef Q_OS_MACOS
#include <sys/sysctl.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

qint64 physicalMemory()
{
#ifdef Q_OS_MACOS
    int64_t bytes = 0;
    size_t len = sizeof bytes;
    if (sysctlbyname("hw.memsize", &bytes, &len, nullptr, 0) == 0) return bytes;
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX status {};
    status.dwLength = sizeof status;
    if (GlobalMemoryStatusEx(&status)) return qint64(status.ullTotalPhys);
#endif
    return 0;
}

// An answer split into the model's thinking (between <think> and </think>, as Qwen3 and DeepSeek-R1 write it) and
// the answer itself.
void splitThinking(const QString &text, QString *thinking, QString *answer)
{
    const QString open = "<think>", close = "</think>";
    const qsizetype start = text.indexOf(open);
    if (start < 0 || !text.left(start).trimmed().isEmpty()) {
        thinking->clear();
        *answer = text;
        return;
    }
    const qsizetype end = text.indexOf(close, start);
    if (end < 0) {
        *thinking = text.mid(start + open.size()).trimmed();
        answer->clear();
        return;
    }
    *thinking = text.mid(start + open.size(), end - start - open.size()).trimmed();
    *answer = text.mid(end + close.size()).trimmed();
}

QString html(const QString &text)
{
    // Models write **bold** whatever they are told: shown as such rather than with its stars.
    static const QRegularExpression bold(R"(\*\*([^*\n]+)\*\*)");
    return text.toHtmlEscaped().replace(bold, "<b>\\1</b>").replace('\n', "<br>");
}

} // namespace

LocalChat::LocalChat(ModelSlotsView *slotsView, const MlDataExporter::Sources &data, QObject *parent)
    : QObject(parent)
    , m_slots(slotsView)   // not "slots": a Qt macro
    , m_engine(new LocalEngine(this))
    , m_opencode(new OpencodeRun(this))
    , m_tools(new LocalTools(data, this))
{
    m_selected = QSettings().value("ai/localModel").toString();
    connect(m_engine, &LocalEngine::stateChanged, this, &LocalChat::stateChanged);
    connect(m_engine, &LocalEngine::loaded, this, [this] {
        if (m_devPrompt.isEmpty()) return;
        const QString prompt = m_devPrompt, context = m_devContext;
        m_devPrompt.clear();
        m_devContext.clear();
        send(prompt, context);
    });
    connect(m_engine, &LocalEngine::token, this, &LocalChat::onToken);
    connect(m_engine, &LocalEngine::answered, this, &LocalChat::answered);
    connect(m_engine, &LocalEngine::failed, this, &LocalChat::fail);
    connect(m_opencode, &OpencodeRun::busyChanged, this, &LocalChat::stateChanged);
    connect(m_opencode, &OpencodeRun::modelsListed, this, [this](const QStringList &models) {
        if (models.isEmpty()) return;   // keeps what it knew
        m_opencodeModels = models;
        emit opencodeModelsChanged();
    });
    connect(m_opencode, &OpencodeRun::text, this, &LocalChat::onToken);
    connect(m_opencode, &OpencodeRun::answered, this, &LocalChat::answered);
    connect(m_opencode, &OpencodeRun::failed, this, [this](const QString &error) {
        m_opencodePrompted = false;   // its session may not have the instructions
        fail(error);
    });
    // A model runs only while a bound slot holds it with the same files: the slots are checked whenever they change.
    connect(m_slots, &ModelSlotsView::slotsChanged, this, [this] {
        refreshChoices();
        if (m_engine->state() == LocalEngine::State::Off) return;
        QStringList files;
        if (!m_slots->mayRun(m_loadedSlot, m_engine->model(), m_loadedFile, &files)) {
            m_engine->unload();
            setStatus(tr("Unloaded: slot %1 no longer holds the model with the files it was bound to.").arg(m_loadedSlot + 1));
        }
    });
    refreshChoices();
}

QString LocalChat::opencodeModel() const
{
    return QSettings().value("ai/opencodeModel").toString();
}

void LocalChat::setOpencodeModel(const QString &model)
{
    QSettings().setValue("ai/opencodeModel", model);
}

void LocalChat::refreshOpencodeModels()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_opencodeModelsAt < 30000) return;
    m_opencodeModelsAt = now;
    m_opencode->listModels();
}

void LocalChat::fail(const QString &error)
{
    // Drop the question and what came of it, so it can be sent again.
    while (m_messages.size() > m_turnStart) m_messages.removeLast();
    if (m_answer >= 0) updateAnswer(false);
    m_pending.clear();
    m_answer = -1;
    setStatus(error, true);
}

bool LocalChat::isOpencode() const
{
    return m_selected == QLatin1String(modelslots::OpencodeId);
}

LocalEngine::State LocalChat::state() const
{
    if (!isOpencode()) return m_engine->state();
    if (!OpencodeRun::available() || m_slots->slotOf(modelslots::OpencodeId) < 0) return LocalEngine::State::Off;
    return m_opencode->busy() ? LocalEngine::State::Busy : LocalEngine::State::Ready;
}

int LocalChat::activeSlot() const
{
    return isOpencode() ? m_slots->slotOf(modelslots::OpencodeId) : m_loadedSlot;
}

QString LocalChat::activeModel() const
{
    return isOpencode() ? modelslots::integrationName(modelslots::OpencodeId) : m_engine->model();
}

void LocalChat::refreshChoices()
{
    m_choices.clear();
    for (const ModelSlotsView::Runnable &r : m_slots->runnable())
        for (const QString &file : r.ggufs)
            m_choices.push_back({r.slot, r.model, file,
                                 tr("Slot %1 · %2 · %3").arg(r.slot + 1).arg(r.model.section('/', 1), QFileInfo(file).fileName())});
    if (const int slot = m_slots->slotOf(modelslots::OpencodeId); slot >= 0)
        m_choices.push_back({slot, modelslots::OpencodeId, modelslots::OpencodeId,
                             tr("Slot %1 · %2").arg(slot + 1).arg(modelslots::integrationName(modelslots::OpencodeId))});
    std::stable_sort(m_choices.begin(), m_choices.end(), [](const Choice &a, const Choice &b) { return a.slot < b.slot; });
    const bool known = std::any_of(m_choices.cbegin(), m_choices.cend(), [&](const Choice &c) { return c.file == m_selected; });
    if (!known && isOpencode()) {   // taken out of its slot
        m_selected.clear();
        clear();
    }
    if (!known && !m_choices.isEmpty() && m_engine->state() == LocalEngine::State::Off)
        m_selected = m_choices.front().file;
    emit choicesChanged();
    emit selectionChanged();
}

void LocalChat::select(const QString &file)
{
    if (file == m_selected) return;
    const bool opencode = isOpencode() || file == QLatin1String(modelslots::OpencodeId);
    m_selected = file;
    QSettings().setValue("ai/localModel", file);
    if (opencode) clear();   // a conversation belongs to one model
    emit selectionChanged();
}

void LocalChat::load()
{
    if (isOpencode()) return;   // nothing to load
    const auto choice = std::find_if(m_choices.cbegin(), m_choices.cend(), [&](const Choice &c) { return c.file == m_selected; });
    if (choice == m_choices.cend()) return;
    QStringList files;
    if (!m_slots->mayRun(choice->slot, choice->model, choice->file, &files)) {
        setStatus(tr("Slot %1 no longer holds this model with the files it was bound to: see Models › Slots.").arg(choice->slot + 1), true);
        refreshChoices();
        return;
    }
    qint64 bytes = 0;
    for (const QString &f : files) bytes += QFileInfo(f).size();
    const qint64 memory = physicalMemory();
    if (memory > 0 && bytes > memory * 3 / 4) {
        setStatus(tr("This model needs about %1 of memory and this Mac has %2: it would not fit.")
                      .arg(QLocale().formattedDataSize(bytes), QLocale().formattedDataSize(memory)), true);
        return;
    }
    m_loadedSlot = choice->slot;
    m_loadedFile = choice->file;
    clear();   // a conversation belongs to one model
    m_engine->load(choice->model, files, QSettings().value("ai/localContext", 8192).toInt());
}

void LocalChat::unload()
{
    m_engine->unload();
}

void LocalChat::runPrompt(const QString &prompt, const QString &context)
{
    m_devPrompt = prompt;
    m_devContext = context;
    if (state() == LocalEngine::State::Off && !m_choices.isEmpty()) load();
    else if (state() == LocalEngine::State::Ready) {
        m_devPrompt.clear();
        m_devContext.clear();
        send(prompt, context);
    }
}

void LocalChat::clear()
{
    m_opencode->reset();
    m_opencodePrompted = false;
    m_messages = QJsonArray();
    m_entries.clear();
    m_answer = -1;
    m_pending.clear();
    emit cleared();
    setStatus({});
}

void LocalChat::stop()
{
    m_cancelled = true;   // no further round of tool calls either
    if (isOpencode()) m_opencode->stop();
    else m_engine->stopAnswer();
}

void LocalChat::setStatus(const QString &text, bool error)
{
    m_status = error ? QString("<span style='color:%1'>%2</span>").arg(Theme::current().Down.name(), text.toHtmlEscaped())
                     : text.toHtmlEscaped();
    m_statusError = error;
    emit statusChanged();
}

void LocalChat::addEntry(const Entry &e)
{
    m_entries.push_back(e);
    emit entryAdded(int(m_entries.size()) - 1);
}

void LocalChat::send(const QString &question, const QString &context)
{
    m_question = question;
    const QString text = question.trimmed();
    if (text.isEmpty() || state() != LocalEngine::State::Ready) return;
    m_turnStart = int(m_messages.size());
    m_round = 0;
    m_toolCalls = 0;
    m_orders = 0;
    m_failedRounds = 0;
    m_cancelled = false;
    // The model reads where the user is before the question; the transcript shows it under the question.
    const QString content = context.isEmpty() ? text : "[The user is in Deepwater, on: " + context + "]\n\n" + text;
    m_messages.append(QJsonObject {{"role", "user"}, {"content", content}});
    Entry q;
    q.kind = Entry::User;
    q.header = tr("You");
    q.text = text;
    q.context = context;
    addEntry(q);
    Entry a;
    a.kind = Entry::Answer;
    a.header = isOpencode() ? activeModel() : m_engine->model().section('/', 1);
    a.writing = true;
    addEntry(a);
    m_answer = int(m_entries.size()) - 1;
    m_pending.clear();
    setStatus(tr("Writing…"));
    ask();
}

// Sends the conversation, with the system prompt that names the tools of the slot's skills as they are now.
void LocalChat::ask()
{
    if (isOpencode()) {
        // Its session keeps the conversation: only the new message goes, after the instructions when the skills changed.
        const QStringList skills = grantedSkills();
        QString message = m_messages.last().toObject().value("content").toString();
        if (!m_opencodePrompted || skills != m_opencodeTold)
            message = LocalTools::systemPrompt(skills, QSettings().value("ai/localSystem").toString(), false) + "\n\n" + message;
        m_opencodePrompted = true;
        m_opencodeTold = skills;
        m_opencode->ask(message, opencodeModel());
        return;
    }
    QJsonArray messages;
    messages.append(QJsonObject {{"role", "system"},
                                 {"content", LocalTools::systemPrompt(grantedSkills(), QSettings().value("ai/localSystem").toString())}});
    for (const QJsonValue &m : std::as_const(m_messages)) messages.append(m);
    m_engine->chat(messages, {{"max_tokens", 2048}, {"temperature", 0.7}, {"top_k", 40}, {"top_p", 0.9}, {"min_p", 0.05}});
}

QStringList LocalChat::grantedSkills() const
{
    if (isOpencode()) return m_slots->skillsOf(m_slots->slotOf(modelslots::OpencodeId));
    QStringList files;
    if (m_engine->state() == LocalEngine::State::Off || !m_slots->mayRun(m_loadedSlot, m_engine->model(), m_loadedFile, &files))
        return {};
    for (const ModelSlotsView::Runnable &r : m_slots->runnable())
        if (r.slot == m_loadedSlot && r.model == m_engine->model()) return r.skills;
    return {};
}

void LocalChat::onToken(const QString &text)
{
    m_pending += text;
    updateAnswer(true);
}

// The answer entry follows what the model writes: its thinking, the answer without its tool calls, whether a call is
// being written.
void LocalChat::updateAnswer(bool writing)
{
    if (m_answer < 0 || m_answer >= m_entries.size()) return;
    Entry &e = m_entries[m_answer];
    QString thinking, answer;
    splitThinking(m_pending, &thinking, &answer);
    e.thinking = thinking;
    e.calling = writing && answer.contains("<tool_call>");
    e.text = LocalTools::withoutCalls(answer);
    e.writing = writing;
    emit entryChanged(m_answer);
}

void LocalChat::answered(const QJsonObject &o)
{
    QString thinking, answer;
    splitThinking(m_pending, &thinking, &answer);
    const QString stop = o.value("stop").toString();
    const QVector<LocalTools::Call> calls = stop == "eos" ? LocalTools::parseCalls(answer) : QVector<LocalTools::Call>();
    // The answer as written, tool calls included: the model reads its own calls before their results.
    m_messages.append(QJsonObject {{"role", "assistant"}, {"content", answer}});
    updateAnswer(false);
    m_pending.clear();
    m_answer = -1;

    const int tokens = o.value("tokens").toInt();
    const double seconds = o.value("gen_ms").toDouble() / 1000.0;
    QString note = tr("%n token(s)", nullptr, tokens);
    if (seconds > 0) note += " · " + tr("%1 tokens/s").arg(QLocale().toString(tokens / seconds, 'f', 1));
    note += " · " + tr("prompt %1 tokens, %2 already evaluated").arg(o.value("prompt_tokens").toInt())
                        .arg(o.value("reused_tokens").toInt());
    if (stop == "length") note += " · " + tr("cut at the token limit");
    else if (stop == "stopped") note += " · " + tr("stopped");
    else if (stop == "context") note += " · " + tr("the context is full: start a new conversation");

    if (calls.isEmpty() || m_cancelled) {
        finishTurn(note);
        return;
    }
    if (m_round >= MaxRounds) {
        finishTurn(note + " · " + tr("stopped after %n round(s) of tool calls", nullptr, MaxRounds));
        return;
    }
    ++m_round;
    setStatus(tr("Calling %n tool(s)…", nullptr, int(calls.size())));
    runCalls(calls, {});
}

// One call after the other (prices wait for Yahoo); then the results go back to the model, which goes on.
void LocalChat::runCalls(QVector<LocalTools::Call> calls, QVector<QJsonObject> results)
{
    if (state() != LocalEngine::State::Ready) return;   // unloaded meanwhile, or taken out of its slot
    if (calls.isEmpty()) {
        if (m_cancelled) {
            finishTurn(tr("stopped"));
            return;
        }
        const bool allFailed = std::all_of(results.begin(), results.end(), [](const QJsonObject &r) { return r.contains("error"); });
        m_failedRounds = allFailed ? m_failedRounds + 1 : 0;
        if (m_failedRounds >= 3) {
            finishTurn(tr("stopped: the tool calls kept failing"));
            return;
        }
        // Twice in a row: it's told to answer with what it has, rather than leave the question without an answer.
        QString responses = LocalTools::responses(results);
        if (m_failedRounds == 2)
            responses += "\n\nThese tool calls keep failing. Don't call a tool again: answer now with what you have, and say "
                         "what you couldn't read.";
        m_messages.append(QJsonObject {{"role", "user"}, {"content", responses}});
        // The next stretch of answer: no header, shown once it has something.
        Entry a;
        a.kind = Entry::Answer;
        a.writing = true;
        addEntry(a);
        m_answer = int(m_entries.size()) - 1;
        setStatus(tr("Writing…"));
        ask();
        return;
    }
    const LocalTools::Call call = calls.takeFirst();
    const QStringList skills = grantedSkills();   // checked at each call: the slot may have changed
    QElapsedTimer clock;
    clock.start();
    QPointer<LocalChat> self(this);
    const QString model = activeModel();
    const int slot = activeSlot();
    auto handle = [self, call, calls, results, clock, model, slot](const QJsonObject &result) mutable {
        if (!self) return;
        if (LocalTools::isOrder(call.name) && !result.contains("error")) ++self->m_orders;
        const QString error = result.value("error").toString();
        const QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
        tooljournal::Entry e;
        e.time = QDateTime::currentDateTime();
        e.model = model;
        e.slot = slot;
        e.tool = call.name.isEmpty() ? QStringLiteral("?") : call.name;
        e.arguments = call.arguments;
        e.outcome = error.isEmpty() ? "ok" : error.startsWith("not allowed") ? "refused" : "error";
        e.detail = error.isEmpty() ? QString::fromUtf8(json.left(160)) : error;
        e.resultChars = int(json.size());
        e.ms = clock.elapsed();
        tooljournal::append(e);
        emit self->journalAdded(e);
        ++self->m_toolCalls;
        const QString args = QString::fromUtf8(QJsonDocument(call.arguments).toJson(QJsonDocument::Compact));
        Entry line;
        line.kind = Entry::Tool;
        line.text = QString("%1(%2) · %3").arg(e.tool, args.mid(1, args.size() - 2),
                                                error.isEmpty() ? LocalChat::tr("%n character(s) of data", nullptr, e.resultChars) : error);
        line.error = !error.isEmpty();
        self->addEntry(line);
        results << result;
        self->runCalls(calls, results);
    };
    if (LocalTools::isOrder(call.name) && m_orders >= LocalTools::MaxOrders)
        handle({{"error", QString("not allowed: at most %1 paper orders per question").arg(LocalTools::MaxOrders)}});
    else
    {
        LocalTools::Call asked = call;
        asked.question = m_question;
        m_tools->call(asked, skills, handle);
    }
}

void LocalChat::finishTurn(const QString &note)
{
    setStatus(m_toolCalls ? note + " · " + tr("%n tool call(s)", nullptr, m_toolCalls) : note);
}

QString LocalChat::renderHeader(const Entry &e)
{
    if (e.header.isEmpty() || e.kind == Entry::Tool) return {};
    const Palette &pal = Theme::current();
    return QString("<span style='color:%1; font-size:10px; letter-spacing:1px'>%2</span>")
        .arg((e.kind == Entry::User ? pal.Accent : pal.TextDim).name(), e.header.toUpper().toHtmlEscaped());
}

QString LocalChat::renderBody(const Entry &e)
{
    const Palette &pal = Theme::current();
    switch (e.kind) {
    case Entry::User: {
        QString out = html(e.text);
        if (!e.context.isEmpty())
            out += QString("<br><span style='color:%1; font-size:11px'>%2</span>").arg(pal.TextFaint.name(), html(e.context));
        return out;
    }
    case Entry::Answer: {
        QString body;
        if (!e.thinking.isEmpty())
            body += QString("<span style='color:%1; font-style:italic'>%2</span>").arg(pal.TextDim.name(), html(e.thinking));
        if (!e.thinking.isEmpty() && !e.text.isEmpty()) body += "<br><br>";
        body += html(e.text);
        if (e.calling)
            body += QString("%1<span style='color:%2; font-size:11px'>%3</span>")
                        .arg(body.isEmpty() ? QString() : "<br>", pal.TextDim.name(), QString::fromUtf8("⚙ ") + tr("calling a tool…"));
        return body;
    }
    case Entry::Tool:
        return QString("<span style='color:%1; font-size:11px'>%2 %3</span>")
            .arg(e.error ? pal.Down.name() : pal.TextDim.name(), QString::fromUtf8("⚙"), e.text.toHtmlEscaped());
    }
    return {};
}

// ---------------------------------------------------------------- OpencodeModelBox

OpencodeModelBox::OpencodeModelBox(LocalChat *chat, QWidget *parent)
    : QComboBox(parent)
    , m_chat(chat)
{
    setObjectName("sourceCombo");
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(16);
    setToolTip(LocalChat::tr("The model opencode answers with; a free one may not be available in your country"));
    connect(this, &QComboBox::currentIndexChanged, this, [this] {
        if (currentIndex() >= 0) m_chat->setOpencodeModel(currentData().toString());
    });
    connect(chat, &LocalChat::opencodeModelsChanged, this, &OpencodeModelBox::rebuild);
    connect(chat, &LocalChat::selectionChanged, this, [this] { setVisible(m_chat->isOpencode()); });
    connect(chat, &LocalChat::stateChanged, this, [this] { setEnabled(m_chat->state() != LocalEngine::State::Busy); });
    rebuild();
    setVisible(chat->isOpencode());
}

void OpencodeModelBox::showEvent(QShowEvent *e)
{
    m_chat->refreshOpencodeModels();   // a provider may have been connected since
    QComboBox::showEvent(e);
}

void OpencodeModelBox::rebuild()
{
    const QString saved = m_chat->opencodeModel();
    blockSignals(true);
    clear();
    addItem(LocalChat::tr("opencode's default model"), QString());
    for (const QString &model : m_chat->opencodeModels()) addItem(model, model);
    if (!saved.isEmpty() && findData(saved) < 0) addItem(saved, saved);   // not listed now: still the choice
    setCurrentIndex(findData(saved));
    blockSignals(false);
}

// ---------------------------------------------------------------- ChatTranscript

ChatTranscript::ChatTranscript(LocalChat *chat, QWidget *parent)
    : QTextEdit(parent)
    , m_chat(chat)
{
    setObjectName("aiTranscript");
    setReadOnly(true);
    connect(chat, &LocalChat::entryAdded, this, &ChatTranscript::renderFrom);
    connect(chat, &LocalChat::entryChanged, this, &ChatTranscript::renderFrom);
    connect(chat, &LocalChat::cleared, this, &ChatTranscript::rebuild);
    rebuild();
}

void ChatTranscript::rebuild()
{
    clear();
    m_starts.clear();
    renderFrom(0);
}

void ChatTranscript::renderFrom(int index)
{
    QScrollBar *bar = verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    const QVector<LocalChat::Entry> &entries = m_chat->entries();
    index = std::clamp(index, 0, int(m_starts.size()));
    QTextCursor c(document());
    if (index < m_starts.size()) {
        c.setPosition(m_starts[index]);
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        c.removeSelectedText();
        m_starts.resize(index);
    }
    c.movePosition(QTextCursor::End);
    for (int i = index; i < entries.size(); ++i) {
        m_starts.push_back(c.position());
        for (const QString &part : {LocalChat::renderHeader(entries[i]), LocalChat::renderBody(entries[i])}) {
            if (part.isEmpty()) continue;
            if (!document()->isEmpty()) c.insertBlock(QTextBlockFormat(), QTextCharFormat());
            c.insertHtml(part);
        }
    }
    if (atBottom) bar->setValue(bar->maximum());
}
