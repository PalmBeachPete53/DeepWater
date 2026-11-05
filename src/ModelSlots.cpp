#include "ModelSlots.h"

#include <QApplication>
#include <QCheckBox>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <functional>

namespace modelslots {

namespace {

constexpr char SettingsKey[] = "ai/slots";

void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// The model id of a drag from the list of models (a QTableWidget's rows: the first column's text), or from a slot.
QString draggedModel(const QMimeData *mime, int *from)
{
    *from = -1;
    if (mime->hasFormat(MimeType)) {
        const QStringList parts = QString::fromUtf8(mime->data(MimeType)).split('|');
        if (parts.size() > 1) *from = parts[1].toInt();
        return parts.value(0);
    }
    const QString list = "application/x-qabstractitemmodeldatalist";
    if (!mime->hasFormat(list)) return {};
    QByteArray data = mime->data(list);
    QDataStream s(&data, QIODevice::ReadOnly);
    while (!s.atEnd()) {
        int row = 0, col = 0;
        QMap<int, QVariant> roles;
        s >> row >> col >> roles;
        if (col == 0) return roles.value(Qt::DisplayRole).toString();
    }
    return {};
}

} // namespace

bool isIntegration(const QString &model)
{
    return model == QLatin1String(ClaudeId) || model == QLatin1String(OpencodeId);
}

QString integrationName(const QString &model)
{
    return model == QLatin1String(ClaudeId) ? QStringLiteral("Claude") : model == QLatin1String(OpencodeId) ? QStringLiteral("opencode") : model;
}

int slotHolding(const QVector<Slot> &all, const QString &model)
{
    for (int i = 0; i < all.size(); ++i)
        if (all[i].model == model) return i;
    return -1;
}

const QVector<Skill> &skills()
{
    static const QVector<Skill> list {
        {"read_data", QT_TRANSLATE_NOOP("ModelSlotsView", "Read Deepwater's data"),
         QT_TRANSLATE_NOOP("ModelSlotsView", "Read only: the tables and charts of every page but Sources and AI, open or not (and taking you to a page when you ask), the series of the countries and indicators, the map's data sets, "
                                             "prices and calendars, through the app; no file, no network, no writing.")},
        {"trade", QT_TRANSLATE_NOOP("ModelSlotsView", "Trade"),
         QT_TRANSLATE_NOOP("ModelSlotsView", "Runs strategies in the Charts tab's tester (which can switch the active chart), "
                                             "reads their trades as the CSV export, and buys and sells on the live paper "
                                             "account: simulated money only, at most 5 orders per question, none above the "
                                             "account's equity.")},
        {"organize", QT_TRANSLATE_NOOP("ModelSlotsView", "Organize"),
         QT_TRANSLATE_NOOP("ModelSlotsView", "Opens and creates Desk workspaces, changes the Journal's account, and prepares a journal entry: its form "
                                             "opens filled in, and nothing is written unless you press Save. No access to your broker.")},
    };
    return list;
}

QVector<Slot> load()
{
    QVector<Slot> list(Count);
    const QJsonArray array = QJsonDocument::fromJson(QSettings().value(SettingsKey).toString().toUtf8()).array();
    for (int i = 0; i < Count && i < array.size(); ++i) {
        const QJsonObject o = array[i].toObject();
        list[i].model = o.value("model").toString();
        for (const QJsonValue &v : o.value("skills").toArray())
            for (const Skill &k : skills())
                if (v.toString() == k.id) list[i].skills << k.id;   // only the skills this version knows
        const QJsonObject files = o.value("files").toObject();
        for (auto it = files.begin(); it != files.end(); ++it) {
            const QJsonArray f = it.value().toArray();   // [sha256, size, modified]
            LocalModels::Fingerprint p;
            p.sha256 = f.at(0).toString();
            p.size = qint64(f.at(1).toDouble());
            p.modified = qint64(f.at(2).toDouble());
            list[i].files.insert(it.key(), p);
        }
    }
    return list;
}

void save(const QVector<Slot> &all)
{
    QJsonArray array;
    for (const Slot &s : all) {
        QJsonObject files;
        for (auto it = s.files.begin(); it != s.files.end(); ++it)
            files.insert(it.key(), QJsonArray {it->sha256, double(it->size), double(it->modified)});
        array << QJsonObject {{"model", s.model}, {"skills", QJsonArray::fromStringList(s.skills)}, {"files", files}};
    }
    QSettings().setValue(SettingsKey, QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

// ---------------------------------------------------------------- Card

// One slot: its number, the well that takes a model or an integration (dashed while empty), where it stands with the
// model's files, the skills it grants, a button to empty it. What is in the well can be dragged to another slot.
class Card : public QFrame
{
public:
    std::function<void(const QString &model, int from)> dropped;
    std::function<void()> cleared;
    std::function<void(const QString &skill, bool on)> skillChanged;
    std::function<bool(const QString &model)> accepts;

    Card(int index, QWidget *parent) : QFrame(parent), m_index(index)
    {
        setObjectName("modelSlot");
        setAcceptDrops(true);
        setMinimumWidth(200);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(14, 12, 14, 12);
        v->setSpacing(8);
        auto *head = new QHBoxLayout;
        auto *title = new QLabel(ModelSlotsView::tr("Slot %1").arg(index + 1).toUpper());
        title->setObjectName("sectionLabel");
        head->addWidget(title);
        head->addStretch(1);
        m_clear = new QToolButton;
        m_clear->setObjectName("slotClear");
        m_clear->setText(QString::fromUtf8("✕"));
        m_clear->setCursor(Qt::PointingHandCursor);
        m_clear->setToolTip(ModelSlotsView::tr("Take it out of this slot"));
        head->addWidget(m_clear);
        v->addLayout(head);

        m_well = new QFrame;
        m_well->setObjectName("slotWell");
        m_well->setMinimumHeight(52);
        auto *wv = new QVBoxLayout(m_well);
        wv->setContentsMargins(10, 6, 10, 6);
        wv->setSpacing(1);
        m_model = new QLabel;
        m_model->setObjectName("slotModel");
        m_model->setWordWrap(true);
        m_model->setAlignment(Qt::AlignCenter);
        m_detail = new QLabel;
        m_detail->setObjectName("sourceMeta");
        m_detail->setAlignment(Qt::AlignCenter);
        wv->addWidget(m_model);
        wv->addWidget(m_detail);
        v->addWidget(m_well);
        m_state = new QLabel;
        m_state->setObjectName("slotState");
        m_state->setWordWrap(true);
        v->addWidget(m_state);
        m_code = new QLabel;
        m_code->setObjectName("slotState");
        m_code->setProperty("tone", "bad");
        m_code->setWordWrap(true);
        v->addWidget(m_code);

        auto *skillsTitle = new QLabel(ModelSlotsView::tr("Skills").toUpper());
        skillsTitle->setObjectName("sectionLabel");
        v->addWidget(skillsTitle);
        for (const Skill &k : skills()) {
            auto *box = new QCheckBox(ModelSlotsView::tr(k.name));
            box->setObjectName("calCheck");
            box->setCursor(Qt::PointingHandCursor);
            box->setToolTip(ModelSlotsView::tr(k.what));
            const QString id = k.id;
            connect(box, &QCheckBox::toggled, this, [this, id](bool on) {
                if (skillChanged && !m_updating) skillChanged(id, on);
            });
            m_boxes.insert(id, box);
            v->addWidget(box);
        }
        v->addStretch(1);
        connect(m_clear, &QToolButton::clicked, this, [this] { if (cleared) cleared(); });
    }

    void display(const Slot &slot, State state, const LocalModels::Entry *entry)
    {
        m_slot = slot;
        m_updating = true;
        const bool empty = slot.model.isEmpty();
        const bool integration = isIntegration(slot.model);
        const bool claude = slot.model == QLatin1String(ClaudeId);
        const bool bad = state == State::Missing || state == State::NotRunnable || state == State::Changed;
        m_well->setProperty("filled", !empty);
        m_well->setProperty("missing", bad);
        repolish(m_well);
        m_model->setText(empty ? ModelSlotsView::tr("Drag a model or an integration here")
                               : integration ? integrationName(slot.model) : slot.model);
        m_model->setProperty("empty", empty);
        repolish(m_model);
        m_detail->setText(empty ? ModelSlotsView::tr("from On this computer or Integration")
                          : claude ? ModelSlotsView::tr("Anthropic API")
                          : integration ? ModelSlotsView::tr("agent, over MCP")
                          : state == State::Missing ? ModelSlotsView::tr("no longer on this computer")
                          : entry ? LocalModels::formatBytes(entry->bytes) : QString());
        m_well->setCursor(empty ? Qt::ArrowCursor : Qt::OpenHandCursor);
        m_well->setToolTip(empty ? QString() : ModelSlotsView::tr("Drag it to another slot to move it"));
        m_clear->setVisible(!empty);

        // Where the slot stands with the model's files.
        int code = 0;
        if (entry)
            for (const QString &f : entry->files) code += LocalModels::fileKind(f) == LocalModels::FileKind::Code;
        QString text;
        switch (state) {
        case State::Empty: break;
        case State::Missing: text = ModelSlotsView::tr("Skills suspended: the model was deleted."); break;
        case State::NotRunnable:
            text = ModelSlotsView::tr("Skills suspended: no GGUF file, and the local engine reads GGUF only."); break;
        case State::Binding: text = ModelSlotsView::tr("Taking the fingerprints of its files…"); break;
        case State::Checking: text = ModelSlotsView::tr("A file changed size or date: checking its fingerprint…"); break;
        case State::Changed:
            text = ModelSlotsView::tr("Skills suspended: its files changed since the slot took it. Drag it in again "
                                      "to accept the new files.");
            break;
        case State::Bound:
            text = QString::fromUtf8("✓ ") + ModelSlotsView::tr("%n file(s) bound by their SHA-256", nullptr, int(slot.files.size()));
            break;
        case State::Integration:
            text = claude ? ModelSlotsView::tr("Nothing in the app calls Claude yet: these skills stay unused.")
                          : QString::fromUtf8("✓ ") + ModelSlotsView::tr("opencode gets the tools of these skills while serving is on.");
            break;
        }
        m_code->setText(ModelSlotsView::tr("%n file(s) that can run code (pickle, Python): never loaded.", nullptr, code));
        m_code->setVisible(code > 0 && state != State::Missing);
        m_state->setText(text);
        m_state->setVisible(!text.isEmpty());
        m_state->setProperty("tone", bad ? "bad" : state == State::Bound || (state == State::Integration && !claude) ? "good" : "dim");
        repolish(m_state);
        for (auto it = m_boxes.begin(); it != m_boxes.end(); ++it)
            it.value()->setChecked(slot.skills.contains(it.key()));
        m_updating = false;
    }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        int from = -1;
        const QString model = draggedModel(e->mimeData(), &from);
        if (model.isEmpty() || from == m_index || (accepts && !accepts(model))) return;
        e->acceptProposedAction();
        setHighlight(true);
    }
    void dragLeaveEvent(QDragLeaveEvent *) override { setHighlight(false); }
    void dropEvent(QDropEvent *e) override
    {
        setHighlight(false);
        int from = -1;
        const QString model = draggedModel(e->mimeData(), &from);
        if (model.isEmpty() || from == m_index || (accepts && !accepts(model))) return;
        e->acceptProposedAction();
        if (dropped) dropped(model, from);
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && m_well->geometry().contains(e->position().toPoint())) m_pressAt = e->position().toPoint();
        else m_pressAt = {-1, -1};
        QFrame::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!(e->buttons() & Qt::LeftButton) || m_pressAt.x() < 0 || m_slot.model.isEmpty()) return;
        if ((e->position().toPoint() - m_pressAt).manhattanLength() < QApplication::startDragDistance()) return;
        auto *mime = new QMimeData;
        mime->setData(MimeType, QString("%1|%2").arg(m_slot.model).arg(m_index).toUtf8());
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(m_well->grab());
        drag->setHotSpot(m_pressAt - m_well->geometry().topLeft());
        m_pressAt = {-1, -1};
        drag->exec(Qt::MoveAction);
    }

private:
    void setHighlight(bool on)
    {
        setProperty("drop", on);
        repolish(this);
    }

