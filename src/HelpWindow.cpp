#include "HelpWindow.h"

#include "Theme.h"

#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextFrame>
#include <QTextTable>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace {

// The chapters in the sidebar's order. The page ids are chapter ids, so the ? icon lands on the page's chapter.
const QStringList Chapters {
    "start", "map", "overview", "economy", "forex", "charts", "calendar", "news",
    "research", "quant", "desk", "journal", "ai", "sources", "devtools", "data",
};

QString readMarkdown(const QString &id, const QString &lang)
{
    for (const QString &l : QStringList {lang, QStringLiteral("en")}) {
        QFile f(QStringLiteral(":/help/%1/%2.md").arg(l, id));
        if (f.open(QIODevice::ReadOnly))
            return QString::fromUtf8(f.readAll());
    }
    return QString();
}

QString titleOf(const QString &markdown)
{
    const QString first = markdown.section('\n', 0, 0).trimmed();
    return first.startsWith("# ") ? first.mid(2).trimmed() : first;
}

} // namespace

HelpWindow::HelpWindow(const QString &language, QWidget *parent)
    : QDialog(parent)
    , m_language(language)
{
    setObjectName("helpWindow");
    setAttribute(Qt::WA_StyledBackground);
    setModal(false);
    setMinimumSize(760, 480);
    const QScreen *screen = QGuiApplication::primaryScreen();
    const QSize room = screen ? screen->availableGeometry().size() - QSize(40, 60) : QSize(1180, 760);
    resize(QSettings().value("help/size", QSize(1180, 760)).toSize().boundedTo(room));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The sidebar wears the settings window's names, so the style sheet dresses both the same.
    auto *sidebar = new QWidget;
    sidebar->setObjectName("settingsSidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(236);
    auto *sl = new QVBoxLayout(sidebar);
    sl->setContentsMargins(12, 18, 12, 12);
    sl->setSpacing(0);
    m_title = new QLabel;
    m_title->setObjectName("settingsTitle");
    sl->addWidget(m_title);
    m_search = new QLineEdit;
    m_search->setObjectName("keyEdit");
    m_search->setClearButtonEnabled(true);
    sl->addWidget(m_search);
    sl->addSpacing(10);
    m_nav = new QListWidget;
    m_nav->setObjectName("settingsNav");
    m_nav->setCursor(Qt::PointingHandCursor);
    for (const QString &id : Chapters) {
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, id);
        m_nav->addItem(item);
    }
    sl->addWidget(m_nav, 1);
    layout->addWidget(sidebar);

    m_text = new QTextBrowser;
    m_text->setObjectName("helpText");
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setOpenLinks(false);
    m_text->setOpenExternalLinks(false);
    layout->addWidget(m_text, 1);

    connect(m_nav, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item)
            loadChapter(item->data(Qt::UserRole).toString());
    });
    connect(m_search, &QLineEdit::textChanged, this, &HelpWindow::search);
    connect(m_search, &QLineEdit::returnPressed, this, &HelpWindow::findNext);
    connect(m_text, &QTextBrowser::anchorClicked, this, &HelpWindow::openLink);

    retranslateUi();
}

QStringList HelpWindow::chapters()
{
    return Chapters;
}

void HelpWindow::showChapter(const QString &id)
{
    select(Chapters.contains(id) ? id : Chapters.first());
    show();
    raise();
    activateWindow();
}

void HelpWindow::select(const QString &id)
{
    for (int i = 0; i < m_nav->count(); ++i) {
        if (m_nav->item(i)->data(Qt::UserRole).toString() == id) {
            if (m_nav->currentRow() != i)
                m_nav->setCurrentRow(i);   // loads the chapter
            return;
        }
    }
}

void HelpWindow::setLanguage(const QString &code)
{
    if (code == m_language)
        return;
    m_language = code;
    m_texts.clear();
    retranslateUi();
}

void HelpWindow::retheme()
{
    if (!m_chapter.isEmpty())
        loadChapter(m_chapter);
}

void HelpWindow::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(e);
}

void HelpWindow::hideEvent(QHideEvent *e)
{
    QSettings().setValue("help/size", size());
    QDialog::hideEvent(e);
}

void HelpWindow::retranslateUi()
{
    setWindowTitle(tr("Help"));
    m_title->setText(tr("HELP"));
    m_search->setPlaceholderText(tr("Search the help"));
    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem *item = m_nav->item(i);
        item->setText(titleOf(markdown(item->data(Qt::UserRole).toString())));
    }
    if (!m_chapter.isEmpty())
        loadChapter(m_chapter);
}

QString HelpWindow::markdown(const QString &id)
{
    auto it = m_texts.find(id);
    if (it == m_texts.end())
        it = m_texts.insert(id, readMarkdown(id, m_language));
    return it.value();
}

void HelpWindow::loadChapter(const QString &id)
{
    m_chapter = id;
    m_text->setMarkdown(markdown(id));
    styleDocument();
    m_text->moveCursor(QTextCursor::Start);
    m_text->verticalScrollBar()->setValue(0);
    const QString q = m_search->text().trimmed();
    if (!q.isEmpty())
        m_text->find(q);
}

