#pragma once

#include <QWidget>

class CotData;
class FxData;
class PolicyRates;
class QButtonGroup;
class QLabel;

// One card per currency pair: last fix, daily change and a 90-day sparkline.
class FxPairCard : public QWidget
{
    Q_OBJECT
public:
    FxPairCard(FxData *fx, PolicyRates *rates, int pair, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    FxData *m_fx;
    PolicyRates *m_rates;
    int m_pair;
};

// Central bank card: current policy rate, last move and 3-year step chart.
class PolicyRateCard : public QWidget
{
    Q_OBJECT
public:
    PolicyRateCard(PolicyRates *rates, int bank, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    PolicyRates *m_rates;
    int m_bank;
};

// CFTC positioning card for one currency: asset managers and leveraged funds,
// net positions as % of open interest, weekly change and 52-week history.
class PositioningCard : public QWidget
{
    Q_OBJECT
public:
    PositioningCard(CotData *cot, int contract, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    CotData *m_cot;
    int m_contract;
};

// Heatmap of pairwise correlations; hover highlights the row and column.
class CorrelationMatrix : public QWidget
{
    Q_OBJECT
public:
    explicit CorrelationMatrix(FxData *fx, QWidget *parent = nullptr);

    void setWindow(int returns);
    void reload() { recompute(); }   // after new data arrived
    int observations() const { return m_used; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF cellRect(int row, int col) const;
    void recompute();

    FxData *m_fx;
    int m_window = 63;
    int m_used = 0;
    QVector<QVector<double>> m_matrix;
    int m_hoverRow = -1;
    int m_hoverCol = -1;
};

class ForexPage : public QWidget
{
    Q_OBJECT
public:
    ForexPage(FxData *fx, PolicyRates *rates, CotData *cot, QWidget *parent = nullptr);

private:
    void refresh();

    FxData *m_fx;
    PolicyRates *m_rates;
    CotData *m_cot;
    QLabel *m_ratesSource;
    QLabel *m_flowsSource;
    QVector<QWidget *> m_repaint;   // cards to repaint when data arrives
    QLabel *m_source;
    QLabel *m_matrixNote;
    CorrelationMatrix *m_matrix;
    QVector<FxPairCard *> m_cards;
};
