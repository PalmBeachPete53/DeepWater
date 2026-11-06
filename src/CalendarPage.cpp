#include "CalendarPage.h"

#include "DeskWidgets.h"
#include "QuantCharts.h"
#include "Theme.h"

#include "quant/ui/PanelsKit.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDateEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

QFont monoFont()
{
    QFont f;
    f.setFamilies(Theme::monoFamilies());
    f.setStyleHint(QFont::Monospace);
    return f;
}

// The flag emoji of a country (two regional indicator letters); the euro area has one too.
QString flag(const QString &iso2)
{
    if (iso2.size() != 2)
        return {};
    char32_t s[2] = {char32_t(0x1F1E6 + iso2[0].toUpper().unicode() - 'A'),
                     char32_t(0x1F1E6 + iso2[1].toUpper().unicode() - 'A')};
    return QString::fromUcs4(s, 2);
}

QString countryName(const QString &iso2)
{
    // English names as in country_names.json, translated through the "Country" context.
    static const QHash<QString, const char *> names {
        {"US", "United States"}, {"GB", "United Kingdom"}, {"DE", "Germany"}, {"FR", "France"},
        {"IT", "Italy"}, {"ES", "Spain"}, {"CH", "Switzerland"}, {"SE", "Sweden"}, {"NO", "Norway"},
        {"JP", "Japan"}, {"CN", "China"}, {"IN", "India"}, {"KR", "South Korea"}, {"AU", "Australia"},
        {"NZ", "New Zealand"}, {"CA", "Canada"}, {"MX", "Mexico"}, {"BR", "Brazil"}, {"ZA", "South Africa"},
        {"TR", "Türkiye"}, {"RU", "Russia"}, {"NL", "Netherlands"}, {"BE", "Belgium"}, {"PT", "Portugal"},
        {"SG", "Singapore"},
    };
    if (iso2 == "EU")
        return CalendarPage::tr("Euro area");
    if (iso2 == "HK")
        return CalendarPage::tr("Hong Kong");
    const char *name = names.value(iso2);
    return name ? QCoreApplication::translate("Country", name) : iso2;
}

QString dayLabel(const QDate &d)
{
    const QLocale loc;
    return loc.toString(d, loc.language() == QLocale::English ? "MMM d" : "d MMM");
}

QString timeZoneLabel()
{
    return QString::fromUtf8(QTimeZone::systemTimeZoneId());
}

// A panel: a title bar (title on the left, controls on the right) over its content.
QFrame *panel(const QString &title, QHBoxLayout **head, QVBoxLayout **body)
{
    auto *frame = new QFrame;
    frame->setObjectName("calPanel");
    auto *v = new QVBoxLayout(frame);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *bar = new QWidget;
    bar->setObjectName("calPanelHead");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(32);
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 0, 6, 0);
    h->setSpacing(6);
    auto *t = new QLabel(title.toUpper());
    t->setObjectName("calPanelTitle");
    h->addWidget(t);
    h->addStretch(1);
    v->addWidget(bar);
    auto *content = new QVBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    v->addLayout(content, 1);
    *head = h;
    *body = content;
    return frame;
}

QLabel *fieldLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper() + ":");
    l->setObjectName("calFieldLabel");
    return l;
}

QTableWidget *makeTable(const QStringList &headers, int rowHeight, const QString &name = "calTable")
{
    auto *t = new QTableWidget(0, headers.size());
    t->setObjectName(name);
    QStringList upper;
    for (const QString &h : headers)
        upper << h.toUpper();
    t->setHorizontalHeaderLabels(upper);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(rowHeight);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->setMouseTracking(true);
    t->setFrameShape(QFrame::NoFrame);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    return t;
}

QTableWidgetItem *cell(const QString &text, const QColor &color = QColor(), Qt::Alignment align = Qt::AlignLeft)
{
    auto *i = new QTableWidgetItem(text);
    if (color.isValid())
        i->setForeground(color);
    i->setTextAlignment(align | Qt::AlignVCenter);
    return i;
}

// Number of decimals that shows a value as published (TradingView sends 0.07, 1.221, 194).
int decimalsOf(double v)
{
    if (std::isnan(v))
        return 0;
    QString s = QString::number(std::abs(v), 'f', 3);
    while (s.endsWith('0'))
        s.chop(1);
    const int dot = s.indexOf('.');
    return dot < 0 || dot == s.size() - 1 ? 0 : int(s.size()) - dot - 1;
}

// Time left before a release; nothing once it is published.
QString countdownText(const QDateTime &when, bool published)
{
    const qint64 left = QDateTime::currentDateTimeUtc().secsTo(when);
    if (published || left < -3600)
        return QStringLiteral("—");
    if (left <= 0)
        return EconomicView::tr("due");
    if (left < 3600)
        return EconomicView::tr("%1 min").arg(std::max<qint64>(1, left / 60));
    if (left < 86400)
        return EconomicView::tr("%1 h %2").arg(left / 3600).arg((left % 3600) / 60, 2, 10, QLatin1Char('0'));
    return EconomicView::tr("%1 d %2 h").arg(left / 86400).arg((left % 86400) / 3600);
}

QString formatValue(double v, int decimals, const QString &unit, const QString &scale)
{
    if (std::isnan(v))
        return QStringLiteral("—");
    QString s = QLocale().toString(v, 'f', decimals) + scale;
    if (unit == "%")
        return s + "%";
    if (unit.isEmpty())
        return s;
    // Currency signs go before the figure ("$62.1B", "-€7.8B"), other units after it ("77B cf").
    static const QRegularExpression sign("[$€£¥₹]");
    if (unit.contains(sign))
        return v < 0 ? "-" + unit + s.mid(1) : unit + s;
    return s + " " + unit;
}

QString money(double v, int decimals = 2)
{
    if (std::isnan(v))
        return QStringLiteral("—");
    const QString s = "$" + QLocale().toString(std::abs(v), 'f', decimals);
    return v < 0 ? "-" + s : s;
}

// The impact column: a framed LOW / MEDIUM / HIGH badge.
class ImpactDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);

        const Palette &ui = Theme::current();
        const int impact = index.data(Qt::UserRole).toInt();
        const QColor color = impact == 2 ? ui.Down : impact == 1 ? ui.Accent : ui.TextDim;
        const QString text = impact == 2 ? CalendarPage::tr("High") : impact == 1 ? CalendarPage::tr("Medium")
                                                                                 : CalendarPage::tr("Low");
        QFont f = opt.font;
        f.setPixelSize(10);
        f.setWeight(QFont::Bold);
        const QFontMetrics fm(f);
        const QString label = text.toUpper();
        const int w = fm.horizontalAdvance(label) + 12, h = 18;
        const QRect r(option.rect.center().x() - w / 2, option.rect.center().y() - h / 2 + 1, w, h);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QColor fill = color;
        fill.setAlpha(28);
        p->setBrush(fill);
        p->setPen(QPen(color, 1));
        p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        p->setFont(f);
        p->setPen(color);
        p->drawText(r, Qt::AlignCenter, label);
        p->restore();
    }
};

QString countdown(qint64 secs)
{
    secs = std::max<qint64>(0, secs);
    const qint64 d = secs / 86400, h = secs % 86400 / 3600, m = secs % 3600 / 60, s = secs % 60;
    if (d > 0)
        return MarketCard::tr("%1d %2h %3m").arg(d).arg(h).arg(m);
    if (h > 0)
        return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    return QString("%1m %2s").arg(m).arg(s);
}

