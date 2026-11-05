#pragma once

#include <QAbstractButton>
#include <QDialog>
#include <QHash>

#include "Theme.h"

class QButtonGroup;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

// Floating settings window: a sidebar of sections on the left, the selected
// section's options on the right. Choices apply immediately via signals.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void setCurrent(const QString &language, const QString &themeId, bool newsTranslation);
    void showPage(int index);   // 0 language, 1 theme, 2 news

signals:
    void languageChosen(const QString &code);
    void themeChosen(const QString &id);
    void newsTranslationChosen(bool on);

protected:
    void changeEvent(QEvent *e) override;

private:
    QWidget *buildLanguagePage();
    QWidget *buildThemePage();
    QWidget *buildNewsPage();
    void retranslateUi();

    QListWidget *m_nav;
    QStackedWidget *m_pages;
    QLabel *m_sidebarTitle;
    QLabel *m_languageTitle;
    QLabel *m_languageHint;
    QLabel *m_themeTitle;
    QLabel *m_themeHint;
    QLabel *m_newsTitle;
    QLabel *m_newsHint;
    QPushButton *m_translationOn;
    QPushButton *m_translationOff;
    QHash<QString, QPushButton *> m_languageButtons;
    QHash<QString, QAbstractButton *> m_themeButtons;
};

// Selectable card showing a miniature globe rendered with a given palette.
class ThemeCard : public QAbstractButton
{
    Q_OBJECT
public:
    ThemeCard(const Palette &palette, QWidget *parent = nullptr);

    void setDescription(const QString &text);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    Palette m_palette;
    QString m_description;
    bool m_hover = false;
};
