#pragma once

#include "ResearchData.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QTimer>
#include <QWidget>

class QButtonGroup;
class QVBoxLayout;
class QLabel;
class QPushButton;

namespace overview {
class Panel;
class PerfChart;
class StatsView;
class Table;
}

// The Overview page: a snapshot of the markets, refreshed every minute while it is shown. The performance of
// SPY, QQQ, DIA and IWM over a range with their statistics; the S&P 500's sectors (SPDR ETFs); US indices and
// Treasury yields; world indices, crypto, currencies and commodities; the largest US companies; the day's gainers,
// losers and most active stocks; the best and worst members of the S&P 500, the Nasdaq-100 or the Dow today.
// Prices: Yahoo Finance; yields: the US Treasury; the list of the largest companies: Nasdaq's screener.
class OverviewPage : public QWidget
{
    Q_OBJECT
public:
    explicit OverviewPage(QWidget *parent = nullptr);
    void setTopStrip(QWidget *strip);   // above the panels

signals:
    // A stock was clicked: open it in Research.
    void openResearch(const QString &symbol);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    QVBoxLayout *m_layout = nullptr;
    void refresh();
    void fetchChart();
    void fetchBoard();
    void fetchSectorHistory();
    void fetchBonds();
    void fetchLargest();
    void fetchMovers();
    void fetchIndexMembers(int index);
    void buildChart();
    void fillBoard();
    void fillSectors();
    void fillLargest();
    void fillIndexMovers();

    // Index performance.
    QButtonGroup *m_chartRange;
    overview::PerfChart *m_chart;
    overview::StatsView *m_stats;
    QHash<QString, QVector<research::Bar>> m_bars;
    int m_chartGeneration = 0;
    int m_chartPending = 0;
    QString m_chartError;

    // Quotes of the fixed lists (indices, world, crypto, FX, commodities, sector ETFs).
    QHash<QString, QJsonObject> m_board;
    QString m_boardError;
    overview::Table *m_usIndices, *m_global, *m_crypto, *m_fx, *m_commodities;

    // Sectors.
    QButtonGroup *m_sectorRange;
    overview::Table *m_sectors;
    QHash<QString, research::Series> m_sectorHistory;   // daily closes over a year

    // Bonds.
    overview::Panel *m_bondsPanel;
    overview::Table *m_bonds;

    // Largest companies: the 30 largest US listings by market cap, ten per page.
    overview::Table *m_largest;
    QLabel *m_largestPage;
    QPushButton *m_prevPage, *m_nextPage;
    QStringList m_largestSymbols;
    QHash<QString, QJsonObject> m_largestQuotes;
    int m_page = 0;

    // Movers.
    overview::Table *m_gainers, *m_losers, *m_active;

    // Top index gainers / losers.
    QButtonGroup *m_gainersIndex, *m_losersIndex;
    overview::Table *m_indexGainers, *m_indexLosers;
    research::Indexes m_indexes;
    QHash<int, QHash<QString, QJsonObject>> m_memberQuotes;   // by index: 0 S&P 500, 1 Nasdaq-100, 2 Dow
    QHash<int, QString> m_memberErrors;

    QTimer m_timer;
    QDateTime m_lastRefresh;
};