QPushButton *segButton(const QString &text)
{
    auto *b = new QPushButton(text.toUpper());
    b->setObjectName("calSegButton");
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

} // namespace

// ---------------------------------------------------------------- EconomicView

EconomicView::EconomicView(EconomicCalendar *calendar, QWidget *parent)
    : QWidget(parent)
    , m_cal(calendar)
{
    QSettings settings;
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    QHBoxLayout *head;
    QVBoxLayout *body;
    outer->addWidget(panel(tr("Economic events"), &head, &body));
    auto *refresh = new QPushButton(tr("Refresh").toUpper());
    refresh->setObjectName("calButton");
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QPushButton::clicked, m_cal, &RemoteDataSet::refreshNow);
    head->addWidget(refresh);

    // Filters: the period tabs and a date on the left, country and impact on the right.
    auto *filters = new QWidget;
    filters->setObjectName("calFilters");
    auto *fl = new QHBoxLayout(filters);
    fl->setContentsMargins(14, 6, 14, 8);
    auto *left = new QVBoxLayout;
    left->setSpacing(6);
    auto *tabsRow = new QWidget;
    tabsRow->setFixedHeight(30);
    auto *tabs = new qtm::kit::WrapBarLayout(tabsRow, 2, 2);
    m_rangeGroup = new QButtonGroup(this);
    const QStringList ranges {tr("Last week"), tr("Yesterday"), tr("Today"), tr("Tomorrow"), tr("Current week"),
                              tr("Next week")};
    for (int i = 0; i < ranges.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("calRangeTab");
        b->setText(ranges[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_rangeGroup->addButton(b, i);
        tabs->addWidget(b);
    }
    connect(m_rangeGroup, &QButtonGroup::idClicked, this, &EconomicView::setRange);
    left->addWidget(tabsRow);
    auto *dateRow = new QHBoxLayout;
    dateRow->setSpacing(8);
    dateRow->addWidget(fieldLabel(tr("Specific date")));
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setObjectName("calDate");
    m_date->setCalendarPopup(true);
    m_date->setFocusPolicy(Qt::ClickFocus);
    m_date->setDisplayFormat(QLocale().language() == QLocale::English ? "MM/dd/yyyy" : "dd/MM/yyyy");
    connect(m_date, &QDateEdit::dateChanged, this, [this](const QDate &d) {
        m_day = d;
        setRange(Day);
    });
    dateRow->addWidget(m_date);
    dateRow->addStretch(1);
    left->addLayout(dateRow);
    fl->addLayout(left, 1);

    auto *right = new QGridLayout;
    right->setHorizontalSpacing(8);
    right->setVerticalSpacing(6);
    right->addWidget(fieldLabel(tr("Country")), 0, 0, Qt::AlignRight);
    m_country = new QComboBox;
    m_country->setObjectName("calCombo");
    m_country->addItem(tr("All countries"), QString());
    for (const QString &c : EconomicCalendar::countries())
        m_country->addItem(flag(c) + "  " + countryName(c), c);
    m_country->setCurrentIndex(std::max(0, m_country->findData(settings.value("calendar/country", "US").toString())));
    connect(m_country, &QComboBox::currentIndexChanged, this, [this] {
        QSettings().setValue("calendar/country", m_country->currentData().toString());
        rebuild();
    });
    right->addWidget(m_country, 0, 1);
    right->addWidget(fieldLabel(tr("Impact")), 1, 0, Qt::AlignRight);
    auto *impacts = new QHBoxLayout;
    impacts->setSpacing(14);
    const QStringList saved = settings.value("calendar/impact", QStringList {"0", "1", "2"}).toStringList();
    const QString impactNames[3] = {tr("Low"), tr("Medium"), tr("High")};
    for (int i = 0; i < 3; ++i) {
        m_impact[i] = new QCheckBox(impactNames[i]);
        m_impact[i]->setObjectName("calCheck");
        m_impact[i]->setCursor(Qt::PointingHandCursor);
        m_impact[i]->setChecked(saved.contains(QString::number(i)));
        connect(m_impact[i], &QCheckBox::toggled, this, [this] {
            QStringList on;
            for (int j = 0; j < 3; ++j)
                if (m_impact[j]->isChecked()) on << QString::number(j);
            QSettings().setValue("calendar/impact", on);
            rebuild();
        });
        impacts->addWidget(m_impact[i]);
    }
    right->addLayout(impacts, 1, 1);
    fl->addLayout(right);
    body->addWidget(filters);

    m_table = makeTable({tr("Date"), tr("Time"), tr("Country"), tr("Event"), tr("Impact"), tr("Actual"),
                         tr("Forecast"), tr("Previous"), tr("Surprise"), tr("In")},
                        34, "calTableLarge");
    m_table->setItemDelegateForColumn(4, new ImpactDelegate(m_table));
    QHeaderView *h = m_table->horizontalHeader();
    m_table->viewport()->installEventFilter(this);
    const int widths[] = {84, 70, 150, 0, 96, 92, 92, 92, 92, 84};
    for (int i = 0; i < 10; ++i) {
        if (i == 3) {
            h->setSectionResizeMode(i, QHeaderView::Stretch);
        } else {
            h->setSectionResizeMode(i, QHeaderView::Fixed);
            m_table->setColumnWidth(i, widths[i]);
        }
        if (i >= 4)
            m_table->model()->setHeaderData(i, Qt::Horizontal, int(Qt::AlignCenter), Qt::TextAlignmentRole);
    }
    body->addWidget(m_table, 1);
    // A click on an event shows its past releases.
    m_table->setToolTip(tr("Click an event for its past figures against the forecasts"));
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (const QTableWidgetItem *it = m_table->item(row, 3))
            showHistory(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString());
    });
    m_history = new QFrame;
    m_history->setObjectName("qtCard");
    m_history->setFixedHeight(290);
    auto *hv = new QVBoxLayout(m_history);
    hv->setContentsMargins(12, 8, 12, 8);
    hv->setSpacing(6);
    auto *hh = new QHBoxLayout;
    m_historyTitle = new QLabel;
    m_historyTitle->setObjectName("qtCardTitle");
    hh->addWidget(m_historyTitle);
    m_historyNote = new QLabel;
    m_historyNote->setObjectName("qtNote");
    hh->addWidget(m_historyNote, 1);
    m_historySource = new QPushButton(tr("Official source"));
    m_historySource->setObjectName("qtChip");
    m_historySource->setCursor(Qt::PointingHandCursor);
    m_historySource->hide();
    connect(m_historySource, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl(m_historySourceUrl)); });
    hh->addWidget(m_historySource);
    auto *historyClose = new QPushButton("✕");
    historyClose->setObjectName("qtChip");
    historyClose->setCursor(Qt::PointingHandCursor);
    connect(historyClose, &QPushButton::clicked, m_history, &QWidget::hide);
    hh->addWidget(historyClose);
    hv->addLayout(hh);
    auto *hb = new QHBoxLayout;
    hb->setSpacing(10);
    m_historyTable = deskui::table({tr("Date"), tr("Period"), tr("Actual"), tr("Forecast"), tr("Surprise")});
    m_historyTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    hb->addWidget(m_historyTable, 2);
    m_historyChart = new quant::MultiLineChart;
    m_historyChart->setZeroLine(false);
    m_historyChart->setLegend(true);
    hb->addWidget(m_historyChart, 3);
    hv->addLayout(hb, 1);
    m_history->hide();
    body->addWidget(m_history);
    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this] {
        if (!isVisible())
            return;
        for (int r = 0; r < m_table->rowCount(); ++r)
            if (QTableWidgetItem *it = m_table->item(r, 9))
                it->setText(countdownText(it->data(Qt::UserRole).toDateTime(), it->data(Qt::UserRole + 1).toBool()));
    });
    clock->start(30 * 1000);
    m_empty = new QLabel;
    m_empty->setObjectName("calEmpty");
    m_empty->setAlignment(Qt::AlignCenter);
    body->addWidget(m_empty, 1);
    m_status = new QLabel;
    m_status->setObjectName("calStatus");
    body->addWidget(m_status);

    connect(m_cal, &RemoteDataSet::updated, this, &EconomicView::rebuild);
    connect(m_cal, &EconomicCalendar::dayFetched, this,
            [this](const QDate &day, const QVector<EconomicCalendar::Event> &events, const QString &error) {
                m_fetchedDay = day;
                m_dayEvents = events;
                m_dayError = error;
                rebuild();
            });
    m_poll.setInterval(60 * 1000);
    connect(&m_poll, &QTimer::timeout, m_cal, &RemoteDataSet::refreshIfStale);
    m_range = std::clamp(settings.value("calendar/range", int(Today)).toInt(), 0, int(NextWeek));
    m_rangeGroup->button(m_range)->setChecked(true);
    rebuild();
}

