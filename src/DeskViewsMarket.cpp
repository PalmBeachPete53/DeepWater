// The Desk's live sections: Markets, Strength, Sessions, Recap, Wire.
#include "DeskViews.h"
#include "NewsReader.h"

#include "desk/NewsTags.h"
#include "desk/Sessions.h"

#include "quant/ui/PanelsKit.h"

#include <QButtonGroup>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QSettings>
#include <QTableWidget>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

namespace deskui {

namespace {

using desk::AssetClass;

QString className(AssetClass c)
{
    switch (c) {
    case AssetClass::Fx: return QObject::tr("Forex");
    case AssetClass::Index: return QObject::tr("Indices");
    case AssetClass::Commodity: return QObject::tr("Commodities");
    case AssetClass::Stock: return QObject::tr("Stocks");
    case AssetClass::Crypto: return QObject::tr("Crypto");
    }
    return {};
}

// ------------------------------------------------------------------ Markets

class MarketsView : public quant::View
{
    Q_OBJECT
public:
    explicit MarketsView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        m_search = new QLineEdit;
        m_search->setObjectName("qtInput");
        m_search->setPlaceholderText(tr("Search %1 markets: EUR/USD, Gold, Nvidia, Bitcoin…").arg(desk::instruments().size()));
        m_search->setClearButtonEnabled(true);
        h->addWidget(m_search, 1);
        h->addWidget(quant::label(tr("CHANGE OVER"), "qtFieldLabel"));
        h->addWidget(quant::segments({"1D", "1W", "1M"}, &m_range));
        h->addWidget(quant::label(tr("ORDER"), "qtFieldLabel"));
        h->addWidget(quant::segments({tr("Default"), tr("Performance"), tr("A-Z")}, &m_order));
        content()->addWidget(top);

        auto *tiles = new QHBoxLayout;
        tiles->setSpacing(10);
        tiles->addWidget(tile(tr("Breadth"), &m_breadth, &m_breadthSub), 1);
        tiles->addWidget(tile(tr("Average move"), &m_average, &m_averageSub), 1);
        tiles->addWidget(tile(tr("Leader"), &m_leader, &m_leaderSub), 1);
        tiles->addWidget(tile(tr("Laggard"), &m_laggard, &m_laggardSub), 1);
        content()->addLayout(tiles);

        auto *columns = new CardGrid(300, 5, Qt::AlignTop);
        const AssetClass classes[] = {AssetClass::Fx, AssetClass::Index, AssetClass::Commodity, AssetClass::Stock, AssetClass::Crypto};
        for (AssetClass cls : classes) {
            QVBoxLayout *body;
            auto *card = quant::card(className(cls), &body);
            body->setContentsMargins(6, 8, 6, 6);
            auto *t = table({tr("Market"), tr("Last"), tr("Change")});
            t->setCursor(Qt::PointingHandCursor);
            fitColumns(t, 0);   // the figures stay whole; a name gives way first
            body->addWidget(t);
            columns->addCard(card);
            m_tables.insert(int(cls), t);
            connect(t, &QTableWidget::cellClicked, this, [this, t](int row, int) {
                if (auto *item = t->item(row, 0))
                    openMarketSheet(item->data(Qt::UserRole).toString(), m_context, this);
            });
        }
        content()->addWidget(columns);
        content()->addStretch(1);
        auto *note = quant::label(tr("Yahoo Finance quotes, about 15 minutes delayed for most markets, refreshed every minute. Click a market for its sheet."), "qtNote");
        content()->addWidget(note);

        connect(m_search, &QLineEdit::textChanged, this, &MarketsView::fill);
        connect(m_range, &QButtonGroup::idClicked, this, &MarketsView::fill);
        connect(m_order, &QButtonGroup::idClicked, this, &MarketsView::fill);
        m_timer = new QTimer(this);
        m_timer->setInterval(60 * 1000);
        connect(m_timer, &QTimer::timeout, this, &MarketsView::fetch);
    }

protected:
    void load() override
    {
        setStatus(tr("Loading the quotes…"));
        fetch();
        QStringList symbols;
        for (const desk::Instrument &i : desk::instruments())
            symbols << i.symbol;
        research::Client::instance()->spark(symbols, "1mo", this, [this](const QHash<QString, research::Series> &series, const QString &) {
            m_history = series;
            fill();
        });
        m_timer->start();
    }

private:
    void fetch()
    {
        QStringList symbols;
        for (const desk::Instrument &i : desk::instruments())
            symbols << i.symbol;
        research::Client::instance()->quotes(symbols, 60, this, [this](const QHash<QString, QJsonObject> &q, const QString &error) {
            m_quotes = q;
            setStatus(q.isEmpty() ? tr("Quotes unavailable: %1").arg(error) : QString());
            fill();
        });
    }

    double changeOf(const desk::Instrument &i) const
    {
        const QJsonObject q = m_quotes.value(i.symbol);
        if (m_range->checkedId() == 0)
            return research::raw(q, "regularMarketChangePercent");
        const research::Series s = m_history.value(i.symbol);
        const int back = m_range->checkedId() == 1 ? 5 : 21;
        const double last = research::raw(q, "regularMarketPrice");
        if (s.size() <= back || !research::ok(last))
            return research::NaN;
        return (last / s[s.size() - 1 - back].value - 1) * 100.0;
    }

