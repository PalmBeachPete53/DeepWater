// The MetaTrader page's logic against a fake MetaCopier that follows its published API (not the real service).
#include "broker/Broker.h"
#include "broker/MetaCopierClient.h"
#include "broker/TradeDesk.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

using namespace broker;

static int g_checks = 0, g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond);                     \
        }                                                                           \
    } while (false)
#define NEAR(a, b) CHECK(std::abs((a) - (b)) < 1e-6)

static SymbolSpec spec(const QString &name, const QString &base, const QString &quote, double contract, int digits = 5)
{
    SymbolSpec s;
    s.name = name;
    s.base = base;
    s.quote = quote;
    s.contractSize = contract;
    s.digits = digits;
    s.minVolume = 0.01;
    s.maxVolume = 100;
    s.stepVolume = 0.01;
    return s;
}

static Account demo(double balance = 100000)
{
    Account a;
    a.id = "a";
    a.typeId = 1;   // MT5
    a.connected = true;
    a.environment = Environment::Demo;
    a.balance = balance;
    a.currency = "USD";
    return a;
}

static PlanInput euroBuy()
{
    PlanInput in;
    in.account = demo();
    in.intent.symbol = "EURUSD";
    in.intent.buy = true;
    in.intent.riskPercent = 1;
    in.intent.stop = 1.0900;
    in.symbols = {spec("EURUSD", "EUR", "USD", 100000)};
    in.quote = {"EURUSD", 1.0999, 1.1000};
    in.quoteToAccount = 1;
    return in;
}