// A narrow window drops Surprise and the time left, so that the event's name stays readable.
bool EconomicView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_table->viewport() && event->type() == QEvent::Resize) {
        const bool narrow = m_table->viewport()->width() < 1060;
        m_table->setColumnHidden(8, narrow);
        m_table->setColumnHidden(9, narrow);
    }
    return QWidget::eventFilter(watched, event);
}

void EconomicView::showEvent(QShowEvent *)
{
    m_cal->refreshIfStale();
    m_poll.start();
    rebuild();   // "today" may have moved on since the tab was last shown
}

void EconomicView::hideEvent(QHideEvent *)
{
    m_poll.stop();
}

void EconomicView::setRange(int range)
{
    m_range = range;
    if (range != Day)
        QSettings().setValue("calendar/range", range);
    if (range == Day) {
        // No period tab is current while a date is picked.
        m_rangeGroup->setExclusive(false);
        for (QAbstractButton *b : m_rangeGroup->buttons())
            b->setChecked(false);
        m_rangeGroup->setExclusive(true);
    }
    rebuild();
}

void EconomicView::showHistory(const QString &country, const QString &title)
{
    if (title.isEmpty())
        return;
    const int serial = ++m_historySerial;
    m_history->show();
    m_historyTitle->setText(QString("%1 · %2").arg(countryName(country), title));
    m_historyNote->setText(tr("Loading three years of releases…"));
    m_historyTable->setRowCount(0);
    const QDate today = QDate::currentDate();
    m_cal->fetchRange(country, today.addDays(-3 * 365), today.addDays(1), this,
                      [this, serial, title](const QVector<EconomicCalendar::Event> &events, const QString &error) {
        if (serial != m_historySerial)
            return;
        const Palette &ui = Theme::current();
        QVector<EconomicCalendar::Event> past;
        for (const EconomicCalendar::Event &e : events)
            if (e.title == title && !std::isnan(e.actual))
                past.push_back(e);
        std::sort(past.begin(), past.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
        if (past.isEmpty()) {
            m_historyNote->setText(error.isEmpty() ? tr("No past release of this event.") : tr("TradingView: %1").arg(error));
            m_historyChart->setData({}, {}, {});
            return;
        }
        QVector<QDate> dates;
        QVector<double> actual, forecast;
        int beat = 0, missed = 0, compared = 0;
        double sum = 0;
        m_historyTable->setRowCount(past.size());
        for (int i = 0; i < past.size(); ++i) {
            const EconomicCalendar::Event &e = past[i];
            dates << e.time.toLocalTime().date();
            actual << e.actual;
            forecast << e.forecast;
            const double surprise = e.actual - e.forecast;
            const int decimals = std::max({decimalsOf(e.actual), decimalsOf(e.forecast)});
            if (!std::isnan(surprise)) {
                ++compared;
                sum += surprise;
                beat += surprise > 0;
                missed += surprise < 0;
            }
            const int r = past.size() - 1 - i;   // newest first
            const QString surpriseText = formatValue(surprise, decimals, e.unit, e.scale);
            m_historyTable->setItem(r, 0, deskui::cell(QLocale().toString(dates.last(), "d MMM yyyy"), ui.TextDim));
            m_historyTable->setItem(r, 1, deskui::cell(e.period, ui.TextDim));
            m_historyTable->setItem(r, 2, deskui::cell(formatValue(e.actual, decimals, e.unit, e.scale)));
            m_historyTable->setItem(r, 3, deskui::cell(formatValue(e.forecast, decimals, e.unit, e.scale), std::isnan(e.forecast) ? ui.TextFaint : ui.Text));
            m_historyTable->setItem(r, 4, deskui::cell(surprise > 0 ? "+" + surpriseText : surpriseText,
                                                       std::isnan(surprise) ? ui.TextFaint : surprise > 0 ? ui.Up : surprise < 0 ? ui.Down : ui.TextDim));
        }
        m_historySourceUrl = past.last().sourceUrl;
        m_historySource->setVisible(m_historySourceUrl.startsWith("http"));
        m_historySource->setToolTip(past.last().source);
        const QString unit = past.last().unit, scale = past.last().scale;
        m_historyChart->setData(dates, {{tr("Actual"), ui.Accent, actual}, {tr("Forecast"), ui.TextDim, forecast}},
                                [unit, scale](double v) { return formatValue(v, decimalsOf(v), unit, scale); });
        m_historyNote->setText(compared == 0 ? tr("%1 releases, none with a forecast.").arg(past.size())
                                             : tr("%1 releases: %2 above the forecast, %3 below, mean surprise %4.")
                                                   .arg(past.size()).arg(beat).arg(missed)
                                                   .arg(formatValue(sum / compared, std::max(2, decimalsOf(past.last().actual)), unit, scale)));
    });
}

void EconomicView::rebuild()
{
    const Palette &ui = Theme::current();
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(1 - today.dayOfWeek());
    QDate from, to;   // local dates, `to` excluded
    switch (m_range) {
    case LastWeek: from = monday.addDays(-7); to = monday; break;
    case Yesterday: from = today.addDays(-1); to = today; break;
    case Today: from = today; to = today.addDays(1); break;
    case Tomorrow: from = today.addDays(1); to = today.addDays(2); break;
    case ThisWeek: from = monday; to = monday.addDays(7); break;
    case NextWeek: from = monday.addDays(7); to = monday.addDays(14); break;
    default: from = m_day; to = m_day.addDays(1); break;
    }

    // Events of the window downloaded with the calendar, or of a day fetched on its own.
    const bool inWindow = m_cal->hasData() && from >= m_cal->from() && to <= m_cal->to();
    const QVector<EconomicCalendar::Event> *source = nullptr;
    QString state;
    if (inWindow) {
        source = &m_cal->events();
    } else if (m_range == Day && m_fetchedDay == m_day) {
        source = &m_dayEvents;
        if (!m_dayError.isEmpty())
            state = tr("TradingView: %1").arg(m_dayError);
    } else if (m_range == Day) {
        m_cal->fetchDay(m_day);
        state = tr("Loading %1 from TradingView…").arg(QLocale().toString(m_day, QLocale::LongFormat));
    } else if (!m_cal->errorString().isEmpty()) {
        state = tr("TradingView: %1").arg(m_cal->errorString());
    } else {
        state = tr("Loading TradingView's economic calendar…");
    }

    const QString country = m_country->currentData().toString();
    const QDateTime start = from.startOfDay(), end = to.startOfDay();
    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    int rows = 0;
    if (source) {
        for (const EconomicCalendar::Event &e : *source) {
            if (e.time < start || e.time >= end || (!country.isEmpty() && e.country != country)
                || !m_impact[e.impact]->isChecked())
                continue;
            const QDateTime local = e.time.toLocalTime();
            const int decimals = std::max({decimalsOf(e.actual), decimalsOf(e.forecast), decimalsOf(e.previous)});
            QColor actualColor = ui.Text;
            if (!std::isnan(e.actual) && !std::isnan(e.forecast) && e.actual != e.forecast)
                actualColor = e.actual > e.forecast ? ui.Up : ui.Down;
            m_table->insertRow(rows);
            m_table->setItem(rows, 0, cell(dayLabel(local.date()), ui.TextDim));
            m_table->setItem(rows, 1, cell(local.toString("HH:mm"), ui.TextDim));
            m_table->setItem(rows, 2, cell(flag(e.country) + "  " + countryName(e.country)));
            auto *title = cell(e.period.isEmpty() ? e.title : QString("%1 (%2)").arg(e.title, e.period));
            if (!e.source.isEmpty())
                title->setToolTip(tr("Source: %1. Click for its past figures against the forecasts.").arg(e.source));
            title->setData(Qt::UserRole, e.country);
            title->setData(Qt::UserRole + 1, e.title);
            m_table->setItem(rows, 3, title);
            auto *impact = cell({});
            impact->setData(Qt::UserRole, e.impact);
            m_table->setItem(rows, 4, impact);
            m_table->setItem(rows, 5, cell(formatValue(e.actual, decimals, e.unit, e.scale),
                                           std::isnan(e.actual) ? ui.TextFaint : actualColor, Qt::AlignHCenter));
            m_table->setItem(rows, 6, cell(formatValue(e.forecast, decimals, e.unit, e.scale),
                                           std::isnan(e.forecast) ? ui.TextFaint : ui.Text, Qt::AlignHCenter));
            m_table->setItem(rows, 7, cell(formatValue(e.previous, decimals, e.unit, e.scale),
                                           std::isnan(e.previous) ? ui.TextFaint : ui.Text, Qt::AlignHCenter));
            // What the market didn't expect: the figure against the forecast.
            const double surprise = e.actual - e.forecast;
            const QString surpriseText = formatValue(surprise, decimals, e.unit, e.scale);
            m_table->setItem(rows, 8, cell(std::isnan(surprise) ? surpriseText : surprise > 0 ? "+" + surpriseText : surpriseText,
                                           std::isnan(surprise) ? ui.TextFaint : surprise == 0 ? ui.TextDim : surprise > 0 ? ui.Up : ui.Down, Qt::AlignHCenter));
            auto *countdown = cell(countdownText(e.time, !std::isnan(e.actual)), ui.TextDim, Qt::AlignHCenter);
            countdown->setData(Qt::UserRole, e.time);
            countdown->setData(Qt::UserRole + 1, !std::isnan(e.actual));
            m_table->setItem(rows, 9, countdown);
            ++rows;
        }
    }
    m_table->setUpdatesEnabled(true);
    m_table->setVisible(rows > 0);
    m_empty->setVisible(rows == 0);
    m_empty->setText(!state.isEmpty() ? state : tr("No event for these filters"));

    const QLocale loc;
    QStringList parts;
    if (!state.isEmpty()) {
        parts << state;
    } else {
        parts << (rows == 0 ? tr("No event for these filters") : tr("%n event(s)", nullptr, rows));
        if (inWindow && m_cal->fetchedAt().isValid())
            parts << tr("updated %1").arg(loc.toString(m_cal->fetchedAt().toLocalTime().time(), "HH:mm"));
        if (inWindow && !m_cal->errorString().isEmpty())
            parts << tr("last refresh failed: %1").arg(m_cal->errorString());
    }
    parts << tr("times in %1").arg(timeZoneLabel()) << tr("source: TradingView");
    m_status->setText(parts.join("  ·  "));
}

// ---------------------------------------------------------------- CorporateView

CorporateView::CorporateView(CorporateCalendar *calendar, QWidget *parent)
    : QWidget(parent)
    , m_cal(calendar)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 6);
    outer->setSpacing(6);
    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    QHBoxLayout *head;
    QVBoxLayout *body;

    grid->addWidget(panel(tr("Earnings calendar"), &head, &body), 0, 0);
    m_upcomingButton = segButton(tr("Upcoming"));
    m_recentButton = segButton(tr("Recent"));
    auto *toggle = new QButtonGroup(this);
    toggle->addButton(m_upcomingButton);
    toggle->addButton(m_recentButton);
    m_recentButton->setChecked(true);
    connect(m_upcomingButton, &QPushButton::clicked, this, [this] { m_upcoming = true; fillEarnings(); });
    connect(m_recentButton, &QPushButton::clicked, this, [this] { m_upcoming = false; fillEarnings(); });
    head->addWidget(m_upcomingButton);
    head->addWidget(m_recentButton);
    m_earnings = makeTable({tr("Symbol"), tr("Company"), tr("Date"), tr("Actual"), tr("Est."), tr("Surprise")}, 20);
    body->addWidget(m_earnings);

    grid->addWidget(panel(tr("Dividends calendar"), &head, &body), 0, 1);
    m_dividends = makeTable({tr("Symbol"), tr("Company"), tr("Ex-date"), tr("Payment"), tr("Amount")}, 20);
    body->addWidget(m_dividends);

    grid->addWidget(panel(tr("Stock splits calendar"), &head, &body), 1, 0);
    m_splits = makeTable({tr("Symbol"), tr("Company"), tr("Date"), tr("Ratio")}, 20);
    body->addWidget(m_splits);

    grid->addWidget(panel(tr("IPO calendar"), &head, &body), 1, 1);
    m_ipos = makeTable({tr("Symbol"), tr("Company"), tr("Date"), tr("Exchange"), tr("Price"), tr("Status")}, 20);
    body->addWidget(m_ipos);

    for (QTableWidget *t : {m_earnings, m_dividends, m_splits, m_ipos}) {
        QHeaderView *h = t->horizontalHeader();
        h->setSectionResizeMode(QHeaderView::Fixed);
        h->setSectionResizeMode(1, QHeaderView::Stretch);
        t->setColumnWidth(0, 70);
        for (int c = 2; c < t->columnCount(); ++c)
            t->setColumnWidth(c, 76);
        // Figures line up on the right, as in the last column's header.
        const int last = t->columnCount() - 1;
        t->model()->setHeaderData(last, Qt::Horizontal, int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    }
    m_ipos->setColumnWidth(3, 130);
    grid->setRowStretch(0, 3);
    grid->setRowStretch(1, 2);
    outer->addLayout(grid, 1);
    m_status = new QLabel;
    m_status->setObjectName("calStatus");
    outer->addWidget(m_status);

    connect(m_cal, &RemoteDataSet::updated, this, &CorporateView::rebuild);
    rebuild();
}