    void fill()
    {
        const Palette &pal = Theme::current();
        const QString needle = m_search->text().trimmed().toLower();
        struct Row {
            const desk::Instrument *i;
            double last, change;
        };
        QHash<int, QVector<Row>> rows;
        int up = 0, down = 0, count = 0;
        double sum = 0;
        const Row *best = nullptr, *worst = nullptr;
        QVector<Row> all;
        for (const desk::Instrument &i : desk::instruments()) {
            if (!needle.isEmpty() && !i.label.toLower().contains(needle) && !i.id.toLower().contains(needle))
                continue;
            const Row r {&i, research::raw(m_quotes.value(i.symbol), "regularMarketPrice"), changeOf(i)};
            rows[int(i.cls)].push_back(r);
            all.push_back(r);
        }
        for (const Row &r : all)
            if (research::ok(r.change)) {
                ++count;
                sum += r.change;
                (r.change >= 0 ? up : down)++;
                if (!best || r.change > best->change) best = &r;
                if (!worst || r.change < worst->change) worst = &r;
            }
        m_breadth->setText(tr("%1 up, %2 down").arg(up).arg(down));
        m_breadthSub->setText(tr("%1 markets with a quote").arg(count));
        setSigned(m_average, count ? pct(sum / count) : "—", sum);
        m_averageSub->setText(tr("across the markets shown"));
        setSigned(m_leader, best ? best->i->label : "—", 1);
        m_leaderSub->setText(best ? pct(best->change) : QString());
        setSigned(m_laggard, worst ? worst->i->label : "—", -1);
        m_laggardSub->setText(worst ? pct(worst->change) : QString());

        for (auto it = m_tables.begin(); it != m_tables.end(); ++it) {
            QVector<Row> list = rows.value(it.key());
            if (m_order->checkedId() == 1)
                std::stable_sort(list.begin(), list.end(), [](const Row &a, const Row &b) {
                    return (research::ok(a.change) ? a.change : -1e9) > (research::ok(b.change) ? b.change : -1e9);
                });
            else if (m_order->checkedId() == 2)
                std::stable_sort(list.begin(), list.end(), [](const Row &a, const Row &b) { return a.i->label < b.i->label; });
            QTableWidget *t = it.value();
            t->setRowCount(list.size());
            for (int r = 0; r < list.size(); ++r) {
                const Row &row = list[r];
                auto *name = cell(row.i->label, {}, Qt::AlignLeft, true);
                name->setData(Qt::UserRole, row.i->id);
                t->setItem(r, 0, name);
                t->setItem(r, 1, cell(research::ok(row.last) ? QLocale().toString(row.last, 'f', row.last >= 1000 && row.i->cls == desk::AssetClass::Index ? 0 : priceDecimals(row.last)) : "—", {}, Qt::AlignRight));
                t->setItem(r, 2, cell(pct(row.change), !research::ok(row.change) ? pal.TextDim : row.change >= 0 ? pal.Up : pal.Down, Qt::AlignRight));
            }
            fitHeight(t);
        }
    }

