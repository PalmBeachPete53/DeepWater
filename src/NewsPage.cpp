#include "NewsPage.h"

#include "AdBlock.h"
#include "NewsReader.h"
#include "QuantCharts.h"
#include "Theme.h"
#include "desk/NewsTags.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace newsui {

namespace {

constexpr int CardWidth = 420;   // the width a column is given at least
constexpr int Gap = 10;
constexpr int Margin = 8;

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

// Draws `text` wrapped in `width`, at most `maxLines` lines, the last one elided; returns the height used.
int drawWrapped(QPainter &p, const QRectF &rect, const QString &text, const QFont &font, const QColor &color, int maxLines)
{
    if (text.isEmpty())
        return 0;
    const QFontMetricsF fm(font);
    const qreal lineHeight = fm.height() + 1;
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    QVector<QTextLine> lines;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(rect.width());
        line.setPosition(QPointF(0, lines.size() * lineHeight));
        lines.push_back(line);
        if (lines.size() == maxLines) break;
    }
    layout.endLayout();
    p.setFont(font);
    p.setPen(color);
    for (int i = 0; i < lines.size(); ++i) {
        const QTextLine &line = lines[i];
        const bool last = i == lines.size() - 1;
        const bool truncated = last && line.textStart() + line.textLength() < text.size();
        const QPointF at(rect.left(), rect.top() + i * lineHeight + fm.ascent());
        if (truncated) {
            const QString rest = text.mid(line.textStart()).trimmed();
            p.drawText(at, fm.elidedText(rest, Qt::ElideRight, rect.width()));
        } else {
            p.drawText(at, text.mid(line.textStart(), line.textLength()).trimmed());
        }
    }
    return int(lines.size() * lineHeight);
}

} // namespace

// ---------------------------------------------------------------- Card

Card::Card(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(heightFor(false));
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
}

void Card::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    setFixedHeight(heightFor(compact));
    update();
}

QRect Card::starRect() const
{
    return QRect(width() - 12 - 18, 7, 18, 18);
}