static void testPlan()
{
    // 1 % of 100,000 is 1,000; 100 points of 0.001 on 100,000 units a lot is 1,000 a lot.
    PlanLine line = buildPlanLine(euroBuy());
    CHECK(line.ok());
    NEAR(line.lots, 1.0);
    NEAR(line.price, 1.1000);
    NEAR(line.riskAmount, 1000);
    NEAR(line.riskPercent, 1);

    // The same risk on twice the balance is twice the lots: what the bridge is for.
    PlanInput in = euroBuy();
    in.account.balance = 200000;
    NEAR(buildPlanLine(in).lots, 2.0);

    // Rounded down, never up: the risk stays under the one asked for.
    in = euroBuy();
    in.account.balance = 123456;
    line = buildPlanLine(in);
    NEAR(line.lots, 1.23);
    CHECK(line.riskPercent <= 1.0);

    // A sale fills at the bid, its stop above.
    in = euroBuy();
    in.intent.buy = false;
    in.intent.stop = 1.1099;
    line = buildPlanLine(in);
    CHECK(line.ok() && !line.buy);
    NEAR(line.price, 1.0999);
    NEAR(line.lots, 1.0);

    // A quote currency other than the account's.
    in = euroBuy();
    in.symbols = {spec("EURJPY", "EUR", "JPY", 100000, 3)};
    in.intent.symbol = "EURJPY";
    in.quote = {"EURJPY", 176.0, 176.5};
    in.intent.stop = 175.5;
    in.quoteToAccount = 1 / 158.0;
    line = buildPlanLine(in);
    CHECK(line.ok());
    CHECK(line.riskPercent <= 1.0 && line.riskPercent > 0.9);

    // What refuses an order.
    const auto refused = [](const std::function<void(PlanInput &)> &change) {
        PlanInput p = euroBuy();
        change(p);
        const PlanLine l = buildPlanLine(p);
        CHECK(l.lots == 0);
        return l.refusal;
    };
    CHECK(refused([](PlanInput &p) { p.account.connected = false; }) == Refusal::NotConnected);
    CHECK(refused([](PlanInput &p) { p.account.wrongCredentials = true; }) == Refusal::WrongCredentials);
    CHECK(refused([](PlanInput &p) { p.account.tradingDisabled = true; }) == Refusal::TradingDisabled);
    // Platforms: the CFD ones are sized, the exchanges and futures brokers aren't checked yet.
    for (int type : {1, 2, 3, 4, 5, 6}) {
        PlanInput p = euroBuy();
        p.account.typeId = type;
        CHECK(buildPlanLine(p).ok());
    }
    CHECK(accountFromJson(QJsonObject {{"type", QJsonObject {{"id", 4}, {"name", "cTrader"}}}}).typeId == 4);
    for (int type : {0, 7, 13, 14, 17})
        CHECK(refused([type](PlanInput &p) { p.account.typeId = type; }) == Refusal::PlatformNotChecked);
    CHECK(refused([](PlanInput &p) { p.account.environment = Environment::Live; }) == Refusal::RealMoneyLocked);
    CHECK(refused([](PlanInput &p) { p.account.environment = Environment::Unknown; }) == Refusal::RealMoneyLocked);
    CHECK(refused([](PlanInput &p) { p.intent.riskPercent = 0; }) == Refusal::RiskInvalid);
    CHECK(refused([](PlanInput &p) { p.intent.riskPercent = 5.01; }) == Refusal::RiskTooHigh);
    CHECK(refused([](PlanInput &p) { p.intent.stop = 0; }) == Refusal::NoStop);
    CHECK(refused([](PlanInput &p) { p.intent.stop = 1.2; }) == Refusal::StopWrongSide);
    CHECK(refused([](PlanInput &p) { p.intent.target = 1.05; }) == Refusal::TargetWrongSide);
    CHECK(refused([](PlanInput &p) { p.account.balance = 0; }) == Refusal::NoBalance);
    CHECK(refused([](PlanInput &p) { p.intent.symbol = "GBPUSD"; }) == Refusal::NoSymbol);
    CHECK(refused([](PlanInput &p) { p.symbols[0].disabled = true; }) == Refusal::SymbolDisabled);
    CHECK(refused([](PlanInput &p) { p.symbols[0].contractSize = 0; }) == Refusal::SymbolIncomplete);
    CHECK(refused([](PlanInput &p) { p.symbols[0].stepVolume = 0; }) == Refusal::SymbolIncomplete);
    CHECK(refused([](PlanInput &p) { p.quote = {}; }) == Refusal::NoQuote);
    CHECK(refused([](PlanInput &p) { p.quoteToAccount = 0; }) == Refusal::NoConversion);
    // At this risk the minimum volume would risk more than asked: refused, not rounded up.
    CHECK(refused([](PlanInput &p) { p.account.balance = 100; }) == Refusal::TooSmall);

    // Real money, once allowed on purpose.
    in = euroBuy();
    in.account.environment = Environment::Live;
    in.allowRealMoney = true;
    CHECK(buildPlanLine(in).ok());

    // The symbol's maximum: fewer lots, less risk, and said so.
    in = euroBuy();
    in.symbols[0].maxVolume = 0.5;
    line = buildPlanLine(in);
    CHECK(line.ok() && line.cappedAtMaximum);
    NEAR(line.lots, 0.5);
    NEAR(line.riskPercent, 0.5);

    // Names.
    const QVector<SymbolSpec> names = {spec("XAUUSD.f", "XAU", "USD", 100), spec("EURUSD", "EUR", "USD", 100000), spec("BTCUSDT", "BTC", "USDT", 1),
                                       spec("GBPUSD.a", "GBP", "USD", 100000), spec("GBPUSD.b", "GBP", "USD", 100000), spec("USDJPYm", "USD", "JPY", 100000)};
    CHECK(resolveSymbol("XAUUSD", names) == "XAUUSD.f");
    CHECK(resolveSymbol("xauusd.f", names) == "XAUUSD.f");
    CHECK(resolveSymbol("EURUSD", names) == "EURUSD");
    CHECK(resolveSymbol("USDJPY", names) == "USDJPYm");
    CHECK(resolveSymbol("BTCUSD", names).isEmpty());   // BTCUSDT is another instrument
    QStringList candidates;
    CHECK(resolveSymbol("GBPUSD", names, &candidates).isEmpty());
    CHECK(candidates.size() == 2);
    CHECK(resolveSymbol("", names).isEmpty());
    bool inverted = false;
    CHECK(conversionSymbol("JPY", "USD", names, &inverted) == "USDJPYm" && inverted);
    CHECK(conversionSymbol("CHF", "USD", names, &inverted).isEmpty());
    NEAR(lotsDownToStep(0.3, 0.01), 0.3);
    NEAR(lotsDownToStep(0.2999, 0.01), 0.29);
    NEAR(roundToDigits(1.234567, 5), 1.23457);
    CHECK(MetaCopierClient::tradingHost("New York") == "api-newyork");
    CHECK(MetaCopierClient::tradingHost("Mars") == "api");
}