    Context m_context;
    QLineEdit *m_search;
    QButtonGroup *m_range, *m_order;
    QLabel *m_breadth, *m_breadthSub, *m_average, *m_averageSub, *m_leader, *m_leaderSub, *m_laggard, *m_laggardSub;
    QHash<int, QTableWidget *> m_tables;
    QHash<QString, QJsonObject> m_quotes;
    QHash<QString, research::Series> m_history;
    QTimer *m_timer;
};

// ------------------------------------------------------------------ Strength

class StrengthView : public quant::View
{
    Q_OBJECT
public:
    explicit StrengthView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        auto *note = quant::label(tr("Unweighted currency indices: each currency against the seven others, every pair counting the same."), "qtNote");
        note->setWordWrap(true);
        h->addWidget(note, 1);
        h->addWidget(quant::segments({"15m", "1H", "4H", "D", "W"}, &m_timeframe, std::clamp(QSettings().value("desk/strength/tf", 1).toInt(), 0, 4)));
        content()->addWidget(top);
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        QVBoxLayout *body;
        auto *chartCard = quant::card(tr("Currency strength, % since the first bar on screen"), &body);
        m_chart = new quant::MultiLineChart;
        m_chart->setMinimumHeight(430);
        body->addWidget(m_chart);
        row->addWidget(chartCard, 3);
        auto *rankCard = quant::card(tr("Ranking"), &body);
        rankCard->setMinimumWidth(200);
        m_rank = table({"#", tr("Currency"), tr("Change")});
        body->addWidget(m_rank);
        body->addStretch(1);
        row->addWidget(rankCard, 1);
        content()->addLayout(row);
        content()->addStretch(1);
        connect(m_timeframe, &QButtonGroup::idClicked, this, [this](int id) {
            QSettings().setValue("desk/strength/tf", id);
            fetch();
        });
    }

protected:
    void load() override { fetch(); }

private:
    void fetch()
    {
        struct Tf { const char *range, *interval; int bars; int merge; };
        static const Tf tfs[] = {{"5d", "15m", 192, 1}, {"1mo", "60m", 240, 1}, {"6mo", "60m", 240, 4}, {"1y", "1d", 260, 1}, {"5y", "1wk", 260, 1}};
        const Tf tf = tfs[m_timeframe->checkedId()];
        QStringList symbols;
        for (const QString &c : desk::majorCurrencies())
            if (c != "USD")
                symbols << desk::dollarPair(c);
        setStatus(tr("Loading the seven dollar pairs…"));
        const int serial = ++m_serial;
        deskdata::loadCandles(symbols, tf.range, tf.interval, this, [this, tf, serial](const deskdata::Candles &candles, const QString &error) {
            if (serial != m_serial)
                return;
            QVector<desk::CurrencySeries> series;
            for (const QString &c : desk::majorCurrencies()) {
                desk::CurrencySeries s;
                s.currency = c;
                bool direct = true;
                const QString pair = desk::dollarPair(c, &direct);
                if (!pair.isEmpty()) {
                    // FX bars of different pairs share their clock: a timeframe of four hours keeps every fourth.
                    const QVector<desk::Candle> bars = candles.value(pair);
                    for (const desk::Candle &b : bars) {
                        if (tf.merge > 1 && (b.time / 3600) % tf.merge != 0)
                            continue;
                        s.times << b.time;
                        s.dollarsPerUnit << (direct ? b.close : b.close > 0 ? 1.0 / b.close : 0.0);
                    }
                }
                series << s;
            }
            desk::Strength strength = desk::currencyStrength(series);
            if (strength.times.size() > tf.bars) {
                const int cut = strength.times.size() - tf.bars;
                strength.times.remove(0, cut);
                for (desk::StrengthLine &l : strength.lines) {
                    l.index.remove(0, cut);
                    const double base = l.index.first();
                    for (double &v : l.index)
                        v = v / base * 100.0;
                }
            }
            setStatus(strength.lines.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
            show(strength);
        });
    }

    void show(const desk::Strength &s)
    {
        const Palette &pal = Theme::current();
        QVector<quant::MultiLineChart::Line> lines;
        for (int i = 0; i < s.lines.size(); ++i) {
            QVector<double> values;
            for (double v : s.lines[i].index)
                values << v - 100.0;
            lines.push_back({s.lines[i].currency, quant::seriesColor(i), values});
        }
        QVector<QDate> dates;
        QStringList labels;
        const bool intraday = m_timeframe->checkedId() < 3;
        for (qint64 t : s.times) {
            const QDateTime local = QDateTime::fromSecsSinceEpoch(t).toLocalTime();
            dates << local.date();
            labels << QLocale().toString(local, intraday ? "d MMM HH:mm" : "d MMM yy");
        }
        m_chart->setXLabels(labels);
        m_chart->setData(dates, lines, [](double v) { return pct(v); });
        const auto rank = desk::rankStrength(s);
        m_rank->setRowCount(rank.size());
        for (int r = 0; r < rank.size(); ++r) {
            m_rank->setItem(r, 0, cell(QString::number(r + 1), pal.TextDim));
            m_rank->setItem(r, 1, cell(rank[r].first, {}, Qt::AlignCenter, true));
            m_rank->setItem(r, 2, cell(pct(rank[r].second), rank[r].second >= 0 ? pal.Up : pal.Down));
        }
        fitHeight(m_rank);
    }

    QButtonGroup *m_timeframe;
    quant::MultiLineChart *m_chart;
    QTableWidget *m_rank;
    int m_serial = 0;
};

// ------------------------------------------------------------------ Sessions

// The next 24 hours: a row per session, the hours open coloured, a line at now.
class SessionTimeline : public QWidget
{
    Q_OBJECT
public:
    explicit SessionTimeline(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(170);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &pal = Theme::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double left = 86, right = width() - 14.0, top = 22, rowH = (height() - top - 6) / 4.0;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QDateTime start = now.toLocalTime();
        const QDateTime dayStart(start.date(), QTime(0, 0), start.timeZone());
        const auto x = [&](const QDateTime &t) { return left + double(dayStart.secsTo(t)) / 86400.0 * (right - left); };
        p.setFont(quant::chartFont(9));
        for (int h = 0; h <= 24; h += 3) {
            const double at = left + h / 24.0 * (right - left);
            p.setPen(pal.TextFaint);
            p.drawText(QRectF(at - 20, 2, 40, 14), Qt::AlignCenter, QString("%1:00").arg(h, 2, 10, QChar('0')));
            p.setPen(QPen(pal.PanelLine, 1));
            p.drawLine(QPointF(at, top), QPointF(at, height() - 6.0));
        }
        const auto &sessions = desk::fxSessions();
        for (int i = 0; i < sessions.size(); ++i) {
            const double y = top + rowH * i;
            p.setFont(quant::chartFont(10, true));
            p.setPen(pal.Text);
            p.drawText(QRectF(0, y, left - 8, rowH), Qt::AlignRight | Qt::AlignVCenter, sessions[i].name);
            for (int d = -1; d <= 1; ++d) {
                QDateTime open, close;
                desk::sessionWindow(sessions[i], start.date().addDays(d), &open, &close);
                if (!open.isValid())
                    continue;
                const double a = std::clamp(x(open), left, right), b = std::clamp(x(close), left, right);
                if (b <= a)
                    continue;
                QColor fill = quant::seriesColor(i);
                fill.setAlpha(open <= now && now < close ? 220 : 90);
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawRoundedRect(QRectF(a, y + rowH * 0.2, b - a, rowH * 0.6), 3, 3);
            }
        }
        const double at = x(now);
        p.setPen(QPen(pal.Accent, 1.6));
        p.drawLine(QPointF(at, top), QPointF(at, height() - 6.0));
    }
};

class SessionsView : public quant::View
{
    Q_OBJECT
public:
    explicit SessionsView(QWidget *parent = nullptr)
        : View(parent)
    {
        QHBoxLayout *h;
        auto *top = quant::bar(&h);
        m_clock = quant::label({}, "qtTitle");
        h->addWidget(m_clock);
        h->addStretch(1);
        m_overlap = quant::label({}, "qtAsOf");
        h->addWidget(m_overlap);
        content()->addWidget(top);
        QVBoxLayout *body;
        auto *timelineCard = quant::card(tr("Today, in your time"), &body);
        m_timeline = new SessionTimeline;
        body->addWidget(m_timeline);
        content()->addWidget(timelineCard);
        auto *cards = new QHBoxLayout;
        cards->setSpacing(10);
        for (const desk::FxSession &s : desk::fxSessions()) {
            QLabel *state, *sub;
            cards->addWidget(tile(s.name.toUpper(), &state, &sub), 1);
            m_states << state;
            m_subs << sub;
        }
        content()->addLayout(cards);
        auto *note = quant::label(tr("The usual business hours of each centre, Monday to Friday, with daylight saving; FX trades around the clock, so these mark when each centre's desks are active. Public holidays aren't known."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        m_timer = new QTimer(this);
        m_timer->setInterval(1000);
        connect(m_timer, &QTimer::timeout, this, &SessionsView::tick);
    }

protected:
    void load() override
    {
        tick();
        m_timer->start();
    }

private:
    static QString countdown(qint64 secs)
    {
        return QString("%1:%2:%3").arg(secs / 3600, 2, 10, QChar('0')).arg((secs % 3600) / 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0'));
    }

    void tick()
    {
        const Palette &pal = Theme::current();
        const QDateTime now = QDateTime::currentDateTimeUtc();
        m_clock->setText(QLocale().toString(now.toLocalTime(), "ddd d MMM HH:mm:ss"));
        const QStringList open = desk::openSessions(now);
        QStringList names;
        for (const desk::FxSession &s : desk::fxSessions())
            if (open.contains(s.id))
                names << s.name;
        m_overlap->setText(names.size() >= 2 ? tr("OVERLAP: %1").arg(names.join(" + ").toUpper())
                           : names.size() == 1 ? tr("OPEN: %1").arg(names.first().toUpper()) : tr("ALL SESSIONS CLOSED"));
        const auto &sessions = desk::fxSessions();
        for (int i = 0; i < sessions.size(); ++i) {
            const desk::SessionState st = desk::sessionState(sessions[i], now);
            const QDateTime localNow = now.toTimeZone(QTimeZone(sessions[i].zone.toUtf8()));
            m_states[i]->setText(st.open ? tr("Open") : tr("Closed"));
            m_states[i]->setStyleSheet(QString("color: %1").arg((st.open ? pal.Up : pal.TextDim).name()));
            const QString when = st.open ? tr("closes in %1").arg(countdown(now.secsTo(st.nextClose))) : tr("opens in %1").arg(countdown(now.secsTo(st.nextOpen)));
            const QDateTime window = st.open ? st.todayOpen : st.nextOpen;
            m_subs[i]->setText(tr("%1\nlocal time %2, hours %3–%4 there\n%5 to %6 for you")
                                   .arg(when, localNow.toString("HH:mm"))
                                   .arg(sessions[i].openHour, 2, 10, QChar('0')).arg(sessions[i].closeHour, 2, 10, QChar('0'))
                                   .arg(QLocale().toString(window.toLocalTime().time(), "HH:mm"), QLocale().toString(st.todayClose.toLocalTime().time(), "HH:mm")));
        }
        if (now.time().second() == 0)
            m_timeline->update();
    }

    QLabel *m_clock, *m_overlap;
    SessionTimeline *m_timeline;
    QVector<QLabel *> m_states, m_subs;
    QTimer *m_timer;
};

// ------------------------------------------------------------------ Recap

struct RecapMarket {
    const char *symbol, *name, *group;
};
const RecapMarket RecapMarkets[] = {{"^GSPC", "S&P 500", QT_TRANSLATE_NOOP("QObject", "Indices")}, {"^NDX", "Nasdaq 100", QT_TRANSLATE_NOOP("QObject", "Indices")}, {"^DJI", "Dow Jones", QT_TRANSLATE_NOOP("QObject", "Indices")},
                                    {"GC=F", "Gold", QT_TRANSLATE_NOOP("QObject", "Metals")}, {"SI=F", "Silver", QT_TRANSLATE_NOOP("QObject", "Metals")},
                                    {"EURUSD=X", "EUR/USD", QT_TRANSLATE_NOOP("QObject", "Forex")}, {"JPY=X", "USD/JPY", QT_TRANSLATE_NOOP("QObject", "Forex")}, {"EURGBP=X", "EUR/GBP", QT_TRANSLATE_NOOP("QObject", "Forex")},
                                    {"BTC-USD", "Bitcoin", QT_TRANSLATE_NOOP("QObject", "Crypto")}, {"ETH-USD", "Ethereum", QT_TRANSLATE_NOOP("QObject", "Crypto")},
                                    {"CL=F", "WTI Crude", QT_TRANSLATE_NOOP("QObject", "Energy")}, {"NG=F", "Natural Gas", QT_TRANSLATE_NOOP("QObject", "Energy")}};

class RecapView : public quant::View
{
    Q_OBJECT
public:
    explicit RecapView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        QVBoxLayout *body;
        auto *registry = quant::card(tr("Recaps"), &body);
        registry->setMaximumWidth(290);
        body->addWidget(quant::segments({tr("Sessions"), tr("Weeks")}, &m_mode));
        m_list = new QListWidget;
        m_list->setObjectName("settingsNav");
        m_list->setMinimumHeight(620);
        m_list->setCursor(Qt::PointingHandCursor);
        body->addWidget(m_list, 1);
        row->addWidget(registry, 0);

        auto *mid = new QVBoxLayout;
        mid->setSpacing(10);
        QHBoxLayout *head;
        auto *file = quant::card({}, &body, &head);
        m_title = quant::label("—", "qtTitle");
        body->addWidget(m_title);
        m_when = quant::label({}, "qtNote");
        body->addWidget(m_when);
        m_read = quant::label({}, "qtCardLabel");
        m_read->setWordWrap(true);
        m_read->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->addWidget(m_read);
        auto *actions = new QHBoxLayout;
        auto *written = quant::label(tr("Written by the app from the figures below, by fixed rules."), "qtNote");
        written->setWordWrap(true);
        actions->addWidget(written, 1);
        actions->addWidget(askAiButton(c, tr("ASK THE AI TO READ IT"),
                                       tr("Use read_page on the Desk page, section Recap, then explain this session in a few sentences: what moved most, the releases and headlines that may explain it. Use only what the page shows and say what it doesn't show.")));
        body->addLayout(actions);
        mid->addWidget(file);
        auto *movesCard = quant::card(tr("Moves of the window, open to close, 15-minute bars"), &body);
        m_moves = table({tr("Market"), tr("Group"), tr("Open"), tr("Close"), tr("Change"), tr("High"), tr("Low")});
        m_moves->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);   // names stay whole
        body->addWidget(m_moves);
        mid->addWidget(movesCard);
        auto *chartCard = quant::card(tr("The window's largest move"), &body);
        m_chart = new quant::MultiLineChart;
        m_chart->setMinimumHeight(220);
        body->addWidget(m_chart);
        mid->addWidget(chartCard);
        // The side tables need the width of a column of their own: under the rest on a laptop.
        auto *flip = new FlipRow(1100);
        flip->setSpacing(10);
        flip->addLayout(mid, 1);

        auto *side = new QVBoxLayout;
        side->setSpacing(10);
        auto *releases = quant::card(tr("Releases in the window"), &body);
        m_releases = table({tr("Time"), tr("Event"), tr("Actual"), tr("Forecast"), tr("Previous")});
        fitColumns(m_releases, 1);
        body->addWidget(m_releases);
        side->addWidget(releases);
        auto *wire = quant::card(tr("Wire in the window"), &body);
        m_wire = table({tr("Time"), tr("Headline")});
        fitColumns(m_wire, 1);
        m_wire->setWordWrap(true);
        body->addWidget(m_wire);
        side->addWidget(wire);
        side->addStretch(1);
        flip->addLayout(side, 1);
        row->addLayout(flip, 4);
        content()->addLayout(row);
        content()->addStretch(1);

        connect(m_mode, &QButtonGroup::idClicked, this, &RecapView::fillList);
        connect(m_list, &QListWidget::currentRowChanged, this, &RecapView::selected);
    }

protected:
    void load() override
    {
        setStatus(tr("Loading the bars and the wire…"));
        QStringList symbols;
        for (const RecapMarket &m : RecapMarkets)
            symbols << m.symbol;
        deskdata::loadCandles(symbols, "1mo", "15m", this, [this](const deskdata::Candles &candles, const QString &error) {
            m_candles = candles;
            setStatus(candles.isEmpty() ? tr("Prices unavailable: %1").arg(error) : QString());
            fillList();
        });
        deskdata::loadFeed("investinglive", "https://investinglive.com/feed/", 5 * 60, this, [this](const QVector<news::Item> &items, const QString &) {
            m_items = items;
            selected(m_list->currentRow());
        });
        if (m_context.calendar)
            m_context.calendar->refreshIfStale();
    }

private:
    double moveOf(const QVector<desk::Candle> &bars, const QDateTime &from, const QDateTime &to, double *open = nullptr, double *close = nullptr, double *high = nullptr, double *low = nullptr) const
    {
        const qint64 a = from.toSecsSinceEpoch(), b = to.toSecsSinceEpoch();
        double first = research::NaN, last = research::NaN, hi = -1e300, lo = 1e300;
        for (const desk::Candle &c : bars) {
            if (c.time < a || c.time >= b)
                continue;
            if (std::isnan(first))
                first = c.open;
            last = c.close;
            hi = std::max(hi, c.high);
            lo = std::min(lo, c.low);
        }
        if (open) *open = first;
        if (close) *close = last;
        if (high) *high = hi;
        if (low) *low = lo;
        return !std::isnan(first) && first > 0 ? (last / first - 1) * 100.0 : research::NaN;
    }

    void fillList()
    {
        m_windows.clear();
        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (m_mode->checkedId() == 0) {
            m_windows = desk::recapWindows(now, 21);
        } else {
            const QDate today = now.date();
            for (int w = 0; w < 5; ++w) {
                const QDate monday = today.addDays(-(today.dayOfWeek() - 1) - 7 * w);
                desk::RecapWindow win {"week", tr("Week of %1").arg(QLocale().toString(monday, "d MMM")), QDateTime(monday, QTime(0, 0), QTimeZone::UTC),
                                       QDateTime(monday.addDays(5), QTime(0, 0), QTimeZone::UTC), false};
                win.running = now < win.end;
                m_windows.push_back(win);
            }
        }
        const QSignalBlocker block(m_list);
        m_list->clear();
        for (const desk::RecapWindow &w : m_windows) {
            // The session's mover, in its title: what you'd look for in the list.
            double best = 0;
            QString name;
            for (const RecapMarket &m : RecapMarkets) {
                const double move = moveOf(m_candles.value(m.symbol), w.start, w.end);
                if (research::ok(move) && std::abs(move) > std::abs(best)) {
                    best = move;
                    name = m.name;
                }
            }
            QString text = w.id == "week" ? w.name : QString("%1  %2").arg(QLocale().toString(w.start.toLocalTime().date(), "ddd d MMM"), w.name);
            text += w.running ? tr("  · live") : QString();
            if (!name.isEmpty())
                text += QString("\n%1 %2").arg(name, pct(best));
            m_list->addItem(text);
        }
        if (!m_windows.isEmpty()) {
            m_list->setCurrentRow(0);
            selected(0);
        }
    }

    void selected(int row)
    {
        if (row < 0 || row >= m_windows.size())
            return;
        const Palette &pal = Theme::current();
        const desk::RecapWindow &w = m_windows[row];
        m_title->setText(w.id == "week" ? w.name.toUpper() : tr("%1 SESSION").arg(w.name.toUpper()));
        m_moves->setRowCount(0);
        int up = 0, down = 0;
        struct Move {
            const RecapMarket *market;
            double change;
        };
        QVector<Move> moves;
        QHash<QString, QStringList> byGroup;
        for (const RecapMarket &m : RecapMarkets) {
            double open, close, high, low;
            const double change = moveOf(m_candles.value(m.symbol), w.start, w.end, &open, &close, &high, &low);
            if (!research::ok(change))
                continue;
            (change >= 0 ? up : down)++;
            moves.push_back({&m, change});
            byGroup[m.group] << QString("%1 %2").arg(m.name, pct(change));
            const int r = m_moves->rowCount();
            m_moves->insertRow(r);
            const int d = priceDecimals(close);
            m_moves->setItem(r, 0, cell(m.name, {}, Qt::AlignLeft, true));
            m_moves->setItem(r, 1, cell(QObject::tr(m.group), pal.TextDim));
            m_moves->setItem(r, 2, cell(QLocale().toString(open, 'f', d)));
            m_moves->setItem(r, 3, cell(QLocale().toString(close, 'f', d)));
            m_moves->setItem(r, 4, cell(pct(change), change >= 0 ? pal.Up : pal.Down, Qt::AlignCenter, true));
            m_moves->setItem(r, 5, cell(QLocale().toString(high, 'f', d)));
            m_moves->setItem(r, 6, cell(QLocale().toString(low, 'f', d)));
        }
        fitHeight(m_moves);

        // Releases and headlines of the window.
        const QLocale loc;
        QVector<EconomicCalendar::Event> events;
        if (m_context.calendar)
            for (const EconomicCalendar::Event &e : m_context.calendar->events())
                if (e.time >= w.start && e.time < w.end && e.impact >= 1 && !std::isnan(e.actual))
                    events << e;
        m_releases->setRowCount(events.size());
        for (int r = 0; r < events.size(); ++r) {
            const EconomicCalendar::Event &e = events[r];
            m_releases->setItem(r, 0, cell(loc.toString(e.time.toLocalTime().time(), "HH:mm"), pal.TextDim));
            m_releases->setItem(r, 1, cell(QString("%1 %2").arg(e.country, e.title), {}, Qt::AlignLeft));
            m_releases->setItem(r, 2, cell(amount(e.actual), !std::isnan(e.forecast) ? (e.actual >= e.forecast ? pal.Up : pal.Down) : QColor(), Qt::AlignCenter, true));
            m_releases->setItem(r, 3, cell(amount(e.forecast)));
            m_releases->setItem(r, 4, cell(amount(e.previous)));
        }
        fitHeight(m_releases, 14);
        QVector<news::Item> items;
        for (const news::Item &i : m_items)
            if (i.published >= w.start && i.published < w.end)
                items << i;
        m_wire->setRowCount(items.size());
        for (int r = 0; r < items.size(); ++r) {
            m_wire->setItem(r, 0, cell(loc.toString(items[r].published.toLocalTime().time(), "HH:mm"), pal.TextDim));
            m_wire->setItem(r, 1, cell(items[r].title, {}, Qt::AlignLeft));
        }
        fitHeight(m_wire, 18);

        m_when->setText(tr("%1 to %2 your time · breadth %3 up, %4 down · %5 releases · %6 headlines%7")
                            .arg(loc.toString(w.start.toLocalTime(), QLocale::ShortFormat), loc.toString(w.end.toLocalTime(), QLocale::ShortFormat))
                            .arg(up).arg(down).arg(events.size()).arg(items.size())
                            .arg(w.running ? tr(" · running") : QString()));

        // The read: figures in words, nothing more.
        if (moves.isEmpty()) {
            m_read->setText(tr("No bar was found in this window: the markets were closed, or the prices aren't loaded yet."));
            m_chart->setData({}, {}, [](double v) { return pct(v); });
            return;
        }
        std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) { return std::abs(a.change) > std::abs(b.change); });
        QStringList paragraphs;
        paragraphs << tr("The largest move was %1 at %2.").arg(QString(moves[0].market->name), pct(moves[0].change));
        for (const char *group : {"Indices", "Metals", "Forex", "Crypto", QT_TRANSLATE_NOOP("QObject", "Energy")})
            if (byGroup.contains(group))
                paragraphs << QString("%1: %2.").arg(QObject::tr(group), byGroup[group].join(", "));
        QVector<const EconomicCalendar::Event *> surprises;
        for (const EconomicCalendar::Event &e : events)
            if (e.impact == 2 && !std::isnan(e.forecast))
                surprises << &e;
        if (!surprises.isEmpty()) {
            QStringList parts;
            for (int i = 0; i < std::min<int>(3, surprises.size()); ++i)
                parts << tr("%1 %2 at %3 for %4 expected").arg(surprises[i]->country, surprises[i]->title, amount(surprises[i]->actual), amount(surprises[i]->forecast));
            paragraphs << tr("High-impact releases: %1.").arg(parts.join("; "));
        }
        m_read->setText(paragraphs.join("\n"));

