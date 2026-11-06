#include "MetaTraderPage.h"

#include "DeskViews.h"
#include "DeskWidgets.h"
#include "QuantViews.h"
#include "SecretStore.h"
#include "Theme.h"
#include "quant/ui/PanelsKit.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

using namespace broker;

namespace {

// A fake server's key stays apart from the real one.
QString secretName()
{
    return MetaCopierClient::testServer().isEmpty() ? QStringLiteral("metacopier-api-key") : QStringLiteral("metacopier-test-key");
}

constexpr double FtmoCapital = 400000;   // ftmo.com › How many accounts can I have?, 6 November 2026

double parsePrice(const QString &text)
{
    QString t = text.trimmed();
    t.replace(',', '.');
    bool ok = false;
    const double v = t.toDouble(&ok);
    return ok ? v : 0;
}

QString money(double v, const QString &currency)
{
    return QLocale().toString(v, 'f', 2) + (currency.isEmpty() ? QString() : " " + currency);
}

QTableWidgetItem *dataCell(const QString &text, const QColor &color = QColor(), Qt::Alignment align = Qt::AlignCenter)
{
    return deskui::cell(text, color, align);
}

} // namespace

MetaTraderPage::MetaTraderPage(QWidget *parent)
    : QWidget(parent)
    , m_client(new MetaCopierClient(this))
    , m_desk(new TradeDesk(m_client, this))
{
    setObjectName("quantPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(quant::mono(12));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("qtScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName("qtContent");
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(12, 10, 12, 16);
    v->setSpacing(10);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto *notice = quant::label(tr("Orders sent from here are real orders on your MetaTrader accounts, through MetaCopier, a paid service that holds your MetaTrader logins. "
                                   "Check that your broker or prop firm allows it: FTMO limits the capital per trader or strategy and may suspend accounts with identical trades."),
                                "qtNote");
    notice->setWordWrap(true);
    v->addWidget(notice);
    v->addWidget(buildKeyBar());
    m_status = quant::label({}, "qtCardLabel");
    m_status->setWordWrap(true);
    m_status->hide();
    v->addWidget(m_status);
    v->addWidget(buildAccounts());
    v->addWidget(buildTicket());
    v->addWidget(buildPlan());
    v->addWidget(buildPositions());
    v->addStretch(1);
    deskui::fitHeight(m_accountsTable);
    deskui::fitHeight(m_planTable);
    deskui::fitHeight(m_positionsTable);

    m_refresh.setInterval(5000);
    connect(&m_refresh, &QTimer::timeout, this, [this] {
        // The service syncs an account's information once a minute; positions are what can change under the user's eyes.
        if (++m_ticks % 6 == 0)
            refreshAccounts();
        refreshPositions();
    });
    m_planClock.setInterval(1000);
    connect(&m_planClock, &QTimer::timeout, this, &MetaTraderPage::updateActions);
    connect(m_desk, &TradeDesk::prepared, this, [this] {
        fillPlan();
        updateActions();
        m_planClock.start();
    });
    connect(m_desk, &TradeDesk::changed, this, [this] {
        fillPlan();
        updateActions();
    });
    connect(m_desk, &TradeDesk::finished, this, [this] {
        fillPlan();
        refreshPositions();
    });

    const QString saved = SecretStore::read(secretName());
    m_client->setApiKey(saved);
    m_key->setPlaceholderText(saved.isEmpty() ? tr("Your MetaCopier API key") : tr("A key is saved in this computer's secret store"));
    updateActions();
}

QWidget *MetaTraderPage::buildKeyBar()
{
    auto *bar = new QFrame;
    bar->setObjectName("qtBar");
    auto *h = new qtm::kit::WrapBarLayout(bar, 10, 6);
    h->setContentsMargins(10, 6, 10, 6);
    h->addWidget(quant::label(tr("METACOPIER KEY"), "qtFieldLabel"));
    m_key = new QLineEdit;
    m_key->setObjectName("qtInput");
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setMinimumWidth(300);
    h->addWidget(m_key);
    m_saveKey = deskui::deskAction(tr("Save"));
    h->addWidget(m_saveKey);
    m_forgetKey = deskui::deskAction(tr("Forget"));
    h->addWidget(m_forgetKey);
    auto *open = deskui::deskAction(tr("Open MetaCopier"));
    open->setToolTip(tr("Accounts are added on MetaCopier's site, which is also where the API key is made"));
    h->addWidget(open);
    connect(open, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://app.metacopier.io")); });
    connect(m_saveKey, &QPushButton::clicked, this, &MetaTraderPage::saveKey);
    connect(m_key, &QLineEdit::returnPressed, this, &MetaTraderPage::saveKey);
    connect(m_forgetKey, &QPushButton::clicked, this, &MetaTraderPage::forgetKey);
    return bar;
}

QWidget *MetaTraderPage::buildAccounts()
{
    QVBoxLayout *body;
    QHBoxLayout *head;
    auto *card = quant::card(tr("Accounts"), &body, &head);
    m_real = new QCheckBox(tr("Allow real-money accounts"));
    m_real->setObjectName("qtCheck");
    m_real->setToolTip(tr("For this session only. An account the service doesn't report as a demo counts as real money."));
    connect(m_real, &QCheckBox::clicked, this, &MetaTraderPage::askRealMoney);
    head->addWidget(m_real);
    m_accountsTable = deskui::table({tr("Armed"), tr("Account"), tr("Server"), tr("Kind"), tr("State"), tr("Balance"), tr("Equity"), tr("Open")});
    deskui::fitColumns(m_accountsTable, 1);
    body->addWidget(m_accountsTable);
    m_capNote = quant::label({}, "qtNote");
    m_capNote->setWordWrap(true);
    m_capNote->hide();
    body->addWidget(m_capNote);
    return card;
}

QWidget *MetaTraderPage::buildTicket()
{
    auto *bar = new QFrame;
    bar->setObjectName("qtBar");
    auto *h = new qtm::kit::WrapBarLayout(bar, 10, 6);
    h->setContentsMargins(10, 8, 10, 8);
    h->addWidget(quant::label(tr("SYMBOL"), "qtFieldLabel"));
    m_symbol = new QLineEdit("EURUSD");
    m_symbol->setObjectName("qtInput");
    m_symbol->setFixedWidth(110);
    h->addWidget(m_symbol);
    h->addWidget(quant::segments({tr("Buy"), tr("Sell")}, &m_side));
    h->addWidget(quant::label(tr("RISK PER ACCOUNT"), "qtFieldLabel"));
    m_risk = new QDoubleSpinBox;
    m_risk->setObjectName("qtSpin");
    m_risk->setRange(0.01, MaxRiskPercent);
    m_risk->setDecimals(2);
    m_risk->setSingleStep(0.1);
    m_risk->setValue(0.5);
    m_risk->setSuffix(" %");
    m_risk->setFixedWidth(100);
    h->addWidget(m_risk);
    h->addWidget(quant::label(tr("STOP"), "qtFieldLabel"));
    m_stop = new QLineEdit;
    m_stop->setObjectName("qtInput");
    m_stop->setFixedWidth(100);
    h->addWidget(m_stop);
    h->addWidget(quant::label(tr("TARGET"), "qtFieldLabel"));
    m_target = new QLineEdit;
    m_target->setObjectName("qtInput");
    m_target->setPlaceholderText(tr("optional"));
    m_target->setFixedWidth(100);
    h->addWidget(m_target);
    m_prepare = deskui::deskAction(tr("Prepare"));
    h->addWidget(m_prepare);
    connect(m_prepare, &QPushButton::clicked, this, &MetaTraderPage::prepare);
    // A changed order is another order: the plan on screen no longer describes it.
    const auto stale = [this] {
        m_desk->clear();
        fillPlan();
        updateActions();
    };
    for (QLineEdit *e : {m_symbol, m_stop, m_target})
        connect(e, &QLineEdit::textEdited, this, stale);
    connect(m_side, &QButtonGroup::idClicked, this, stale);
    connect(m_risk, &QDoubleSpinBox::valueChanged, this, stale);
    return bar;
}

QWidget *MetaTraderPage::buildPlan()
{
    QVBoxLayout *body;
    QHBoxLayout *head;
    auto *card = quant::card(tr("Plan"), &body, &head);
    m_send = deskui::deskAction(tr("Send to the armed accounts"));
    head->addWidget(m_send);
    connect(m_send, &QPushButton::clicked, this, &MetaTraderPage::sendPlan);
    m_planTable = deskui::table({tr("Account"), tr("Symbol"), tr("Side"), tr("Price"), tr("Stop"), tr("Target"), tr("Lots"), tr("Risk"), tr("State")});
    deskui::fitColumns(m_planTable, 8);
    body->addWidget(m_planTable);
    m_planNote = quant::label(tr("Prepare an order to see what each account would send. Lots are rounded down, so an account never risks more than asked."), "qtNote");
    m_planNote->setWordWrap(true);
    body->addWidget(m_planNote);
    return card;
}

QWidget *MetaTraderPage::buildPositions()
{
    QVBoxLayout *body;
    QHBoxLayout *head;
    auto *card = quant::card(tr("Positions"), &body, &head);
    m_closeAll = deskui::deskAction(tr("Close all on the armed accounts"));
    head->addWidget(m_closeAll);
    connect(m_closeAll, &QPushButton::clicked, this, &MetaTraderPage::closeAllArmed);
    m_positionsTable = deskui::table({tr("Account"), tr("Symbol"), tr("Side"), tr("Lots"), tr("Open"), tr("Stop"), tr("Target"), tr("Profit"), {}});
    deskui::fitColumns(m_positionsTable, 0);
    connect(m_positionsTable, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column != 8 || row >= m_positions.size())
            return;
        const Held held = m_positions[row];
        if (QMessageBox::question(this, tr("Close the position"),
                                  tr("Close %1 %2 lots of %3 on %4?").arg(held.position.type).arg(held.position.volume).arg(held.position.symbol, held.account.alias))
            == QMessageBox::Yes)
            closePosition(held.account, held.position.id);
    });
    body->addWidget(m_positionsTable);
    return card;
}

