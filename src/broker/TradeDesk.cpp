#include "broker/TradeDesk.h"

#include <QSettings>

#include <algorithm>
#include <memory>

namespace broker {

TradeDesk::TradeDesk(MetaCopierClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

void TradeDesk::clear()
{
    ++m_generation;
    m_lines.clear();
    m_sent = false;
    m_pending = 0;
}

void TradeDesk::prepare(const Intent &intent, const QVector<Account> &accounts)
{
    clear();
    const int generation = m_generation;
    m_pending = int(accounts.size());
    for (const Account &account : accounts) {
        SentLine line;
        line.plan.account = account;
        m_lines << line;
    }
    if (m_lines.isEmpty()) {
        emit prepared();
        return;
    }
    for (int i = 0; i < m_lines.size(); ++i)
        priceLine(generation, i, intent);
}

void TradeDesk::lineDone(int generation)
{
    if (generation != m_generation || --m_pending > 0)
        return;
    m_plannedAt.start();
    emit prepared();
}

// Each account on its own: its information now, its symbols, the price, and the conversion when it needs one.
void TradeDesk::priceLine(int generation, int index, const Intent &intent)
{
    auto input = std::make_shared<PlanInput>();
    input->account = m_lines[index].plan.account;
    input->intent = intent;
    input->allowRealMoney = m_allowReal;
    const auto finish = [this, generation, index, input] {
        if (generation != m_generation)
            return;
        m_lines[index].plan = buildPlanLine(*input);
        lineDone(generation);
    };
    const auto withSymbols = [this, generation, input, finish](const QVector<SymbolSpec> &symbols) {
        if (generation != m_generation)
            return;
        input->symbols = symbols;
        const QString name = resolveSymbol(input->intent.symbol, symbols);
        if (name.isEmpty()) {
            finish();
            return;
        }
        m_client->quote(input->account, name, [this, generation, input, finish, symbols, name](const Quote &q, const QString &error) {
            if (generation != m_generation)
                return;
            if (!error.isEmpty()) {
                finish();
                return;
            }
            input->quote = q;
            const auto spec = std::find_if(symbols.begin(), symbols.end(), [&name](const SymbolSpec &s) { return s.name == name; });
            if (spec == symbols.end() || spec->quote.compare(input->account.currency, Qt::CaseInsensitive) == 0) {
                input->quoteToAccount = 1;
                finish();
                return;
            }
            bool inverted = false;
            const QString pair = conversionSymbol(spec->quote, input->account.currency, symbols, &inverted);
            if (pair.isEmpty()) {
                finish();
                return;
            }
            m_client->quote(input->account, pair, [generation, input, finish, inverted, this](const Quote &c, const QString &err) {
                if (generation != m_generation)
                    return;
                const double mid = (c.bid + c.ask) / 2;
                if (err.isEmpty() && mid > 0)
                    input->quoteToAccount = inverted ? 1 / mid : mid;
                finish();
            });
        });
    };
    m_client->information(input->account, [this, generation, input, finish, withSymbols](const QJsonObject &info, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty()) {
            input->account.connected = false;   // what it holds now isn't known
            finish();
            return;
        }
        applyInformation(&input->account, info);
        const auto known = m_symbols.constFind(input->account.id);
        if (known != m_symbols.constEnd()) {
            withSymbols(*known);
            return;
        }
        m_client->symbols(input->account, [this, generation, input, finish, withSymbols](const QVector<SymbolSpec> &symbols, const QString &err) {
            if (generation != m_generation)
                return;
            if (!err.isEmpty() || symbols.isEmpty()) {
                finish();
                return;
            }
            m_symbols.insert(input->account.id, symbols);
            withSymbols(symbols);
        });
    });
}

bool TradeDesk::canSend() const
{
    if (m_sent || m_pending > 0 || !m_plannedAt.isValid() || m_plannedAt.elapsed() > PlanLifetimeMs)
        return false;
    return std::any_of(m_lines.begin(), m_lines.end(), [](const SentLine &l) { return l.plan.ok(); });
}

// The service keeps the last ids to refuse the same order twice, from 0 to 999. Kept across launches, so that an order sent
// just after a restart doesn't reuse one.
int TradeDesk::nextRequestId(const QString &accountId)
{
    QSettings settings;
    const QString key = "metatrader/requestId/" + accountId;
    const int id = settings.value(key, -1).toInt() + 1;
    const int next = id > 999 ? 0 : id;
    settings.setValue(key, next);
    return next;
}

bool TradeDesk::send()
{
    if (!canSend())
        return false;
    m_sent = true;
    for (int i = 0; i < m_lines.size(); ++i)
        if (m_lines[i].plan.ok())
            sendLine(i);
    emit changed();
    settle();
    return true;
}

void TradeDesk::sendLine(int index)
{
    SentLine &line = m_lines[index];
    line.state = LineState::Sending;
    line.requestId = nextRequestId(line.plan.account.id);
    OrderRequest order;
    order.symbol = line.plan.symbol;
    order.buy = line.plan.buy;
    order.lots = line.plan.lots;
    order.stop = line.plan.stop;
    order.target = line.plan.target;
    order.requestId = line.requestId;
    const int generation = m_generation;
    m_client->openPosition(line.plan.account, order, [this, index, generation](SendOutcome outcome, const QString &message) {
        if (generation != m_generation)
            return;
        SentLine &l = m_lines[index];
        l.message = message;
        if (outcome == SendOutcome::Rejected) {
            l.state = LineState::Rejected;
        } else {
            // Accepted says only that the service took it; no answer may still mean it was executed. Both get looked for.
            l.state = LineState::Accepted;
            checkLine(index, 0);
        }
        emit changed();
        settle();
    });
}

void TradeDesk::checkLine(int index, int attempt)
{
    if (attempt >= m_checkDelays.size()) {
        m_lines[index].state = LineState::NotConfirmed;
        emit changed();
        settle();
        return;
    }
    const int generation = m_generation;
    QTimer::singleShot(m_checkDelays[attempt], this, [this, index, attempt, generation] {
        if (generation != m_generation)
            return;
        m_client->positions(m_lines[index].plan.account, [this, index, attempt, generation](const QVector<Position> &positions, const QString &) {
            if (generation != m_generation)
                return;
            SentLine &l = m_lines[index];
            const QString marker = QStringLiteral("API|%1|").arg(l.requestId);
            for (const Position &p : positions) {
                if (p.symbol == l.plan.symbol && (p.requestId == QString::number(l.requestId) || p.comment.startsWith(marker))) {
                    l.state = LineState::Confirmed;
                    l.positionId = p.id;
                    l.message.clear();
                    emit changed();
                    settle();
                    return;
                }
            }
            checkLine(index, attempt + 1);
        });
    });
}

void TradeDesk::settle()
{
    for (const SentLine &l : std::as_const(m_lines))
        if (l.plan.ok() && (l.state == LineState::Sending || l.state == LineState::Accepted))
            return;
    emit finished();
}

} // namespace broker
