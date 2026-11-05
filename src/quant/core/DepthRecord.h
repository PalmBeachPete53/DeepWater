#pragma once

// Recordings of the liquidity heatmap's order book (a Deepwater addition to the Charts tab, not in Quant Terminal):
// what Binance sent, so the heatmap can be rebuilt later exactly as it was drawn live. Pure code (no files): the depth
// stream (server/DepthStream) writes the bytes to disk and reads them back.
//
// A recording is a header and blocks. Header: "DWDEPTH1", a format version (u16), the pair (u8 length + UTF-8), the
// tick size (f64) and the exchange (u8: 1 = binance.us). A block: its length (u32, big-endian) and qCompress()ed
// records; a block cut short (the app stopped while writing) ends the reading, everything before it stays good.
//
// Records, each a type (u8) and the time (f64, UTC seconds), then:
//   Reset       nothing: a new session or file, the book starts over
//   Snapshot    a REST snapshot as received (it goes through OrderBook::setSnapshot, buffered updates and all)
//   Update      a diff as received: firstId, lastId − firstId, prevLastId + 1 (0 = none), the levels
//   Lost        nothing: the socket closed, updates were missed (OrderBook::desync)
//   Checkpoint  the book as it was (when a new file starts): a snapshot with the last update id applied
// Integers are LEB128 varints, signed ones zigzagged; levels are a count of bids and of asks, then each level's tick
// (the first absolute, the next ones as differences) and its quantity in units of 1e-8 (exact for Binance, whose
// quantities have at most 8 decimals). About 8 bytes a level before compression.

#include "Heatmap.h"
#include "OrderBook.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace qtm::depthrec {

enum class Type : uint8_t { Reset = 1, Snapshot = 2, Update = 3, Lost = 4, Checkpoint = 5 };

struct Header {
    QString ticker;
    double tickSize = 0;
    bool us = false;
};

struct Record {
    Type type = Type::Reset;
    double time = 0;
    depth::DepthUpdate update;       // Update
    depth::DepthSnapshot snapshot;   // Snapshot, Checkpoint
};

constexpr double QTY_UNIT = 1e-8;

QByteArray encodeHeader(const Header &h);

// Records go in, compressed blocks come out (each ready to append to the file).
class Writer
{
public:
    void reset(double time);
    void lost(double time);
    void snapshot(double time, const depth::DepthSnapshot &s, Type type = Type::Snapshot);
    void update(const depth::DepthUpdate &u);
    // The checkpoint of a book: its levels and last update id.
    void checkpoint(double time, const depth::OrderBook &book);
    size_t pending() const { return size_t(m_buf.size()); }
    QByteArray takeBlock(int level = 6);   // empty when there is nothing

private:
    void levels(const std::vector<depth::Level> &bids, const std::vector<depth::Level> &asks);
    QByteArray m_buf;
};

// Reads a whole recording (header, then the records block by block).
class Reader
{
public:
    explicit Reader(const QByteArray &data);
    bool ok() const { return m_ok; }      // the header was good
    const Header &header() const { return m_h; }
    bool next(Record &r);                 // false at the end, or at a block cut short or damaged
    bool truncated() const { return m_truncated; }

private:
    bool nextBlock();
    QByteArray m_data;
    qsizetype m_pos = 0;
    QByteArray m_block;
    qsizetype m_bpos = 0;
    Header m_h;
    bool m_ok = false, m_truncated = false;
};

// Records into a book and a sampler, as the live feed does, columns out.
class Replayer
{
public:
    explicit Replayer(const heatmap::SamplerParams &p) : m_sampler(p) {}
    void feed(const Record &r);
    // The columns made so far (taken: the next call starts empty).
    std::vector<heatmap::Column> take();
    const depth::OrderBook &book() const { return m_book; }

private:
    depth::OrderBook m_book;
    heatmap::Sampler m_sampler;
    std::vector<heatmap::Column> m_cols;
};

} // namespace qtm::depthrec
