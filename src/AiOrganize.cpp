#include "AiOrganize.h"

#include "DeskPage.h"
#include "JournalData.h"
#include "JournalPage.h"

#include <QDateTime>
#include <QJsonArray>
#include <QUuid>

namespace aiorganize {

namespace {

QJsonObject refused(const QString &why)
{
    return {{"error", why}};
}

QJsonObject listAll()
{
    int current = 0;
    const QVector<deskui::Workspace> spaces = deskui::loadWorkspaces(&current);
    const QStringList names = deskui::sectionNames();
    QJsonArray workspaces, sections, accounts;
    for (const deskui::Workspace &s : spaces) {
        QJsonArray panels;
        for (int p : s.panels)
            panels.append(names.value(p));
        workspaces.append(QJsonObject {{"name", s.name}, {"panels", panels}});
    }
    for (int i = 0; i < deskui::sectionCount(); ++i)
        sections.append(names.value(i));
    JournalStore *store = JournalStore::instance();
    for (const auto &a : store->accounts())
        accounts.append(QJsonObject {{"name", a.name}, {"currency", a.currency}, {"trades", int(a.trades.size())}});
    return {{"workspaces", workspaces}, {"current_workspace", spaces.value(current).name}, {"sections", sections},
            {"journal_accounts", accounts}, {"current_journal_account", store->account().name}};
}

int findWorkspace(const QVector<deskui::Workspace> &spaces, const QString &name)
{
    for (int i = 0; i < spaces.size(); ++i)
        if (spaces[i].name.compare(name, Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

QJsonObject openWorkspace(const Context &context, const QJsonObject &a)
{
    const QString name = a.value("name").toString().trimmed();
    const int index = findWorkspace(deskui::loadWorkspaces(), name);
    if (index < 0)
        return refused("no workspace named \"" + name + "\": list_workspaces gives their names");
    DeskPage *desk = context.showDesk();
    if (!desk)
        return refused("the Desk isn't available");
    desk->openWorkspace(index);
    return {{"opened", deskui::loadWorkspaces().value(index).name}};
}

QJsonObject createWorkspace(const Context &context, const QJsonObject &a)
{
    const QString name = a.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 40)
        return refused("a workspace needs a name of 1 to 40 characters");
    QVector<deskui::Workspace> spaces = deskui::loadWorkspaces();
    if (findWorkspace(spaces, name) >= 0)
        return refused("a workspace named \"" + name + "\" exists: nothing was changed");
    const QStringList names = deskui::sectionNames();
    const QJsonArray wanted = a.value("panels").toArray();
    if (wanted.isEmpty() || wanted.size() > 6)
        return refused("a workspace holds 1 to 6 panels");
    deskui::Workspace space;
    space.name = name;
    for (const QJsonValue &v : wanted) {
        int found = -1;
        for (int i = 0; i < deskui::sectionCount(); ++i)
            if (names.value(i).compare(v.toString().trimmed(), Qt::CaseInsensitive) == 0)
                found = i;
        if (found < 0)
            return refused("no section \"" + v.toString() + "\": list_workspaces gives their names. Nothing was changed");
        space.panels << found;
    }
    spaces << space;
    int current = 0;
    deskui::loadWorkspaces(&current);
    deskui::saveWorkspaces(spaces, current);
    DeskPage *desk = context.showDesk();
    if (!desk)
        return refused("the Desk isn't available");
    desk->openWorkspace(int(spaces.size()) - 1);
    return {{"created", name}, {"panels", int(space.panels.size())}};
}

QJsonObject switchAccount(const QJsonObject &a)
{
    const QString name = a.value("name").toString().trimmed();
    JournalStore *store = JournalStore::instance();
    for (int i = 0; i < store->accounts().size(); ++i)
        if (store->accounts()[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            store->setCurrent(i);
            return {{"current_journal_account", store->account().name}};
        }
    return refused("no journal account named \"" + name + "\": list_workspaces gives their names");
}

void draft(const Context &context, const QJsonObject &a, const QString &question, const std::function<void(const QJsonObject &)> &done)
{
    desk::Trade t;
    t.symbol = a.value("symbol").toString().trimmed().toUpper();
    const QString side = a.value("side").toString().trimmed().toLower();
    if (t.symbol.isEmpty())
        return done(refused("a trade needs its symbol"));
    if (side != "long" && side != "short")
        return done(refused("side is long or short"));
    t.isLong = side == "long";
    t.volume = a.value("volume").toDouble();
    if (t.volume <= 0)
        return done(refused("a trade needs its volume, in lots"));
    const auto time = [&](const char *key) { return QDateTime::fromString(a.value(key).toString(), Qt::ISODate); };
    t.openTime = time("open_time");
    t.closeTime = time("close_time");
    if (!a.value("close_time").toString().isEmpty() && !t.closeTime.isValid())
        return done(refused("close_time isn't an ISO date"));
    t.openPrice = a.value("open_price").toDouble();
    t.closePrice = a.value("close_price").toDouble();
    t.stopLoss = a.value("stop_loss").toDouble();
    t.takeProfit = a.value("take_profit").toDouble();
    t.profit = a.value("profit").toDouble();
    t.strategy = a.value("strategy").toString().trimmed();
    t.notes = a.value("notes").toString().trimmed();
    t.id = "manual-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QStringList emptied = desk::keepStated(t, question);
    QJsonArray left;
    for (const QString &field : emptied)
        left.append(field);
    JournalPage::draftTrade(t, emptied, !question.trimmed().isEmpty(), context.dialogParent, [done, left](bool saved, const desk::Trade &checked) {
        if (!saved)
            return done({{"saved", false}, {"why", "the user cancelled the form: nothing was written"}, {"left_empty", left}});
        done({{"saved", true}, {"symbol", checked.symbol}, {"side", checked.isLong ? "long" : "short"}, {"volume", checked.volume},
              {"left_empty", left}, {"note", "these are the values the user saved, which they may have changed"}});
    });
}

} // namespace

void run(const Context &context, const QString &tool, const QJsonObject &a, const QString &question, std::function<void(const QJsonObject &)> done)
{
    if (tool == "list_workspaces") done(listAll());
    else if (tool == "open_workspace") done(openWorkspace(context, a));
    else if (tool == "create_workspace") done(createWorkspace(context, a));
    else if (tool == "switch_journal_account") done(switchAccount(a));
    else if (tool == "draft_journal_trade") draft(context, a, question, done);
    else done(refused("no tool named \"" + tool + "\""));
}

} // namespace aiorganize