void MetaTraderPage::showEvent(QShowEvent *)
{
    if (m_client->hasKey() && m_accounts.isEmpty())
        loadAccounts();
    if (m_client->hasKey())
        m_refresh.start();
}

void MetaTraderPage::hideEvent(QHideEvent *)
{
    m_refresh.stop();
    m_planClock.stop();
}

void MetaTraderPage::setStatus(const QString &text, bool error)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
    m_status->setStyleSheet(error ? QStringLiteral("color: %1;").arg(Theme::current().Down.name()) : QString());
}

void MetaTraderPage::saveKey()
{
    const QString key = m_key->text().trimmed();
    if (key.isEmpty())
        return;
    // Tried before it is kept: a refused key doesn't replace a good one.
    const QString previous = SecretStore::read(secretName());
    m_client->setApiKey(key);
    m_saveKey->setEnabled(false);
    setStatus(tr("Trying the key…"));
    m_client->accounts([this, key, previous](const QVector<Account> &accounts, const QString &error) {
        m_saveKey->setEnabled(true);
        if (!error.isEmpty()) {
            m_client->setApiKey(previous);
            setStatus(tr("MetaCopier refused this key: %1").arg(error), true);
            return;
        }
        QString why;
        if (!SecretStore::write(secretName(), key, &why)) {
            m_client->setApiKey(previous);
            setStatus(tr("The key wasn't saved: %1. Nothing was kept.").arg(why), true);
            return;
        }
        m_key->clear();
        m_key->setPlaceholderText(tr("A key is saved in this computer's secret store"));
        m_accounts = accounts;
        setStatus(accounts.isEmpty() ? tr("No account on this key: add them on MetaCopier's site.") : QString());
        fillAccounts();
        updateActions();
        refreshPositions();
        m_refresh.start();
    });
}