// ---------------------------------------------------------------- the fake service

class FakeService : public QTcpServer
{
public:
    QString key = "test-key";
    QStringList log;                 // "POST a /positions {...}"
    QHash<QString, QJsonArray> open; // per account
    QString rejectAccount;           // answers 400
    QString silentAccount;           // closes without answering, and opens nothing
    QString lostAnswerAccount;       // opens the position, then closes without answering
    QString neverAppearsAccount;     // says yes, and nothing appears

    FakeService()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
                    buffer->append(socket->readAll());
                    const int end = buffer->indexOf("\r\n\r\n");
                    if (end < 0) return;
                    const QByteArray head = buffer->left(end).toLower();
                    int length = 0;
                    if (const int at = head.indexOf("content-length:"); at >= 0)
                        length = head.mid(at + 15, head.indexOf('\r', at) - at - 15).trimmed().toInt();
                    if (buffer->size() < end + 4 + length) return;
                    handle(socket, buffer->left(end), buffer->mid(end + 4, length));
                    buffer->clear();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    static QJsonObject information(double balance, const QString &env, bool connected = true)
    {
        QJsonObject o {{"connected", connected}, {"balance", balance}, {"equity", balance}, {"currency", "USD"}, {"leverage", 100}, {"openPositionsCount", 0}};
        if (!env.isEmpty()) o["environment"] = env;
        return o;
    }
    static QJsonObject account(const QString &id, const QString &alias, const QString &server, double balance, const QString &env, bool connected = true)
    {
        return {{"id", id}, {"alias", alias}, {"loginServer", server}, {"loginAccountNumber", "1000" + id}, {"propFirm", true},
                {"region", QJsonObject {{"id", 2}, {"name", "London"}}}, {"type", QJsonObject {{"id", 1}, {"name", "MT5"}}},
                {"status", QJsonObject {{"id", 1}, {"name", "Connected"}}}, {"accountInformation", information(balance, env, connected)}};
    }
    QHash<QString, QJsonObject> accounts {
        {"a", account("a", "Challenge 100k", "FTMO-Demo", 100000, "DEMO")},
        {"b", account("b", "Challenge 200k", "FTMO-Demo2", 200000, "DEMO")},
        {"l", account("l", "Funded", "FTMO-Server", 100000, "LIVE")},
        {"d", account("d", "Off", "FTMO-Demo", 50000, "DEMO", false)},
    };