    int m_index;
    Slot m_slot;
    QFrame *m_well;
    QLabel *m_model;
    QLabel *m_detail;
    QLabel *m_state;
    QLabel *m_code;   // the files that can run code, if any
    QToolButton *m_clear;
    QHash<QString, QCheckBox *> m_boxes;
    QPoint m_pressAt {-1, -1};
    bool m_updating = false;
};

// ---------------------------------------------------------------- Integration chip

namespace {

class IntegrationChip : public QFrame
{
public:
    explicit IntegrationChip(const QString &id) : m_id(id)
    {
        setObjectName("slotWell");
        setProperty("filled", true);
        setCursor(Qt::OpenHandCursor);
        setToolTip(ModelSlotsView::tr("Drag it into a slot to give it the slot's skills"));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(10, 6, 14, 6);
        h->setSpacing(8);
        auto *grip = new QLabel(QString::fromUtf8("⠿"));
        grip->setObjectName("sourceMeta");
        h->addWidget(grip);
        auto *name = new QLabel(integrationName(id));
        name->setObjectName("slotModel");
        h->addWidget(name);
    }

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        m_pressAt = e->button() == Qt::LeftButton ? e->position().toPoint() : QPoint(-1, -1);
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!(e->buttons() & Qt::LeftButton) || m_pressAt.x() < 0) return;
        if ((e->position().toPoint() - m_pressAt).manhattanLength() < QApplication::startDragDistance()) return;
        auto *mime = new QMimeData;
        mime->setData(MimeType, m_id.toUtf8());
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(grab());
        drag->setHotSpot(m_pressAt);
        m_pressAt = {-1, -1};
        drag->exec(Qt::MoveAction);
    }

