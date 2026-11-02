// Order book recordings (see DepthRecord.h). A Deepwater addition, not ported from Quant Terminal.
#include "DepthRecord.h"

#include <QtEndian>

#include <cmath>
#include <cstring>

namespace qtm::depthrec {

namespace {
constexpr char MAGIC[8] = {'D', 'W', 'D', 'E', 'P', 'T', 'H', '1'};
constexpr uint16_t VERSION = 1;

void put8(QByteArray &b, uint8_t v) { b.append(char(v)); }
void putF64(QByteArray &b, double v)
{
    char c[8];
    std::memcpy(c, &v, 8);   // little-endian on every machine the app runs on (arm64, x86-64)
    b.append(c, 8);
}
void putVar(QByteArray &b, uint64_t v)
{
    while (v >= 0x80) {
        b.append(char(uint8_t(v) | 0x80));
        v >>= 7;
    }
    b.append(char(uint8_t(v)));
}
void putZig(QByteArray &b, int64_t v) { putVar(b, (uint64_t(v) << 1) ^ uint64_t(v >> 63)); }

// Reads from a buffer; any read past the end sets `bad` and returns 0.
struct In {
    const QByteArray &b;
    qsizetype &pos;
    bool bad = false;
    uint8_t u8()
    {
        if (pos >= b.size()) {
            bad = true;
            return 0;
        }
        return uint8_t(b[pos++]);
    }
    double f64()
    {
        if (pos + 8 > b.size()) {
            bad = true;
            return 0;
        }
        double v;
        std::memcpy(&v, b.constData() + pos, 8);
        pos += 8;
        return v;
    }
    uint64_t var()
    {
        uint64_t v = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            const uint8_t c = u8();
            if (bad) return 0;
            v |= uint64_t(c & 0x7f) << shift;
            if (!(c & 0x80)) return v;
        }
        bad = true;
        return 0;
    }
    int64_t zig()
    {
        const uint64_t v = var();
        return int64_t(v >> 1) ^ -int64_t(v & 1);
    }
};

bool readLevels(In &in, std::vector<depth::Level> &bids, std::vector<depth::Level> &asks)
{
    const uint64_t nb = in.var(), na = in.var();
    if (in.bad || nb > 10'000'000 || na > 10'000'000) return false;
    for (auto [side, n] : {std::pair{&bids, nb}, std::pair{&asks, na}}) {
        side->clear();
        side->reserve(size_t(n));
        int64_t tick = 0;
        for (uint64_t i = 0; i < n; ++i) {
            tick = i == 0 ? in.zig() : tick + in.zig();
            const double q = double(in.var()) * QTY_UNIT;
            if (in.bad) return false;
            side->push_back({tick, q});
        }
    }
    return true;
}
} // namespace

QByteArray encodeHeader(const Header &h)
{
    QByteArray b(MAGIC, 8);
    put8(b, uint8_t(VERSION & 0xff));
    put8(b, uint8_t(VERSION >> 8));
    const QByteArray t = h.ticker.toUtf8().left(255);
    put8(b, uint8_t(t.size()));
    b.append(t);
    putF64(b, h.tickSize);
    put8(b, h.us ? 1 : 0);
    return b;
}

// ---------------------------------------------------------------- writer

void Writer::levels(const std::vector<depth::Level> &bids, const std::vector<depth::Level> &asks)
{
    putVar(m_buf, bids.size());
    putVar(m_buf, asks.size());
    for (const auto *side : {&bids, &asks}) {
        int64_t prev = 0;
        for (size_t i = 0; i < side->size(); ++i) {
            const depth::Level &l = (*side)[i];
            putZig(m_buf, i == 0 ? l.tick : l.tick - prev);
            prev = l.tick;
            putVar(m_buf, uint64_t(std::llround(std::max(0.0, l.qty) / QTY_UNIT)));
        }
    }
}

void Writer::reset(double time)
{
    put8(m_buf, uint8_t(Type::Reset));
    putF64(m_buf, time);
}

void Writer::lost(double time)
{
    put8(m_buf, uint8_t(Type::Lost));
    putF64(m_buf, time);
}

void Writer::snapshot(double time, const depth::DepthSnapshot &s, Type type)
{
    put8(m_buf, uint8_t(type));
    putF64(m_buf, time);
    putVar(m_buf, uint64_t(std::max<int64_t>(0, s.lastUpdateId)));
    levels(s.bids, s.asks);
}

