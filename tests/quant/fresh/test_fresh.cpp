// How often sources are asked and what a published figure makes stale (src/fresh), then a real RemoteDataSet against a
// fake server. No network beyond the loopback address.
#include "MacroData.h"
#include "ResearchData.h"
#include "HttpCache.h"
#include "fresh/Fresh.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimeZone>
#include <QTimer>

#include <cstdio>
#include <functional>

using namespace fresh;

static int g_checks = 0, g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond);                     \
        }                                                                           \
    } while (0)

static QDateTime utc(int y, int m, int d, int h, int min = 0, int s = 0)
{
    return QDateTime(QDate(y, m, d), QTime(h, min, s), QTimeZone::UTC);
}

static void testPeriods()
{
    CHECK(checkEverySecs(12 * 3600, false) == 12 * 3600);
    CHECK(checkEverySecs(12 * 3600, true) == LightCheckSecs);
    CHECK(checkEverySecs(60, true) == 60);   // never slower than its own period
    CHECK(checkEverySecs(12 * 3600, false, 1000) == SmallCheckSecs);
    CHECK(checkEverySecs(12 * 3600, false, SmallAnswerBytes + 1) == 12 * 3600);
    CHECK(checkEverySecs(12 * 3600, true, SmallAnswerBytes + 1) == LightCheckSecs);
    CHECK(checkEverySecs(600, false, 1000) == 600);

    const Validators none, a {"\"x\"", {}}, b {"\"y\"", {}};
    CHECK(judgeConditional(none, 200, a) == Conditional::Unknown);
    CHECK(judgeConditional(a, 304, {}) == Conditional::Works);
    CHECK(judgeConditional(a, 200, a) == Conditional::Ignored);
    CHECK(judgeConditional(a, 200, b) == Conditional::Unknown);   // new data proves nothing

    // CME's week, on the week of 2026-11-02 (Monday) to 2026-11-08 (Sunday).
    CHECK(futuresMarketOpen(utc(2026, 11, 2, 12)));
    CHECK(futuresMarketOpen(utc(2026, 11, 6, 21, 59)));
    CHECK(!futuresMarketOpen(utc(2026, 11, 6, 22, 0)));
    CHECK(!futuresMarketOpen(utc(2026, 11, 7, 12)));
    CHECK(!futuresMarketOpen(utc(2026, 11, 8, 21, 59)));
    CHECK(futuresMarketOpen(utc(2026, 11, 8, 22, 0)));
    CHECK(futuresEverySecs(utc(2026, 11, 4, 12)) == 60);
    CHECK(futuresEverySecs(utc(2026, 11, 7, 12)) == 900);
    // Another zone reads the same instant: Friday 23:30 in Paris is 22:30 UTC.
    CHECK(!futuresMarketOpen(QDateTime(QDate(2026, 11, 6), QTime(23, 30), QTimeZone(3600))));
    CHECK(futuresMarketOpen(QDateTime(QDate(2026, 11, 6), QTime(22, 30), QTimeZone(3600))));
}

static void testSchedule()
{
    const QDateTime now = utc(2026, 11, 4, 12);
    QVector<Job> jobs {
        {"fresh", 3600, now.addSecs(-60), {}},
        {"late", 3600, now.addSecs(-4000), {}},
        {"later", 3600, now.addSecs(-9000), {}},
        {"never", 3600, {}, {}},
    };
    // One at a time, the one never fetched first, then the longest overdue.
    CHECK(takeDue(jobs, now, 1) == QStringList {"never"});
    CHECK(takeDue(jobs, now, 1) == QStringList {"later"});
    CHECK(takeDue(jobs, now, 1) == QStringList {"late"});
    CHECK(takeDue(jobs, now, 1).isEmpty());   // "fresh" isn't due, the others were just asked

    // Asked and without an answer: left alone for the gap, then asked again. One that never answered waits its whole
    // period: a data set that fails tries again on its own timer.
    CHECK(takeDue(jobs, now.addSecs(29), 5).isEmpty());
    CHECK(takeDue(jobs, now.addSecs(30), 5) == (QStringList {"later", "late"}));
    CHECK(takeDue(jobs, now.addSecs(3600), 5).contains("never"));

    // A source with no answers of its own runs on its attempts.
    QVector<Job> own {{"fx", 300, {}, now}};
    CHECK(takeDue(own, now.addSecs(299), 5).isEmpty());
    CHECK(takeDue(own, now.addSecs(300), 5) == QStringList {"fx"});
    CHECK(takeDue(own, now.addSecs(301), 5).isEmpty());

    // A period under the gap isn't held back by it.
    QVector<Job> quick {{"q", 10, now, now}};
    CHECK(takeDue(quick, now.addSecs(10), 1) == QStringList {"q"});
}

