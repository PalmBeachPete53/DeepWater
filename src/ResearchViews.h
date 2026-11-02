#pragma once

#include "ResearchCharts.h"
#include "ResearchData.h"

#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTableWidget;
class QTreeWidget;
class QVBoxLayout;

namespace research {

// --- Small building blocks shared by the sections.

QFont mono(int px = 12, bool bold = false);
// A framed block with an orange title.
QFrame *panel(const QString &title, QVBoxLayout **body, QHBoxLayout **head = nullptr);
// Segmented buttons ("ANNUAL | QUARTERLY"): the group's ids are the indexes; the first is checked.
QWidget *segments(const QStringList &labels, QButtonGroup **group, int checked = 0);
QTableWidget *table(const QStringList &headers);
// A "Label ........ value" list in a card.
class StatList : public QWidget
{
    Q_OBJECT
public:
    explicit StatList(QWidget *parent = nullptr);
    void clear();
    void add(const QString &label, const QString &value, const QColor &color = QColor(), const QString &tip = {});
    // Refilling instead of clearing: begin(), add() each row (the existing ones are reused, in order), end()
    // hides the rows left over. Much cheaper than new labels in this stylesheet-heavy app, and rows are
    // never hidden and shown again (showing a widget lays its parent out at once).
    void begin() { m_rows = 0; }
    void end();

private:
    QGridLayout *m_grid;
    QVector<QPair<QLabel *, QLabel *>> m_pairs;
    QVector<QColor> m_colors;   // each value's colour as last styled
    int m_rows = 0;
};
// Cards laid out in as many columns as fit, rows as tall as their tallest card.
class FlowGrid : public QWidget
{
    Q_OBJECT
public:
    explicit FlowGrid(int minColumnWidth, QWidget *parent = nullptr);
    void addWidget(QWidget *w);
    void clear();

protected:
    void resizeEvent(QResizeEvent *) override;

private:
    void relayout(bool force);
    QGridLayout *m_grid;
    QVector<QWidget *> m_items;
    int m_minWidth;
    int m_columns = 0;
};
QColor changeColor(double v);
// The small "EXPERIMENTAL" tag on Deepwater's own estimates (not validated yet); `why` goes in its tooltip.
QLabel *experimentalTag(const QString &why);
QString experimentalReason();   // the common part of those tooltips

// A section of the Research page. It loads the current symbol when shown (and again when the symbol
// changes while it is shown).
class View : public QWidget
{
    Q_OBJECT
public:
    explicit View(QWidget *parent = nullptr);
    void setSymbol(const QString &symbol);
    QString symbol() const { return m_symbol; }

signals:
    void symbolRequested(const QString &symbol);

protected:
    void showEvent(QShowEvent *) override;
    // Fills the view for m_symbol. Callbacks should check that the symbol hasn't changed since.
    virtual void load() = 0;
    // Called when the symbol changes, before load(): only views that depend on it reload.
    virtual bool dependsOnSymbol() const { return true; }
    void setStatus(const QString &text);   // a line at the top, hidden when empty
    QVBoxLayout *content() const { return m_content; }
    QScrollArea *scroll() const { return m_scroll; }

