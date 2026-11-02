#pragma once

#include "Chokepoints.h"
#include "PortWatch.h"

#include <QWidget>

class MarketData;
class QLabel;
class QPainter;

// Stacked hourly bar chart of passages (tankers at the bottom, cargo on top).
// Hours without recorded data are shaded so they don't read as "no traffic".
void drawHourlyChart(QPainter &p, const QRectF &rect, const QVector<ChokepointMonitor::Counts> &hours,
                     const QVector<bool> &recorded, const QColor &tankerColor, const QColor &cargoColor);

// Stacked daily bars from IMF PortWatch: tankers, dry bulk, then other types.
void drawDailyChart(QPainter &p, const QRectF &rect, const QVector<PortWatch::Day> &days,
                    const QColor &tankerColor, const QColor &bulkColor, const QColor &otherColor);

// Text summary of the latest PortWatch day and the 7-day vs 90-day trend.
struct PortWatchSummary {
    bool valid = false;
    PortWatch::Day latest;
    double avg7 = 0;
    double avg90 = 0;
    double changePct = 0;
};
PortWatchSummary summarize(const PortWatch *portWatch, const QString &portId);

class DailyChart : public QWidget
{
    Q_OBJECT
public:
    DailyChart(MarketData *data, QWidget *parent = nullptr);
    void setDays(const QVector<PortWatch::Day> &days);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    MarketData *m_data;
    QVector<PortWatch::Day> m_days;
};

class HourlyChart : public QWidget
{
    Q_OBJECT
public:
    HourlyChart(MarketData *data, QWidget *parent = nullptr);
    void setHours(const QVector<ChokepointMonitor::Counts> &hours, const QVector<bool> &recorded);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    MarketData *m_data;
    QVector<ChokepointMonitor::Counts> m_hours;
    QVector<bool> m_recorded;
};

// One clickable card per chokepoint: live count, 48 h passages and hourly chart.
class ChokepointCard : public QWidget
{
    Q_OBJECT
public:
    ChokepointCard(ChokepointMonitor *monitor, PortWatch *portWatch, MarketData *data, int zone,
                   QWidget *parent = nullptr);

signals:
    void clicked(int zone);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    ChokepointMonitor *m_monitor;
    PortWatch *m_portWatch;
    MarketData *m_data;
    int m_zone;
    bool m_hover = false;
};

// Header page listing every monitored chokepoint.
class ChokepointsPage : public QWidget
{
    Q_OBJECT
public:
    ChokepointsPage(ChokepointMonitor *monitor, PortWatch *portWatch, VesselStore *vessels, MarketData *data,
                    QWidget *parent = nullptr);

signals:
    void chokepointActivated(int zone);

private:
    void refresh();
    void updateSourceLabel();

    ChokepointMonitor *m_monitor;
    PortWatch *m_portWatch;
    QLabel *m_sourceLabel;
    QVector<ChokepointCard *> m_cards;
};
