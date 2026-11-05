#pragma once

#include "AiModels.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;

// AI › Models › Slots: where a model gets its skills. A slot holds one model of this computer (dragged from the
// list above, or from another slot; only those the local engine can run: GGUF) or one integration (Claude, opencode,
// dragged from Models › Integration) and grants it the slot's skills, which stay with the slot when it leaves; nothing
// has a skill outside a slot, and only a model held by a bound slot can run (AI › Local).
//
// A slot is bound to the model's files as they were when it took the model: their SHA-256 (with their sizes and
// dates to see a change without hashing). If a file changes, appears or goes, the slot's skills are suspended until
// the model is dragged in again; an integration has no files to bind. Kept in the settings ("ai/slots", JSON). The
// skills are the permissions it runs with: the tools it is offered and allowed to call (LocalTools), checked at every call.
namespace modelslots {

constexpr int Count = 4;
// The mime type of a dragged model: its id ("org/name"), and for a drag out of a slot, "|<slot index>".
inline constexpr char MimeType[] = "application/x-deepwater-model";
// An integration in a slot: Slot::model holds one of these instead of "org/name". It sits in one slot at a time.
inline constexpr char ClaudeId[] = "integration:claude";
inline constexpr char OpencodeId[] = "integration:opencode";
bool isIntegration(const QString &model);
QString integrationName(const QString &model);

struct Skill {
    const char *id;
    const char *name;   // QT_TRANSLATE_NOOP("ModelSlotsView", …)
    const char *what;
};
// The skills a slot can grant: reading Deepwater's data, trading (the tester and the paper account of the Charts tab).
const QVector<Skill> &skills();

struct Slot {
    QString model;        // "org/name" or an integration's id, empty when the slot is free
    QStringList skills;   // ids of skills()
    QHash<QString, LocalModels::Fingerprint> files;   // the model's files when the slot took it
};
QVector<Slot> load();     // always Count slots
void save(const QVector<Slot> &all);
int slotHolding(const QVector<Slot> &all, const QString &model);   // the first slot holding it, or -1

// Where a slot stands, from its model's files on disk against those it was bound to.
enum class State {
    Empty,
    Missing,        // the model is no longer on this computer
    NotRunnable,    // no GGUF file
    Binding,        // taking the fingerprints of a model just dropped
    Checking,       // a file's size or date changed: hashing again
    Changed,        // the files differ from those the slot took: skills suspended
    Bound,          // the skills apply
    Integration,    // holds an integration: no files to check
};

class Card;

} // namespace modelslots

class ModelSlotsView;

namespace modelslots {
// What the Integration part drags into a slot: the integration's name, and beside it the slot that holds it and what
// that slot grants, kept up to date from `view`.
QWidget *integrationChip(const char *id, ModelSlotsView *view);
} // namespace modelslots

class ModelSlotsView : public QWidget
{
    Q_OBJECT
public:
    explicit ModelSlotsView(QWidget *parent = nullptr);
    // The models on this computer: checks each slot's model against the files it was bound to.
    void setInstalled(const QVector<LocalModels::Entry> &models);
    // The skills a model has: those of the slots holding it, bound to its current files (none outside a slot).
    QStringList grantedSkills(const QString &model) const;

    // A model the local engine may run: one held by a bound slot, with its GGUF files (absolute paths; a model split
    // in parts counts once, by its first part) and the slot's skills.
    struct Runnable {
        int slot = -1;
        QString model;
        QStringList ggufs;
        QStringList skills;
    };
    QVector<Runnable> runnable() const;
    // The slot holding an integration (-1 when none does), and a slot's skills as they are now.
    int slotOf(const QString &integration) const;
    QStringList skillsOf(int slot) const;
    // Checks again, against the files on disk, that a slot still holds `model` bound to the same files: the last
    // check before the engine loads it. The files to let the engine read (every part of a split model) go to `files`.
    bool mayRun(int slot, const QString &model, const QString &gguf, QStringList *files) const;

signals:
    void slotsChanged();   // a slot took, lost or re-checked a model, or its skills changed

private:
    void drop(int slot, const QString &model, int from);
    void bind(int slot);                // fingerprints the slot's model, then saves
    void check(int slot);               // compares the model's files with the slot's
    void refresh();
    void say(const QString &text, bool error);

    QVector<modelslots::Slot> m_slots;
    QVector<modelslots::State> m_states;
    QVector<int> m_generation;          // per slot: a fingerprint that comes back for an older drop is dropped
    QVector<modelslots::Card *> m_cards;
    QHash<QString, LocalModels::Entry> m_installed;
    QLabel *m_note;
    QLabel *m_status;
};