static void testWatch()
{
    const QDateTime due = utc(2026, 11, 4, 13, 30);
    const QVector<Release> cpi {{due, "US", "CPI", 2, false}};
    CHECK(planWatch(cpi, due.addSecs(-61)).pollSecs == 0);
    CHECK(planWatch(cpi, due.addSecs(-60)).pollSecs == 10);
    CHECK(planWatch(cpi, due.addSecs(5)).countries == QStringList {"US"});
    CHECK(planWatch(cpi, due.addSecs(WatchMinutes * 60)).pollSecs == 10);
    CHECK(planWatch(cpi, due.addSecs(WatchMinutes * 60 + 1)).pollSecs == 0);   // it won't come: back to the usual period

    // Once it's there, nothing to watch.
    CHECK(planWatch({{due, "US", "CPI", 2, true}}, due).pollSecs == 0);

    // The fastest of what's due, and each country once.
    const QVector<Release> several {{due, "CA", "Jobs", 1, false}, {due, "US", "CPI", 2, false}, {due, "US", "Core", 1, false}, {due, "JP", "Later", 2, false}};
    const WatchPlan both = planWatch(several, due);
    CHECK(both.pollSecs == 10);
    CHECK(both.countries == (QStringList {"CA", "JP", "US"}));
    CHECK(planWatch({{due, "CA", "Jobs", 0, false}}, due).pollSecs == 30);

    // A figure that came out.
    const QVector<Release> before {{due, "US", "CPI", 2, false}, {due, "CA", "Jobs", 1, true}};
    const QVector<Release> after {{due, "US", "CPI", 2, true}, {due, "CA", "Jobs", 1, true}, {due, "JP", "New", 1, false}};
    const QVector<Release> out = newActuals(before, after);
    CHECK(out.size() == 1 && out.value(0).title == "CPI");
    CHECK(newActuals(after, after).isEmpty());
}

static void testFollowUps()
{
    const QDateTime seen = utc(2026, 11, 4, 13, 30, 12);
    FollowUps f;
    f.add({seen, "US", "Low", 0, true}, seen);
    CHECK(f.size() == 0);   // low impact isn't worth the requests
    f.add({seen, "US", "CPI", 2, true}, seen);
    CHECK(f.size() == followUpDelays().size());
    CHECK(f.takeDue(seen).size() == 1);
    CHECK(f.takeDue(seen).isEmpty());
    CHECK(f.takeDue(seen.addSecs(119)).isEmpty());
    const QVector<FollowUp> two = f.takeDue(seen.addSecs(120));
    CHECK(two.size() == 1 && two.value(0).country == "US" && two.value(0).release == seen);
    // A sleep past several delays asks once.
    CHECK(f.takeDue(seen.addSecs(24 * 3600)).size() == 1);
    CHECK(f.size() == 0);

    CHECK(targetsFor("US", "CPI").groups.contains("fred"));
    CHECK(!targetsFor("US", "CPI").policyRates);
    CHECK(targetsFor("US", "Fed Interest Rate Decision").policyRates);
    CHECK(targetsFor("US", "Fed Interest Rate Decision").groups.contains("bis"));
    CHECK(targetsFor("EU", "ECB Interest Rate Decision").groups.contains("ecb"));
    CHECK(targetsFor("US", "EIA Crude Oil Stocks Change").groups.contains("eia"));
    CHECK(!targetsFor("CA", "Crude thing").groups.contains("eia"));
    CHECK(targetsFor("ZZ", "Anything").groups.isEmpty());
    for (const char *c : {"US", "EU", "DE", "GB", "JP", "CA", "AU", "FR"})
        CHECK(!targetsFor(c, "CPI").groups.contains("oecd"));   // the OECD republishes weeks later
}

// ---------------------------------------------------------------- a real data set

