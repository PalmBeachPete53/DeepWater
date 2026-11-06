#pragma once

#include <QTimeZone>

#include <QDateTime>
#include <QTimer>
#include <QWidget>

#include "CalendarData.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QToolButton;

// Economic tab: releases of the day, the week or a chosen date, filtered by country and impact, with the
// actual figure against the forecast.
namespace quant { class MultiLineChart; }
class QFrame;

class EconomicView : public QWidget
{
    Q_OBJECT
public:
    explicit EconomicView(EconomicCalendar *calendar, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum Range { LastWeek, Yesterday, Today, Tomorrow, ThisWeek, NextWeek, Day };
    void setRange(int range);
    void rebuild();
    void showHistory(const QString &country, const QString &title);

    EconomicCalendar *m_cal;
    int m_range = Today;
    QDate m_day;                    // Range Day: the date picked
    QDate m_fetchedDay;             // a day outside the calendar's window, downloaded on its own
    QVector<EconomicCalendar::Event> m_dayEvents;
    QString m_dayError;
    QButtonGroup *m_rangeGroup;
    QDateEdit *m_date;
    QComboBox *m_country;
    QCheckBox *m_impact[3];
    QTableWidget *m_table;
    QLabel *m_empty;                // instead of the table when nothing matches
    QFrame *m_history = nullptr;    // one release's past figures against their forecasts, under the table
    QLabel *m_historyTitle = nullptr, *m_historyNote = nullptr;
    QPushButton *m_historySource = nullptr;   // the publisher's page for this figure
    QString m_historySourceUrl;
    QTableWidget *m_historyTable = nullptr;
    quant::MultiLineChart *m_historyChart = nullptr;
    int m_historySerial = 0;
    QLabel *m_status;
    QTimer m_poll;                  // fresh actuals while the tab is shown
};

// Corporate tab: earnings, dividends, splits and IPOs of US listings.
class CorporateView : public QWidget
{
    Q_OBJECT
public:
    explicit CorporateView(CorporateCalendar *calendar, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *) override;

private:
    void rebuild();
    void fillEarnings();

    CorporateCalendar *m_cal;
    bool m_upcoming = false;
    QPushButton *m_upcomingButton;
    QPushButton *m_recentButton;
    QTableWidget *m_earnings;
    QTableWidget *m_dividends;
    QTableWidget *m_splits;
    QTableWidget *m_ipos;
    QLabel *m_status;
};

// An exchange or futures market and its weekly trading sessions, in its own time zone.
struct MarketHours {
    struct Session {
        int day;           // Qt::DayOfWeek the session opens
        QTime open;
        int closeDays;     // the session closes that many days later (0: the same day)
        QTime close;
    };
    QString name;
    QString country;       // ISO 3166 alpha-2
    QByteArray tz;         // IANA time zone
    QString symbol;        // a Yahoo index of the exchange, to catch holidays; empty for futures
    QVector<Session> sessions;
    QString hours;         // futures: the trading week as written by the exchange
    QString breakText;     // futures: the daily break
    bool futures = false;
};

// One market's card: name, open / closed / break, hours and the time to the next open or close.
class MarketCard : public QWidget
{
    Q_OBJECT
public:
    MarketCard(const MarketHours &market, const MarketSessions *sessions, QWidget *parent = nullptr);

    enum Status { Open, Break, Closed, Holiday };
    struct State {
        Status status = Closed;
        QDateTime next;    // UTC: the close when open, else the next open
        QDateTime sessionStart, sessionEnd;   // the current or next session, UTC
    };
    State state(const QDateTime &nowUtc) const;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    struct Interval {
        QDateTime start, end;   // UTC
    };
    // The sessions from three days back to ten ahead, today's dropped on a holiday: rebuilt when the
    // market's date or the sessions data change (each card is asked twice a second).
    const QVector<Interval> &intervals(const QDate &today, bool *holiday) const;

    MarketHours m_market;
    const MarketSessions *m_sessions;
    QTimeZone m_tz;
    mutable QVector<Interval> m_intervals;
    mutable QDate m_intervalsDay;
    mutable QDateTime m_intervalsFetched;
    mutable bool m_intervalsHoliday = false;
};

// Cards in as many columns as fit.
class CardGrid : public QWidget
{
    Q_OBJECT
public:
    explicit CardGrid(int cardHeight, QWidget *parent = nullptr);
    void addCard(QWidget *card);
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override { return QSize(800, heightForWidth(800)); }

protected:
    void resizeEvent(QResizeEvent *) override;

private:
    int columns(int w) const;
    QVector<QWidget *> m_cards;
    int m_cardHeight;
};

// Market hours tab: every exchange's state, live, in sections.
class MarketHoursView : public QWidget
{
    Q_OBJECT
public:
    explicit MarketHoursView(MarketSessions *sessions, QWidget *parent = nullptr);

    static QVector<MarketHours> stockExchanges();
    static QVector<MarketHours> futuresMarkets();
    static QStringList sessionSymbols();

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    void tick();

    MarketSessions *m_sessions;
    QLabel *m_clock;
    QLabel *m_stockCount;
    QLabel *m_futuresCount;
    QVector<MarketCard *> m_stockCards, m_futuresCards;
    QTimer m_timer;
    int m_ticks = 0;
};

// The Calendar page: a sub-header with the Economic, Corporate and Market hours tabs.
class CalendarPage : public QWidget
{
    Q_OBJECT
public:
    CalendarPage(EconomicCalendar *economic, CorporateCalendar *corporate, MarketSessions *sessions,
                 QWidget *parent = nullptr);

private:
    void showTab(int index);

    QStackedWidget *m_stack;
    QButtonGroup *m_tabs;
};
