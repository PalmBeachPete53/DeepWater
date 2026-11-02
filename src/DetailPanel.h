#pragma once

#include <QPointer>
#include <QPushButton>
#include <QWidget>

class AnnualChart;
class ChokepointMonitor;
class MacroIndicators;
class RankingList;
class QScrollArea;
class QSlider;
class PortWatch;
class HourlyChart;
class MarketData;
class VesselStore;
class QLabel;
class QVBoxLayout;
class Sparkline;
struct Commodity;

// Left sidebar details, shown over the home panel when something is selected:
// a cluster of sites, a site, a commodity, a vessel, a chokepoint, a macro
// indicator ranking or a country's economy.
class DetailPanel : public QWidget
{
    Q_OBJECT
public:
    DetailPanel(MarketData *data, VesselStore *vessels, ChokepointMonitor *chokepoints, PortWatch *portWatch,
                MacroIndicators *macro, QWidget *parent = nullptr);

    bool showingIndicator() const { return m_mode == Mode::Indicator; }
    // Indicator highlighted in country views (-1 for none).
    void setActiveIndicator(int id) { m_activeIndicator = id; }

public slots:
    void showSites(const QVector<int> &siteIndices);
    void showCommodity(const QString &commodityId);
    void showVessel(qint64 mmsi);
    void showChokepoint(int zone);
    void showIndicator(int id, int year);
    void showCountry(const QString &iso3);
    // The supply chain layer (DetailPanelSupply.cpp): its overview, and a point of it ("port:<id>", "oil:<id>",
    // "event:<id>").
    void showSupplyOverview();
    void showSupplyPoint(const QString &key);
    bool showingSupply() const { return m_mode == Mode::SupplyOverview || m_mode == Mode::SupplyPoint; }
    // Dev flags --scroll-end / --scroll <0..1>: scrolls the sheet (screenshots).
    void scrollTo(double fraction);
    // Re-renders an indicator or country view after new data arrived.
    void refreshMacro();
    // Route info for the vessel being shown (empty port = destination not resolved).
    void setVesselRoute(const QString &port, double remainingKm);

signals:
    void siteActivated(int siteIndex);
    void commodityActivated(const QString &commodityId);
    void vesselActivated(qint64 mmsi);
    void closeRequested();
    void countryActivated(const QString &iso3);
    void yearChanged(int year);
    void supplyPointActivated(const QString &key);

private:
    enum class Mode { None, Cluster, Site, Commodity, Vessel, Chokepoint, Indicator, Country, SupplyOverview, SupplyPoint };

    void clear();
    // `titleAccessory` (a help button…) sits just after the title's text.
    void addHeader(const QString &kicker, const QColor &kickerColor, const QString &title,
                   const QString &subtitle, bool closable = true, QWidget *titleAccessory = nullptr);
    void addQuoteCard(const QString &commodityId);
    void addSiteList(const QString &label, const QVector<int> &sites, int exclude = -1);
    // A country's primary, secondary and tertiary sectors: shares of GDP and of jobs.
    void addSectors(const QString &iso3);
    // The latest monthly and quarterly figures (Eurostat, OECD, IMF, BLS) and the bodies behind the country's
    // statistics (the catalogue of the sources named on tradingeconomics.com).
    void addLatestFigures(const QString &iso3);
    void addOfficialSources(const QString &iso3);
    // Exports and imports by product (UN Comtrade), downloaded when the sheet is shown.
    void addTradeByProduct(const QString &iso3);
    // Its real estate (house prices, their valuation) and its debt (households, companies, government).
    void addProperty(const QString &iso3);
    void addDebt(const QString &iso3);
    void refreshRanking();
    void refreshQuote();
    void refreshVessel();
    void refreshChokepoint();
    void addPortSheet(const QString &id);
    void addOilSiteSheet(const QString &id);
    void addDisruptionSheet(int id);
    // A row that opens a point of the supply chain layer.
    QPushButton *supplyLink(const QString &text, const QString &key);

    MarketData *m_data;
    VesselStore *m_vessels;
    ChokepointMonitor *m_chokepoints;
    PortWatch *m_portWatch;
    MacroIndicators *m_macro;
    int m_indicator = -1;
    int m_activeIndicator = -1;
    int m_year = 0;
    QString m_country;
    QPointer<RankingList> m_ranking;
    QPointer<QLabel> m_rankingSummary;
    QPointer<QLabel> m_yearLabel;
    QString m_region;   // ranking filter: a continent code, empty for every country; kept across indicators
    qint64 m_vesselMmsi = 0;
    QString m_routePort;
    double m_routeKm = 0;
    int m_zone = -1;
    QString m_supplyKey;   // the point shown, empty for the overview
    QPointer<QLabel> m_zoneStats;
    QPointer<HourlyChart> m_zoneChart;
    QPointer<QVBoxLayout> m_zoneList;
    QVector<qint64> m_zoneListShips;   // last list shown, to skip identical rebuilds
    // The list's title and buttons, kept and refilled when the list changes (once a second as ships
    // move), with the ship each button opens.
    QPointer<QLabel> m_zoneListTitle;
    QVector<QPointer<QPushButton>> m_zoneButtons;
    QVector<qint64> m_zoneButtonShips;
    QPointer<QLabel> m_vesselStats;
    QVBoxLayout *m_layout;
    QScrollArea *m_scroll = nullptr;
    Mode m_mode = Mode::None;

    // Live widgets updated on every price tick.
    QString m_quoteCommodity;
    QPointer<QLabel> m_priceLabel;
    QPointer<QLabel> m_changeLabel;
    QPointer<Sparkline> m_chart;
    QPointer<QLabel> m_statsLabel;
    QPointer<QLabel> m_unitLabel;
    QPointer<QLabel> m_sourceLabel;
};