private:
    QString m_id;
    QPoint m_pressAt {-1, -1};
};

} // namespace

QWidget *integrationChip(const char *id, ModelSlotsView *view)
{
    const QString model = QString::fromLatin1(id);
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(10);
    h->addWidget(new IntegrationChip(model));
    auto *state = new QLabel;
    state->setObjectName("slotState");
    state->setWordWrap(true);
    h->addWidget(state, 1);
    const auto update = [state, model] {
        const QVector<Slot> all = load();
        const int slot = slotHolding(all, model);
        QStringList names;
        if (slot >= 0)
            for (const Skill &k : skills())
                if (all[slot].skills.contains(QString::fromLatin1(k.id))) names << ModelSlotsView::tr(k.name);
        const bool claude = model == QLatin1String(ClaudeId);
        QString text = slot < 0 ? ModelSlotsView::tr("In no slot: it gets none of Deepwater's tools. Drag it into a slot below.")
                       : names.isEmpty() ? ModelSlotsView::tr("In slot %1, which grants no skill: tick one to give it tools.").arg(slot + 1)
                                         : ModelSlotsView::tr("In slot %1, which grants: %2.").arg(slot + 1).arg(names.join(", "));
        if (claude && slot >= 0) text += ' ' + ModelSlotsView::tr("Nothing in the app calls Claude yet, so it reads nothing for now.");
        state->setText(text);
        state->setProperty("tone", !claude && !names.isEmpty() ? "good" : "dim");
        repolish(state);
    };
    QObject::connect(view, &ModelSlotsView::slotsChanged, state, update);
    update();
    return row;
}

} // namespace modelslots