void MetaTraderPage::forgetKey()
{
    SecretStore::remove(secretName());
    m_client->setApiKey({});
    m_key->setPlaceholderText(tr("Your MetaCopier API key"));
    m_refresh.stop();
    m_accounts.clear();
    m_armed.clear();
    m_positions.clear();
    m_desk->clear();
    fillAccounts();
    fillPlan();
    fillPositions();
    setStatus(tr("The key was removed from this computer."));
    updateActions();
}

void MetaTraderPage::loadAccounts()
{
    setStatus(tr("Loading the accounts…"));
    m_client->accounts([this](const QVector<Account> &accounts, const QString &error) {
        if (!error.isEmpty()) {
            setStatus(tr("MetaCopier didn't answer: %1").arg(error), true);
            return;
        }
        m_accounts = accounts;
        setStatus(accounts.isEmpty() ? tr("No account on this key: add them on MetaCopier's site.") : QString());
        fillAccounts();
        updateActions();
        refreshPositions();
    });
}

void MetaTraderPage::refreshAccounts()
{
    for (int i = 0; i < m_accounts.size(); ++i) {
        const QString id = m_accounts[i].id;
        m_client->information(m_accounts[i], [this, id](const QJsonObject &info, const QString &error) {
            for (Account &a : m_accounts) {
                if (a.id != id)
                    continue;
                if (error.isEmpty())
                    applyInformation(&a, info);
                else
                    a.connected = false;
            }
            fillAccounts();
            updateActions();
        });
    }
}

