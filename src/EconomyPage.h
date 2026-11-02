#pragma once

#include "EconomyData.h"

#include <QPixmap>
#include <QTimer>
#include <QWidget>

class CountriesView;
class QButtonGroup;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace economy {

// One indicator over time: an area under the line (or against zero), the last value and its change in the top
// right corner, the date of the last observation and the source under the axis, a crosshair and a tooltip on
// hover. 0…100 indexes can show their zones behind the line, the line then coloured by zone.
class ChartView : public QWidget
{
    Q_OBJECT
public:
    explicit ChartView(const Indicator &indicator, QWidget *parent = nullptr);
    void setSeries(const Series &series);
    void setRange(const QString &range);
    // Shown greyed in place of the chart while there is no data ("Loading…", an error).
    void setMessage(const QString &message);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QRectF plotRect() const;
    void paintLayer(QPainter &p, const QRectF &plot);
    double xOf(const QRectF &plot, const QDate &d) const;
    double yOf(const QRectF &plot, double v) const;
    void changed() { m_layer = QPixmap(); update(); }

    Indicator m_ind;
    Series m_all;
    int m_first = 0;           // first point of the range
    QString m_range;
    QString m_message;
    double m_lo = 0, m_hi = 1;
    QVector<double> m_ticks;
    QDate m_from, m_to;
    QPixmap m_layer;
    QString m_layerTheme;
    QPointF m_mouse;
    bool m_hover = false;
};

// A dial of a 0…100 index's last value, its class ("Extreme fear", "BTC leaning") and the date; optionally the
// components it is built from, as bars on the right.
class GaugeView : public QWidget
{
    Q_OBJECT
public:
    explicit GaugeView(const Indicator &indicator, QWidget *parent = nullptr);
    void setSeries(const Series &series);
    void setComponents(const QVector<FearGreedComponent> &components);
    void setMessage(const QString &message);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Indicator m_ind;
    Series m_series;
    QVector<FearGreedComponent> m_components;
    QString m_message;
};

// Formats a value of the indicator's unit: "4.40%", "$31.49T", "159.04M", "¥153.50"; `axis` for tick labels,
// with `step` the distance between ticks.
QString formatValue(const Indicator &indicator, double value, double step = 0);

} // namespace economy

// The Economy page: a sub-header with its sections (Sentiment, Crypto, Growth, Inflation, Employment, Rates,
// Housing, Consumer, Liquidity, Global; then Countries, a CountriesView), each a grid of panels with one indicator
// each, and "Updating…" while
// downloads are under way. A section is built and loaded the first time it is shown; the section is remembered.
class EconomyPage : public QWidget
{
    Q_OBJECT
public:
    explicit EconomyPage(QWidget *parent = nullptr);
    void showSection(int index);
    // --economy-country: the Countries section on one country's page (tab -1: the last one shown).
    void openCountry(const QString &iso3, int tab = -1);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    QWidget *buildSection(int index);
    int countriesSection() const { return int(economy::sections().size()); }
    QWidget *buildCell(int section, const economy::Cell &cell);
    void reload(bool force);
    void started();
    void finished(const QString &error);
    void updateStatus();

    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
    QLabel *m_status;
    QPushButton *m_refresh;
    QVector<QWidget *> m_built;                          // per section, null until shown
    QVector<QVector<std::function<void()>>> m_loaders;   // per section: each panel's load
    int m_pending = 0;
    int m_errors = 0;
    QString m_lastError;
    QTimer m_timer;
    CountriesView *m_countries = nullptr;
};
