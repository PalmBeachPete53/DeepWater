#pragma once

#include "NewsData.h"

#include <QWidget>

class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class QToolButton;

namespace newsui {

class WebHost;

// The article reader: a floating panel over the News page with the article's web page embedded (a WebHost: the
// system's WebKit on macOS, Qt WebEngine elsewhere), its title and site, back, the favorites star, "open in the
// browser" and close. The page behind is dimmed; a click on it, the × or Escape closes the reader. The web view
// is created the first time an article is opened and kept; closing loads a blank page so nothing keeps playing.
// The shield in the header shows the ad blocker's state (and, when the engine reports it, what it blocked); a
// notice appears when the site answers an HTTP error to the embedded view.
class Reader : public QWidget
{
    Q_OBJECT
public:
    explicit Reader(QWidget *parent);   // covers its parent (the page)

    void open(const news::Item &item);
    void closeReader();
    bool isOpen() const { return isVisible(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void placePanel();
    void updateMeta();
    void updateShield();
    void updateStar();

    news::Item m_item;
    QFrame *m_panel;
    QLabel *m_title;
    QLabel *m_meta;
    QToolButton *m_back;
    QToolButton *m_star;
    QPushButton *m_browser;
    QToolButton *m_close;
    QProgressBar *m_progress;
    QLabel *m_notice;                 // the site refused the embedded view (HTTP 403…)
    QLabel *m_shield;
    QWidget *m_body;                  // the web view's slot
    WebHost *m_host = nullptr;
};

} // namespace newsui
