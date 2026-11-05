#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

class DeskPage;
class QWidget;

// The tools of the Organize skill: Desk workspaces, the Journal's account, and a journal entry the user confirms. They
// change how the app is arranged; the one that writes a trade only opens its form.
namespace aiorganize {

struct Context {
    std::function<DeskPage *()> showDesk;   // takes the user to the Desk
    QWidget *dialogParent = nullptr;
};
// `question`: what the user asked, or empty when the app doesn't know it (an outside tool).
void run(const Context &context, const QString &tool, const QJsonObject &arguments, const QString &question, std::function<void(const QJsonObject &)> done);

} // namespace aiorganize