// ---------------------------------------------------------------- ModelSlotsView

using namespace modelslots;

ModelSlotsView::ModelSlotsView(QWidget *parent)
    : QWidget(parent)
    , m_slots(load())
    , m_states(Count, State::Empty)
    , m_generation(Count, 0)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto *title = new QLabel(tr("Slots").toUpper());
    title->setObjectName("sectionLabel");
    v->addWidget(title);
    m_note = new QLabel(tr("Drag a model of this computer or an integration (Claude, opencode) into a slot and tick the "
                           "skills the slot gives it: it can do nothing else. A slot takes GGUF models only, and is bound to "
                           "their files: if one changes, its skills are suspended. Only a model in a bound slot can run "
                           "(AI › Local), and its skills are all it can reach: each call is checked and written to the journal."));
    m_note->setObjectName("dim");
    m_note->setWordWrap(true);
    v->addWidget(m_note);
    m_status = new QLabel;   // why a drop was refused
    m_status->setObjectName("slotState");
    m_status->setWordWrap(true);
    m_status->hide();
    v->addWidget(m_status);
    auto *row = new QHBoxLayout;
    row->setSpacing(12);
    for (int i = 0; i < Count; ++i) {
        auto *card = new Card(i, this);
        card->accepts = [this](const QString &model) { return isIntegration(model) || m_installed.contains(model); };
        card->dropped = [this, i](const QString &model, int from) { drop(i, model, from); };
        card->cleared = [this, i] {
            ++m_generation[i];
            m_slots[i].model.clear();   // the slot keeps its skills
            m_slots[i].files.clear();
            m_states[i] = State::Empty;
            save(m_slots);
            refresh();
        };
        card->skillChanged = [this, i](const QString &skill, bool on) {
            QStringList &list = m_slots[i].skills;
            list.removeAll(skill);
            if (on) list << skill;
            save(m_slots);
            emit slotsChanged();
        };
        m_cards << card;
        row->addWidget(card, 1);
    }
    v->addLayout(row);
    refresh();
}

void ModelSlotsView::setInstalled(const QVector<LocalModels::Entry> &models)
{
    m_installed.clear();
    for (const LocalModels::Entry &e : models) m_installed.insert(e.id, e);
    for (int i = 0; i < Count; ++i)
        if (m_states[i] != State::Binding && m_states[i] != State::Checking) check(i);
    refresh();
}