    void handle(QTcpSocket *socket, const QByteArray &head, const QByteArray &body)
    {
        const QList<QByteArray> first = head.split('\r').first().split(' ');
        const QString verb = QString::fromLatin1(first.value(0));
        const QString path = QString::fromLatin1(first.value(1)).section('?', 0, 0);
        const auto answer = [socket](int status, const QByteArray &json) {
            socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                          + QByteArray::number(json.size()) + "\r\n\r\n" + json);
            socket->disconnectFromHost();
        };
        const auto json = [](const auto &value) { return QJsonDocument(value).toJson(QJsonDocument::Compact); };
        if (!QString::fromLatin1(head).contains("X-API-KEY: " + key, Qt::CaseInsensitive)) {
            answer(401, R"({"message":"bad key"})");
            return;
        }
        const QString prefix = "/rest/api/v1/accounts";
        if (path == prefix) {
            QJsonArray all;
            for (const QString &id : {"a", "b", "l", "d"}) all << accounts.value(id);
            answer(200, json(all));
            return;
        }
        const QStringList parts = path.mid(prefix.size() + 1).split('/');
        const QString id = parts.value(0), what = parts.value(1);
        log << verb + " " + id + " /" + what + (body.isEmpty() ? QString() : " " + QString::fromUtf8(body));
        if (what == "information") {
            answer(200, json(accounts.value(id).value("accountInformation").toObject()));
        } else if (what == "symbols") {
            const auto sym = [](const QString &n, const QString &b, const QString &q, double lot, int digits) {
                return QJsonObject {{"name", n}, {"baseCurrency", b}, {"quoteCurrency", q}, {"lotSize", lot}, {"digits", digits},
                                    {"minimalVolume", 0.01}, {"maximalVolume", 100}, {"stepVolume", 0.01}, {"disabled", false}};
            };
            answer(200, json(QJsonArray {sym("EURUSD", "EUR", "USD", 100000, 5), sym("EURJPY", "EUR", "JPY", 100000, 3), sym("USDJPY", "USD", "JPY", 100000, 3), sym("XAUUSD" + QString(id == "b" ? ".f" : ""), "XAU", "USD", 100, 2)}));
        } else if (what == "quote") {
            const QString s = parts.value(2);
            const double bid = s.startsWith("EURUSD") ? 1.0999 : s.startsWith("EURJPY") ? 176.0 : s.startsWith("USDJPY") ? 158.0 : 4000.0;
            answer(200, json(QJsonObject {{"symbol", s}, {"bid", bid}, {"ask", bid + (s.startsWith("EURUSD") ? 0.0001 : 0.5)}}));
        } else if (what == "positions" && verb == "GET") {
            answer(200, json(open.value(id)));
        } else if (what == "positions" && verb == "POST") {
            if (id == rejectAccount) { answer(400, R"({"message":"Market closed"})"); return; }
            if (id == silentAccount) { socket->abort(); return; }
            const QJsonObject o = QJsonDocument::fromJson(body).object();
            if (id != neverAppearsAccount) {
                QJsonArray list = open.value(id);
                const QString rid = QString::number(o.value("requestId").toInt());
                list << QJsonObject {{"id", id + "-" + rid}, {"symbol", o.value("symbol")}, {"orderType", o.value("orderType")}, {"volume", o.value("volume")},
                                     {"stopLoss", o.value("stopLoss")}, {"takeProfit", o.value("takeProfit")}, {"openPrice", 1.1}, {"profit", 12.5},
                                     {"requestId", rid}, {"comment", "API|" + rid + "|Deepwater"}};
                open.insert(id, list);
            }
            if (id == lostAnswerAccount) { socket->abort(); return; }
            answer(204, {});
        } else if (what == "positions" && verb == "DELETE") {
            QJsonArray list;
            for (const QJsonValue &p : open.value(id))
                if (p.toObject().value("id").toString() != parts.value(2)) list << p;
            open.insert(id, list);
            answer(204, {});
        } else if (what == "actions") {
            open.remove(id);
            answer(204, {});
        } else {
            answer(404, R"({"message":"no such thing"})");
        }
    }

    int orders(const QString &id) const
    {
        int n = 0;
        for (const QString &line : log)
            if (line.startsWith("POST " + id + " /positions")) ++n;
        return n;
    }
};