namespace {

QPainterPath starPath(const QRectF &r)
{
    QPainterPath path;
    const QPointF c = r.center();
    const qreal outer = r.width() / 2, inner = outer * 0.45;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -M_PI / 2 + i * M_PI / 5;
        const qreal radius = i % 2 == 0 ? outer : inner;
        const QPointF pt(c.x() + radius * std::cos(angle), c.y() + radius * std::sin(angle));
        if (i == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    path.closeSubpath();
    return path;
}

} // namespace

void Card::setItem(const news::Item &item)
{
    m_item = item;
    m_impact = int(desk::tagHeadline(item.title).impact);
    QString tip = item.title;
    if (!item.summary.isEmpty()) tip += "\n\n" + item.summary;
    if (!item.url.isEmpty()) tip += "\n\n" + item.url;
    setToolTip(tip);
    update();
}

void Card::paintEvent(QPaintEvent *)
{
    const Palette &c = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(m_hover ? c.ControlBorderHover : c.PanelLine);
    p.setBrush(m_hover ? c.Hover : c.Panel);
    p.drawRoundedRect(r, 3, 3);

    const int pad = 12;
    const QRectF inner = rect().adjusted(pad, 9, -pad, -9);
    const QFont titleFont = quant::chartFont(13, true);
    const QFont bodyFont = quant::chartFont(10);
    const QFont footFont = quant::chartFont(10);
    const QFont badgeFont = quant::chartFont(9, true);

    // Footer first: it sits on the bottom line; the summary stops above it.
    const QFontMetricsF footFm(footFont);
    const qreal footY = inner.bottom() - footFm.descent();
    qreal x = inner.left();
    if (m_item.published.isValid()) {
        const QDateTime local = m_item.published.toLocalTime();
        const QString date = QLocale().toString(local, NewsPage::tr("MMM d, yyyy HH:mm"));
        p.setFont(footFont);
        p.setPen(c.TextDim);
        p.drawText(QPointF(x, footY), date);
        x += footFm.horizontalAdvance(date) + 8;
        QFont italic = footFont;
        italic.setItalic(true);
        const QString age = news::ago(m_item.published);
        p.setFont(italic);
        p.setPen(c.TextFaint);
        p.drawText(QPointF(x, footY), age);
        x += QFontMetricsF(italic).horizontalAdvance(age) + 8;
    }
    // The badge, right-aligned.
    qreal badgeLeft = inner.right();
    if (!m_item.symbol.isEmpty()) {
        const QFontMetricsF bm(badgeFont);
        const qreal w = bm.horizontalAdvance(m_item.symbol) + 14;
        const qreal h = bm.height() + 4;
        const QRectF badge(inner.right() - w, inner.bottom() - h + 1, w, h);
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(c.Accent, 46));
        p.drawRoundedRect(badge, 2, 2);
        p.setFont(badgeFont);
        p.setPen(c.Accent);
        p.drawText(badge, Qt::AlignCenter, m_item.symbol);
        badgeLeft = badge.left() - 10;
    }
    if (m_impact != int(desk::NewsImpact::Normal)) {
        const bool high = m_impact == int(desk::NewsImpact::High);
        const QString tag = high ? NewsPage::tr("HIGH IMPACT") : NewsPage::tr("IMPORTANT");
        const QFontMetricsF bm(badgeFont);
        const qreal w = bm.horizontalAdvance(tag) + 14;
        const qreal h = bm.height() + 4;
        const QRectF badge(badgeLeft - w, inner.bottom() - h + 1, w, h);
        const QColor color = high ? c.Down : c.TextDim;
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(color, 46));
        p.drawRoundedRect(badge, 2, 2);
        p.setFont(badgeFont);
        p.setPen(color);
        p.drawText(badge, Qt::AlignCenter, tag);
        badgeLeft = badge.left() - 10;
    }
    if (!m_item.site.isEmpty()) {
        p.setFont(footFont);
        p.setPen(c.TextFaint);
        const QString site = (m_item.published.isValid() ? "| " : "") + m_item.site;
        p.drawText(QPointF(x, footY), footFm.elidedText(site, Qt::ElideRight, std::max<qreal>(0, badgeLeft - x)));
    }

    // The star: filled in the accent colour when the article is a favorite, an outline otherwise.
    {
        const bool starred = news::Favorites::instance()->contains(m_item.url);
        const QRectF sr = QRectF(starRect()).adjusted(3, 3, -3, -3);
        p.setPen(QPen(starred ? c.Accent : m_starHover ? c.Text : c.TextFaint, 1.2));
        p.setBrush(starred ? QBrush(c.Accent) : m_starHover ? QBrush(withAlpha(c.Text, 40)) : Qt::NoBrush);
        p.drawPath(starPath(sr));
    }

    // Title (short of the star), then the summary in what is left above the footer.
    const QRectF titleRect(inner.left(), inner.top(), inner.width() - 26, inner.height());
    const int used = drawWrapped(p, titleRect, m_item.title, titleFont, c.Text, m_compact ? 1 : 2);
    const qreal bodyTop = inner.top() + used + 4;
    const qreal bodyBottom = inner.bottom() - footFm.height() - 4;
    const QFontMetricsF bodyFm(bodyFont);
    const int bodyLines = std::clamp(int((bodyBottom - bodyTop) / (bodyFm.height() + 1)), 0, m_compact ? 2 : 3);
    if (bodyLines > 0)
        drawWrapped(p, QRectF(inner.left(), bodyTop, inner.width(), bodyBottom - bodyTop), m_item.summary, bodyFont,
                    c.TextDim, bodyLines);
}

void Card::mousePressEvent(QMouseEvent *e)
{
    if (m_item.url.isEmpty())
        return;
    if (e->button() == Qt::LeftButton && starRect().contains(e->pos())) {
        emit starToggled(m_item);
        return;
    }
    if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier)))
        emit activated(m_item, true);
    else if (e->button() == Qt::LeftButton)
        emit activated(m_item, false);
}

