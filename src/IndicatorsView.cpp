#include "IndicatorsView.h"

#include <QCoreApplication>

#include "IndicatorData.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTableWidget>
#include <QTextStream>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

using namespace countries;

namespace {

enum Column { Country, Last, Previous, Reference, Unit, Next, ColumnCount };
constexpr int SortRole = Qt::UserRole;
constexpr int IsoRole = Qt::UserRole + 1;

// A cell sorted by its SortRole (a number, a time) when it has one, else by its text; empty cells last.
class Cell : public QTableWidgetItem
{
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem &other) const override
    {
        const QVariant a = data(SortRole), b = other.data(SortRole);
        const bool ascending = !tableWidget() || tableWidget()->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder;
        if (a.isValid() != b.isValid())
            return ascending ? a.isValid() : !a.isValid();   // empty cells at the bottom either way
        if (a.isValid())
            return a.toDouble() < b.toDouble();
        if (text().isEmpty() != other.text().isEmpty())
            return ascending ? !text().isEmpty() : text().isEmpty();
        return text().localeAwareCompare(other.text()) < 0;
    }
};

Cell *cell(const QString &text = {})
{
    auto *it = new Cell(text);
    it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return it;
}

QScrollArea *scrollArea(QWidget *content)
{
    auto *scroll = new QScrollArea;
    scroll->setObjectName("ecoScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    return scroll;
}

// "13 Nov 2026 14:30", local time; the date alone for a release at midnight UTC (a day without an hour).
QString releaseText(const QDateTime &utc)
{
    if (!utc.isValid())
        return {};
    const QLocale loc;
    if (utc.time() == QTime(0, 0))
        return loc.toString(utc.date(), "d MMM yyyy");
    return loc.toString(utc.toLocalTime(), "d MMM yyyy HH:mm");
}

} // namespace

// ---------------------------------------------------------------- IndicatorPage