void CorporateView::showEvent(QShowEvent *)
{
    m_cal->refreshIfStale();
    rebuild();
}

void CorporateView::fillEarnings()
{
    const Palette &ui = Theme::current();
    const QDate today = QDate::currentDate();
    QVector<const CorporateCalendar::Earnings *> rows;
    for (const CorporateCalendar::Earnings &e : m_cal->earnings()) {
        const bool reported = !std::isnan(e.eps);
        if (m_upcoming ? (!reported && e.date >= today) : reported)
            rows.push_back(&e);
    }
    // Recent: the latest results first.
    if (!m_upcoming)
        std::stable_sort(rows.begin(), rows.end(), [](auto *a, auto *b) { return a->date > b->date; });
    const QStringList headers = m_upcoming
        ? QStringList {tr("Symbol"), tr("Company"), tr("Date"), tr("Time"), tr("Est."), tr("Last year")}
        : QStringList {tr("Symbol"), tr("Company"), tr("Date"), tr("Actual"), tr("Est."), tr("Surprise")};
    for (int c = 0; c < headers.size(); ++c)
        m_earnings->horizontalHeaderItem(c)->setText(headers[c].toUpper());

    m_earnings->setUpdatesEnabled(false);
    m_earnings->setRowCount(rows.size());
    const Qt::Alignment right = Qt::AlignRight;
    for (int r = 0; r < rows.size(); ++r) {
        const CorporateCalendar::Earnings &e = *rows[r];
        m_earnings->setItem(r, 0, cell(e.symbol, ui.Text));
        auto *name = cell(e.name, ui.TextDim);
        name->setToolTip(e.name);
        m_earnings->setItem(r, 1, name);
        m_earnings->setItem(r, 2, cell(dayLabel(e.date), ui.TextDim));
        if (m_upcoming) {
            const QString time = e.time == "pre" ? tr("Pre-market") : e.time == "after" ? tr("After close") : "—";
            m_earnings->setItem(r, 3, cell(time, ui.TextDim));
            m_earnings->setItem(r, 4, cell(money(e.estimate), ui.Text));
            m_earnings->setItem(r, 5, cell(money(e.lastYearEps), ui.TextDim, right));
        } else {
            // Nasdaq lists some results before it checks them: no surprise yet, and the figure may still change.
            auto *eps = cell(money(e.eps), std::isnan(e.surprisePct) ? ui.TextDim : ui.Text);
            if (std::isnan(e.surprisePct))
                eps->setToolTip(tr("Not confirmed by Nasdaq yet (no surprise published)"));
            m_earnings->setItem(r, 3, eps);
            m_earnings->setItem(r, 4, cell(money(e.estimate), ui.TextDim));
            QString surprise = "—";
            QColor color = ui.TextFaint;
            if (!std::isnan(e.surprisePct)) {
                surprise = QString("%1 %2%").arg(e.surprisePct >= 0 ? "▲" : "▼",
                                                  QLocale().toString(std::abs(e.surprisePct), 'f', 1));
                color = e.surprisePct >= 0 ? ui.Up : ui.Down;
            }
            m_earnings->setItem(r, 5, cell(surprise, color, right));
        }
    }
    m_earnings->setUpdatesEnabled(true);
}