void MetaTraderPage::refreshPositions()
{
    const int generation = ++m_generation;
    auto gathered = std::make_shared<QVector<Held>>();
    auto left = std::make_shared<int>(int(m_accounts.size()));
    if (m_accounts.isEmpty()) {
        m_positions.clear();
        fillPositions();
        return;
    }
    for (const Account &account : std::as_const(m_accounts)) {
        m_client->positions(account, [this, account, generation, gathered, left](const QVector<Position> &positions, const QString &) {
            for (const Position &p : positions)
                *gathered << Held {account, p};
            if (--*left == 0 && generation == m_generation) {
                m_positions = *gathered;
                fillPositions();
            }
        });
    }
}

void MetaTraderPage::fillAccounts()
{
    const Palette &ui = Theme::current();
    m_accountsTable->setRowCount(int(m_accounts.size()));
    double armedBalance = 0;
    for (int r = 0; r < m_accounts.size(); ++r) {
        const Account &a = m_accounts[r];
        const bool real = a.environment != Environment::Demo;
        const bool locked = real && !m_desk->allowRealMoney();
        if (locked)
            m_armed.remove(a.id);
        // Kept from one refresh to the next: a box remade every few seconds would lose a click.
        auto *arm = qobject_cast<QCheckBox *>(m_accountsTable->cellWidget(r, 0));
        if (!arm) {
            arm = new QCheckBox;
            arm->setObjectName("qtCheck");
            arm->setCursor(Qt::PointingHandCursor);
            connect(arm, &QCheckBox::clicked, this, [this, arm](bool on) {
                const QString id = arm->property("account").toString();
                if (on)
                    m_armed.insert(id);
                else
                    m_armed.remove(id);
                m_desk->clear();
                fillPlan();
                fillAccounts();
                updateActions();
            });
            m_accountsTable->setCellWidget(r, 0, arm);
        }
        arm->setProperty("account", a.id);
        arm->setChecked(m_armed.contains(a.id));
        arm->setEnabled(!locked);
        arm->setToolTip(locked ? tr("Real money: allow it above first") : tr("Include this account in the orders"));
        m_accountsTable->setItem(r, 1, dataCell(a.alias.isEmpty() ? a.login : a.alias, QColor(), Qt::AlignLeft));
        m_accountsTable->setItem(r, 2, dataCell(a.server));
        const QString kind = a.environment == Environment::Demo ? tr("Demo") : a.environment == Environment::Live ? tr("Real money") : tr("Unknown: counted as real");
        m_accountsTable->setItem(r, 3, dataCell(kind, real ? ui.Down : QColor()));
        const QString state = a.wrongCredentials ? tr("Wrong login") : a.tradingDisabled ? tr("Read only") : a.connected ? tr("Connected") : tr("Not connected");
        m_accountsTable->setItem(r, 4, dataCell(state, a.connected && !a.wrongCredentials && !a.tradingDisabled ? ui.Up : ui.Down));
        m_accountsTable->setItem(r, 5, dataCell(money(a.balance, a.currency)));
        m_accountsTable->setItem(r, 6, dataCell(money(a.equity, a.currency)));
        m_accountsTable->setItem(r, 7, dataCell(QString::number(a.openPositions)));
        if (m_armed.contains(a.id))
            armedBalance += a.balance;
    }
    deskui::fitHeight(m_accountsTable);
    m_capNote->setVisible(armedBalance > FtmoCapital);
    if (armedBalance > FtmoCapital)
        m_capNote->setText(tr("The armed accounts hold more than %1. If they are FTMO accounts, identical trades over its limit of $400,000 per trader or strategy, counted before scaling, can get them suspended.")
                               .arg(QLocale().toString(FtmoCapital, 'f', 0)));
}