IndicatorPage::IndicatorPage(const ListItem &item, QWidget *parent)
    : QWidget(parent)
    , m_name(QCoreApplication::translate("CountryIndicators", item.name.toUtf8().constData()))
    , m_loader(new IndicatorLoader(item, this))
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *bar = new QWidget;
    bar->setObjectName("ctryBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(40);
    auto *bh = new QHBoxLayout(bar);
    bh->setContentsMargins(10, 0, 12, 0);
    bh->setSpacing(10);
    auto *backButton = new QToolButton;
    backButton->setObjectName("ctryBack");
    backButton->setText("‹ " + tr("Indicators"));
    backButton->setCursor(Qt::PointingHandCursor);
    connect(backButton, &QToolButton::clicked, this, &IndicatorPage::back);
    bh->addWidget(backButton);
    auto *title = new QLabel(text(item.name));
    title->setObjectName("ctryTitle");
    bh->addWidget(title);
    bh->addStretch(1);
    m_status = new QLabel;
    m_status->setObjectName("ecoStatus");
    bh->addWidget(m_status);
    auto *exportButton = new QPushButton(tr("Export CSV").toUpper());
    exportButton->setObjectName("newsRefresh");
    exportButton->setCursor(Qt::PointingHandCursor);
    exportButton->setToolTip(tr("Save the table as a CSV file"));
    connect(exportButton, &QPushButton::clicked, this, &IndicatorPage::exportCsv);
    bh->addWidget(exportButton);
    m_refresh = new QPushButton(tr("Refresh").toUpper());
    m_refresh->setObjectName("newsRefresh");
    m_refresh->setCursor(Qt::PointingHandCursor);
    m_refresh->setToolTip(tr("Download this indicator again for every country"));
    connect(m_refresh, &QPushButton::clicked, m_loader, &IndicatorLoader::refresh);
    bh->addWidget(m_refresh);
    v->addWidget(bar);

    // Regions: every country, or one of the catalogue's groups.
    auto *nav = new QWidget;
    nav->setObjectName("qtSubBar");
    nav->setAttribute(Qt::WA_StyledBackground);
    nav->setFixedHeight(34);
    auto *nh = new QHBoxLayout(nav);
    nh->setContentsMargins(6, 0, 12, 0);
    nh->setSpacing(2);
    m_regions = new QButtonGroup(this);
    const QVector<Group> &groups = catalogue().groups;
    for (int i = 0; i <= groups.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText((i == 0 ? tr("All") : text(groups[i - 1].name)).toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(34);
        m_regions->addButton(b, i);
        nh->addWidget(b);
    }
    nh->addStretch(1);
    v->addWidget(nav);

    auto *content = new QWidget;
    content->setObjectName("ecoContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto *cv = new QVBoxLayout(content);
    cv->setContentsMargins(14, 12, 14, 10);
    cv->setSpacing(8);
    const QStringList countries = m_loader->countries();
    m_table = new QTableWidget(int(countries.size()), ColumnCount);
    m_table->setObjectName("ctryTable");
    m_table->setHorizontalHeaderLabels({tr("Country"), tr("Last"), tr("Previous"), tr("Reference"), tr("Unit"), tr("Next release")});
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setWordWrap(false);
    QHeaderView *h = m_table->horizontalHeader();
    h->setHighlightSections(false);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(Country, QHeaderView::Stretch);
    h->resizeSection(Last, 120);
    h->resizeSection(Previous, 120);
    h->resizeSection(Reference, 110);
    h->resizeSection(Unit, 200);
    h->resizeSection(Next, 160);
    h->setSortIndicatorShown(true);
    QFont bold = m_table->font();
    bold.setBold(true);
    for (int r = 0; r < countries.size(); ++r) {
        auto *name = cell(countryName(countries[r]));
        name->setFont(bold);
        name->setData(IsoRole, countries[r]);
        m_table->setItem(r, Country, name);
        m_rows.insert(countries[r], name);
        for (int c = Last; c < ColumnCount; ++c)
            m_table->setItem(r, c, cell());
        m_table->item(r, Last)->setText("…");
        m_table->item(r, Last)->setForeground(Theme::current().TextFaint);
    }
    m_table->setColumnHidden(Next, true);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(Country, Qt::AscendingOrder);
    cv->addWidget(m_table, 1);
    m_note = new QLabel;
    m_note->setObjectName("ctryNote");
    m_note->setWordWrap(true);
    cv->addWidget(m_note);
    v->addWidget(content, 1);

    connect(m_regions, &QButtonGroup::idClicked, this, &IndicatorPage::filterRegion);
    connect(m_loader, &IndicatorLoader::rowChanged, this, &IndicatorPage::updateRow);
    connect(m_loader, &IndicatorLoader::busyChanged, this, &IndicatorPage::updateStatus);
    ReleaseCalendar *calendar = ReleaseCalendar::instance();
    connect(calendar, &ReleaseCalendar::updated, this, &IndicatorPage::updateReleases);
    calendar->load();

    const int region = std::clamp(QSettings().value("economy/indicatorRegion", 0).toInt(), 0, int(groups.size()));
    m_regions->button(region)->setChecked(true);
    filterRegion(region);
    for (const QString &iso3 : countries)
        updateRow(iso3);
    updateReleases();
    updateStatus();
}

void IndicatorPage::updateRow(const QString &iso3)
{
    QTableWidgetItem *name = m_rows.value(iso3);
    if (!name)
        return;
    const Row row = m_loader->row(iso3);
    // Cells change their row as they are sorted: sorting waits for the row to be filled in.
    m_table->setSortingEnabled(false);
    const int r = name->row();
    const QColor down = Theme::current().Down, normal = Theme::current().Text, faint = Theme::current().TextFaint;
    if (row.state == Row::Pending) {
        for (int c = Last; c < Next; ++c) {
            m_table->item(r, c)->setText({});
            m_table->item(r, c)->setData(SortRole, {});
        }
        m_table->item(r, Last)->setText("…");
        m_table->item(r, Last)->setForeground(faint);
    } else if (row.state == Row::Found) {
        const bool currency = m_loader->item().ids.value(0) == "currency";
        const double values[] = {row.last, row.previous};
        for (int k = 0; k < 2; ++k) {
            QTableWidgetItem *c = m_table->item(r, Last + k);
            // Exchange rates below 10 (EUR/USD 1.1234) need four decimals.
            c->setText(formatValue(values[k], currency && std::abs(values[k]) < 10 ? 4 : -1));
            c->setData(SortRole, std::isfinite(values[k]) ? QVariant(values[k]) : QVariant());
            c->setForeground(std::isfinite(values[k]) && values[k] < 0 ? down : normal);
        }
        QTableWidgetItem *ref = m_table->item(r, Reference);
        ref->setText(periodLabel(row.date, row.freq));
        ref->setData(SortRole, row.date.isValid() ? QVariant(double(row.date.toJulianDay())) : QVariant());
        m_table->item(r, Unit)->setText(row.unitText());
        m_table->item(r, Unit)->setForeground(Theme::current().TextDim);
        QString tip = row.source + (row.code.isEmpty() ? QString() : " · " + row.code);
        if (!row.note.isEmpty())
            tip += "\n" + text(row.note);
        if (const QString fusion = CountryLoader::fusionTip(row); !fusion.isEmpty())
            tip += "\n\n" + fusion;
        for (int c = Country; c < ColumnCount; ++c)
            m_table->item(r, c)->setToolTip(tip);
    }
    m_table->setSortingEnabled(true);
    applyHidden();
}

void IndicatorPage::applyHidden()
{
    // Rows of the region with a value or still loading; set again after each sort, rows having moved.
    const int region = m_regions->checkedId();
    const QStringList members = region > 0 ? catalogue().groups.value(region - 1).countries : QStringList();
    for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) {
        const bool inRegion = region <= 0 || members.contains(it.key());
        m_table->setRowHidden(it.value()->row(), !inRegion || m_loader->row(it.key()).state == Row::Missing);
    }
}

void IndicatorPage::updateReleases()
{
    const ReleaseCalendar *calendar = ReleaseCalendar::instance();
    const ListItem &item = m_loader->item();
    const bool shown = calendar->covers(item);
    m_table->setColumnHidden(Next, !shown);
    if (!shown)
        return;
    m_table->setSortingEnabled(false);
    for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) {
        const ReleaseCalendar::Release next = calendar->next(it.key(), item);
        QTableWidgetItem *c = m_table->item(it.value()->row(), Next);
        c->setText(releaseText(next.time));
        c->setData(SortRole, next.time.isValid() ? QVariant(double(next.time.toSecsSinceEpoch())) : QVariant());
        c->setForeground(Theme::current().TextDim);
        c->setToolTip(next.period.isEmpty() ? QString() : tr("The figure for %1").arg(next.period));
    }
    m_table->setSortingEnabled(true);
    applyHidden();
}

