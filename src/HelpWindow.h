#pragma once

#include <QDialog>
#include <QHash>

class QLabel;
class QLineEdit;
class QListWidget;
class QTextBrowser;
class QUrl;

// The documentation window (the header's ? icon): a sidebar of chapters on the left, the chapter's text on the right,
// a search box over every chapter. The chapters are Markdown files compiled in as resources, one per language
// (resources/help/<lang>/<id>.md, the first line `# Title` being the chapter's name in the sidebar); a chapter missing
// in a language falls back to English. The ? icon opens the chapter of the page shown; `help:<id>` links jump between
// chapters, other links open in the browser.
class HelpWindow : public QDialog
{
    Q_OBJECT
public:
    explicit HelpWindow(const QString &language, QWidget *parent = nullptr);

    static QStringList chapters();                 // the ids, in the sidebar's order (the page ids, plus a few)
    // Shows the window on this chapter (the page ids are chapter ids; unknown: the first chapter).
    void showChapter(const QString &id);
    QString currentChapter() const { return m_chapter; }
    void setLanguage(const QString &code);         // reloads the titles and the chapter shown
    void retheme();                                // after a theme change: the text's colours

protected:
    void changeEvent(QEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    void retranslateUi();
    void select(const QString &id);                // the sidebar's row, which loads the chapter
    void loadChapter(const QString &id);
    void styleDocument();                          // headings, code, tables and links in the theme's colours
    void search(const QString &text);
    void findNext();
    void openLink(const QUrl &url);
    QString markdown(const QString &id);           // in the current language, cached

    QString m_language;
    QString m_chapter;
    QHash<QString, QString> m_texts;               // chapter id → markdown, for the language shown
    QLabel *m_title;
    QLineEdit *m_search;
    QListWidget *m_nav;
    QTextBrowser *m_text;
};
