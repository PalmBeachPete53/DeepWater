#pragma once

#include "NewsData.h"

#include <QTimer>
#include <QWidget>

class QComboBox;

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;

namespace newsui {

class Reader;

// One headline: title, summary, date · age · site, the ticker as a badge, a star (the favorites) in the top right
// corner. A click opens the article in the reader; a middle click, or Cmd / Ctrl + click, in the browser; the star
// toggles the favorite.
class Card : public QWidget
{
    Q_OBJECT
public:
    explicit Card(QWidget *parent = nullptr);
    void setItem(const news::Item &item);
    const news::Item &item() const { return m_item; }
    // Compact: the column view's one-line title and two-line summary.
    void setCompact(bool compact);
    static int heightFor(bool compact) { return compact ? 80 : 118; }

signals:
    void activated(const news::Item &item, bool inBrowser);
    void starToggled(const news::Item &item);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRect starRect() const;

    news::Item m_item;
    bool m_hover = false;
    bool m_starHover = false;
    bool m_compact = false;
    int m_impact = 0;   // desk::NewsImpact of the headline, read once
};

// The cards, newest first: in as many columns as the width allows (420 px each), or one per line (the column
// view, with compact cards).
class Grid : public QWidget
{
    Q_OBJECT
public:
    explicit Grid(QWidget *parent = nullptr);
    void setItems(const QVector<news::Item> &items);
    void setColumnView(bool column);
    bool columnView() const { return m_column; }
    // Shown greyed in place of the cards while there are none ("Loading…", an error).
    void setMessage(const QString &message);
    // The ages ("22m ago") move on: repaint.
    void tick();
    int count() const { return m_cards.size(); }
    const news::Item *itemAt(int index) const;

signals:
    void activated(const news::Item &item, bool inBrowser);
    void starToggled(const news::Item &item);

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    void relayout();

    QVector<Card *> m_cards;
    QString m_message;
    int m_columns = 0;
    bool m_column = false;
};

} // namespace newsui

// The News page: a sub-header with the categories (general, stocks, crypto, forex, and the favorites) and a ticker
// filter, the "News wire" strip with its refresh button, and the headlines as cards; a card opens the article in a
// floating reader over the page, its star keeps it in the favorites. Refreshed every five minutes while shown; the
// category is remembered.
class NewsPage : public QWidget
{
    Q_OBJECT
public:
    explicit NewsPage(QWidget *parent = nullptr);

    void showCategory(int index);
    void setTicker(const QString &ticker);
    // 0: grid, 1: column; remembered.
    void setViewMode(int mode);
    // Opens the reader on the index-th article (once the list is loaded): the --news-article dev flag.
    void openArticle(int index);
    // Stars the index-th article (once the list is loaded): the --news-favorite dev flag.
    void toggleFavorite(int index);
    // Opens the reader on any URL: the --news-url dev flag (to see how a site renders or what headers go out).
    void openUrl(const QString &url);
    static constexpr int FavoritesTab = 4;   // the tab after the four categories

signals:
    void askAi(const QString &prompt);   // the article being read, for the AI side panel

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void refresh(bool force);
    void fill(const QVector<news::Item> &items, const QString &provider, const QString &error);
    void applyTags();   // the impact and topic filters, over the last list received

    QButtonGroup *m_tabs;
    QLineEdit *m_filter;
    QComboBox *m_impact;
    QComboBox *m_topic;
    QVector<news::Item> m_received;
    QString m_provider, m_error;
    QLabel *m_note;
    QButtonGroup *m_views;
    QPushButton *m_refresh;
    QScrollArea *m_scroll;
    newsui::Grid *m_grid;
    newsui::Reader *m_reader;
    int m_pendingArticle = -1;
    int m_pendingFavorite = -1;
    QTimer m_timer;         // the periodic refresh
    QTimer m_tick;          // the ages
    QTimer m_debounce;      // typing in the filter
    int m_generation = 0;
    QDateTime m_lastRefresh;
};