void IndicatorPage::filterRegion(int index)
{
    QSettings().setValue("economy/indicatorRegion", index);
    applyHidden();
    updateStatus();
}

void IndicatorPage::updateStatus()
{
    const int pending = m_loader->pending();
    m_refresh->setEnabled(pending == 0);
    if (pending > 0) {
        m_status->setText(tr("Updating… %n country(ies) left", "", pending).toUpper());
        m_status->setProperty("state", "busy");
    } else {
        m_status->setText({});
        m_status->setProperty("state", "ok");
    }
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);

    // Under the table: how many countries it has, and those left out for want of a key.
    const int region = m_regions->checkedId();
    const QStringList members = region > 0 ? catalogue().groups.value(region - 1).countries : m_loader->countries();
    int found = 0;
    QStringList keyed;
    for (const QString &iso3 : members) {
        const Row row = m_loader->row(iso3);
        if (row.state == Row::Found)
            ++found;
        else if (row.state == Row::Missing && row.needsKey)
            keyed << countryName(iso3);
    }
    QStringList lines;
    if (pending == 0 && found < members.size())
        lines << tr("%1 of %2 countries: the others have no free source for this indicator.").arg(found).arg(members.size());
    if (!keyed.isEmpty())
        lines << tr("A free API key, saved in Sources, would add: %1.").arg(keyed.join(", "));
    m_note->setText(lines.join("\n"));
    m_note->setVisible(!lines.isEmpty());
}

void IndicatorPage::exportCsv()
{
    const ListItem &item = m_loader->item();
    QString file = item.name.toLower();
    file.replace(QRegularExpression("[^a-z0-9]+"), "-");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + file + ".csv",
        tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    QTextStream out(&f);
    auto quote = [](QString s) { return s.contains(',') || s.contains('"') ? '"' + s.replace('"', "\"\"") + '"' : s; };
    auto num = [](double v) { return std::isfinite(v) ? QString::number(v, 'g', 12) : QString(); };
    out << "Country,ISO3,Last,Previous,Reference,Unit,Next release,Source,Code\n";
    // In the table's order, the rows shown.
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->isRowHidden(r))
            continue;
        const QString iso3 = m_table->item(r, Country)->data(IsoRole).toString();
        const Row row = m_loader->row(iso3);
        if (row.state != Row::Found)
            continue;
        const QDateTime next = ReleaseCalendar::instance()->next(iso3, item).time;
        out << quote(countryName(iso3)) << ',' << iso3 << ',' << num(row.last) << ',' << num(row.previous) << ','
            << row.date.toString(Qt::ISODate) << ',' << quote(QString(row.unit).replace("{CUR}", row.currency)) << ','
            << (next.isValid() ? next.toString(Qt::ISODate) : QString()) << ',' << quote(row.source) << ','
            << quote(row.code) << '\n';
    }
}

