#include "SettingsDialog.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

// Language names are always shown in their own language.
const QList<QPair<QString, QString>> Languages {
    {"en", "English"},
    {"fr", QString::fromUtf8("Français")},
};

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("settingsDialog");
    setAttribute(Qt::WA_StyledBackground);
    setModal(false);
    setFixedSize(1100, 650);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *sidebar = new QWidget;
    sidebar->setObjectName("settingsSidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(190);
    auto *sl = new QVBoxLayout(sidebar);
    sl->setContentsMargins(12, 18, 12, 12);
    sl->setSpacing(0);
    m_sidebarTitle = new QLabel;
    m_sidebarTitle->setObjectName("settingsTitle");
    sl->addWidget(m_sidebarTitle);
    m_nav = new QListWidget;
    m_nav->setObjectName("settingsNav");
    m_nav->setCursor(Qt::PointingHandCursor);
    m_nav->addItem(QString());
    m_nav->addItem(QString());
    sl->addWidget(m_nav, 1);
    layout->addWidget(sidebar);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildLanguagePage());
    m_pages->addWidget(buildThemePage());
    layout->addWidget(m_pages, 1);

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    retranslateUi();
}

QWidget *SettingsDialog::buildLanguagePage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(28, 24, 28, 24);
    l->setSpacing(6);
    m_languageTitle = new QLabel;
    m_languageTitle->setObjectName("pageTitle");
    m_languageHint = new QLabel;
    m_languageHint->setObjectName("dim");
    m_languageHint->setWordWrap(true);
    l->addWidget(m_languageTitle);
    l->addWidget(m_languageHint);
    l->addSpacing(14);

    auto *group = new QButtonGroup(page);
    for (const auto &lang : Languages) {
        const QString code = lang.first;
        auto *b = new QPushButton(lang.second);
        b->setObjectName("optionCard");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(48);
        group->addButton(b);
        m_languageButtons.insert(code, b);
        l->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, code] { emit languageChosen(code); });
    }
    l->addStretch();
    return page;
}

QWidget *SettingsDialog::buildThemePage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(28, 24, 28, 24);
    l->setSpacing(6);
    m_themeTitle = new QLabel;
    m_themeTitle->setObjectName("pageTitle");
    m_themeHint = new QLabel;
    m_themeHint->setObjectName("dim");
    m_themeHint->setWordWrap(true);
    l->addWidget(m_themeTitle);
    l->addWidget(m_themeHint);
    l->addSpacing(14);

    auto *grid = new QGridLayout;
    grid->setSpacing(14);
    auto *group = new QButtonGroup(page);
    const QVector<Palette> &palettes = Theme::palettes();
    for (int i = 0; i < palettes.size(); ++i) {
        const Palette &palette = palettes[i];
        auto *card = new ThemeCard(palette);
        group->addButton(card);
        m_themeButtons.insert(palette.id, card);
        grid->addWidget(card, i / 5, i % 5);   // five across: two rows of cards fit the window's height
        connect(card, &QAbstractButton::clicked, this, [this, id = palette.id] { emit themeChosen(id); });
    }
    grid->setColumnStretch(5, 1);
    l->addLayout(grid);
    l->addStretch();
    return page;
}

void SettingsDialog::setCurrent(const QString &language, const QString &themeId)
{
    if (QPushButton *b = m_languageButtons.value(language))
        b->setChecked(true);
    if (QAbstractButton *b = m_themeButtons.value(themeId))
        b->setChecked(true);
}

void SettingsDialog::showPage(int index)
{
    m_nav->setCurrentRow(index);
}

void SettingsDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(e);
}

void SettingsDialog::retranslateUi()
{
    setWindowTitle(tr("Settings"));
    m_sidebarTitle->setText(tr("SETTINGS"));
    m_nav->item(0)->setText(tr("Language"));
    m_nav->item(1)->setText(tr("Theme"));
    m_languageTitle->setText(tr("Language"));
    m_languageHint->setText(tr("Choose the language used across the interface and market data."));
    m_themeTitle->setText(tr("Theme"));
    m_themeHint->setText(tr("Choose the color palette for the globe and panels."));

    const QHash<QString, QString> descriptions {
        {"ember", tr("Warm bronze tones of the original design.")},
        {"abyss", tr("Deep-sea blues with bioluminescent accents.")},
        {"jade", tr("Forest-dark greens with polished jade accents.")},
        {"oxide", tr("Iron-oxide reds over dark rust.")},
        {"nova", tr("Night-sky purples with a lavender glow.")},
        {"bloomberg", tr("Amber on black, after the Bloomberg Terminal.")},
        {"glacier", tr("Light theme: ice whites and glacial blues.")},
        {"dune", tr("Light theme: beige and sand with ochre accents.")},
        {"sakura", tr("Light theme: cherry-blossom pinks over petal white.")},
    };
    for (auto it = m_themeButtons.begin(); it != m_themeButtons.end(); ++it)
        static_cast<ThemeCard *>(it.value())->setDescription(descriptions.value(it.key()));
}

// ---------------------------------------------------------------- ThemeCard

ThemeCard::ThemeCard(const Palette &palette, QWidget *parent)
    : QAbstractButton(parent)
    , m_palette(palette)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
}

