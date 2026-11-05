#include "NewsReader.h"

#include "NewsWebHost.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

namespace newsui {

namespace {
constexpr int Margin = 28;          // around the panel
constexpr int MaxWidth = 1320;
constexpr int MaxHeight = 980;
}

Reader::Reader(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("newsReaderOverlay");
    setAttribute(Qt::WA_StyledBackground, false);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    m_panel = new QFrame(this);
    m_panel->setObjectName("newsReader");
    m_panel->setAttribute(Qt::WA_StyledBackground);
    auto *v = new QVBoxLayout(m_panel);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *head = new QWidget;
    head->setObjectName("newsReaderHead");
    head->setAttribute(Qt::WA_StyledBackground);
    head->setFixedHeight(48);
    auto *h = new QHBoxLayout(head);
    h->setContentsMargins(10, 0, 8, 0);
    h->setSpacing(8);
    m_back = new QToolButton;
    m_back->setObjectName("newsReaderButton");
    m_back->setText("←");
    m_back->setToolTip(tr("Back"));
    m_back->setCursor(Qt::PointingHandCursor);
    m_back->setEnabled(false);
    h->addWidget(m_back);
    auto *titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(1);
    m_title = new QLabel;
    m_title->setObjectName("newsReaderTitle");
    m_title->setTextFormat(Qt::PlainText);
    m_meta = new QLabel;
    m_meta->setObjectName("newsReaderMeta");
    m_meta->setTextFormat(Qt::PlainText);
    titles->addWidget(m_title);
    titles->addWidget(m_meta);
    h->addLayout(titles, 1);
    m_shield = new QLabel;
    m_shield->setObjectName("newsReaderShield");
    m_shield->setTextFormat(Qt::PlainText);
    h->addWidget(m_shield);
    m_star = new QToolButton;
    m_star->setObjectName("newsReaderButton");
    m_star->setCheckable(true);
    m_star->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_star);
    m_ask = new QPushButton(tr("Ask the AI").toUpper());
    m_ask->setObjectName("newsRefresh");
    m_ask->setCursor(Qt::PointingHandCursor);
    m_ask->setToolTip(tr("Sends this article's headline and its feed summary to the local model in the side panel; the page itself stays here"));
    h->addWidget(m_ask);
    m_browser = new QPushButton(tr("Open in the browser").toUpper());
    m_browser->setObjectName("newsRefresh");
    m_browser->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_browser);
    m_close = new QToolButton;
    m_close->setObjectName("newsReaderButton");
    m_close->setText("✕");
    m_close->setToolTip(tr("Close (Esc)"));
    m_close->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_close);
    v->addWidget(head);

    m_progress = new QProgressBar;
    m_progress->setObjectName("newsReaderProgress");
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(2);
    v->addWidget(m_progress);

    // Shown when the site answers an HTTP error to the embedded view.
    m_notice = new QLabel;
    m_notice->setObjectName("newsReaderNotice");
    m_notice->setWordWrap(true);
    m_notice->setTextFormat(Qt::PlainText);
    m_notice->hide();
    v->addWidget(m_notice);

    m_body = new QWidget;
    m_body->setObjectName("newsReaderBody");
    m_body->setAttribute(Qt::WA_StyledBackground);
    auto *bl = new QVBoxLayout(m_body);
    bl->setContentsMargins(0, 0, 0, 0);
    v->addWidget(m_body, 1);

    connect(m_close, &QToolButton::clicked, this, &Reader::closeReader);
    connect(m_ask, &QPushButton::clicked, this, [this] {
        // Words of an article, not a table or a chart: they can go in the question.
        const QString summary = m_item.summary.trimmed();
        emit askAi(tr("Explain what this article means for the markets, in a few sentences. You only have its headline and its summary: say so if that isn't enough, and don't invent figures.\n\nHeadline: %1\nSource: %2").arg(m_item.title, m_item.site)
                   + (summary.isEmpty() ? QString() : tr("\nSummary: %1").arg(summary.left(1500))));
    });
    connect(m_browser, &QPushButton::clicked, this, [this] {
        const QUrl url = m_host && m_host->url().isValid() && m_host->url().scheme().startsWith("http") ? m_host->url() : QUrl(m_item.url);
        QDesktopServices::openUrl(url);
    });
    connect(m_back, &QToolButton::clicked, this, [this] { if (m_host) m_host->back(); });
    connect(m_star, &QToolButton::clicked, this, [this] { news::Favorites::instance()->toggle(m_item); });
    connect(news::Favorites::instance(), &news::Favorites::changed, this, &Reader::updateStar);
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), m_panel);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &Reader::closeReader);
}