// ---------------------------------------------------------------- IndicatorsView

IndicatorsView::IndicatorsView(QWidget *parent)
    : QWidget(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget;
    v->addWidget(m_stack);

    auto *content = new QWidget;
    content->setObjectName("ecoContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto *cv = new QVBoxLayout(content);
    cv->setContentsMargins(18, 14, 18, 18);
    cv->setSpacing(14);
    m_search = new QLineEdit;
    m_search->setObjectName("newsFilter");
    m_search->setPlaceholderText(tr("Search indicators…"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedSize(240, 28);
    connect(m_search, &QLineEdit::textChanged, this, &IndicatorsView::filter);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        for (const Item &it : std::as_const(m_items))
            if (!it.button->isHidden() && it.button->isEnabled()) {
                it.button->click();
                return;
            }
    });
    cv->addWidget(m_search);

    // Four columns of sections, as the list's file places them.
    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    QVector<QVBoxLayout *> column;
    for (int i = 0; i < 4; ++i) {
        auto *c = new QVBoxLayout;
        c->setSpacing(14);
        column << c;
        columns->addLayout(c, 1);
    }
    for (const ListSection &s : indicatorList()) {
        auto *box = new QFrame;
        box->setObjectName("ctryGroup");
        auto *bv = new QVBoxLayout(box);
        bv->setContentsMargins(0, 0, 0, 0);
        bv->setSpacing(0);
        auto *head = new QLabel((s.star ? "★ " : "") + text(s.name));
        head->setObjectName("ctryGroupTitle");
        bv->addWidget(head);
        for (const ListItem &item : s.items) {
            auto *b = new QPushButton(text(item.name));
            b->setObjectName("ctryItem");
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            if (item.ids.isEmpty()) {
                b->setEnabled(false);
                b->setToolTip(text(item.why));
            }
            connect(b, &QPushButton::clicked, this, [this, name = item.name] { openIndicator(name); });
            bv->addWidget(b);
            m_items.push_back({b, text(item.name), item.name, box});
        }
        column[std::clamp(s.column, 0, 3)]->addWidget(box);
        m_sections << box;
    }
    for (QVBoxLayout *c : std::as_const(column))
        c->addStretch(1);
    cv->addLayout(columns);
    cv->addStretch(1);
    m_stack->addWidget(scrollArea(content));
}

void IndicatorsView::filter(const QString &query)
{
    const QString q = query.trimmed();
    QHash<QWidget *, int> shown;
    for (const Item &it : std::as_const(m_items)) {
        const bool match = q.isEmpty() || it.name.contains(q, Qt::CaseInsensitive) || it.english.contains(q, Qt::CaseInsensitive);
        it.button->setVisible(match);
        if (match)
            shown[it.section]++;
    }
    for (QWidget *s : std::as_const(m_sections))
        s->setVisible(shown.value(s) > 0);
}

bool IndicatorsView::openIndicator(const QString &name)
{
    const ListItem *item = findListItem(name);
    if (!item || item->ids.isEmpty())
        return false;
    if (m_page) {
        m_stack->removeWidget(m_page);
        m_page->deleteLater();
    }
    m_page = new IndicatorPage(*item);
    connect(m_page, &IndicatorPage::back, this, &IndicatorsView::showList);
    m_stack->addWidget(m_page);
    m_stack->setCurrentWidget(m_page);
    return true;
}

void IndicatorsView::showList()
{
    m_stack->setCurrentIndex(0);
    if (m_page) {
        m_stack->removeWidget(m_page);
        m_page->deleteLater();
    }
}

QString IndicatorsView::aiContext() const
{
    return m_page && m_stack->currentWidget() == m_page ? m_page->name() : QString();
}