QStringList ModelSlotsView::grantedSkills(const QString &model) const
{
    QStringList out;
    for (int i = 0; i < Count; ++i)
        if (m_slots[i].model == model && m_states[i] == State::Bound)
            for (const QString &s : m_slots[i].skills)
                if (!out.contains(s)) out << s;
    return out;
}

namespace {
// The parts of a split GGUF ("name-00001-of-00003.gguf"): llama.cpp opens them all from the first.
const QRegularExpression &splitPart()
{
    static const QRegularExpression re("^(.*)-(\\d{5})-of-(\\d{5})\\.gguf$", QRegularExpression::CaseInsensitiveOption);
    return re;
}
} // namespace

QVector<ModelSlotsView::Runnable> ModelSlotsView::runnable() const
{
    QVector<Runnable> out;
    for (int i = 0; i < Count; ++i) {
        if (m_states[i] != State::Bound) continue;
        Runnable r;
        r.slot = i;
        r.model = m_slots[i].model;
        r.skills = m_slots[i].skills;
        const QString root = HuggingFaceHub::modelsRoot() + '/' + r.model + '/';
        QStringList files = m_slots[i].files.keys();
        files.sort();
        for (const QString &f : files) {
            if (QFileInfo(f).suffix().compare("gguf", Qt::CaseInsensitive) != 0) continue;
            const QRegularExpressionMatch m = splitPart().match(QFileInfo(f).fileName());
            if (m.hasMatch() && m.captured(2).toInt() != 1) continue;
            r.ggufs << root + f;
        }
        if (!r.ggufs.isEmpty()) out << r;
    }
    return out;
}

int ModelSlotsView::slotOf(const QString &integration) const
{
    return slotHolding(m_slots, integration);
}

QStringList ModelSlotsView::skillsOf(int slot) const
{
    return slot >= 0 && slot < m_slots.size() ? m_slots[slot].skills : QStringList();
}

bool ModelSlotsView::mayRun(int slot, const QString &model, const QString &gguf, QStringList *files) const
{
    if (slot < 0 || slot >= Count || m_states[slot] != State::Bound || m_slots[slot].model != model) return false;
    const Slot &s = m_slots[slot];
    const QHash<QString, LocalModels::Fingerprint> now = LocalModels::stat(model);
    if (now.size() != s.files.size()) return false;
    for (auto f = now.begin(); f != now.end(); ++f) {
        const auto bound = s.files.constFind(f.key());
        if (bound == s.files.constEnd() || bound->size != f->size || bound->modified != f->modified) return false;
    }
    const QString root = HuggingFaceHub::modelsRoot() + '/' + model + '/';
    if (!gguf.startsWith(root) || !s.files.contains(gguf.mid(root.size()))) return false;
    files->clear();
    *files << gguf;
    const QRegularExpressionMatch m = splitPart().match(QFileInfo(gguf).fileName());
    if (m.hasMatch()) {
        const QString dir = QFileInfo(gguf).path();
        const int parts = m.captured(3).toInt();
        for (int p = 2; p <= parts; ++p) {
            const QString part = QString("%1/%2-%3-of-%4.gguf").arg(dir, m.captured(1)).arg(p, 5, 10, QChar('0')).arg(m.captured(3));
            if (!s.files.contains(part.mid(root.size()))) return false;
            *files << part;
        }
    }
    return true;
}

void ModelSlotsView::drop(int slot, const QString &model, int from)
{
    if (isIntegration(model)) {
        // It gets this slot's skills and leaves any other slot: there is one opencode and one Claude.
        say({}, false);
        for (int i = 0; i < Count; ++i)
            if (i != slot && m_slots[i].model == model) {
                ++m_generation[i];
                m_slots[i].model.clear();
                m_slots[i].files.clear();
                m_states[i] = State::Empty;
            }
        ++m_generation[slot];
        m_slots[slot].model = model;
        m_slots[slot].files.clear();
        m_states[slot] = State::Integration;
        save(m_slots);
        refresh();
        return;
    }
    const auto it = m_installed.constFind(model);
    if (it == m_installed.constEnd()) return;
    if (!LocalModels::runnable(it->files)) {
        say(tr("%1 has no GGUF file, and the local engine reads GGUF only: a slot can't take it. A GGUF version is "
               "often on the Hub as \"%2-GGUF\".").arg(model, model.section('/', 1)), true);
        return;
    }
    say({}, false);
    // The skills belong to the slot: a model dropped here, from the list or from another slot, gets this slot's
    // skills (none on a new slot: a skill is always a choice), and the slot it left keeps its own. A model moved
    // keeps the files its slot was bound to; one from the list is bound to its files as they are now.
    if (from >= 0 && from < Count && from != slot) {
        ++m_generation[slot];
        m_slots[slot].model = model;
        m_slots[slot].files = m_slots[from].files;
        ++m_generation[from];
        m_slots[from].model.clear();
        m_slots[from].files.clear();
        m_states[from] = State::Empty;
        save(m_slots);
        check(slot);
    } else {
        m_slots[slot].model = model;
        m_slots[slot].files.clear();
        save(m_slots);
        bind(slot);
    }
    refresh();
}