void Reader::open(const news::Item &item)
{
    m_item = item;
    if (!m_host) {
        m_host = WebHost::create(m_body);
        m_body->layout()->addWidget(m_host);
        connect(m_host, &WebHost::loadStarted, this, [this] {
            m_progress->setValue(0);
            m_progress->show();
            m_notice->hide();
        });
        connect(m_host, &WebHost::loadProgress, m_progress, &QProgressBar::setValue);
        connect(m_host, &WebHost::loadFinished, this, [this] {
            m_progress->hide();
            m_back->setEnabled(m_host->canGoBack());
            updateMeta();
        });
        connect(m_host, &WebHost::urlChanged, this, [this] {
            m_back->setEnabled(m_host->canGoBack());
            updateMeta();
        });
        connect(m_host, &WebHost::httpError, this, [this](const QUrl &url, int code) {
            if (code != 401 && code != 403 && code != 429 && code != 451)
                return;
            QString host = url.host();
            if (host.startsWith("www.")) host.remove(0, 4);
            m_notice->setText(tr("%1 refuses the embedded view (HTTP %2): its bot protection does not accept this web engine. "
                                 "Use \"Open in the browser\".").arg(host).arg(code));
            m_notice->show();
        });
        connect(m_host, &WebHost::blockerChanged, this, &Reader::updateShield);
    }
    m_host->newPage();
    updateShield();
    updateStar();
    m_title->setText(item.title);
    m_title->setToolTip(item.title);
    updateMeta();
    m_back->setEnabled(false);
    m_progress->setValue(0);
    m_progress->show();
    m_notice->hide();
    setGeometry(parentWidget()->rect());
    placePanel();
    show();
    raise();
    m_host->load(QUrl(item.url));
    m_host->focusContent();
}

void Reader::closeReader()
{
    if (!isVisible())
        return;
    if (m_host)
        m_host->loadBlank();
    hide();
}

void Reader::updateMeta()
{
    QStringList parts;
    QString site = m_item.site;
    if (m_host && m_host->url().isValid() && m_host->url().scheme().startsWith("http")) {
        QString h = m_host->url().host();
        if (h.startsWith("www.")) h.remove(0, 4);
        if (!h.isEmpty()) site = h;
    }
    if (!site.isEmpty()) parts << site;
    if (m_item.published.isValid()) {
        parts << QLocale().toString(m_item.published.toLocalTime(), tr("MMM d, yyyy HH:mm"));
        parts << news::ago(m_item.published);
    }
    if (!m_item.symbol.isEmpty()) parts << m_item.symbol;
    m_meta->setText(parts.join("  ·  "));
}

void Reader::updateStar()
{
    const bool starred = news::Favorites::instance()->contains(m_item.url);
    m_star->setChecked(starred);
    m_star->setText(starred ? "★" : "☆");
    m_star->setToolTip(starred ? tr("Remove from favorites") : tr("Add to favorites"));
}

void Reader::updateShield()
{
    if (!m_host) {
        m_shield->hide();
        return;
    }
    switch (m_host->blockerState()) {
    case WebHost::BlockerState::Off:
        m_shield->hide();
        return;
    case WebHost::BlockerState::Preparing:
        m_shield->show();
        m_shield->setText(tr("filters loading…"));
        m_shield->setToolTip(tr("Downloading and compiling Brave's filter lists"));
        return;
    case WebHost::BlockerState::Active:
        break;
    }
    m_shield->show();
    const int n = m_host->blockedCount();
    m_shield->setText(n < 0 ? tr("filters active") : tr("%n blocked", nullptr, n));
    m_shield->setToolTip(n < 0 ? tr("Brave's filter lists are applied by %1 (ads, trackers, cookie notices)").arg(m_host->engineName())
                               : tr("Requests blocked on this page by Brave's filter lists (ads, trackers, cookie notices)"));
}

void Reader::placePanel()
{
    const int w = std::min(MaxWidth, width() - 2 * Margin);
    const int h = std::min(MaxHeight, height() - 2 * Margin);
    m_panel->setGeometry((width() - w) / 2, (height() - h) / 2, std::max(320, w), std::max(240, h));
    const QFontMetrics fm(m_title->font());
    m_title->setText(fm.elidedText(m_item.title, Qt::ElideRight, std::max(100, m_panel->width() - 380)));
}

void Reader::resizeEvent(QResizeEvent *)
{
    placePanel();
}

void Reader::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QColor dim = Theme::current().Background;
    dim.setAlpha(170);
    p.fillRect(rect(), dim);
}

void Reader::mousePressEvent(QMouseEvent *e)
{
    // Outside the panel: close.
    if (!m_panel->geometry().contains(e->pos()))
        closeReader();
}

} // namespace newsui