void Writer::update(const depth::DepthUpdate &u)
{
    put8(m_buf, uint8_t(Type::Update));
    putF64(m_buf, u.time);
    putVar(m_buf, uint64_t(std::max<int64_t>(0, u.firstId)));
    putVar(m_buf, uint64_t(std::max<int64_t>(0, u.lastId - u.firstId)));
    putVar(m_buf, uint64_t(u.prevLastId + 1));   // −1 (none) → 0
    levels(u.bids, u.asks);
}

void Writer::checkpoint(double time, const depth::OrderBook &book)
{
    depth::DepthSnapshot s;
    s.lastUpdateId = book.lastUpdateId();
    s.time = time;
    for (const auto &[t, q] : book.bids()) s.bids.push_back({t, q});
    for (const auto &[t, q] : book.asks()) s.asks.push_back({t, q});
    snapshot(time, s, Type::Checkpoint);
}

QByteArray Writer::takeBlock(int level)
{
    if (m_buf.isEmpty()) return {};
    const QByteArray z = qCompress(m_buf, level);
    m_buf.clear();
    QByteArray out(4, '\0');
    qToBigEndian<quint32>(quint32(z.size()), out.data());
    out.append(z);
    return out;
}

// ---------------------------------------------------------------- reader

Reader::Reader(const QByteArray &data) : m_data(data)
{
    In in{m_data, m_pos};
    char magic[8];
    if (m_data.size() < 8) return;
    std::memcpy(magic, m_data.constData(), 8);
    if (std::memcmp(magic, MAGIC, 8) != 0) return;
    m_pos = 8;
    const uint16_t version = uint16_t(in.u8() | (in.u8() << 8));
    if (version != VERSION) return;
    const uint8_t n = in.u8();
    if (in.bad || m_pos + n > m_data.size()) return;
    m_h.ticker = QString::fromUtf8(m_data.mid(m_pos, n));
    m_pos += n;
    m_h.tickSize = in.f64();
    m_h.us = in.u8() == 1;
    m_ok = !in.bad && m_h.tickSize > 0;
}

bool Reader::nextBlock()
{
    if (m_pos >= m_data.size()) return false;
    if (m_pos + 4 > m_data.size()) {
        m_truncated = true;
        return false;
    }
    const quint32 len = qFromBigEndian<quint32>(m_data.constData() + m_pos);
    if (m_pos + 4 + qsizetype(len) > m_data.size()) {
        m_truncated = true;
        return false;
    }
    m_block = qUncompress(reinterpret_cast<const uchar *>(m_data.constData() + m_pos + 4), qsizetype(len));
    m_pos += 4 + qsizetype(len);
    m_bpos = 0;
    if (m_block.isEmpty()) {   // damaged
        m_truncated = true;
        return false;
    }
    return true;
}

bool Reader::next(Record &r)
{
    if (!m_ok) return false;
    while (m_bpos >= m_block.size())
        if (!nextBlock()) return false;
    In in{m_block, m_bpos};
    const uint8_t type = in.u8();
    r = Record();
    r.type = Type(type);
    r.time = in.f64();
    switch (r.type) {
    case Type::Reset:
    case Type::Lost:
        break;
    case Type::Snapshot:
    case Type::Checkpoint:
        r.snapshot.lastUpdateId = int64_t(in.var());
        r.snapshot.time = r.time;
        if (!readLevels(in, r.snapshot.bids, r.snapshot.asks)) in.bad = true;
        break;
    case Type::Update:
        r.update.time = r.time;
        r.update.firstId = int64_t(in.var());
        r.update.lastId = r.update.firstId + int64_t(in.var());
        r.update.prevLastId = int64_t(in.var()) - 1;
        if (!readLevels(in, r.update.bids, r.update.asks)) in.bad = true;
        break;
    default:
        in.bad = true;
    }
    if (in.bad) {   // a damaged block: stop here
        m_truncated = true;
        m_block.clear();
        m_bpos = 0;
        m_pos = m_data.size();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- replay

void Replayer::feed(const Record &r)
{
    auto sample = [&](double t) {
        std::vector<heatmap::Column> c = m_sampler.advance(m_book, t);
        m_cols.insert(m_cols.end(), std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
    };
    switch (r.type) {
    case Type::Reset:
        m_book.reset();
        m_sampler.reset();
        break;
    case Type::Snapshot:
    case Type::Checkpoint:
        m_book.setSnapshot(r.snapshot);
        break;
    case Type::Update:
        sample(r.update.time);   // the columns before it show the book before it, as live
        m_book.apply(r.update);
        break;
    case Type::Lost:
        sample(r.time);
        m_book.desync();
        break;
    }
}

std::vector<heatmap::Column> Replayer::take()
{
    std::vector<heatmap::Column> out;
    out.swap(m_cols);
    return out;
}

} // namespace qtm::depthrec