QVector<Account> MetaTraderPage::armedAccounts() const
{
    QVector<Account> out;
    for (const Account &a : m_accounts)
        if (m_armed.contains(a.id))
            out << a;
    return out;
}

void MetaTraderPage::prepare()
{
    Intent intent;
    intent.symbol = m_symbol->text().trimmed();
    intent.buy = m_side->checkedId() == 0;
    intent.riskPercent = m_risk->value();
    intent.stop = parsePrice(m_stop->text());
    intent.target = parsePrice(m_target->text());
    if (armedAccounts().isEmpty()) {
        setStatus(tr("Arm at least one account."), true);
        return;
    }
    if (intent.symbol.isEmpty() || !(intent.stop > 0)) {
        setStatus(tr("A symbol and a stop are needed: without a stop, the risk can't be worked out."), true);
        return;
    }
    setStatus({});
    m_desk->prepare(intent, armedAccounts());
    m_prepare->setEnabled(false);
    m_planNote->setText(tr("Pricing the order on each account…"));
}

QString MetaTraderPage::refusalText(const PlanLine &line) const
{
    switch (line.refusal) {
    case Refusal::None: return {};
    case Refusal::NoSymbol: return tr("No such symbol on this account");
    case Refusal::AmbiguousSymbol: return tr("Several symbols could be it: %1").arg(line.candidates.join(", "));
    case Refusal::SymbolDisabled: return tr("Symbol disabled by the broker");
    case Refusal::SymbolIncomplete: return tr("The service gave no contract size or volume steps");
    case Refusal::NoQuote: return tr("No price");
    case Refusal::NoConversion: return tr("No price to convert into the account's currency");
    case Refusal::NoBalance: return tr("No balance known");
    case Refusal::RiskInvalid: return tr("Invalid risk");
    case Refusal::RiskTooHigh: return tr("Risk over %1 %").arg(MaxRiskPercent);
    case Refusal::NoStop: return tr("No stop");
    case Refusal::StopWrongSide: return tr("Stop on the wrong side of the price");
    case Refusal::TargetWrongSide: return tr("Target on the wrong side of the price");
    case Refusal::TooSmall: return tr("Under the minimum volume at this risk");
    case Refusal::NotConnected: return tr("Not connected");
    case Refusal::WrongCredentials: return tr("Wrong MetaTrader login");
    case Refusal::TradingDisabled: return tr("Read only");
    case Refusal::RealMoneyLocked: return tr("Real money not allowed");
    case Refusal::NotChecked: return tr("The risk couldn't be checked");
    }
    return {};
}

QString MetaTraderPage::stateText(const SentLine &line) const
{
    if (!line.plan.ok())
        return refusalText(line.plan);
    switch (line.state) {
    case LineState::Planned: return line.plan.cappedAtMaximum ? tr("Ready, at the symbol's maximum volume") : tr("Ready");
    case LineState::Sending: return tr("Sending…");
    case LineState::Accepted: return tr("Sent, checking on the account…");
    case LineState::Confirmed: return tr("Open");
    case LineState::Rejected: return tr("Refused: %1").arg(line.message);
    case LineState::NotConfirmed: return tr("Not seen on the account: check in MetaTrader before sending again");
    }
    return {};
}

