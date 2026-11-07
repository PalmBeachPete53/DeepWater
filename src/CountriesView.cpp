#include "CountriesView.h"

#include "ReleaseWatcher.h"

#include "CountryData.h"
#include "Theme.h"

#include "quant/ui/PanelsKit.h"

#include <QButtonGroup>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTableWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

using namespace countries;

namespace {

enum Column { Name, Last, Previous, Highest, Lowest, Unit, Date, Source, ColumnCount };

QScrollArea *scrollArea(QWidget *content)
{
    auto *scroll = new QScrollArea;
    scroll->setObjectName("ecoScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

QTableWidgetItem *item(const QString &text = {})
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return it;
}

} // namespace

// ---------------------------------------------------------------- CountryPage

CountryPage::CountryPage(const QString &iso3, QWidget *parent)
    : QWidget(parent)
    , m_iso3(iso3)
    , m_loader(new CountryLoader(iso3, this))
{
    // A figure of this country has come out: its table reads again what has aged, if it is the one on screen.
    if (auto *watcher = ReleaseWatcher::instance())
        connect(watcher, &ReleaseWatcher::sourcesAsked, this, [this](const QString &country, const QStringList &) {
            const QString mine = m_iso3 == "EMU" ? QStringLiteral("EU") : countries::iso2(m_iso3);
            if (country == mine && isVisible())
                m_loader->refresh(false);
        });
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *bar = new QWidget;
    bar->setObjectName("ctryBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(40);
    auto *bh = new QHBoxLayout(bar);
    bh->setContentsMargins(10, 0, 30, 0);   // clear of the side flaps
    bh->setSpacing(10);
    auto *backButton = new QToolButton;
    backButton->setObjectName("ctryBack");
    backButton->setText("‹ " + tr("Countries"));
    backButton->setCursor(Qt::PointingHandCursor);
    connect(backButton, &QToolButton::clicked, this, &CountryPage::back);
    bh->addWidget(backButton);
    auto *title = new QLabel(tr("%1 Indicators").arg(countryName(iso3)));
    title->setObjectName("ctryTitle");
    bh->addWidget(title);
    bh->addStretch(1);
    m_status = new QLabel;
    m_status->setObjectName("ecoStatus");
    bh->addWidget(m_status);
    auto *exportButton = new QPushButton(tr("Export CSV").toUpper());
    exportButton->setObjectName("newsRefresh");
    exportButton->setCursor(Qt::PointingHandCursor);
    exportButton->setToolTip(tr("Save the table of this tab as a CSV file"));
    connect(exportButton, &QPushButton::clicked, this, &CountryPage::exportCsv);
    bh->addWidget(exportButton);
    m_refresh = new QPushButton(tr("Refresh").toUpper());
    m_refresh->setObjectName("newsRefresh");
    m_refresh->setCursor(Qt::PointingHandCursor);
    m_refresh->setToolTip(tr("Download this country's indicators again"));
    connect(m_refresh, &QPushButton::clicked, m_loader, &CountryLoader::refresh);
    bh->addWidget(m_refresh);
    v->addWidget(bar);

    auto *nav = new QWidget;
    nav->setObjectName("qtSubBar");
    nav->setAttribute(Qt::WA_StyledBackground);
    nav->setFixedHeight(34);
    auto *nh = new qtm::kit::WrapBarLayout(nav, 2);
    nh->setContentsMargins(6, 0, 12, 0);
    m_tabs = new QButtonGroup(this);
    const Catalogue &cat = catalogue();
    for (int i = 0; i < cat.tabs.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(text(cat.tabs[i].name).toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(34);
        m_tabs->addButton(b, i);
        nh->addWidget(b);
    }
    v->addWidget(nav);

    m_stack = new QStackedWidget;
    v->addWidget(m_stack, 1);
    m_built.resize(cat.tabs.size());
    for (int i = 0; i < cat.tabs.size(); ++i)
        m_stack->addWidget(new QWidget);

    connect(m_tabs, &QButtonGroup::idClicked, this, &CountryPage::showTab);
    connect(m_loader, &CountryLoader::rowChanged, this, &CountryPage::updateRow);
    connect(m_loader, &CountryLoader::busyChanged, this, &CountryPage::updateStatus);
    updateStatus();
}

void CountryPage::showTab(int index)
{
    if (index < 0 || index >= m_built.size())
        return;
    m_tabs->button(index)->setChecked(true);
    QSettings().setValue("economy/countryTab", index);
    if (!m_built[index].page) {
        QWidget *page = buildTab(index);
        QWidget *placeholder = m_stack->widget(index);
        m_stack->insertWidget(index, page);
        m_stack->removeWidget(placeholder);
        placeholder->deleteLater();
        m_loader->loadTab(index);
        updateTab(index);
    }
    m_stack->setCurrentIndex(index);
}

QWidget *CountryPage::buildTab(int index)
{
    const TabDef &def = catalogue().tabs[index];
    auto *content = new QWidget;
    content->setObjectName("ecoContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(12);
    Tab &tab = m_built[index];
    for (const QStringList &ids : def.tables) {
        auto *t = new QTableWidget(ids.size(), ColumnCount);
        t->setObjectName("ctryTable");
        t->setHorizontalHeaderLabels({QString(), tr("Last"), tr("Previous"), tr("Highest"), tr("Lowest"), tr("Unit"),
                                      tr("Period"), tr("Source")});
        t->verticalHeader()->setVisible(false);
        t->verticalHeader()->setDefaultSectionSize(30);
        t->setSelectionMode(QAbstractItemView::NoSelection);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setShowGrid(false);
        t->setFocusPolicy(Qt::NoFocus);
        t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        t->setWordWrap(false);
        QHeaderView *h = t->horizontalHeader();
        h->setHighlightSections(false);
        h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        h->setSectionResizeMode(Name, QHeaderView::Stretch);
        for (int c = Last; c <= Lowest; ++c)
            h->resizeSection(c, 110);
        h->resizeSection(Unit, 170);
        h->resizeSection(Date, 110);
        h->resizeSection(Source, 200);
        QFont bold = t->font();
        bold.setBold(true);
        for (int r = 0; r < ids.size(); ++r) {
            const IndicatorDef ind = catalogue().indicators.value(ids[r]);
            auto *name = item(text(ind.name));
            name->setFont(bold);
            t->setItem(r, Name, name);
            for (int c = Last; c < ColumnCount; ++c)
                t->setItem(r, c, item());
            t->item(r, Last)->setText("…");
            t->item(r, Last)->setForeground(Theme::current().TextFaint);
        }
        v->addWidget(t);
        tab.tables.push_back({t, ids});
    }
    tab.note = new QLabel;
    tab.note->setObjectName("ctryNote");
    tab.note->setWordWrap(true);
    v->addWidget(tab.note);
    v->addStretch(1);
    tab.page = scrollArea(content);
    return tab.page;
}

void CountryPage::updateRow(const QString &id)
{
    const Row row = m_loader->row(id);
    const QColor down = Theme::current().Down, normal = Theme::current().Text, faint = Theme::current().TextFaint;
    for (int ti = 0; ti < m_built.size(); ++ti) {
        Tab &tab = m_built[ti];
        if (!tab.page)
            continue;
        bool touched = false;
        for (const Table &t : std::as_const(tab.tables)) {
            const int r = t.ids.indexOf(id);
            if (r < 0)
                continue;
            touched = true;
            t.widget->setRowHidden(r, row.state == Row::Missing);
            if (row.state == Row::Pending) {
                for (int c = Last; c < ColumnCount; ++c)
                    t.widget->item(r, c)->setText({});
                t.widget->item(r, Last)->setText("…");
                t.widget->item(r, Last)->setForeground(faint);
                continue;
            }
            if (row.state != Row::Found)
                continue;
            const double values[] = {row.last, row.previous, row.highest, row.lowest};
            for (int k = 0; k < 4; ++k) {
                QTableWidgetItem *cell = t.widget->item(r, Last + k);
                // Exchange rates below 10 (EUR/USD 1.1234) need four decimals.
                const int decimals = id == "currency" && std::abs(values[k]) < 10 ? 4 : -1;
                cell->setText(formatValue(values[k], decimals));
                cell->setForeground(std::isfinite(values[k]) && values[k] < 0 ? down : normal);
            }
            t.widget->item(r, Unit)->setText(row.unitText());
            t.widget->item(r, Unit)->setForeground(Theme::current().TextDim);
            t.widget->item(r, Date)->setText(periodLabel(row.date, row.freq));
            t.widget->item(r, Source)->setText(row.source);
            t.widget->item(r, Source)->setForeground(faint);
            QString tip = row.source + (row.code.isEmpty() ? QString() : " · " + row.code);
            if (!row.note.isEmpty())
                tip += "\n" + text(row.note);
            if (const QString fusion = CountryLoader::fusionTip(row); !fusion.isEmpty())
                tip += "\n\n" + fusion;
            if (row.latestOnly)
                tip += "\n" + tr("This source gives the latest and previous values only: no history for the highest "
                                 "and lowest.");
            for (int c = Name; c < ColumnCount; ++c)
                t.widget->item(r, c)->setToolTip(tip);
        }
        if (touched)
            updateTab(ti);
    }
}

void CountryPage::updateTab(int index)
{
    Tab &tab = m_built[index];
    if (!tab.page)
        return;
    QStringList missing, keyed;
    int pending = 0;
    QSet<QString> seen;
    for (const Table &t : std::as_const(tab.tables)) {
        fitTable(t.widget);
        for (const QString &id : t.ids) {
            if (seen.contains(id))
                continue;
            seen.insert(id);
            const Row row = m_loader->row(id);
            if (row.state == Row::Pending)
                ++pending;
            else if (row.state == Row::Missing)
                (row.needsKey ? keyed : missing) << text(catalogue().indicators.value(id).name);
        }
    }
    QStringList lines;
    if (!keyed.isEmpty())
        lines << tr("%n indicator(s) need a free API key, saved in Sources: %1.", "", keyed.size()).arg(keyed.join(", "));
    if (!missing.isEmpty())
        lines << tr("%n indicator(s) of this tab have no free source for %1.", "", missing.size()).arg(countryName(m_iso3));
    tab.note->setText(lines.join("\n"));
    tab.note->setToolTip(missing.isEmpty() ? QString() : missing.join("\n"));
    tab.note->setVisible(pending == 0 && !lines.isEmpty());
}

void CountryPage::fitTable(QTableWidget *table)
{
    int visible = 0;
    for (int r = 0; r < table->rowCount(); ++r)
        if (!table->isRowHidden(r))
            ++visible;
    const int header = table->horizontalHeader()->sizeHint().height();
    table->setFixedHeight(header + visible * table->verticalHeader()->defaultSectionSize() + 2 * table->frameWidth() + 2);
    table->setVisible(visible > 0);
}

void CountryPage::updateStatus()
{
    const int pending = m_loader->pending();
    m_refresh->setEnabled(pending == 0);
    if (pending > 0) {
        m_status->setText(tr("Updating…").toUpper());
        m_status->setProperty("state", "busy");
    } else {
        m_status->setText({});
        m_status->setProperty("state", "ok");
    }
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

void CountryPage::exportCsv()
{
    const int index = m_tabs->checkedId();
    if (index < 0 || !m_built.value(index).page)
        return;
    const QString name = QString("%1-%2.csv").arg(m_iso3, catalogue().tabs[index].id);
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + name,
        tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    QTextStream out(&f);
    auto quote = [](QString s) { return s.contains(',') || s.contains('"') ? '"' + s.replace('"', "\"\"") + '"' : s; };
    out << "Indicator,Last,Previous,Highest,Lowest,Unit,Period,Source,Code\n";
    for (const Table &t : std::as_const(m_built[index].tables))
        for (const QString &id : t.ids) {
            const Row row = m_loader->row(id);
            if (row.state != Row::Found)
                continue;
            auto num = [](double v) { return std::isfinite(v) ? QString::number(v, 'g', 12) : QString(); };
            out << quote(catalogue().indicators.value(id).name) << ',' << num(row.last) << ',' << num(row.previous) << ','
                << num(row.highest) << ',' << num(row.lowest) << ',' << quote(QString(row.unit).replace("{CUR}", row.currency)) << ','
                << row.date.toString(Qt::ISODate) << ',' << quote(row.source) << ',' << quote(row.code) << '\n';
        }
}

// ---------------------------------------------------------------- CountriesView

CountriesView::CountriesView(QWidget *parent)
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
    m_search->setPlaceholderText(tr("Search countries…"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedSize(240, 28);
    connect(m_search, &QLineEdit::textChanged, this, &CountriesView::filter);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        // Enter opens the only country left, or the first one shown.
        for (const Item &it : std::as_const(m_items))
            if (!it.button->isHidden()) {
                it.button->click();
                return;
            }
    });
    cv->addWidget(m_search);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    for (const Group &g : catalogue().groups) {
        auto *box = new QFrame;
        box->setObjectName("ctryGroup");
        auto *bv = new QVBoxLayout(box);
        bv->setContentsMargins(0, 0, 0, 0);
        bv->setSpacing(0);
        auto *head = new QLabel((g.name == "G20" ? "★ " : "") + text(g.name));
        head->setObjectName("ctryGroupTitle");
        bv->addWidget(head);
        QVector<std::pair<QString, QString>> list;
        for (const QString &iso3 : g.countries)
            list.push_back({countryName(iso3), iso3});
        // The G20 keep their order (by the size of their economies); the regions are sorted by name.
        if (g.name != "G20")
            std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
        for (const auto &[name, iso3] : list) {
            auto *b = new QPushButton(name);
            b->setObjectName("ctryItem");
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);   // a focused button would scroll the list to itself
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            connect(b, &QPushButton::clicked, this, [this, code = iso3] { openCountry(code); });
            bv->addWidget(b);
            m_items.push_back({b, name, box});
        }
        columns->addWidget(box, 1, Qt::AlignTop);
        m_groups << box;
    }
    cv->addLayout(columns);
    cv->addStretch(1);
    m_stack->addWidget(scrollArea(content));
}

void CountriesView::filter(const QString &query)
{
    const QString q = query.trimmed();
    QHash<QWidget *, int> shown;
    for (const Item &it : std::as_const(m_items)) {
        const bool match = q.isEmpty() || it.name.contains(q, Qt::CaseInsensitive);
        it.button->setVisible(match);
        if (match)
            shown[it.group]++;
    }
    for (QWidget *g : std::as_const(m_groups))
        g->setVisible(shown.value(g) > 0);
}

void CountriesView::openCountry(const QString &iso3, int tab)
{
    if (m_page) {
        m_stack->removeWidget(m_page);
        m_page->deleteLater();
    }
    m_page = new CountryPage(iso3);
    connect(m_page, &CountryPage::back, this, &CountriesView::showList);
    m_stack->addWidget(m_page);
    m_stack->setCurrentWidget(m_page);
    if (tab < 0)
        tab = QSettings().value("economy/countryTab", 0).toInt();
    m_page->showTab(std::clamp(tab, 0, int(catalogue().tabs.size()) - 1));
}

void CountriesView::showList()
{
    m_stack->setCurrentIndex(0);
    if (m_page) {
        m_stack->removeWidget(m_page);
        m_page->deleteLater();
    }
}

QString CountryPage::aiContext() const
{
    const int tab = m_tabs->checkedId();
    const QVector<TabDef> &tabs = catalogue().tabs;
    QString s = QStringLiteral("%1 (%2)").arg(countryName(m_iso3), m_iso3);
    if (tab >= 0 && tab < tabs.size()) s += ", " + text(tabs[tab].name);
    return s;
}

QString CountriesView::aiContext() const
{
    return m_page && m_stack->currentWidget() == m_page ? m_page->aiContext() : QString();
}