void Card::mouseMoveEvent(QMouseEvent *e)
{
    const bool over = starRect().contains(e->pos());
    if (over != m_starHover) {
        m_starHover = over;
        setToolTip(over ? (news::Favorites::instance()->contains(m_item.url) ? tr("Remove from favorites") : tr("Add to favorites"))
                        : m_item.title + (m_item.summary.isEmpty() ? QString() : "\n\n" + m_item.summary)
                              + (m_item.url.isEmpty() ? QString() : "\n\n" + m_item.url));
        update();
    }
}

void Card::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void Card::leaveEvent(QEvent *)
{
    m_hover = false;
    m_starHover = false;
    update();
}

// ---------------------------------------------------------------- Grid

Grid::Grid(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
}

void Grid::setItems(const QVector<news::Item> &items)
{
    while (m_cards.size() > items.size()) {
        delete m_cards.takeLast();
    }
    while (m_cards.size() < items.size()) {
        auto *card = new Card(this);
        card->setCompact(m_column);
        connect(card, &Card::activated, this, &Grid::activated);
        connect(card, &Card::starToggled, this, &Grid::starToggled);
        card->show();
        m_cards.push_back(card);
    }
    for (int i = 0; i < items.size(); ++i)
        m_cards[i]->setItem(items[i]);
    m_columns = 0;   // force a layout
    relayout();
    update();
}

void Grid::setMessage(const QString &message)
{
    m_message = message;
    update();
}

const news::Item *Grid::itemAt(int index) const
{
    return index >= 0 && index < m_cards.size() ? &m_cards[index]->item() : nullptr;
}

void Grid::tick()
{
    for (Card *c : m_cards)
        c->update();
}

void Grid::resizeEvent(QResizeEvent *)
{
    relayout();
}

void Grid::setColumnView(bool column)
{
    if (m_column == column)
        return;
    m_column = column;
    for (Card *c : m_cards)
        c->setCompact(column);
    relayout();
}

void Grid::relayout()
{
    const int columns = m_column ? 1 : std::clamp((width() - 2 * Margin + Gap) / (CardWidth + Gap), 1, 6);
    const int cardWidth = (width() - 2 * Margin - (columns - 1) * Gap) / columns;
    const int rows = m_cards.isEmpty() ? 0 : (m_cards.size() + columns - 1) / columns;
    const int cardHeight = Card::heightFor(m_column);
    const int gap = m_column ? 6 : Gap;
    for (int i = 0; i < m_cards.size(); ++i) {
        const int row = i / columns, col = i % columns;
        m_cards[i]->setGeometry(Margin + col * (cardWidth + Gap), Margin + row * (cardHeight + gap), cardWidth, cardHeight);
    }
    m_columns = columns;
    const int h = rows ? 2 * Margin + rows * cardHeight + (rows - 1) * gap : 80;
    if (h != minimumHeight()) {
        setMinimumHeight(h);
        setMaximumHeight(h);
    }
}

void Grid::paintEvent(QPaintEvent *)
{
    if (!m_cards.isEmpty() || m_message.isEmpty())
        return;
    QPainter p(this);
    p.setFont(quant::chartFont(11));
    p.setPen(Theme::current().TextFaint);
    p.drawText(rect().adjusted(Margin + 8, Margin, -Margin, -Margin), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_message);
}

} // namespace newsui

// ---------------------------------------------------------------- NewsPage