void MetaTraderPage::fillPlan()
{
    const Palette &ui = Theme::current();
    const QVector<SentLine> &lines = m_desk->lines();
    m_planTable->setRowCount(int(lines.size()));
    for (int r = 0; r < lines.size(); ++r) {
        const SentLine &l = lines[r];
        const PlanLine &p = l.plan;
        const bool ok = p.ok();
        const QString dash = QStringLiteral("—");
        m_planTable->setItem(r, 0, dataCell(p.account.alias.isEmpty() ? p.account.login : p.account.alias, QColor(), Qt::AlignLeft));
        m_planTable->setItem(r, 1, dataCell(ok ? p.symbol : dash));
        m_planTable->setItem(r, 2, dataCell(ok ? (p.buy ? tr("Buy") : tr("Sell")) : dash, p.buy ? ui.Up : ui.Down));
        m_planTable->setItem(r, 3, dataCell(ok ? QString::number(p.price, 'f', p.digits) : dash));
        m_planTable->setItem(r, 4, dataCell(ok ? QString::number(p.stop, 'f', p.digits) : dash));
        m_planTable->setItem(r, 5, dataCell(ok && p.target > 0 ? QString::number(p.target, 'f', p.digits) : dash));
        m_planTable->setItem(r, 6, dataCell(ok ? QString::number(p.lots, 'f', 2) : dash, QColor(), Qt::AlignCenter));
        m_planTable->setItem(r, 7, dataCell(ok ? QStringLiteral("%1 (%2 %)").arg(money(p.riskAmount, p.account.currency), QLocale().toString(p.riskPercent, 'f', 2)) : dash));
        const bool bad = !ok || l.state == LineState::Rejected || l.state == LineState::NotConfirmed;
        m_planTable->setItem(r, 8, dataCell(stateText(l), bad ? ui.Down : l.state == LineState::Confirmed ? ui.Up : QColor(), Qt::AlignLeft));
    }
    deskui::fitHeight(m_planTable);
    if (lines.isEmpty())
        m_planNote->setText(tr("Prepare an order to see what each account would send. Lots are rounded down, so an account never risks more than asked."));
    else
        m_planNote->setText(tr("A plan holds for %1 seconds: prices move. Sending uses it up.").arg(TradeDesk::PlanLifetimeMs / 1000));
}

void MetaTraderPage::fillPositions()
{
    const Palette &ui = Theme::current();
    m_positionsTable->setRowCount(int(m_positions.size()));
    for (int r = 0; r < m_positions.size(); ++r) {
        const Held &h = m_positions[r];
        m_positionsTable->setItem(r, 0, dataCell(h.account.alias.isEmpty() ? h.account.login : h.account.alias, QColor(), Qt::AlignLeft));
        m_positionsTable->setItem(r, 1, dataCell(h.position.symbol));
        m_positionsTable->setItem(r, 2, dataCell(h.position.type == "Buy" ? tr("Buy") : h.position.type == "Sell" ? tr("Sell") : h.position.type, h.position.type == "Buy" ? ui.Up : ui.Down));
        m_positionsTable->setItem(r, 3, dataCell(QString::number(h.position.volume, 'f', 2)));
        m_positionsTable->setItem(r, 4, dataCell(QString::number(h.position.openPrice, 'g', 10)));
        m_positionsTable->setItem(r, 5, dataCell(h.position.stopLoss > 0 ? QString::number(h.position.stopLoss, 'g', 10) : QStringLiteral("—")));
        m_positionsTable->setItem(r, 6, dataCell(h.position.takeProfit > 0 ? QString::number(h.position.takeProfit, 'g', 10) : QStringLiteral("—")));
        m_positionsTable->setItem(r, 7, dataCell(money(h.position.profit, h.account.currency), h.position.profit >= 0 ? ui.Up : ui.Down));
        m_positionsTable->setItem(r, 8, dataCell(tr("Close"), ui.Accent));
    }
    deskui::fitHeight(m_positionsTable);
}

