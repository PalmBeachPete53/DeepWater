#pragma once

#include <QWidget>

class MarketData;
class QLabel;
class VesselStore;
class QVBoxLayout;
class MarketRow;

// Left sidebar home: market overview and top movers, layer toggles and the
// market watchlist. Details of a selection are shown over it (DetailPanel).
class LeftPanel : public QWidget
{
    Q_OBJECT
public:
    LeftPanel(MarketData *data, VesselStore *vessels, QWidget *parent = nullptr);

    void setActiveCommodity(const QString &id);

signals:
    void commoditySelected(const QString &id);

private:
    void refreshPrices();
    void refreshMovers();

    MarketData *m_data;
    void refreshVesselCounts();

    VesselStore *m_vessels;
    QVector<MarketRow *> m_rows;
    QVector<MarketRow *> m_movers;
    QString m_activeCommodity;
    QLabel *m_tankerCount;
    QLabel *m_cargoCount;
    QLabel *m_unknownCount;
};

// A single clickable watchlist row: name, sparkline, price and daily change.
class MarketRow : public QWidget
{
    Q_OBJECT
public:
    MarketRow(MarketData *data, const QString &commodityId, QWidget *parent = nullptr);

    QString commodityId() const { return m_id; }
    void setCommodity(const QString &id);
    void setActive(bool active);
    void refresh();

signals:
    void clicked(const QString &id);

protected:
    bool event(QEvent *) override;   // the tooltip: where the price comes from
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    MarketData *m_data;
    QString m_id;
    bool m_active = false;
    bool m_hover = false;
};