namespace {

// The view buttons' icons, painted: four tiles for the grid, three bars for the column. Dim when off, accent
// when on.
QIcon viewIcon(bool grid)
{
    const Palette &c = Theme::current();
    QIcon icon;
    for (const auto &[color, state] : {std::pair {c.TextDim, QIcon::Off}, std::pair {c.Accent, QIcon::On}}) {
        QPixmap pm(32, 32);
        pm.setDevicePixelRatio(2);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        if (grid) {
            for (int i = 0; i < 4; ++i)
                p.drawRoundedRect(QRectF(2 + (i % 2) * 7, 2 + (i / 2) * 7, 5, 5), 1, 1);
        } else {
            for (int i = 0; i < 3; ++i)
                p.drawRoundedRect(QRectF(2, 2.5 + i * 4.5, 12, 2.5), 1, 1);
        }
        icon.addPixmap(pm, QIcon::Normal, state);
        icon.addPixmap(pm, QIcon::Active, state);
    }
    return icon;
}

} // namespace

NewsPage::NewsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("newsPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(quant::chartFont(12));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Categories and the ticker filter, right under the main header.
    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(36);
    auto *sh = new QHBoxLayout(sub);
    sh->setContentsMargins(6, 0, 12, 0);
    sh->setSpacing(2);
    m_tabs = new QButtonGroup(this);
    const QStringList names {tr("General"), tr("Stocks"), tr("Crypto"), tr("Forex"), tr("Favorites")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(names[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        sh->addWidget(b);
    }
    sh->addSpacing(14);
    m_filter = new QLineEdit;
    m_filter->setObjectName("newsFilter");
    m_filter->setPlaceholderText(tr("Filter ticker...").toUpper());
    m_filter->setClearButtonEnabled(true);
    m_filter->setFixedWidth(180);
    m_filter->setFixedHeight(22);
    sh->addWidget(m_filter);
    sh->addSpacing(14);
    const auto makeCombo = [&](const QString &tip) {
        auto *combo = new QComboBox;
        combo->setObjectName("resCombo");
        combo->setCursor(Qt::PointingHandCursor);
        combo->setFixedHeight(24);
        combo->setToolTip(tip);
        sh->addWidget(combo);
        return combo;
    };
    const QString tagsTip = tr("Read from the words of the headline, not its meaning: a guide, not a verdict");
    m_impact = makeCombo(tagsTip);
    m_impact->addItem(tr("All impacts"), 0);
    m_impact->addItem(tr("Important and high"), 1);
    m_impact->addItem(tr("High impact"), 2);
    sh->addSpacing(6);
    m_topic = makeCombo(tagsTip);
    m_topic->addItem(tr("All topics"), QString());
    m_topic->addItem(tr("Central banks"), "central_bank");
    m_topic->addItem(tr("Inflation"), "inflation");
    m_topic->addItem(tr("Employment"), "employment");
    m_topic->addItem(tr("GDP"), "gdp");
    m_topic->addItem(tr("PMI"), "pmi");
    m_topic->addItem(tr("Retail sales"), "retail");
    m_topic->addItem(tr("Trade"), "trade");
    m_topic->addItem(tr("Earnings"), "earnings");
    m_impact->setCurrentIndex(std::clamp(QSettings().value("news/impact", 0).toInt(), 0, m_impact->count() - 1));
    m_topic->setCurrentIndex(std::max(0, m_topic->findData(QSettings().value("news/topic").toString())));
    connect(m_impact, &QComboBox::currentIndexChanged, this, [this](int i) {
        QSettings().setValue("news/impact", i);
        applyTags();
    });
    connect(m_topic, &QComboBox::currentIndexChanged, this, [this] {
        QSettings().setValue("news/topic", m_topic->currentData().toString());
        applyTags();
    });
    sh->addStretch(1);
    v->addWidget(sub);

    // The strip: title, state, refresh.
    auto *bar = new QWidget;
    bar->setObjectName("newsBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(28);
    auto *bh = new QHBoxLayout(bar);
    bh->setContentsMargins(8, 0, 8, 0);
    bh->setSpacing(10);
    auto *title = new QLabel(tr("News wire").toUpper());
    title->setObjectName("newsTitle");
    bh->addWidget(title);
    m_note = new QLabel;
    m_note->setObjectName("newsNote");
    bh->addWidget(m_note, 1);
    // The view: grid or column.
    m_views = new QButtonGroup(this);
    m_views->setExclusive(true);
    const struct { bool grid; QString tip; } views[] {{true, tr("Grid")}, {false, tr("Column")}};
    for (int i = 0; i < 2; ++i) {
        auto *b = new QToolButton;
        b->setObjectName("newsViewButton");
        b->setIcon(viewIcon(views[i].grid));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(views[i].tip);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(22, 20);
        m_views->addButton(b, i);
        bh->addWidget(b);
    }
    bh->addSpacing(4);
    m_refresh = new QPushButton(tr("Refresh").toUpper());
    m_refresh->setObjectName("newsRefresh");
    m_refresh->setCursor(Qt::PointingHandCursor);
    bh->addWidget(m_refresh);
    v->addWidget(bar);

    m_scroll = new QScrollArea;
    m_scroll->setObjectName("newsScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName("newsContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    m_grid = new newsui::Grid;
    cl->addWidget(m_grid);
    cl->addStretch(1);
    m_scroll->setWidget(content);
    v->addWidget(m_scroll, 1);

    m_reader = new newsui::Reader(this);
    connect(m_reader, &newsui::Reader::askAi, this, &NewsPage::askAi);
    adblock::Blocker::instance()->start();   // the lists download while the headlines are read
    connect(m_grid, &newsui::Grid::starToggled, this, [](const news::Item &item) { news::Favorites::instance()->toggle(item); });
    connect(news::Favorites::instance(), &news::Favorites::changed, this, [this] {
        if (m_tabs->checkedId() == FavoritesTab)
            refresh(false);
        else
            m_grid->tick();
    });
    connect(m_grid, &newsui::Grid::activated, this, [this](const news::Item &item, bool inBrowser) {
        if (inBrowser)
            QDesktopServices::openUrl(QUrl(item.url));
        else
            m_reader->open(item);
    });

    connect(m_tabs, &QButtonGroup::idClicked, this, &NewsPage::showCategory);
    connect(m_views, &QButtonGroup::idClicked, this, &NewsPage::setViewMode);
    setViewMode(QSettings().value("news/view", "grid").toString() == "column" ? 1 : 0);
    connect(m_refresh, &QPushButton::clicked, this, [this] { refresh(true); });
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(450);
    connect(m_filter, &QLineEdit::textChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(m_filter, &QLineEdit::returnPressed, this, [this] {
        m_debounce.stop();
        refresh(false);
    });
    connect(&m_debounce, &QTimer::timeout, this, [this] { refresh(false); });
    m_timer.setInterval(5 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { refresh(true); });
    m_tick.setInterval(30 * 1000);
    connect(&m_tick, &QTimer::timeout, this, [this] { m_grid->tick(); });

    const int saved = std::clamp(QSettings().value("news/section", 0).toInt(), 0, int(names.size()) - 1);
    m_tabs->button(saved)->setChecked(true);
}

void NewsPage::setViewMode(int mode)
{
    mode = mode == 1 ? 1 : 0;
    m_views->button(mode)->setChecked(true);
    m_grid->setColumnView(mode == 1);
    QSettings().setValue("news/view", mode == 1 ? "column" : "grid");
}

void NewsPage::showCategory(int index)
{
    if (index < 0 || index >= m_tabs->buttons().size())
        return;
    m_tabs->button(index)->setChecked(true);
    QSettings().setValue("news/section", index);
    refresh(false);
}

void NewsPage::setTicker(const QString &ticker)
{
    m_filter->setText(ticker);
}

void NewsPage::openArticle(int index)
{
    if (const news::Item *item = m_grid->itemAt(index)) {
        m_pendingArticle = -1;
        m_reader->open(*item);
    } else {
        m_pendingArticle = index;
    }
}

void NewsPage::resizeEvent(QResizeEvent *)
{
    if (m_reader->isOpen())
        m_reader->setGeometry(rect());
}

void NewsPage::showEvent(QShowEvent *)
{
    refresh(false);
    m_timer.start();
    m_tick.start();
}

void NewsPage::hideEvent(QHideEvent *)
{
    m_timer.stop();
    m_tick.stop();
}

void NewsPage::refresh(bool force)
{
    static const QRegularExpression notSymbol("[^A-Za-z0-9.\\-=^]");
    const QString ticker = m_filter->text().trimmed().toUpper().remove(notSymbol);
    const int generation = ++m_generation;
    if (m_tabs->checkedId() == FavoritesTab) {
        // The starred articles, as saved; the ticker filter applies to their badge and text.
        QVector<news::Item> items;
        for (const news::Item &i : news::Favorites::instance()->items())
            if (ticker.isEmpty() || i.symbol == ticker || i.title.contains(ticker, Qt::CaseInsensitive)
                || i.summary.contains(ticker, Qt::CaseInsensitive))
                items.push_back(i);
        fill(items, "favorites", QString());
        return;
    }
    const auto category = news::Category(m_tabs->checkedId() < 0 ? 0 : m_tabs->checkedId());
    if (m_grid->findChildren<newsui::Card *>().isEmpty())
        m_grid->setMessage(tr("Loading…"));
    m_note->setText(tr("Loading…"));
    m_refresh->setEnabled(false);
    news::Wire::instance()->fetch(category, ticker, force, this,
                                  [this, generation](const QVector<news::Item> &items, const QString &provider, const QString &error) {
                                      if (generation != m_generation)
                                          return;
                                      fill(items, provider, error);
                                  });
}

void NewsPage::fill(const QVector<news::Item> &items, const QString &provider, const QString &error)
{
    m_refresh->setEnabled(true);
    m_lastRefresh = QDateTime::currentDateTimeUtc();
    m_received = items;
    m_provider = provider;
    m_error = error;
    applyTags();
    if (m_pendingFavorite >= 0 && !items.isEmpty())
        toggleFavorite(m_pendingFavorite);
    if (m_pendingArticle >= 0 && !items.isEmpty())
        openArticle(m_pendingArticle);
}

void NewsPage::applyTags()
{
    const int minImpact = m_impact->currentData().toInt();
    const QString topic = m_topic->currentData().toString();
    QVector<news::Item> items;
    for (const news::Item &i : std::as_const(m_received)) {
        const desk::NewsTags tags = desk::tagHeadline(i.title);
        if (int(tags.impact) >= minImpact && (topic.isEmpty() || tags.topics.contains(topic)))
            items.push_back(i);
    }
    const QString &provider = m_provider, &error = m_error;
    m_grid->setItems(items);
    m_scroll->verticalScrollBar()->setValue(0);
    if (items.isEmpty())
        m_grid->setMessage(!m_received.isEmpty()       ? tr("No article for these filters")
                           : provider == "favorites"   ? tr("No favorite yet: star a card, or the article in the reader.")
                           : error.isEmpty()           ? tr("No article")
                                                       : error);
    QStringList note;
    note << (items.size() == m_received.size() ? tr("%n article(s)", nullptr, items.size())
                                               : tr("%1 of %2 articles").arg(items.size()).arg(m_received.size()));
    note << (provider == "fmp" ? "Financial Modeling Prep" : provider == "favorites" ? tr("favorites") : tr("free feeds"));
    note << QLocale().toString(m_lastRefresh.toLocalTime(), "HH:mm");
    if (!error.isEmpty() && !items.isEmpty())
        note << error;
    m_note->setText(note.join(" · "));
    m_note->setToolTip(error);
}

void NewsPage::openUrl(const QString &url)
{
    news::Item item;
    item.title = url;
    item.url = url;
    item.site = QUrl(url).host();
    m_reader->open(item);
}

void NewsPage::toggleFavorite(int index)
{
    if (const news::Item *item = m_grid->itemAt(index)) {
        m_pendingFavorite = -1;
        news::Favorites::instance()->toggle(*item);
    } else {
        m_pendingFavorite = index;
    }
}
