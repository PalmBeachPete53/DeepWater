#pragma once

#include <QHash>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

namespace countries {
class IndicatorLoader;
struct ListItem;
}

// One indicator for every country: a title bar (back to the list, the indicator's name, the loading state, CSV
// export, refresh), the regions (All, G20, America, Europe, Africa, Asia, Oceania) and a table sorted by any
// column: country, last, previous, reference (the period of the last value), unit and next release, that column
// shown when the economic calendar has releases of the indicator. Countries no free source covers are left out.
class IndicatorPage : public QWidget
{
    Q_OBJECT
public:
    explicit IndicatorPage(const countries::ListItem &item, QWidget *parent = nullptr);
    QString name() const { return m_name; }   // translated

signals:
    void back();

private:
    void updateRow(const QString &iso3);
    void updateReleases();
    void updateStatus();
    void filterRegion(int index);
    void applyHidden();
    void exportCsv();

    QString m_name;
    countries::IndicatorLoader *m_loader;
    QTableWidget *m_table;
    QHash<QString, QTableWidgetItem *> m_rows;   // the country cell of each row (rows move when sorted)
    QButtonGroup *m_regions;
    QLabel *m_status;
    QLabel *m_note;
    QPushButton *m_refresh;
};

// Economy › Indicators: the indicators by section (Main Indicators, Markets, Labour, Prices…) with a search box;
// clicking one opens its IndicatorPage. Those without a free source are greyed, the reason in their tooltip.
class IndicatorsView : public QWidget
{
    Q_OBJECT
public:
    explicit IndicatorsView(QWidget *parent = nullptr);
    // By its English name (any case); false when there is none such.
    bool openIndicator(const QString &name);
    void showList();
    QString aiContext() const;   // the indicator open, if any

private:
    void filter(const QString &text);

    QStackedWidget *m_stack;
    QLineEdit *m_search;
    struct Item {
        QPushButton *button;
        QString name, english;
        QWidget *section;
    };
    QVector<Item> m_items;
    QVector<QWidget *> m_sections;
    QPointer<IndicatorPage> m_page;
};