void ThemeCard::setDescription(const QString &text)
{
    m_description = text;
    update();
}

QSize ThemeCard::sizeHint() const
{
    return QSize(158, 200);
}

void ThemeCard::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void ThemeCard::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void ThemeCard::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();   // the card frame follows the active theme
    const Palette &pal = m_palette;         // the preview shows this card's theme

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(isChecked() ? ui.Accent : (m_hover ? ui.ControlBorderHover : ui.ControlBorder),
                  isChecked() ? 1.5 : 1.0));
    p.setBrush(isChecked() ? ui.Active : (m_hover ? ui.Hover : ui.Panel));
    p.drawRoundedRect(frame, 10, 10);

    // Preview: background grid + miniature globe.
    const QRectF preview(8, 8, width() - 16, 92);
    QPainterPath clip;
    clip.addRoundedRect(preview, 7, 7);
    p.save();
    p.setClipPath(clip);
    p.fillRect(preview, pal.Background);
    p.setPen(QPen(pal.GridLine, 1));
    for (double x = preview.left() + 12; x < preview.right(); x += 16)
        p.drawLine(QPointF(x, preview.top()), QPointF(x, preview.bottom()));
    for (double y = preview.top() + 12; y < preview.bottom(); y += 16)
        p.drawLine(QPointF(preview.left(), y), QPointF(preview.right(), y));

    // The globe is drawn at its original 48 px radius, scaled to the preview.
    const double r = 48, scale = 38.0 / r;
    p.translate(preview.center());
    p.scale(scale, scale);
    const QPointF c(0, 0);
    QRadialGradient halo(c, r * 1.15);
    QColor h0 = pal.Halo; h0.setAlpha(0);
    QColor h1 = pal.Halo; h1.setAlpha(90);
    halo.setColorAt(0.8, h1);
    halo.setColorAt(1.0, h0);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(c, r * 1.15, r * 1.15);

    QRadialGradient ocean(c - QPointF(r * 0.2, r * 0.25), r * 1.1);
    ocean.setColorAt(0, pal.OceanInner);
    ocean.setColorAt(1, pal.OceanOuter);
    p.setBrush(ocean);
    p.drawEllipse(c, r, r);

    QPainterPath globe;
    globe.addEllipse(c, r, r);
    p.setClipPath(globe, Qt::IntersectClip);
    p.setBrush(pal.Land);
    p.setPen(QPen(pal.LandBorder, 1));
    p.drawEllipse(QRectF(c.x() - 46, c.y() - 34, 40, 30));
    p.drawEllipse(QRectF(c.x() - 30, c.y() + 2, 22, 40));
    p.drawEllipse(QRectF(c.x() + 8, c.y() - 40, 46, 26));
    p.drawEllipse(QRectF(c.x() + 10, c.y() - 10, 28, 42));
    // Back to the preview's clip (set in widget coordinates), at the globe's scale again.
    p.restore();
    p.save();
    p.setClipPath(clip);
    p.translate(preview.center());
    p.scale(scale, scale);

    p.setPen(QPen(pal.Halo, 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r, r);

    // A few markers, in the shared category colors.
    const QList<QPair<QPointF, QPair<QString, QColor>>> dots {
        {{-26, -20}, {"energy", QColor("#E07A4F")}}, {{-14, -8}, {"precious", QColor("#E8C468")}},
        {{22, -26}, {"agri", QColor("#A08CC8")}}, {{30, -14}, {"industrial", QColor("#C9A27A")}},
        {{-20, 22}, {"softs", QColor("#8DBFA8")}}, {{20, 8}, {"energy", QColor("#E07A4F")}},
    };
    for (const auto &[off, cat] : dots) {
        p.setPen(QPen(pal.ClusterFill, 1));
        p.setBrush(pal.categoryColors.value(cat.first, cat.second));
        p.drawEllipse(c + off, 3.2, 3.2);
    }
    QColor fill = pal.ClusterFill; fill.setAlpha(235);
    p.setBrush(fill);
    p.setPen(QPen(pal.Accent, 2));
    p.drawEllipse(c + QPointF(4, -22), 9, 9);
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(9);
    p.setFont(f);
    p.setPen(pal.Text);
    p.drawText(QRectF(c.x() + 4 - 9, c.y() - 22 - 9, 18, 18), Qt::AlignCenter, "3");
    p.restore();

    // Name + description, in the active UI colors.
    f.setPointSizeF(12.5);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(12, 106, width() - 40, 22), Qt::AlignLeft | Qt::AlignVCenter, pal.name);
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(ui.Accent);
        const QPointF mc(width() - 18, 117);
        p.drawEllipse(mc, 8, 8);
        QPainterPath tick;
        tick.moveTo(mc + QPointF(-3.5, 0));
        tick.lineTo(mc + QPointF(-1, 2.8));
        tick.lineTo(mc + QPointF(3.8, -2.6));
        p.setPen(QPen(ui.Panel, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tick);
    }
    f.setBold(false);
    f.setPointSizeF(10.5);
    p.setFont(f);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(12, 130, width() - 24, 66), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               m_description);
}