    QString m_symbol;

private:
    bool m_dirty = true;
    QLabel *m_status;
    QVBoxLayout *m_content;
    QScrollArea *m_scroll;
};

class KeyMetricsView : public View
{
    Q_OBJECT
public:
    explicit KeyMetricsView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fill();
    // The seven answers of a symbol mostly arrive in the same event loop pass (cached): one fill for them.
    void scheduleFill();
    QJsonObject m_summary;
    Fundamentals m_fund;
    Statement m_balance, m_income;   // yearly
    Statement m_balanceQ;            // quarterly: the latest balance sheet
    Series m_employees;
    QStringList m_indexes;
    QWidget *m_header = nullptr;
    QLabel *m_name, *m_ticker, *m_price, *m_change, *m_initial, *m_description;
    QPushButton *m_more;
    QHBoxLayout *m_tags, *m_links;
    StatList *m_general;
    LineChart *m_employeeChart;
    QLabel *m_employeeTitle;
    DonutChart *m_overview;
    StatList *m_overviewLegend;
    FlowGrid *m_cards;
    // The cards, kept and refilled by each fill (the same ones, in the same order); the unused are hidden.
    QVector<QPair<QFrame *, StatList *>> m_cardPool;
    bool m_expanded = false;
    bool m_fillPending = false;
};

class FinancialsView : public View
{
    Q_OBJECT
public:
    explicit FinancialsView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fill(const Statement &s);
    QButtonGroup *m_kind, *m_period;
    QTableWidget *m_table;
};

class FinancialChartsView : public View
{
    Q_OBJECT
public:
    explicit FinancialChartsView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fill();
    Fundamentals m_fund;
    QButtonGroup *m_period, *m_range;
    BarChart *m_income, *m_margin, *m_eps, *m_cash, *m_balance;
    QLabel *m_source;
};

class OwnershipView : public View
{
    Q_OBJECT
public:
    explicit OwnershipView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fillBreakdown(const QJsonObject &summary);
    void fillHolders(const Holders &h, double sharesOut);
    void fillInsiders(const QVector<InsiderTrade> &trades);
    ProportionBar *m_bar;
    QLabel *m_parts[3];
    QLabel *m_partShares[3];
    QLabel *m_holdersNote;
    QTableWidget *m_holders;
    QWidget *m_volume;
    QLabel *m_tradesNote;
    QTableWidget *m_trades;
    double m_sharesOut = NaN;
    double m_price = NaN;
};

class RatingsView : public View
{
    Q_OBJECT
public:
    explicit RatingsView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fill();
    void fillHistory();
    QJsonObject m_summary;
    Series m_prices;
    QLabel *m_basedOn, *m_average, *m_valuation[2], *m_badge, *m_target, *m_grades[3], *m_quant;
    Gauge *m_gauge;
    HBars *m_bars;
    RatingScale *m_scale;
    QLabel *m_targets[3];
    LineChart *m_forecast;
    QButtonGroup *m_historyRange;
    StackedBarChart *m_history;
    QLabel *m_historyNote;
    QTableWidget *m_changes;
};

class CompareView : public View
{
    Q_OBJECT
public:
    explicit CompareView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void rebuildSymbols();
    void fillTable();
    QButtonGroup *m_mode, *m_tab;
    QComboBox *m_sector, *m_industry;
    QWidget *m_experimental;   // the tag naming the experimental columns (General tab)
    QTableWidget *m_table;
    QVector<Listing> m_listings;
    QStringList m_peers;
    QStringList m_symbols;
    QHash<QString, QJsonObject> m_summaries;
    int m_generation = 0;
    bool m_fillPending = false;   // a table fill is queued (summaries arrive one by one)
};

class CompareChartsView : public View
{
    Q_OBJECT
public:
    explicit CompareChartsView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    void fetch();
    void fill();
    QButtonGroup *m_period, *m_mode;
    QComboBox *m_metric;
    QLineEdit *m_a, *m_b;
    LineChart *m_chart;
    QLabel *m_source;
    QHash<QString, Fundamentals> m_fund;
};

class SectorsView : public View
{
    Q_OBJECT
public:
    explicit SectorsView(QWidget *parent = nullptr);

protected:
    void load() override;
    bool dependsOnSymbol() const override { return false; }

private:
    void fill();
    // Members, listings and two rounds of prices arrive apart: one fill per event loop pass.
    void scheduleFill();
    bool m_fillPending = false;
    QButtonGroup *m_index, *m_period;
    QLabel *m_kicker;
    DonutChart *m_donut;
    QTreeWidget *m_tree;
    QLabel *m_note;
    Indexes m_indexes;
    QHash<QString, Listing> m_listings;
    QHash<QString, Series> m_history;   // a year of daily closes
    QHash<QString, Series> m_recent;    // five days: the last price and the previous close
    bool m_haveIndexes = false, m_haveListings = false, m_haveHistory = false, m_haveRecent = false;
};

class IcsView : public View
{
    Q_OBJECT
public:
    explicit IcsView(QWidget *parent = nullptr);
    void setSearch(const QString &text);   // the industry / company search box (--search on the Research page)

protected:
    void load() override;
    bool dependsOnSymbol() const override { return false; }

private:
    void buildTree();
    void showSelection();
    QLineEdit *m_search;
    QTreeWidget *m_tree;
    QLabel *m_filter;
    QWidget *m_overview;
    QLabel *m_totals[3];
    QWidget *m_detail;
    StatList *m_aggregate;
    QTableWidget *m_members;
    QLabel *m_hint;
    QVector<Listing> m_listings;
};

class SplcView : public View
{
    Q_OBJECT
public:
    explicit SplcView(QWidget *parent = nullptr);

protected:
    void load() override;

private:
    SupplyChainGraph *m_graph;
    QLabel *m_note;
};

} // namespace research