// QTextDocument's Markdown importer sets no colours beyond the widget's own: headings, code, tables and links
// get the theme's here, once the document is built (the style sheet reaches the widget, not the document's parts).
void HelpWindow::styleDocument()
{
    const Palette &pal = Theme::current();
    QTextDocument *doc = m_text->document();
    // The style sheet gives widgets a pixel size (13px), which a font read back reports as a point size of -1:
    // the document's font is set in pixels, from the application's families.
    QFont base = QApplication::font();
    base.setPixelSize(15);
    doc->setDefaultFont(base);
    doc->setDocumentMargin(32);

    struct Patch { int position; int length; QTextCharFormat format; };
    QVector<Patch> patches;   // applied after the walk: merging formats re-cuts the fragments being walked
    QTextCursor cursor(doc);
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat bf = block.blockFormat();
        const int level = bf.headingLevel();
        const bool code = bf.nonBreakableLines();
        if (level > 0) {
            bf.setTopMargin(level == 1 ? 0 : level == 2 ? 26 : 18);
            bf.setBottomMargin(level == 1 ? 14 : 8);
        } else if (code) {
            bf.setBackground(pal.Panel);
            bf.setLeftMargin(12);
            bf.setRightMargin(12);
            bf.setTopMargin(block.previous().blockFormat().nonBreakableLines() ? 0 : 8);
            bf.setBottomMargin(block.next().blockFormat().nonBreakableLines() ? 0 : 8);
        } else {
            bf.setBottomMargin(bf.indent() > 0 ? 3 : 8);
            bf.setLineHeight(116, QTextBlockFormat::ProportionalHeight);
        }
        cursor.setPosition(block.position());
        cursor.setBlockFormat(bf);

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            const QTextCharFormat cf = frag.charFormat();
            QTextCharFormat patch;
            if (cf.isAnchor()) {
                patch.setForeground(pal.Accent);
                patch.setFontUnderline(false);
            } else if (code) {
                patch.setForeground(pal.Text);
            } else if (cf.fontFixedPitch()) {
                patch.setBackground(pal.Panel);
                patch.setForeground(pal.Text);
            } else if (level > 0) {
                patch.setForeground(level >= 3 ? pal.TextSection : pal.Text);
                patch.setFontWeight(QFont::DemiBold);
            }
            if (!patch.isEmpty())
                patches.push_back({frag.position(), frag.length(), patch});
        }
    }
    for (const Patch &p : patches) {
        cursor.setPosition(p.position);
        cursor.setPosition(p.position + p.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(p.format);
    }

    std::function<void(QTextFrame *)> walk = [&](QTextFrame *frame) {
        for (QTextFrame *child : frame->childFrames()) {
            if (auto *table = qobject_cast<QTextTable *>(child)) {
                QTextTableFormat tf = table->format();
                tf.setCellPadding(7);
                tf.setCellSpacing(0);
                tf.setBorder(0.5);
                tf.setBorderBrush(pal.PanelLine);
                tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
                tf.setTopMargin(4);
                tf.setBottomMargin(12);
                tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
                table->setFormat(tf);
                for (int c = 0; c < table->columns(); ++c) {
                    QTextTableCell cell = table->cellAt(0, c);
                    QTextCharFormat cf = cell.format();
                    cf.setBackground(pal.Panel);
                    cell.setFormat(cf);
                }
            }
            walk(child);
        }
    };
    walk(doc->rootFrame());

    QPalette p = m_text->palette();
    p.setColor(QPalette::Link, pal.Accent);
    p.setColor(QPalette::LinkVisited, pal.Accent);
    m_text->setPalette(p);
}

// Hides the chapters that don't mention the text, goes to the first that does when the shown one doesn't, and
// highlights the first match.
void HelpWindow::search(const QString &text)
{
    const QString q = text.trimmed();
    QString first;
    bool currentMatches = false;
    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem *item = m_nav->item(i);
        const QString id = item->data(Qt::UserRole).toString();
        const bool match = q.isEmpty() || markdown(id).contains(q, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match && first.isEmpty())
            first = id;
        if (match && id == m_chapter)
            currentMatches = true;
    }
    if (q.isEmpty())
        return;
    if (!currentMatches && !first.isEmpty()) {
        select(first);   // loads it and finds the text
        return;
    }
    m_text->moveCursor(QTextCursor::Start);
    m_text->find(q);
}

void HelpWindow::findNext()
{
    const QString q = m_search->text().trimmed();
    if (q.isEmpty())
        return;
    if (!m_text->find(q)) {
        m_text->moveCursor(QTextCursor::Start);
        m_text->find(q);
    }
}

void HelpWindow::openLink(const QUrl &url)
{
    if (url.scheme() == "help") {
        const QString id = url.path().isEmpty() ? url.host() : url.path();
        if (Chapters.contains(id))
            select(id);
        return;
    }
    QDesktopServices::openUrl(url);
}