void CorporateView::rebuild()
{
    const Palette &ui = Theme::current();
    const QLocale loc;
    const Qt::Alignment right = Qt::AlignRight;
    fillEarnings();

    const QDate today = QDate::currentDate();
    QVector<const CorporateCalendar::Dividend *> dividends;
    for (const CorporateCalendar::Dividend &d : m_cal->dividends())
        if (d.exDate >= today)
            dividends.push_back(&d);
    m_dividends->setRowCount(dividends.size());
    for (int r = 0; r < dividends.size(); ++r) {
        const CorporateCalendar::Dividend &d = *dividends[r];
        m_dividends->setItem(r, 0, cell(d.symbol, ui.Text));
        auto *name = cell(d.name, ui.TextDim);
        name->setToolTip(d.name);
        m_dividends->setItem(r, 1, name);
        m_dividends->setItem(r, 2, cell(dayLabel(d.exDate), ui.Text));
        m_dividends->setItem(r, 3, cell(d.payDate.isValid() ? dayLabel(d.payDate) : "—", ui.TextDim));
        // Cents, or as many decimals as the dividend has up to four ($0.0061, $0.28906 → $0.2891).
        int decimals = 2;
        while (decimals < 4 && std::abs(d.amount * std::pow(10, decimals) - std::round(d.amount * std::pow(10, decimals))) > 1e-6)
            ++decimals;
        m_dividends->setItem(r, 4, cell(money(d.amount, decimals), ui.Text, right));
    }

    const auto &splits = m_cal->splits();
    m_splits->setRowCount(splits.size());
    for (int r = 0; r < splits.size(); ++r) {
        const CorporateCalendar::Split &s = splits[r];
        m_splits->setItem(r, 0, cell(s.symbol, ui.Text));
        auto *name = cell(s.name, ui.TextDim);
        name->setToolTip(s.name);
        m_splits->setItem(r, 1, name);
        m_splits->setItem(r, 2, cell(s.date.isValid() ? dayLabel(s.date) : "—", s.date < today ? ui.TextDim : ui.Text));
        // "1 : 10" is a reverse split (ten shares become one).
        const QStringList ab = s.ratio.split(':');
        const bool reverse = ab.size() == 2 && ab[0].trimmed().toDouble() < ab[1].trimmed().toDouble();
        auto *ratio = cell(s.ratio, reverse ? ui.Down : ui.Up, right);
        ratio->setToolTip(reverse ? tr("Reverse split") : tr("Forward split"));
        m_splits->setItem(r, 3, ratio);
    }

    // Expected IPOs first (soonest at the top), then the ones priced, latest first.
    QVector<const CorporateCalendar::Ipo *> ipos;
    for (const CorporateCalendar::Ipo &i : m_cal->ipos())
        if (!i.priced) ipos.push_back(&i);
    for (auto it = m_cal->ipos().crbegin(); it != m_cal->ipos().crend(); ++it)
        if (it->priced) ipos.push_back(&*it);
    m_ipos->setRowCount(ipos.size());
    for (int r = 0; r < ipos.size(); ++r) {
        const CorporateCalendar::Ipo &i = *ipos[r];
        m_ipos->setItem(r, 0, cell(i.symbol, ui.Text));
        auto *name = cell(i.name, ui.TextDim);
        name->setToolTip(std::isnan(i.amount) ? i.name
                                              : tr("%1 · %2 offered").arg(i.name, money(i.amount, 0)));
        m_ipos->setItem(r, 1, name);
        m_ipos->setItem(r, 2, cell(dayLabel(i.date), i.priced ? ui.TextDim : ui.Text));
        m_ipos->setItem(r, 3, cell(i.exchange, ui.TextDim));
        m_ipos->setItem(r, 4, cell(i.price.isEmpty() ? "—" : "$" + i.price, ui.Text));
        m_ipos->setItem(r, 5, cell(i.priced ? tr("Priced") : tr("Expected"), i.priced ? ui.TextDim : ui.Accent, right));
    }

    QStringList parts;
    if (!m_cal->hasData())
        parts << (m_cal->errorString().isEmpty() ? tr("Loading Nasdaq's calendars (about 25 requests)…")
                                                 : tr("Nasdaq: %1").arg(m_cal->errorString()));
    else if (m_cal->fetchedAt().isValid())
        parts << tr("updated %1").arg(loc.toString(m_cal->fetchedAt().toLocalTime(), "d MMM HH:mm"));
    if (m_cal->hasData() && !m_cal->errorString().isEmpty())
        parts << tr("last refresh failed: %1").arg(m_cal->errorString());
    parts << tr("US listings · source: Nasdaq");
    m_status->setText(parts.join("  ·  "));
}