namespace {

struct Fake {
    QByteArray body = "one";
    QByteArray etag;            // empty: no validators
    bool honours = true;        // false: sends everything whatever it's asked
    int requests = 0;
    int sent304 = 0;
    QByteArray lastIfNoneMatch;
};

class FakeServer : public QTcpServer
{
public:
    explicit FakeServer(Fake *state) : m_state(state)
    {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *s = nextPendingConnection();
            connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            connect(s, &QTcpSocket::readyRead, s, [this, s] {
                const QByteArray request = s->readAll();
                ++m_state->requests;
                m_state->lastIfNoneMatch.clear();
                for (const QByteArray &line : request.split('\n'))
                    if (line.toLower().startsWith("if-none-match:"))
                        m_state->lastIfNoneMatch = line.mid(14).trimmed();
                const bool match = m_state->honours && !m_state->etag.isEmpty() && m_state->lastIfNoneMatch == m_state->etag;
                QByteArray head = match ? "HTTP/1.1 304 Not Modified\r\n" : "HTTP/1.1 200 OK\r\n";
                if (!m_state->etag.isEmpty())
                    head += "ETag: " + m_state->etag + "\r\n";
                const QByteArray body = match ? QByteArray() : m_state->body;
                if (match)
                    ++m_state->sent304;
                head += "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n";
                s->write(head + body);
                s->disconnectFromHost();
            });
        });
    }

private:
    Fake *m_state;
};

class FakeSet : public RemoteDataSet
{
public:
    FakeSet(const QString &name, quint16 port, qint64 stale, QObject *parent = nullptr)
        : RemoteDataSet(name, stale, parent), m_port(port)
    {
        loadCached();
    }
    QByteArray seen;
    int parses = 0;

protected:
    QUrl url() const override { return QUrl(QString("http://127.0.0.1:%1/data").arg(m_port)); }
    bool parse(const QByteArray &body) override
    {
        ++parses;
        seen = body;
        return true;
    }

private:
    quint16 m_port;
};

// Runs until the set has answered.
void settle(RemoteDataSet *set, const std::function<void()> &start)
{
    start();
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (!set->isFetching())
            loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
}

void removeCache(const QString &name)
{
    HttpCache::waitForWrites();
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    for (const QString &file : dir.entryList({"cache_" + name + "*"}, QDir::Files))
        dir.remove(file);
}

} // namespace

static void testConditional()
{
    const QString name = QString("test_fresh_conditional_%1").arg(QCoreApplication::applicationPid());
    Fake state;
    state.etag = "\"v1\"";
    FakeServer server(&state);
    {
        FakeSet set(name, server.serverPort(), 12 * 3600);
        set.waitCached();
        int updates = 0;
        QObject::connect(&set, &RemoteDataSet::updated, [&] { ++updates; });
        CHECK(set.checkEverySecs() == 12 * 3600);   // nothing known yet
        state.body = QByteArray(int(SmallAnswerBytes) + 1, 'x');   // large: its own period until a check proves free

        settle(&set, [&] { set.refreshNow(); });
        CHECK(set.seen == state.body);
        CHECK(set.parses == 1);
        CHECK(updates == 1);
        CHECK(state.lastIfNoneMatch.isEmpty());
        CHECK(set.checkEverySecs() == 12 * 3600);
        const QDateTime changed = set.changedAt();

        // The same again, asked by hand: a 304, nothing parsed, and the answer told.
        settle(&set, [&] { set.refreshNow(); });
        CHECK(state.lastIfNoneMatch == "\"v1\"");
        CHECK(state.sent304 == 1);
        CHECK(set.parses == 1);
        CHECK(updates == 2);
        CHECK(set.errorString().isEmpty());
        CHECK(set.changedAt() == changed);
        CHECK(set.checkEverySecs() == LightCheckSecs);   // now known to be free

        // New data goes through.
        state.body = "two";
        state.etag = "\"v2\"";
        settle(&set, [&] { set.refreshNow(); });
        CHECK(set.seen == "two");
        CHECK(set.parses == 2);
        CHECK(updates == 3);
    }
    // A later launch reads the copy and keeps the validators: its first check is free.
    HttpCache::waitForWrites();
    {
        FakeSet set(name, server.serverPort(), 12 * 3600);
        set.waitCached();
        CHECK(set.seen == "two");
        CHECK(set.parses == 1);
        CHECK(set.checkEverySecs() == LightCheckSecs);
        const int before = state.sent304;
        settle(&set, [&] { set.refreshNow(); });
        CHECK(state.sent304 == before + 1);
        CHECK(set.parses == 1);
    }
    removeCache(name);
}

