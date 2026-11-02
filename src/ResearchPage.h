#pragma once

#include <QTimer>
#include <QWidget>

class QButtonGroup;
class QCompleter;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QStandardItemModel;

namespace research { class View; }
namespace quant { class MarketStateStrip; }

// The Research page: a symbol picker under the main header, a sub-header with the sections (key metrics,
// financials, financial charts, ownership, ratings, compare, compare charts, sectors, ICS, SPLC) and the
// section shown. The symbol and the section are remembered.
class ResearchPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResearchPage(QWidget *parent = nullptr);

    void setSymbol(const QString &symbol);
    void showSection(int index);
    void searchIcs(const QString &text);   // shows the ICS section and fills its search box

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    void search(const QString &text);
    void commit();
    void refreshQuote();

    QLineEdit *m_edit;
    QCompleter *m_completer;
    QStandardItemModel *m_hits;
    QLabel *m_quote;
    quant::MarketStateStrip *m_state;   // the regime and volatility from Quant › Markov
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
    QList<research::View *> m_views;
    QString m_symbol;
    QTimer m_searchDelay;
    QTimer m_quoteTimer;
    QString m_firstHit;   // the best match of the last search, for Enter on a company name
};
