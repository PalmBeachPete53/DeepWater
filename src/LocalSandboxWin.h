#pragma once

#include <QString>
#include <QStringList>

class QProcess;

// The Windows counterpart of the Seatbelt profile in LocalEngine: starts the helper in an AppContainer, Windows' own
// sandbox. A process in one has no network unless given a capability (it gets none), and reads and writes only what is
// granted to its container or open to all of them: here, read access to the model's files and the helper's folder. A job
// object also forbids it any child process, and ends it with the app.
namespace localsandbox {

// Creates the container if needed and grants it read access to these files and to `folder`, with what's in it. False,
// with the reason, when it can't: the caller then doesn't start the helper rather than run it outside the sandbox.
bool prepare(const QStringList &files, const QString &folder, QString *error);

// Makes `process`'s next start() create it in the container, and puts it in the job once it runs.
void applyTo(QProcess *process);

} // namespace localsandbox
