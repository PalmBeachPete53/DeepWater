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
class QToolButton;

namespace countries {
class CountryLoader;
}

// One country's indicators: a title bar (back to the list, the country's name, the loading state, CSV export,
// refresh), the tabs (Overview, GDP, Labour, Prices, Money, Trade, Government, Business, Consumer, Housing,
// Energy, Health) and their tables: last, previous, highest, lowest, unit, period and source of each indicator
// a free source publishes for the country. A tab is built and loaded the first time it is shown.
class CountryPage : public QWidget
{
    Q_OBJECT
public:
    explicit CountryPage(const QString &iso3, QWidget *parent = nullptr);
    void showTab(int index);
    // "France (FRA), Prices": the AI side panel's context.
    QString aiContext() const;

signals:
    void back();

private:
    struct Table {
        QTableWidget *widget = nullptr;
        QStringList ids;
    };
    struct Tab {
        QWidget *page = nullptr;
        QVector<Table> tables;
        QLabel *note = nullptr;
    };
    QWidget *buildTab(int index);
    void updateRow(const QString &id);
    void updateTab(int index);
    void fitTable(QTableWidget *table);
    void updateStatus();
    void exportCsv();

    QString m_iso3;
    countries::CountryLoader *m_loader;
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
    QLabel *m_status;
    QPushButton *m_refresh;
    QVector<Tab> m_built;
};

// Economy › Countries: the countries by group (G20, America, Europe, Africa, Asia, Oceania) with a search box;
// clicking one opens its CountryPage, on the tab shown last.
class CountriesView : public QWidget
{
    Q_OBJECT
public:
    explicit CountriesView(QWidget *parent = nullptr);
    void openCountry(const QString &iso3, int tab = -1);
    void showList();
    QString aiContext() const;   // the country open, if any

private:
    void filter(const QString &text);

    QStackedWidget *m_stack;
    QLineEdit *m_search;
    struct Item {
        QPushButton *button;
        QString name;
        QWidget *group;
    };
    QVector<Item> m_items;
    QVector<QWidget *> m_groups;
    QPointer<CountryPage> m_page;
};
