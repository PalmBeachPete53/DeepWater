#include "JournalData.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {

QString filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/journal/journal.json";
}

} // namespace

JournalStore *JournalStore::instance()
{
    static auto *store = new JournalStore(qApp);
    return store;
}

JournalStore::JournalStore(QObject *parent)
    : QObject(parent)
{
    load();
}

void JournalStore::load()
{
    QFile f(filePath());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue &v : root.value("accounts").toArray()) {
            const QJsonObject o = v.toObject();
            Account a;
            a.name = o.value("name").toString();
            a.currency = o.value("currency").toString("USD");
            a.balance = o.value("balance").toDouble(10000);
            a.lastImport = QDateTime::fromString(o.value("lastImport").toString(), Qt::ISODate);
            for (const QJsonValue &t : o.value("trades").toArray())
                a.trades.push_back(desk::Trade::fromJson(t.toObject()));
            m_accounts.push_back(a);
        }
        m_current = root.value("current").toInt();
    }
    if (m_accounts.isEmpty())
        m_accounts.push_back({tr("Main account")});
    m_current = std::clamp(m_current, 0, int(m_accounts.size()) - 1);
}

void JournalStore::save() const
{
    if (m_memoryOnly)
        return;
    QJsonArray accounts;
    for (const Account &a : m_accounts) {
        QJsonArray trades;
        for (const desk::Trade &t : a.trades)
            trades.append(t.toJson());
        QJsonObject o {{"name", a.name}, {"currency", a.currency}, {"balance", a.balance}, {"trades", trades}};
        if (a.lastImport.isValid())
            o.insert("lastImport", a.lastImport.toString(Qt::ISODate));
        accounts.append(o);
    }
    QDir().mkpath(QFileInfo(filePath()).absolutePath());
    QSaveFile f(filePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject {{"accounts", accounts}, {"current", m_current}}).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void JournalStore::setCurrent(int index)
{
    if (index < 0 || index >= m_accounts.size() || index == m_current)
        return;
    m_current = index;
    save();
    emit changed();
}

void JournalStore::addAccount(const QString &name, const QString &currency, double balance)
{
    Account a;
    a.name = name;
    a.currency = currency;
    a.balance = balance;
    m_accounts.push_back(a);
    m_current = int(m_accounts.size()) - 1;
    save();
    emit changed();
}

void JournalStore::editAccount(const QString &name, const QString &currency, double balance)
{
    Account &a = m_accounts[m_current];
    a.name = name;
    a.currency = currency;
    a.balance = balance;
    save();
    emit changed();
}

void JournalStore::removeAccount()
{
    m_accounts.removeAt(m_current);
    if (m_accounts.isEmpty())
        m_accounts.push_back({tr("Main account")});
    m_current = std::clamp(m_current, 0, int(m_accounts.size()) - 1);
    save();
    emit changed();
}

void JournalStore::importTrades(const QVector<desk::Trade> &trades, int *added, int *updated)
{
    Account &a = m_accounts[m_current];
    a.trades = desk::mergeTrades(a.trades, trades, added, updated);
    a.lastImport = QDateTime::currentDateTimeUtc();
    save();
    emit changed();
}

void JournalStore::saveTrade(const desk::Trade &trade)
{
    desk::Trade t = trade;
    if (t.id.isEmpty())
        t.id = "manual-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    Account &a = m_accounts[m_current];
    a.trades = desk::mergeTrades(a.trades, {t});
    save();
    emit changed();
}

void JournalStore::removeTrade(const QString &id)
{
    Account &a = m_accounts[m_current];
    a.trades.erase(std::remove_if(a.trades.begin(), a.trades.end(), [&](const desk::Trade &t) { return t.id == id; }), a.trades.end());
    save();
    emit changed();
}

QStringList JournalStore::symbols() const
{
    QStringList out;
    for (const desk::Trade &t : account().trades)
        if (!out.contains(t.symbol))
            out << t.symbol;
    out.sort();
    return out;
}

QStringList JournalStore::strategies() const
{
    QStringList out;
    for (const desk::Trade &t : account().trades)
        if (!t.strategy.isEmpty() && !out.contains(t.strategy))
            out << t.strategy;
    out.sort();
    return out;
}

QVector<int> JournalStore::years() const
{
    QVector<int> out;
    for (const desk::Trade &t : account().trades) {
        const int y = (t.isClosed() ? t.closeTime : t.openTime).date().year();
        if (!out.contains(y))
            out << y;
    }
    std::sort(out.begin(), out.end(), std::greater<int>());
    return out;
}

void JournalStore::loadSampleForThisRun(const QString &path)
{
    m_memoryOnly = true;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    m_accounts = {{tr("Sample account")}};
    m_current = 0;
    m_accounts[0].trades = desk::readStatement(f.readAll()).trades;
    m_accounts[0].lastImport = QDateTime::currentDateTimeUtc();
    emit changed();
}