        const qint64 a = w.start.toSecsSinceEpoch(), b = w.end.toSecsSinceEpoch();
        QVector<QDate> dates;
        QStringList labels;
        QVector<double> values;
        double first = research::NaN;
        for (const desk::Candle &c : m_candles.value(moves[0].market->symbol)) {
            if (c.time < a || c.time >= b)
                continue;
            if (std::isnan(first))
                first = c.open;
            const QDateTime local = QDateTime::fromSecsSinceEpoch(c.time).toLocalTime();
            dates << local.date();
            labels << loc.toString(local, w.id == "week" ? "ddd HH:mm" : "HH:mm");
            values << (c.close / first - 1) * 100.0;
        }
        m_chart->setXLabels(labels);
        m_chart->setData(dates, {{moves[0].market->name, pal.Accent, values}}, [](double v) { return pct(v); });
    }

    Context m_context;
    QButtonGroup *m_mode;
    QListWidget *m_list;
    QLabel *m_title, *m_when, *m_read;
    QTableWidget *m_moves, *m_releases, *m_wire;
    quant::MultiLineChart *m_chart;
    QVector<desk::RecapWindow> m_windows;
    deskdata::Candles m_candles;
    QVector<news::Item> m_items;
};

// ------------------------------------------------------------------ Wire

