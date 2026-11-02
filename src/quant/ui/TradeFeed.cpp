// Binance trades for the footprint chart (see TradeFeed.h). A Deepwater addition.
#include "TradeFeed.h"

#include "Api.h"
#include "../server/Server.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <algorithm>
#include <atomic>
#include <memory>

namespace qtm {

namespace {
// The same stream hosts as the chart's kline socket (ChartPane::openSocket).
const QStringList WS_HOSTS = {"wss://stream.binance.com:9443/ws/", "wss://data-stream.binance.vision/ws/"};
const QStringList WS_HOSTS_US = {"wss://stream.binance.us:9443/ws/"};

// A page of trades and the exchange it came from (Providers::binanceAggTrades).
QJsonObject aggTrades(const QString &ticker, qint64 fromId, int limit)
{
    bool us = false;
    const QJsonArray t = Backend::instance()->binanceAggTrades(ticker, fromId, limit, &us);
    return QJsonObject{{"t", t}, {"us", us}};
}
constexpr int PAGE = 1000;          // Binance's most trades per request
constexpr int PARALLEL_PAGES = 4;   // history pages asked for at once
constexpr int PAGE_GAP_MS = 100;    // between batches: ~10 requests a second, well under Binance's weight limit
constexpr int RETRY_MS = 5000;
std::atomic<quint64> g_tapeGen{0};   // generations unique across feeds, so a reader can't mistake a new feed for its old one
} // namespace

TradeFeed::TradeFeed(const QString &ticker, double tfSeconds, double chartStart, int budget, QObject *parent)
    : QObject(parent), m_ticker(ticker.toUpper()), m_tf(tfSeconds), m_chartStart(chartStart),
      m_budget(size_t(std::max(PAGE, budget)))
{
    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_tapeGen = ++g_tapeGen;
    fetchTickSize();
}

TradeFeed::~TradeFeed()
{
    stop();
}

TradeFeed::Raw TradeFeed::parse(const QJsonObject &o)
{
    auto num = [&](const char *key) {
        const QJsonValue v = o.value(key);
        return v.isString() ? v.toString().toDouble() : v.toDouble();
    };
    Raw r;
    r.id = o.value("a").toInteger(-1);
    r.t.time = o.value("T").toDouble() / 1000;
    r.t.seq = r.id;
    r.t.price = num("p");
    r.t.qty = num("q");
    r.t.side = footprint::binanceSide(o.value("m").toBool());
    return r;
}

void TradeFeed::setError(const QString &e)
{
    if (m_error == e) return;
    m_error = e;
    emit stateChanged();
}

void TradeFeed::stop()
{
    m_stopped = true;
    ++m_token;
    m_forwardBusy = false;
    m_retry->stop();
    m_retry->disconnect();
    if (m_ws) {
        QWebSocket *s = m_ws;
        m_ws = nullptr;
        s->close();
    }
}

void TradeFeed::resume()
{
    if (!m_stopped) return;
    m_stopped = false;
    if (!m_tickKnown) {
        fetchTickSize();
        return;
    }
    if (m_exchangeKnown) openSocket(0);
    if (m_hiId < 0) fetchLatest();
    else if (m_backfilling) fetchBack();
    // A gap left while stopped shows with the first live trade (its id skips ahead): drain() fetches it.
}

void TradeFeed::setChartStart(double t)
{
    if (!(t > m_chartStart)) return;
    m_chartStart = t;
    m_book.dropBefore(t);
    while (!m_tape.empty() && m_tape.front().t.time < t) m_tape.pop_front();
    if (!std::isnan(m_completeFrom)) m_completeFrom = std::max(m_completeFrom, t);
}

// ---------------------------------------------------------------- requests

void TradeFeed::fetchTickSize()
{
    const int token = m_token;
    const QString ticker = m_ticker;
    api::run(this, [ticker] { return QJsonObject{{"tick", Backend::instance()->binanceTickSize(ticker)}}; },
        [this, token](const QJsonObject &r) {
            if (token != m_token) return;
            m_book = footprint::Book(footprint::TickGrid{r.value("tick").toDouble(), m_book.grid().rowTicks});
            m_tickKnown = true;
            setError(QString());
            fetchLatest();   // opens the socket once it knows which exchange answered
        },
        [this, token](const QString &e) {
            if (token != m_token) return;
            setError(e);
            m_retry->disconnect();
            connect(m_retry, &QTimer::timeout, this, [this] { fetchTickSize(); });
            m_retry->start(RETRY_MS);
        });
}

void TradeFeed::fetchLatest()
{
    const int token = m_token;
    const QString ticker = m_ticker;
    api::run(this, [ticker] { return aggTrades(ticker, -1, PAGE); },
        [this, token](const QJsonObject &r) {
            if (token != m_token) return;
            const QJsonArray arr = r.value("t").toArray();
            const bool us = r.value("us").toBool();
            if (!m_exchangeKnown || us != m_us || !m_ws) {
                m_us = us;
                m_exchangeKnown = true;
                if (m_ws) {
                    QWebSocket *s = m_ws;
                    m_ws = nullptr;
                    s->close();
                }
                m_pending.clear();
                openSocket(0);
            }
            if (arr.isEmpty()) {
                setError("No trades from Binance for " + m_ticker);
                return;
            }
            for (const QJsonValue v : arr) {
                const Raw t = parse(v.toObject());
                if (m_hiId >= 0 && t.id != m_hiId + 1) continue;   // ids go up one by one
                if (m_loId < 0) m_loId = t.id;
                if (addTrade(t)) m_tape.push_back(t);
                m_hiId = t.id;
            }
            const double oldest = parse(arr.first().toObject()).t.time;
            m_completeFrom = oldest <= m_chartStart ? m_chartStart : footprint::candleOpen(oldest, m_tf) + m_tf;
            m_backfilling = true;
            setError(QString());
            emit stateChanged();
            drain();
            emit changed();
            QTimer::singleShot(PAGE_GAP_MS, this, [this, token] {
                if (token == m_token) fetchBack();
            });
        },
        [this, token](const QString &e) {
            if (token != m_token) return;
            setError(e);
            m_retry->disconnect();
            connect(m_retry, &QTimer::timeout, this, [this] { fetchLatest(); });
            m_retry->start(RETRY_MS);
        });
}

// Further back in time, until the budget or the chart's first bar: ids are consecutive, so the next pages are
// known in advance and asked for together (each request waits ~0.3 s on the network, not on Binance's limits).
void TradeFeed::fetchBack()
{
    if (m_stopped || !m_backfilling) return;
    const bool reachedStart = !std::isnan(m_completeFrom) && m_completeFrom <= m_chartStart;
    if (m_loId <= 0 || reachedStart || m_book.tradeCount() >= m_budget) {
        m_backfilling = false;
        emit stateChanged();
        return;
    }
    struct Batch {
        QVector<QPair<qint64, int>> pages;   // fromId, limit
        QVector<QJsonArray> got;
        int left = 0;
        QString error;
    };
    auto batch = std::make_shared<Batch>();
    for (int k = 0; k < PARALLEL_PAGES; ++k) {
        const qint64 to = m_loId - qint64(k) * PAGE;   // exclusive
        if (to <= 0) break;
        const qint64 from = std::max<qint64>(0, to - PAGE);
        batch->pages.push_back({from, int(to - from)});
    }
    batch->got.resize(batch->pages.size());
    batch->left = int(batch->pages.size());
    const int token = m_token;
    const QString ticker = m_ticker;
    auto finish = [this, token, batch] {
        if (token != m_token) return;
        if (!batch->error.isEmpty()) {
            setError(batch->error);
            m_retry->disconnect();
            connect(m_retry, &QTimer::timeout, this, [this] { fetchBack(); });
            m_retry->start(RETRY_MS);
            return;
        }
        const qint64 below = m_loId;
        const qint64 floor = batch->pages.last().first;
        qint64 lo = below;
        double oldest = NaN;
        std::vector<Raw> older;
        for (const QJsonArray &arr : batch->got)
            for (const QJsonValue v : arr) {
                const Raw t = parse(v.toObject());
                if (t.id < floor || t.id >= below) continue;
                if (t.id < lo) {
                    lo = t.id;
                    oldest = t.t.time;
                }
                if (addTrade(t)) older.push_back(t);
            }
        // the pages arrive in any order: onto the tape's front newest first, so it stays in id order
        std::sort(older.begin(), older.end(), [](const Raw &a, const Raw &b) { return a.id < b.id; });
        for (auto it = older.rbegin(); it != older.rend(); ++it) m_tape.push_front(*it);
        if (lo == below) {   // nothing older: the pair's first trade
            m_loId = 0;
        } else {
            m_loId = lo;
            m_completeFrom = oldest <= m_chartStart ? m_chartStart : footprint::candleOpen(oldest, m_tf) + m_tf;
        }
        if (!m_error.isEmpty()) setError(QString());
        emit changed();
        QTimer::singleShot(PAGE_GAP_MS, this, [this, token] {
            if (token == m_token) fetchBack();
        });
    };
    for (int k = 0; k < batch->pages.size(); ++k) {
        const auto [from, limit] = batch->pages[k];
        api::run(this, [ticker, from, limit] { return aggTrades(ticker, from, limit); },
            [this, batch, k, finish](const QJsonObject &r) {
                if (r.value("us").toBool() != m_us) batch->error = "Binance answered from another exchange (trying again)";
                else batch->got[k] = r.value("t").toArray();
                if (--batch->left == 0) finish();
            },
            [batch, finish](const QString &e) {
                batch->error = e;
                if (--batch->left == 0) finish();
            });
    }
}

// The trades between the last one in the book and the first live one waiting.
void TradeFeed::fetchForward()
{
    if (m_forwardBusy || m_stopped) return;
    m_forwardBusy = true;
    const qint64 from = m_hiId + 1;
    const int token = m_token;
    const QString ticker = m_ticker;
    api::run(this, [ticker, from] { return aggTrades(ticker, from, PAGE); },
        [this, token, from](const QJsonObject &r) {
            if (token != m_token) return;
            m_forwardBusy = false;
            if (r.value("us").toBool() != m_us) {   // the other exchange's ids: not these trades
                setError("Binance answered from another exchange (trying again)");
                QTimer::singleShot(RETRY_MS, this, [this, token] {
                    if (token == m_token) drain();
                });
                return;
            }
            for (const QJsonValue v : r.value("t").toArray()) {
                const Raw t = parse(v.toObject());
                if (t.id != m_hiId + 1) continue;
                if (addTrade(t)) m_tape.push_back(t);
                m_hiId = t.id;
            }
            emit changed();
            drain();
        },
        [this, token](const QString &e) {
            if (token != m_token) return;
            m_forwardBusy = false;
            setError(e);
            QTimer::singleShot(RETRY_MS, this, [this, token] {
                if (token == m_token) drain();
            });
        });
}

// ---------------------------------------------------------------- live

void TradeFeed::openSocket(int hostIdx)
{
    if (m_stopped) return;
    const QStringList &hosts = m_us ? WS_HOSTS_US : WS_HOSTS;
    if (hostIdx >= hosts.size()) {
        setError("Binance's trade stream isn't reachable (trying again)");
        m_retry->disconnect();
        connect(m_retry, &QTimer::timeout, this, [this] { openSocket(0); });
        m_retry->start(RETRY_MS);
        return;
    }
    auto *sock = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_ws = sock;
    auto opened = std::make_shared<bool>(false);
    auto closed = std::make_shared<bool>(false);
    connect(sock, &QWebSocket::connected, this, [opened] { *opened = true; });
    connect(sock, &QWebSocket::textMessageReceived, this, [this, sock](const QString &m) {
        if (m_ws != sock) return;
        const QJsonObject o = QJsonDocument::fromJson(m.toUtf8()).object();
        if (o.value("e").toString() != "aggTrade") return;
        m_pending.push_back(parse(o));
        drain();
    });
    auto onClose = [this, sock, opened, closed, hostIdx] {
        if (*closed) return;
        *closed = true;
        sock->deleteLater();
        if (m_ws != sock) return;
        m_ws = nullptr;
        if (m_stopped) return;
        if (!*opened) {
            openSocket(hostIdx + 1);
        } else {
            m_retry->disconnect();
            connect(m_retry, &QTimer::timeout, this, [this] { openSocket(0); });
            m_retry->start(3000);
        }
    };
    connect(sock, &QWebSocket::disconnected, this, onClose);
    connect(sock, &QWebSocket::errorOccurred, this, [sock, onClose](QAbstractSocket::SocketError) {
        if (sock->state() == QAbstractSocket::UnconnectedState) onClose();
    });
    sock->open(QUrl(hosts[hostIdx] + m_ticker.toLower() + "@aggTrade"));
}

// Live trades go in once every trade before them is in.
void TradeFeed::drain()
{
    if (m_hiId < 0 || m_stopped) return;
    bool added = false;
    while (!m_pending.empty()) {
        const Raw &r = m_pending.front();
        if (r.id <= m_hiId) {
            m_pending.pop_front();
            continue;
        }
        if (r.id == m_hiId + 1) {
            if (addTrade(r)) m_tape.push_back(r);
            m_hiId = r.id;
            m_pending.pop_front();
            added = true;
            continue;
        }
        // A gap. Longer than the whole budget (away for hours): start over from the latest trades.
        if (size_t(r.id - m_hiId) > m_budget) {
            m_book.clear();
            m_tape.clear();
            m_tapeGen = ++g_tapeGen;
            m_pending.clear();
            m_loId = m_hiId = -1;
            m_completeFrom = NaN;
            m_backfilling = false;
            fetchLatest();
            emit changed();
            return;
        }
        fetchForward();
        break;
    }
    if (!added) return;
    // Live, the book keeps about the budget: the oldest candles go, whole, once it's half as much again.
    if (!m_backfilling && m_book.tradeCount() > m_budget + m_budget / 2) {
        const double first = m_book.trimToTrades(m_budget);
        while (!m_tape.empty() && !(m_tape.front().t.time >= first)) m_tape.pop_front();
        if (!std::isnan(first)) m_completeFrom = std::max(std::isnan(m_completeFrom) ? first : m_completeFrom, first);
    }
    emit changed();
}

bool TradeFeed::addTrade(const Raw &r)
{
    if (r.t.time < m_chartStart || !(r.t.qty > 0)) return false;
    m_book.add(footprint::candleOpen(r.t.time, m_tf), r.t, {r.t.side, false});
    return true;
}

} // namespace qtm