static void testSameBody()
{
    const QString name = QString("test_fresh_samebody_%1").arg(QCoreApplication::applicationPid());
    Fake state;   // no validators at all
    FakeServer server(&state);
    FakeSet set(name, server.serverPort(), 0);
    set.waitCached();
    int updates = 0;
    QObject::connect(&set, &RemoteDataSet::updated, [&] { ++updates; });
    settle(&set, [&] { set.refreshIfStale(); });
    CHECK(set.parses == 1);
    CHECK(updates == 1);
    const QDateTime first = set.fetchedAt();

    // Asked on its period and nothing new: no parsing, and no page told to redraw.
    QEventLoop pause;
    QTimer::singleShot(1100, &pause, &QEventLoop::quit);
    pause.exec();
    settle(&set, [&] { set.refreshIfStale(); });
    CHECK(state.requests == 2);
    CHECK(set.parses == 1);
    CHECK(updates == 1);
    CHECK(set.fetchedAt() > first);
    CHECK(set.changedAt() < set.fetchedAt());
    CHECK(set.checkEverySecs() == 0);   // no 304 seen: its own period stays

    state.body = "other";
    settle(&set, [&] { set.refreshIfStale(); });
    CHECK(set.parses == 2);
    CHECK(updates == 2);
    removeCache(name);
}

static void testIgnored()
{
    const QString name = QString("test_fresh_ignored_%1").arg(QCoreApplication::applicationPid());
    Fake state;
    state.etag = "\"same\"";
    state.honours = false;   // as BIS and OECD did, November 2026: validators, and the whole body every time
    FakeServer server(&state);
    FakeSet set(name, server.serverPort(), 12 * 3600);
    set.waitCached();
    settle(&set, [&] { set.refreshNow(); });
    settle(&set, [&] { set.refreshNow(); });
    CHECK(state.lastIfNoneMatch == "\"same\"");
    CHECK(state.sent304 == 0);
    CHECK(set.parses == 1);                       // the hash spared the parsing
    CHECK(set.checkEverySecs() == SmallCheckSecs);   // never the free period: the check isn't free, only small
    state.body = QByteArray(int(SmallAnswerBytes) + 1, 'x');
    settle(&set, [&] { set.refreshNow(); });
    CHECK(set.checkEverySecs() == 12 * 3600);
    removeCache(name);
}

static void testFailure()
{
    const QString name = QString("test_fresh_failure_%1").arg(QCoreApplication::applicationPid());
    Fake state;
    FakeServer server(&state);
    FakeSet set(name, server.serverPort(), 0);
    set.waitCached();
    settle(&set, [&] { set.refreshIfStale(); });
    CHECK(set.parses == 1);
    const int requests = state.requests;
    server.close();
    FakeSet dead(name + "_dead", 1, 0);   // nothing listens on port 1
    dead.waitCached();
    settle(&dead, [&] { dead.refreshIfStale(); });
    CHECK(!dead.errorString().isEmpty());
    // Failed a moment ago: the scheduler's next pass doesn't hammer it, its own retry will.
    dead.refreshIfStale();
    CHECK(!dead.isFetching());
    CHECK(state.requests == requests);
    removeCache(name);
    removeCache(name + "_dead");
}

// research::Client, which most pages read through.
static void testClient()
{
    const int pid = int(QCoreApplication::applicationPid());
    const QString key = QString("testfresh%1:series").arg(pid);
    auto *client = research::Client::instance();
    Fake state;
    state.etag = "\"a\"";
    FakeServer server(&state);
    const QUrl url(QString("http://127.0.0.1:%1/series").arg(server.serverPort()));
    auto read = [&](qint64 maxAge) {
        QByteArray out;
        QString error;
        QEventLoop loop;
        QObject ctx;
        client->raw(key, url, maxAge, false, &ctx, [&](const QByteArray &body, const QString &err) {
            out = body;
            error = err;
            loop.quit();
        });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(error.isEmpty());
        return out;
    };
    CHECK(read(3600) == "one");
    CHECK(state.requests == 1);
    CHECK(read(3600) == "one");
    CHECK(state.requests == 1);   // younger than an hour: not asked

    // Expired: asked "unless it's still this", and it is.
    CHECK(read(0) == "one");
    CHECK(state.requests == 2);
    CHECK(state.lastIfNoneMatch == "\"a\"");
    CHECK(state.sent304 == 1);

    // New data goes through, with its new validators remembered.
    state.body = "two";
    state.etag = "\"b\"";
    CHECK(read(0) == "two");
    CHECK(read(0) == "two");
    CHECK(state.lastIfNoneMatch == "\"b\"");

    // A figure has come out: a copy read before it isn't good any more, whatever its age limit.
    const int before = state.requests;
    client->forgetOlderThan(QString("testfresh%1").arg(pid), QDateTime::currentDateTimeUtc().addSecs(1));
    CHECK(read(24 * 3600) == "two");
    CHECK(state.requests == before + 1);
    removeCache("testfresh");
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    testPeriods();
    testSchedule();
    testWatch();
    testFollowUps();
    testConditional();
    testSameBody();
    testIgnored();
    testFailure();
    testClient();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