void MetaTraderPage::updateActions()
{
    const bool key = m_client->hasKey();
    m_forgetKey->setEnabled(key);
    m_prepare->setEnabled(key && !m_armed.isEmpty());
    m_send->setEnabled(m_desk->canSend());
    m_closeAll->setEnabled(!m_armed.isEmpty());
    // A plan that ran out says so instead of a button gone grey for no reason.
    const QVector<SentLine> &lines = m_desk->lines();
    if (!lines.isEmpty() && !m_desk->canSend() && std::all_of(lines.begin(), lines.end(), [](const SentLine &l) { return l.state == LineState::Planned; })
        && std::any_of(lines.begin(), lines.end(), [](const SentLine &l) { return l.plan.ok(); }))
        m_planNote->setText(tr("This plan is too old: prepare the order again."));
}

void MetaTraderPage::sendPlan()
{
    if (!m_desk->canSend())
        return;
    QStringList summary;
    bool real = false;
    int count = 0;
    double maxRisk = 0;
    for (const SentLine &l : m_desk->lines()) {
        if (!l.plan.ok())
            continue;
        ++count;
        real = real || l.plan.account.environment != Environment::Demo;
        maxRisk = std::max(maxRisk, l.plan.riskPercent);
        summary << tr("%1: %2 %3 lots of %4, risk %5")
                       .arg(l.plan.account.alias, l.plan.buy ? tr("Buy") : tr("Sell"), QString::number(l.plan.lots, 'f', 2), l.plan.symbol,
                            money(l.plan.riskAmount, l.plan.account.currency));
    }
    QMessageBox box(this);
    box.setIcon(real ? QMessageBox::Warning : QMessageBox::Question);
    box.setWindowTitle(tr("Send this order"));
    box.setText(real ? tr("Send to %n account(s), REAL MONEY included?", nullptr, count) : tr("Send to %n account(s)?", nullptr, count));
    box.setInformativeText(summary.join('\n'));
    QPushButton *go = box.addButton(tr("Send"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != go)
        return;
    // The dialog may have stayed open past the plan's life: send() refuses it then.
    if (!m_desk->send())
        setStatus(tr("This plan is too old: prepare the order again."), true);
    updateActions();
}

void MetaTraderPage::closePosition(const Account &account, const QString &positionId)
{
    m_client->closePosition(account, positionId, [this](SendOutcome outcome, const QString &message) {
        if (outcome != SendOutcome::Accepted)
            setStatus(tr("The position wasn't closed: %1").arg(message), true);
        refreshPositions();
    });
}

void MetaTraderPage::closeAllArmed()
{
    const QVector<Account> accounts = armedAccounts();
    if (accounts.isEmpty())
        return;
    if (QMessageBox::warning(this, tr("Close all"), tr("Close every position on %n armed account(s), including those not opened from here?", nullptr, int(accounts.size())),
                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;
    for (const Account &account : accounts) {
        const QString name = account.alias;
        m_client->closeAll(account, [this, name](SendOutcome outcome, const QString &message) {
            if (outcome != SendOutcome::Accepted)
                setStatus(tr("%1 wasn't closed: %2").arg(name, message), true);
            refreshPositions();
        });
    }
}

// Asked again at each launch, and by typing it: a tick alone is too easy to leave on by accident.
void MetaTraderPage::askRealMoney(bool on)
{
    if (on) {
        const QString word = tr("REAL");
        bool ok = false;
        const QString typed = QInputDialog::getText(this, tr("Real money"),
                                                    tr("Orders will be able to reach real-money accounts until the app closes. Type %1 to allow it.").arg(word),
                                                    QLineEdit::Normal, {}, &ok);
        on = ok && typed.trimmed() == word;
    }
    m_real->setChecked(on);
    m_desk->setAllowRealMoney(on);
    m_desk->clear();
    fillAccounts();
    fillPlan();
    updateActions();
}