class WireView : public quant::View
{
    Q_OBJECT
public:
    explicit WireView(const Context &c, QWidget *parent = nullptr)
        : View(parent)
        , m_context(c)
    {
        auto *top = new QFrame;
        top->setObjectName("qtBar");
        auto *h = new qtm::kit::WrapBarLayout(top, 10, 6);
        h->setContentsMargins(10, 6, 10, 6);
        h->addWidget(quant::segments({tr("All"), tr("Headlines"), tr("Movers"), tr("Releases")}, &m_filter));
        m_counts = quant::label({}, "qtNote");
        h->addWidget(m_counts);
        auto *liveTv = quant::label(tr("LIVE TV"), "qtFieldLabel");
        liveTv->setProperty("marginLeftAuto", true);
        h->addWidget(liveTv);
        const struct { const char *name, *url; } channels[] = {{"Bloomberg Television", "https://www.youtube.com/@markets/live"},
                                                               {"Yahoo Finance", "https://www.youtube.com/@YahooFinance/live"},
                                                               {"CNBC Television", "https://www.youtube.com/@CNBCtelevision/live"}};
        for (const auto &ch : channels) {
            auto *b = deskAction(ch.name);
            b->setToolTip(tr("Opens the channel's live stream on YouTube, here"));
            connect(b, &QPushButton::clicked, this, [this, url = QString(ch.url), name = QString(ch.name)] { openHere(url, name); });
            h->addWidget(b);
        }
        content()->addWidget(top);
        QVBoxLayout *body;
        auto *card = quant::card({}, &body);
        body->setContentsMargins(4, 4, 4, 4);
        m_table = table({tr("Time"), tr("Age"), tr("Kind"), tr("Item")});
        fitColumns(m_table, 3);
        m_table->setCursor(Qt::PointingHandCursor);
        connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            const QString url = m_table->item(row, 0)->data(Qt::UserRole).toString();
            if (!url.isEmpty())
                openHere(url, m_table->item(row, 3)->text());
        });
        body->addWidget(m_table);
        content()->addWidget(card);
        auto *note = quant::label(tr("Headlines from InvestingLive's public feed, refreshed every minute; moves over 3% from the delayed quotes; releases from the economic calendar. A feed, not a professional squawk: minutes behind at best. Click a headline to open it."), "qtNote");
        note->setWordWrap(true);
        content()->addWidget(note);
        content()->addStretch(1);
        connect(m_filter, &QButtonGroup::idClicked, this, &WireView::fill);
        m_timer = new QTimer(this);
        m_timer->setInterval(60 * 1000);
        connect(m_timer, &QTimer::timeout, this, &WireView::fetch);
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        View::resizeEvent(e);
        if (m_reader && m_reader->isOpen())
            m_reader->setGeometry(rect());
    }
    void load() override
    {
        fetch();
        m_timer->start();
    }