void ModelSlotsView::bind(int i)
{
    m_states[i] = State::Binding;
    const int generation = ++m_generation[i];
    const QString model = m_slots[i].model;
    QPointer<ModelSlotsView> self(this);
    QtConcurrent::run([model] { return LocalModels::fingerprint(model); })
        .then(this, [self, i, generation, model](const QHash<QString, LocalModels::Fingerprint> &files) {
            if (!self || self->m_generation[i] != generation || self->m_slots[i].model != model) return;
            self->m_slots[i].files = files;
            save(self->m_slots);
            self->m_states[i] = files.isEmpty() ? State::Missing : State::Bound;
            self->refresh();
        });
    refresh();
}

void ModelSlotsView::check(int i)
{
    Slot &s = m_slots[i];
    const auto it = m_installed.constFind(s.model);
    if (s.model.isEmpty()) { m_states[i] = State::Empty; return; }
    if (isIntegration(s.model)) { m_states[i] = State::Integration; return; }
    if (it == m_installed.constEnd()) { m_states[i] = State::Missing; return; }
    if (!LocalModels::runnable(it->files)) { m_states[i] = State::NotRunnable; return; }
    if (s.files.isEmpty()) { bind(i); return; }   // a slot saved before the fingerprints: bound to the files of now

    const QHash<QString, LocalModels::Fingerprint> now = LocalModels::stat(s.model);
    if (QSet<QString>(now.keyBegin(), now.keyEnd()) != QSet<QString>(s.files.keyBegin(), s.files.keyEnd())) {
        m_states[i] = State::Changed;   // a file appeared or went
        return;
    }
    bool same = true;
    for (auto f = now.begin(); f != now.end() && same; ++f)
        same = f->size == s.files.value(f.key()).size && f->modified == s.files.value(f.key()).modified;
    if (same) { m_states[i] = State::Bound; return; }

    // A size or a date changed: the hashes say whether the content did (a copy keeps the bytes, not the date).
    m_states[i] = State::Checking;
    const int generation = ++m_generation[i];
    const QString model = s.model;
    QPointer<ModelSlotsView> self(this);
    QtConcurrent::run([model] { return LocalModels::fingerprint(model); })
        .then(this, [self, i, generation, model](const QHash<QString, LocalModels::Fingerprint> &files) {
            if (!self || self->m_generation[i] != generation || self->m_slots[i].model != model) return;
            Slot &slot = self->m_slots[i];
            bool same = files.size() == slot.files.size();
            for (auto f = files.begin(); f != files.end() && same; ++f)
                same = !f->sha256.isEmpty() && f->sha256 == slot.files.value(f.key()).sha256;
            if (same) {
                slot.files = files;   // the same bytes: the new sizes and dates, to not hash again
                save(self->m_slots);
            }
            self->m_states[i] = same ? State::Bound : State::Changed;
            self->refresh();
        });
}

void ModelSlotsView::refresh()
{
    for (int i = 0; i < Count; ++i) {
        const auto it = m_installed.constFind(m_slots[i].model);
        m_cards[i]->display(m_slots[i], m_states[i], it == m_installed.constEnd() ? nullptr : &it.value());
    }
    emit slotsChanged();
}

void ModelSlotsView::say(const QString &text, bool error)
{
    m_status->setText(text);
    m_status->setProperty("tone", error ? "bad" : "dim");
    repolish(m_status);
    m_status->setVisible(!text.isEmpty());
    if (!text.isEmpty()) {
        QPointer<QLabel> status = m_status;
        const QString shown = text;
        QTimer::singleShot(12000, this, [status, shown] {
            if (status && status->text() == shown) status->hide();
        });
    }
}