// ---------------------------------------------------------------- MarketCard

MarketCard::MarketCard(const MarketHours &market, const MarketSessions *sessions, QWidget *parent)
    : QWidget(parent)
    , m_market(market)
    , m_sessions(sessions)
    , m_tz(market.tz)
{
}

const QVector<MarketCard::Interval> &MarketCard::intervals(const QDate &today, bool *holiday) const
{
    const QDateTime fetched = m_sessions ? m_sessions->fetchedAt() : QDateTime();
    if (today == m_intervalsDay && fetched == m_intervalsFetched) {
        *holiday = m_intervalsHoliday;
        return m_intervals;
    }
    const QTimeZone &tz = m_tz;
    QVector<Interval> intervals;
    for (QDate d = today.addDays(-3); d <= today.addDays(10); d = d.addDays(1))
        for (const MarketHours::Session &s : m_market.sessions)
            if (s.day == d.dayOfWeek())
                intervals.push_back({QDateTime(d, s.open, tz).toUTC(), QDateTime(d.addDays(s.closeDays), s.close, tz).toUTC()});
    std::sort(intervals.begin(), intervals.end(), [](const Interval &a, const Interval &b) { return a.start < b.start; });

    // A holiday: the exchange should have opened today, but the latest session Yahoo knows of (asked since) is
    // an older one. Future holidays aren't known: the next open assumes a regular day.
    bool closed = false;
    if (const MarketSessions::Session *last = m_sessions ? m_sessions->session(m_market.symbol) : nullptr) {
        auto isToday = [&](const Interval &i) { return i.start.toTimeZone(tz).date() == today; };
        const auto first = std::find_if(intervals.cbegin(), intervals.cend(), isToday);
        if (first != intervals.cend() && fetched >= first->start.addSecs(15 * 60)
            && last->start.toTimeZone(tz).date() < today) {
            closed = true;
            intervals.removeIf(isToday);
        }
    }
    m_intervals = intervals;
    m_intervalsDay = today;
    m_intervalsFetched = fetched;
    m_intervalsHoliday = closed;
    *holiday = closed;
    return m_intervals;
}

MarketCard::State MarketCard::state(const QDateTime &nowUtc) const
{
    const QDate today = nowUtc.toTimeZone(m_tz).date();
    bool holiday = false;
    const QVector<Interval> &intervals = this->intervals(today, &holiday);

    State st;
    const Interval *prev = nullptr, *next = nullptr;
    for (const Interval &i : intervals) {
        if (i.start <= nowUtc && nowUtc < i.end) {
            st.status = Open;
            st.next = i.end;
            st.sessionStart = i.start;
            st.sessionEnd = i.end;
            return st;
        }
        if (i.end <= nowUtc)
            prev = &i;
        else if (!next)
            next = &i;
    }
    if (next) {
        st.next = next->start;
        st.sessionStart = next->start;
        st.sessionEnd = next->end;
    }
    // A short gap between two sessions is a break (lunch in Asia, the daily halt of futures).
    if (prev && next && prev->end.secsTo(next->start) <= 3 * 3600)
        st.status = Break;
    else
        st.status = holiday ? Holiday : Closed;
    return st;
}

void MarketCard::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const State st = state(now);
    const QTimeZone &tz = m_tz;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(ui.ControlBorder, 1));
    p.setBrush(ui.Background);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    const double pad = 12, w = width() - 2 * pad;
    QFont f = font();

    // Status pill, top right.
    const QColor statusColor = st.status == Open ? ui.Up : st.status == Break ? ui.Accent : ui.Down;
    const QString statusText = (st.status == Open      ? tr("Open")
                                : st.status == Break   ? tr("Break")
                                : st.status == Holiday ? tr("Holiday")
                                                       : tr("Closed"))
                                   .toUpper();
    f.setPixelSize(10);
    f.setWeight(QFont::Bold);
    p.setFont(f);
    const double pillW = p.fontMetrics().horizontalAdvance(statusText) + 26, pillH = 22;
    const QRectF pill(width() - pad - pillW, 11, pillW, pillH);
    QColor fill = statusColor;
    fill.setAlpha(30);
    QColor border = statusColor;
    border.setAlpha(110);
    p.setPen(QPen(border, 1));
    p.setBrush(fill);
    p.drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    p.setPen(Qt::NoPen);
    p.setBrush(statusColor);
    p.drawEllipse(QPointF(pill.left() + 10, pill.center().y()), 2.6, 2.6);
    p.setPen(statusColor);
    p.drawText(pill.adjusted(16, 0, -2, 0), Qt::AlignCenter, statusText);

    // Flag, name and country.
    const double textLeft = pad + 24, textW = pill.left() - 8 - textLeft;
    QFont emoji = f;
    emoji.setPixelSize(15);
    emoji.setWeight(QFont::Normal);
    p.setFont(emoji);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 10, 22, 22), Qt::AlignLeft | Qt::AlignVCenter, flag(m_market.country));
    f.setPixelSize(12);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.drawText(QRectF(textLeft, 9, textW, 16), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(m_market.name, Qt::ElideRight, int(textW)));
    f.setPixelSize(11);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(textLeft, 25, textW, 15), Qt::AlignLeft | Qt::AlignVCenter, countryName(m_market.country));

    // Rows: a dim label, then its value.
    double y = 50;
    const double labelW = 84, rowH = 17;
    auto row = [&](const QString &label, const QString &value, const QColor &color, const QString &trailing = {}) {
        p.setPen(ui.TextDim);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.drawText(QRectF(pad, y, labelW, rowH), Qt::AlignLeft | Qt::AlignVCenter, label);
        p.setPen(color);
        p.drawText(QRectF(pad + labelW, y, w - labelW, rowH), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(value, Qt::ElideRight, int(w - labelW - (trailing.isEmpty() ? 0 : 60))));
        if (!trailing.isEmpty()) {
            p.setPen(ui.TextDim);
            p.drawText(QRectF(pad, y, w, rowH), Qt::AlignRight | Qt::AlignVCenter, trailing);
        }
        y += rowH;
    };

    const QString abbrev = tz.abbreviation(st.sessionStart.isValid() ? st.sessionStart : now);
    if (m_market.futures) {
        row(tr("Hours:"), m_market.hours, ui.Text);
        if (st.sessionStart.isValid())
            row(tr("UTC:"), QString("%1 - %2 UTC").arg(st.sessionStart.toString("HH:mm"), st.sessionEnd.toString("HH:mm")),
                ui.Text);
        else
            row(tr("UTC:"), "—", ui.TextFaint);
        row(tr("Break:"), m_market.breakText, ui.TextDim);
    } else {
        // Hours of a regular day (Monday's sessions): open to close, and the lunch break if any.
        QVector<MarketHours::Session> day;
        for (const MarketHours::Session &s : m_market.sessions)
            if (s.day == Qt::Monday) day.push_back(s);
        if (!day.isEmpty()) {
            row(tr("Hours:"), QString("%1 - %2 %3").arg(day.first().open.toString("HH:mm"),
                                                        day.last().close.toString("HH:mm"), abbrev),
                ui.Text);
            if (day.size() > 1)
                row(tr("Lunch:"), QString("%1 - %2").arg(day[0].close.toString("HH:mm"), day[1].open.toString("HH:mm")),
                    ui.TextDim);
            else
                row(tr("Lunch:"), tr("none"), ui.TextFaint);
        }
    }

    // Time to the next change, and when it happens in the viewer's time.
    if (st.next.isValid()) {
        const QDateTime local = st.next.toLocalTime();
        const QString at = local.date() == QDate::currentDate()
                               ? local.toString("HH:mm")
                               : QLocale().toString(local, "ddd HH:mm");
        const QString label = st.status == Open ? tr("Closes:") : st.status == Break ? tr("Resumes:") : tr("Opens:");
        f.setWeight(QFont::DemiBold);
        row(label, countdown(now.secsTo(st.next)), st.status == Open ? ui.Up : st.status == Break ? ui.Accent : ui.Down, at);
    }
}