template <typename F> static void waitFor(TradeDesk *sender, F signal, int ms = 8000)
{
    QEventLoop loop;
    QObject::connect(sender, signal, &loop, &QEventLoop::quit);
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static QVector<Account> fetchAccounts(MetaCopierClient &client, QString *error = nullptr)
{
    QVector<Account> out;
    QEventLoop loop;
    client.accounts([&](const QVector<Account> &a, const QString &e) {
        out = a;
        if (error) *error = e;
        loop.quit();
    });
    loop.exec();
    return out;
}

static Account pick(const QVector<Account> &all, const QString &id)
{
    for (const Account &a : all)
        if (a.id == id) return a;
    return {};
}

static void testService()
{
    FakeService service;
    CHECK(service.listen(QHostAddress::LocalHost));
    qputenv("DEEPWATER_METACOPIER_URL", QByteArray("http://127.0.0.1:") + QByteArray::number(service.serverPort()));
    CHECK(!MetaCopierClient::testServer().isEmpty());

    MetaCopierClient client;
    QString error;
    client.setApiKey("wrong");
    CHECK(fetchAccounts(client, &error).isEmpty());
    CHECK(error == "bad key");
    client.setApiKey("test-key");
    const QVector<Account> all = fetchAccounts(client, &error);
    CHECK(error.isEmpty() && all.size() == 4);
    CHECK(pick(all, "b").balance == 200000 && pick(all, "b").environment == Environment::Demo && pick(all, "b").region == "London");
    CHECK(pick(all, "l").environment == Environment::Live);

    TradeDesk desk(&client);
    desk.setCheckDelays({20, 40});
    Intent intent;
    intent.symbol = "EURUSD";
    intent.buy = true;
    intent.riskPercent = 1;
    intent.stop = 1.09;

    // Priced per account; the real one is locked and the unconnected one refused, and neither is ever sent.
    desk.prepare(intent, all);
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(desk.lines().size() == 4);
    const auto line = [&desk](const QString &id) { for (const SentLine &l : desk.lines()) if (l.plan.account.id == id) return l; return SentLine(); };
    NEAR(line("a").plan.lots, 1.0);
    NEAR(line("b").plan.lots, 2.0);
    CHECK(line("l").plan.refusal == Refusal::RealMoneyLocked);
    CHECK(line("d").plan.refusal == Refusal::NotConnected);
    CHECK(desk.canSend());
    CHECK(service.orders("a") == 0);   // pricing sends nothing

    CHECK(desk.send());
    waitFor(&desk, &TradeDesk::finished);
    CHECK(line("a").state == LineState::Confirmed && line("a").positionId == "a-" + QString::number(line("a").requestId));
    CHECK(line("b").state == LineState::Confirmed);
    CHECK(service.orders("a") == 1 && service.orders("b") == 1);
    CHECK(service.orders("l") == 0 && service.orders("d") == 0);
    CHECK(service.log.filter("POST a /positions").first().contains("\"volume\":1"));
    CHECK(service.log.filter("POST b /positions").first().contains("\"volume\":2"));
    CHECK(service.log.filter("POST a /positions").first().contains("\"stopLoss\":1.09"));

    // A plan goes once.
    CHECK(!desk.canSend());
    CHECK(!desk.send());
    CHECK(service.orders("a") == 1);

    // A symbol under another name on one account, and the lots of the gold's contract.
    Intent gold = intent;
    gold.symbol = "XAUUSD";
    gold.stop = 3950;
    desk.prepare(gold, {pick(all, "a"), pick(all, "b")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(line("b").plan.symbol == "XAUUSD.f" && line("a").plan.symbol == "XAUUSD");
    NEAR(line("a").plan.lots, 1000.0 / (100 * 50.5) - std::fmod(1000.0 / (100 * 50.5), 0.01));

    // A conversion through the account's own pair.
    Intent yen = intent;
    yen.symbol = "EURJPY";
    yen.stop = 175.0;
    desk.prepare(yen, {pick(all, "a")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(line("a").plan.ok() && line("a").plan.riskPercent > 0.9 && line("a").plan.riskPercent <= 1.0);

    // Real money allowed on purpose.
    desk.setAllowRealMoney(true);
    desk.prepare(intent, {pick(all, "l")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(line("l").plan.ok());
    desk.setAllowRealMoney(false);

    // One account's refusal doesn't stop the other.
    service.rejectAccount = "a";
    desk.prepare(intent, {pick(all, "a"), pick(all, "b")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(desk.send());
    waitFor(&desk, &TradeDesk::finished);
    CHECK(line("a").state == LineState::Rejected && line("a").message == "Market closed");
    CHECK(line("b").state == LineState::Confirmed);
    service.rejectAccount.clear();

    // An answer lost after the order went: found on the account, and not sent again.
    service.lostAnswerAccount = "a";
    const int before = service.orders("a");
    desk.prepare(intent, {pick(all, "a")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(desk.send());
    waitFor(&desk, &TradeDesk::finished);
    CHECK(line("a").state == LineState::Confirmed);
    CHECK(service.orders("a") == before + 1);
    service.lostAnswerAccount.clear();

    // No answer and nothing on the account: said so, not retried.
    service.silentAccount = "b";
    const int beforeB = service.orders("b");
    desk.prepare(intent, {pick(all, "b")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(desk.send());
    waitFor(&desk, &TradeDesk::finished);
    CHECK(line("b").state == LineState::NotConfirmed);
    CHECK(service.orders("b") == beforeB + 1);
    service.silentAccount.clear();

    // Accepted, then nothing appears.
    service.neverAppearsAccount = "a";
    desk.prepare(intent, {pick(all, "a")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(desk.send());
    waitFor(&desk, &TradeDesk::finished);
    CHECK(line("a").state == LineState::NotConfirmed);
    service.neverAppearsAccount.clear();

    // A service that can't say what an account holds now: refused.
    Account gone = pick(all, "a");
    gone.id = "../nothing";
    desk.prepare(intent, {gone});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(!desk.lines().first().plan.ok());
    CHECK(!desk.canSend());

    // Reading the positions, closing one, closing all.
    QVector<Position> held;
    QEventLoop loop;
    QTimer::singleShot(8000, &loop, &QEventLoop::quit);
    client.positions(pick(all, "a"), [&](const QVector<Position> &p, const QString &) { held = p; loop.quit(); });
    loop.exec();
    CHECK(!held.isEmpty());
    CHECK(held.first().symbol == "EURUSD" && held.first().volume == 1 && held.first().profit == 12.5 && held.first().type == "Buy");
    SendOutcome outcome = SendOutcome::Unknown;
    client.closePosition(pick(all, "a"), held.first().id, [&](SendOutcome o, const QString &) { outcome = o; loop.quit(); });
    loop.exec();
    CHECK(outcome == SendOutcome::Accepted);
    CHECK(service.open.value("a").size() == held.size() - 1);
    client.closeAll(pick(all, "b"), [&](SendOutcome o, const QString &) { outcome = o; loop.quit(); });
    loop.exec();
    CHECK(outcome == SendOutcome::Accepted && service.open.value("b").isEmpty());

    // The ids go round, and not back to one just used.
    QSettings().setValue("metatrader/requestId/a", 999);
    service.log.clear();
    desk.prepare(intent, {pick(all, "a")});
    waitFor(&desk, &TradeDesk::prepared);
    CHECK(desk.send());
    waitFor(&desk, &TradeDesk::finished);
    CHECK(service.log.filter("POST a /positions").first().contains("\"requestId\":0"));

    qunsetenv("DEEPWATER_METACOPIER_URL");
    qputenv("DEEPWATER_METACOPIER_URL", "https://evil.example.com");
    CHECK(MetaCopierClient::testServer().isEmpty());   // only the loopback address
    qunsetenv("DEEPWATER_METACOPIER_URL");
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QCoreApplication app(argc, argv);
    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("DeepwaterTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    const QStringList args = app.arguments();
    if (const int serve = args.indexOf("--serve"); serve > 0 && serve + 1 < args.size()) {
        auto *service = new FakeService;
        service->key = "test-key";
        if (!service->listen(QHostAddress::LocalHost, quint16(args.at(serve + 1).toInt())))
            return 2;
        std::printf("fake MetaCopier on port %d, key test-key\n", service->serverPort());
        std::fflush(stdout);
        return app.exec();
    }
    testPlan();
    testService();
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