private:
    // In the News reader, over this section: articles, and the TV channels' live pages.
    void openHere(const QString &url, const QString &title)
    {
        news::Item item;
        item.url = url;
        item.title = title;
        item.site = QUrl(url).host();
        if (!m_reader) {
            m_reader = new newsui::Reader(this);
            if (m_context.page)
                connect(m_reader, &newsui::Reader::askAi, m_context.page, &DeskPage::askAi);
        }
        m_reader->open(item);
    }

    struct Entry {
        QDateTime time;
        int kind;   // 1 headline, 2 mover, 3 release
        QString text, url;
        desk::NewsImpact impact;
    };

    void fetch()
    {
        deskdata::loadFeed("investinglive", "https://investinglive.com/feed/", 60, this, [this](const QVector<news::Item> &items, const QString &error) {
            m_items = items;
            setStatus(items.isEmpty() && !error.isEmpty() ? tr("The wire is unavailable: %1").arg(error) : QString());
            fill();
        });
        QStringList symbols;
        for (const desk::Instrument &i : desk::instruments())
            symbols << i.symbol;
        research::Client::instance()->quotes(symbols, 60, this, [this](const QHash<QString, QJsonObject> &q, const QString &) {
            const QDateTime now = QDateTime::currentDateTimeUtc();
            for (const desk::Instrument &i : desk::instruments()) {
                const QJsonObject o = q.value(i.symbol);
                const double change = research::raw(o, "regularMarketChangePercent"), last = research::raw(o, "regularMarketPrice");
                const double seen = m_moverLevels.value(i.id, 0);
                // A market enters the wire when it crosses another 3% step, not at every refresh.
                if (research::ok(change) && std::abs(change) >= 3 && std::abs(change) >= std::abs(seen) + 3) {
                    m_moverLevels.insert(i.id, change);
                    const qint64 at = o.value("regularMarketTime").toInteger();
                    m_movers.push_back({at > 0 ? QDateTime::fromSecsSinceEpoch(at, QTimeZone::UTC) : now, 2,
                                        tr("%1 %2, now %3").arg(i.label, pct(change), QLocale().toString(last, 'f', priceDecimals(last))), {}, desk::NewsImpact::Important});
                }
            }
            fill();
        });
        if (m_context.calendar)
            m_context.calendar->refreshIfStale();
    }

    void fill()
    {
        const Palette &pal = Theme::current();
        QVector<Entry> all;
        for (const news::Item &i : m_items)
            all.push_back({i.published, 1, i.title, i.url, desk::tagHeadline(i.title).impact});
        all += m_movers;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        int releases = 0;
        if (m_context.calendar)
            for (const EconomicCalendar::Event &e : m_context.calendar->events())
                if (!std::isnan(e.actual) && e.time <= now && e.time.secsTo(now) < 2 * 86400 && e.impact >= 1) {
                    ++releases;
                    QString text = QString("%1 %2: %3").arg(e.country, e.title, amount(e.actual));
                    if (!std::isnan(e.forecast))
                        text += tr(" (forecast %1, previous %2)").arg(amount(e.forecast), amount(e.previous));
                    all.push_back({e.time, 3, text, {}, e.impact == 2 ? desk::NewsImpact::High : desk::NewsImpact::Important});
                }
        std::sort(all.begin(), all.end(), [](const Entry &a, const Entry &b) { return a.time > b.time; });
        m_counts->setText(tr("%1 headlines, %2 movers, %3 releases").arg(m_items.size()).arg(m_movers.size()).arg(releases));
        const int filter = m_filter->checkedId();
        QVector<Entry> shown;
        for (const Entry &e : all)
            if ((filter == 0 || e.kind == filter) && shown.size() < 200)
                shown << e;
        m_table->setRowCount(shown.size());
        const QStringList kinds {{}, tr("HEADLINE"), tr("MOVER"), tr("RELEASE")};
        for (int r = 0; r < shown.size(); ++r) {
            const Entry &e = shown[r];
            auto *time = cell(QLocale().toString(e.time.toLocalTime(), "HH:mm:ss"), pal.TextDim);
            time->setData(Qt::UserRole, e.url);
            m_table->setItem(r, 0, time);
            const qint64 age = e.time.secsTo(now);
            m_table->setItem(r, 1, cell(age < 3600 ? tr("%1m ago").arg(std::max<qint64>(0, age / 60)) : age < 86400 ? tr("%1h ago").arg(age / 3600) : tr("%1d ago").arg(age / 86400), pal.TextFaint));
            m_table->setItem(r, 2, cell(kinds[e.kind], e.kind == 2 ? pal.Accent : e.kind == 3 ? pal.Up : pal.TextDim));
            m_table->setItem(r, 3, cell(e.text, e.impact == desk::NewsImpact::High ? pal.Text : pal.TextDim, Qt::AlignLeft, e.impact == desk::NewsImpact::High));
        }
        fitHeight(m_table);
    }

    Context m_context;
    QButtonGroup *m_filter;
    QLabel *m_counts;
    QTableWidget *m_table;
    QTimer *m_timer;
    QVector<news::Item> m_items;
    newsui::Reader *m_reader = nullptr;
    QVector<Entry> m_movers;
    QHash<QString, double> m_moverLevels;
};

} // namespace

QWidget *makeMarkets(const Context &c) { return new MarketsView(c); }
QWidget *makeStrength(const Context &) { return new StrengthView; }
QWidget *makeSessions(const Context &) { return new SessionsView; }
QWidget *makeRecap(const Context &c) { return new RecapView(c); }
QWidget *makeWire(const Context &c) { return new WireView(c); }

} // namespace deskui

#include "DeskViewsMarket.moc"