// ---------------------------------------------------------------- CardGrid

CardGrid::CardGrid(int cardHeight, QWidget *parent)
    : QWidget(parent)
    , m_cardHeight(cardHeight)
{
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void CardGrid::addCard(QWidget *card)
{
    card->setParent(this);
    m_cards.push_back(card);
    updateGeometry();
}

int CardGrid::columns(int w) const
{
    return std::clamp((w + 10) / (250 + 10), 1, 8);
}

int CardGrid::heightForWidth(int w) const
{
    const int cols = columns(w);
    const int rows = (int(m_cards.size()) + cols - 1) / cols;
    return rows * m_cardHeight + std::max(0, rows - 1) * 10;
}

void CardGrid::resizeEvent(QResizeEvent *)
{
    const int gap = 10, cols = columns(width());
    const double cw = (width() - gap * (cols - 1)) / double(cols);
    for (int i = 0; i < m_cards.size(); ++i) {
        const int c = i % cols, r = i / cols;
        const int x = int(std::round(c * (cw + gap)));
        const int x2 = int(std::round(c * (cw + gap) + cw));
        m_cards[i]->setGeometry(x, r * (m_cardHeight + gap), x2 - x, m_cardHeight);
    }
}

// ---------------------------------------------------------------- MarketHoursView

QVector<MarketHours> MarketHoursView::stockExchanges()
{
    using S = MarketHours::Session;
    // Monday to Friday, opening to closing, with a lunch break where the exchange has one (local times).
    auto weekdays = [](QTime open, QTime close, QTime lunchStart = {}, QTime lunchEnd = {}) {
        QVector<S> out;
        for (int d = Qt::Monday; d <= Qt::Friday; ++d) {
            if (lunchStart.isValid()) {
                out.push_back({d, open, 0, lunchStart});
                out.push_back({d, lunchEnd, 0, close});
            } else {
                out.push_back({d, open, 0, close});
            }
        }
        return out;
    };
    auto t = [](int h, int m) { return QTime(h, m); };
    // B3 trades an hour longer while New York is on standard time (November to March), to keep its overlap
    // with Wall Street.
    // Decided for the coming trading day (today on a weekday); the page is rebuilt often enough (language, theme)
    // that a session or two at most can be off after the switch.
    QDate day = QDate::currentDate();
    while (day.dayOfWeek() > 5)
        day = day.addDays(1);
    const bool nyDst = QTimeZone("America/New_York").isDaylightTime(QDateTime(day, QTime(12, 0), QTimeZone("America/New_York")));
    return {
        {tr("Euronext Amsterdam"), "NL", "Europe/Amsterdam", "^AEX", weekdays(t(9, 0), t(17, 30))},
        {tr("Euronext Brussels"), "BE", "Europe/Brussels", "^BFX", weekdays(t(9, 0), t(17, 30))},
        {tr("Euronext Paris"), "FR", "Europe/Paris", "^FCHI", weekdays(t(9, 0), t(17, 30))},
        {tr("Frankfurt Stock Exchange (Xetra)"), "DE", "Europe/Berlin", "^GDAXI", weekdays(t(9, 0), t(17, 30))},
        {tr("London Stock Exchange"), "GB", "Europe/London", "^FTSE", weekdays(t(8, 0), t(16, 30))},
        {tr("Euronext Lisbon"), "PT", "Europe/Lisbon", "PSI20.LS", weekdays(t(8, 0), t(16, 30))},
        {tr("B3 - Brasil Bolsa Balcão"), "BR", "America/Sao_Paulo", "^BVSP",
         weekdays(t(10, 0), nyDst ? t(17, 0) : t(18, 0))},
        {tr("NASDAQ Stock Market"), "US", "America/New_York", "^IXIC", weekdays(t(9, 30), t(16, 0))},
        {tr("New York Stock Exchange"), "US", "America/New_York", "^NYA", weekdays(t(9, 30), t(16, 0))},
        {tr("Toronto Stock Exchange"), "CA", "America/Toronto", "^GSPTSE", weekdays(t(9, 30), t(16, 0))},
        {tr("TSX Venture Exchange"), "CA", "America/Toronto", "^SPCDNX", weekdays(t(9, 30), t(16, 0))},
        {tr("Australian Securities Exchange"), "AU", "Australia/Sydney", "^AXJO", weekdays(t(10, 0), t(16, 0))},
        {tr("Korea Exchange"), "KR", "Asia/Seoul", "^KS11", weekdays(t(9, 0), t(15, 30))},
        {tr("Tokyo Stock Exchange"), "JP", "Asia/Tokyo", "^N225", weekdays(t(9, 0), t(15, 30), t(11, 30), t(12, 30))},
        {tr("Singapore Exchange"), "SG", "Asia/Singapore", "^STI", weekdays(t(9, 0), t(17, 0), t(12, 0), t(13, 0))},
        {tr("Hong Kong Stock Exchange"), "HK", "Asia/Hong_Kong", "^HSI", weekdays(t(9, 30), t(16, 0), t(12, 0), t(13, 0))},
        {tr("Shanghai Stock Exchange"), "CN", "Asia/Shanghai", "000001.SS",
         weekdays(t(9, 30), t(15, 0), t(11, 30), t(13, 0))},
        {tr("Shenzhen Stock Exchange"), "CN", "Asia/Shanghai", "399001.SZ",
         weekdays(t(9, 30), t(15, 0), t(11, 30), t(13, 0))},
        {tr("Bombay Stock Exchange"), "IN", "Asia/Kolkata", "^BSESN", weekdays(t(9, 15), t(15, 30))},
        {tr("National Stock Exchange of India"), "IN", "Asia/Kolkata", "^NSEI", weekdays(t(9, 15), t(15, 30))},
        {tr("Johannesburg Stock Exchange"), "ZA", "Africa/Johannesburg", "^J203.JO", weekdays(t(9, 0), t(17, 0))},
    };
}

QVector<MarketHours> MarketHoursView::futuresMarkets()
{
    using S = MarketHours::Session;
    // Electronic sessions that open the evening before (Sunday to Thursday) and close the next afternoon.
    auto overnight = [](QTime open, QTime close) {
        QVector<S> out;
        for (int d : {int(Qt::Sunday), int(Qt::Monday), int(Qt::Tuesday), int(Qt::Wednesday), int(Qt::Thursday)})
            out.push_back({d, open, 1, close});
        return out;
    };
    auto weekdays = [](QTime open, QTime close) {
        QVector<S> out;
        for (int d = Qt::Monday; d <= Qt::Friday; ++d)
            out.push_back({d, open, 0, close});
        return out;
    };
    MarketHours cme {tr("CME Group (Globex)"), "US", "America/Chicago", {}, overnight(QTime(17, 0), QTime(16, 0)),
                     tr("Sun 17:00 - Fri 16:00 CT"), tr("daily 16:00 - 17:00 CT"), true};
    MarketHours eurex {tr("Eurex Exchange"), "DE", "Europe/Berlin", {}, weekdays(QTime(1, 0), QTime(22, 0)),
                       tr("Mon - Fri 01:00 - 22:00 CET"), tr("overnight, 22:00 - 01:00"), true};
    MarketHours iceUs {tr("ICE Futures U.S."), "US", "America/New_York", {}, overnight(QTime(18, 0), QTime(17, 0)),
                       tr("Sun 18:00 - Fri 17:00 ET"), tr("daily 17:00 - 18:00 ET"), true};
    MarketHours iceEu {tr("ICE Futures Europe"), "GB", "Europe/London", {}, weekdays(QTime(1, 0), QTime(23, 0)),
                       tr("Mon - Fri 01:00 - 23:00 UK"), tr("overnight, ~2 hours"), true};
    MarketHours lme {tr("London Metal Exchange (LMEselect)"), "GB", "Europe/London", {}, weekdays(QTime(1, 0), QTime(19, 0)),
                     tr("Mon - Fri 01:00 - 19:00 UK"), tr("overnight, ~6 hours"), true};
    return {cme, eurex, iceUs, iceEu, lme};
}

QStringList MarketHoursView::sessionSymbols()
{
    QStringList out;
    for (const MarketHours &m : stockExchanges())
        out << m.symbol;
    return out;
}

namespace {

QWidget *section(const QString &title, QLabel **count, CardGrid *grid)
{
    auto *frame = new QFrame;
    frame->setObjectName("calSection");
    auto *v = new QVBoxLayout(frame);
    v->setContentsMargins(16, 12, 16, 16);
    v->setSpacing(10);
    auto *head = new QHBoxLayout;
    auto *t = new QLabel(title);
    t->setObjectName("calSectionTitle");
    head->addWidget(t);
    head->addStretch(1);
    *count = new QLabel;
    (*count)->setObjectName("calSectionCount");
    head->addWidget(*count);
    v->addLayout(head);
    auto *line = new QFrame;
    line->setObjectName("divider");
    line->setFixedHeight(1);
    v->addWidget(line);
    v->addWidget(grid);
    return frame;
}

} // namespace

MarketHoursView::MarketHoursView(MarketSessions *sessions, QWidget *parent)
    : QWidget(parent)
    , m_sessions(sessions)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName("calContent");
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(24, 22, 24, 24);
    l->setSpacing(6);

    auto *title = new QLabel(tr("Global Market Hours"));
    title->setObjectName("calHeroTitle");
    title->setAlignment(Qt::AlignCenter);
    l->addWidget(title);
    auto *subtitle = new QLabel(tr("Real-time status of markets around the world"));
    subtitle->setObjectName("calHeroSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    l->addWidget(subtitle);
    l->addSpacing(6);
    m_clock = new QLabel;
    m_clock->setObjectName("calClockPill");
    l->addWidget(m_clock, 0, Qt::AlignHCenter);
    l->addSpacing(18);

    auto *stocks = new CardGrid(124);
    for (const MarketHours &m : stockExchanges()) {
        auto *card = new MarketCard(m, m_sessions);
        stocks->addCard(card);
        m_stockCards.push_back(card);
    }
    l->addWidget(section(tr("Stock Exchanges"), &m_stockCount, stocks));
    l->addSpacing(14);
    auto *futures = new CardGrid(124);
    for (const MarketHours &m : futuresMarkets()) {
        auto *card = new MarketCard(m, nullptr);
        futures->addCard(card);
        m_futuresCards.push_back(card);
    }
    l->addWidget(section(tr("Futures Markets"), &m_futuresCount, futures));
    l->addSpacing(10);
    auto *note = new QLabel(tr("Regular sessions in each exchange's local time; the time on the right of a card is "
                               "yours. Holidays are caught on the day from Yahoo Finance (an exchange that hasn't "
                               "opened when it should have); future ones aren't known, nor pre- and after-market "
                               "sessions."));
    note->setObjectName("calStatus");
    note->setWordWrap(true);
    l->addWidget(note);
    l->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &MarketHoursView::tick);
    connect(m_sessions, &RemoteDataSet::updated, this, &MarketHoursView::tick);
    tick();
}

void MarketHoursView::showEvent(QShowEvent *)
{
    m_sessions->refreshIfStale();
    m_timer.start();
    tick();
}

void MarketHoursView::hideEvent(QHideEvent *)
{
    m_timer.stop();
}

void MarketHoursView::tick()
{
    // Yahoo's sessions are a quarter of an hour fresh at most while the tab is shown.
    if (++m_ticks % 60 == 0)
        m_sessions->refreshIfStale();
    const QDateTime now = QDateTime::currentDateTime();
    m_clock->setText(QString("🌐  %1  ·  %2 %3")
                         .arg(timeZoneLabel(), QLocale().toString(now.time(), "HH:mm:ss"), now.timeZoneAbbreviation()));
    const QDateTime utc = now.toUTC();
    auto count = [&](const QVector<MarketCard *> &cards, QLabel *label) {
        int open = 0;
        for (MarketCard *c : cards) {
            if (c->state(utc).status == MarketCard::Open) ++open;
            c->update();
        }
        label->setText(tr("%1 markets · %2 open").arg(cards.size()).arg(open));
    };
    count(m_stockCards, m_stockCount);
    count(m_futuresCards, m_futuresCount);
}

// ---------------------------------------------------------------- CalendarPage

CalendarPage::CalendarPage(EconomicCalendar *economic, CorporateCalendar *corporate, MarketSessions *sessions,
                           QWidget *parent)
    : QWidget(parent)
{
    setObjectName("calendarPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(monoFont());
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Sub-header, right under the main one.
    auto *bar = new QWidget;
    bar->setObjectName("calSubBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(36);
    auto *h = new qtm::kit::WrapBarLayout(bar, 4);
    h->setContentsMargins(12, 0, 12, 0);
    m_tabs = new QButtonGroup(this);
    const QStringList names {tr("Economic"), tr("Corporate"), tr("Market hours")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("calSubTab");
        b->setText(names[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        h->addWidget(b);
    }
    v->addWidget(bar);

    m_stack = new QStackedWidget;
    m_stack->addWidget(new EconomicView(economic));
    m_stack->addWidget(new CorporateView(corporate));
    m_stack->addWidget(new MarketHoursView(sessions));
    v->addWidget(m_stack, 1);
    connect(m_tabs, &QButtonGroup::idClicked, this, &CalendarPage::showTab);
    showTab(std::clamp(QSettings().value("calendar/tab", 0).toInt(), 0, 2));
}

void CalendarPage::showTab(int index)
{
    m_tabs->button(index)->setChecked(true);
    m_stack->setCurrentIndex(index);
    QSettings().setValue("calendar/tab", index);
}
