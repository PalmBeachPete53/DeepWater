#include "ResearchPage.h"

#include "QuantMarketState.h"
#include "ResearchViews.h"
#include "Theme.h"

#include "quant/ui/PanelsKit.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>

using namespace research;

ResearchPage::ResearchPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("researchPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(mono(12));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // The symbol picker, right under the main header.
    auto *bar = new QWidget;
    bar->setObjectName("resSymbolBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(46);
    auto *h = new qtm::kit::WrapBarLayout(bar, 16, 4);
    h->setContentsMargins(10, 6, 30, 6);   // clear of the side flaps
    m_edit = new QLineEdit;
    m_edit->setObjectName("resSymbolEdit");
    m_edit->setFixedWidth(240);
    m_edit->setPlaceholderText(tr("Symbol or company…"));
    m_hits = new QStandardItemModel(this);
    m_completer = new QCompleter(m_hits, this);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->popup()->setObjectName("resCompleterPopup");
    m_completer->popup()->setFont(mono(12));
    m_edit->setCompleter(m_completer);
    h->addWidget(m_edit);
    m_quote = new QLabel;
    m_quote->setObjectName("resQuote");
    m_quote->setTextFormat(Qt::RichText);
    m_quote->setProperty("flexGrow", 1);
    h->addWidget(m_quote);
    m_state = new quant::MarketStateStrip(nullptr, true);
    m_state->setToolTip(tr("Regime and volatility forecast from Quant › Markov"));
    h->addWidget(m_state);
    v->addWidget(bar);

    // Sections.
    auto *sub = new QWidget;
    sub->setObjectName("resSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(34);
    auto *sh = new qtm::kit::WrapBarLayout(sub, 2);
    sh->setContentsMargins(6, 0, 12, 0);
    m_tabs = new QButtonGroup(this);
    m_sectionNames = {tr("Key Metrics"), tr("Financials"), tr("Financial Charts"), tr("Ownership"), tr("Ratings"),
                      tr("Compare"), tr("Compare Charts"), tr("Sectors"), "ICS", "SPLC"};
    const QStringList &names = m_sectionNames;
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("resSubTab");
        b->setText(names[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(34);
        m_tabs->addButton(b, i);
        sh->addWidget(b);
    }
    m_tabs->button(8)->setToolTip(tr("Industrial Classification System"));
    m_tabs->button(9)->setToolTip(tr("Supply chain"));
    v->addWidget(sub);

    m_stack = new QStackedWidget;
    m_views = {new KeyMetricsView, new FinancialsView, new FinancialChartsView, new OwnershipView, new RatingsView,
               new CompareView, new CompareChartsView, new SectorsView, new IcsView, new SplcView};
    for (View *view : std::as_const(m_views)) {
        m_stack->addWidget(view);
        connect(view, &View::symbolRequested, this, [this](const QString &s) {
            setSymbol(s);
            showSection(0);
        });
    }
    v->addWidget(m_stack, 1);

    m_searchDelay.setSingleShot(true);
    m_searchDelay.setInterval(250);
    connect(&m_searchDelay, &QTimer::timeout, this, [this] { search(m_edit->text().trimmed()); });
    connect(m_edit, &QLineEdit::textEdited, this, [this] { m_searchDelay.start(); });
    connect(m_edit, &QLineEdit::returnPressed, this, &ResearchPage::commit);
    connect(m_completer, qOverload<const QModelIndex &>(&QCompleter::activated), this, [this](const QModelIndex &i) {
        const QString sym = i.data(Qt::UserRole).toString();
        if (!sym.isEmpty())
            QTimer::singleShot(0, this, [this, sym] { setSymbol(sym); });
    });
    connect(m_tabs, &QButtonGroup::idClicked, this, &ResearchPage::showSection);
    m_quoteTimer.setInterval(60 * 1000);
    connect(&m_quoteTimer, &QTimer::timeout, this, &ResearchPage::refreshQuote);

    showSection(std::clamp(QSettings().value("research/section", 0).toInt(), 0, int(m_views.size()) - 1));
    setSymbol(QSettings().value("research/symbol", "AAPL").toString());
}

void ResearchPage::setSymbol(const QString &symbol)
{
    const QString s = symbol.trimmed().toUpper();
    if (s.isEmpty())
        return;
    m_edit->setText(s);
    if (s == m_symbol)
        return;
    m_symbol = s;
    m_state->setSymbol(s);
    QSettings().setValue("research/symbol", s);
    m_quote->setText(QString("<b>%1</b>").arg(s.toHtmlEscaped()));
    for (View *view : std::as_const(m_views))
        view->setSymbol(s);
    refreshQuote();
}

void ResearchPage::searchIcs(const QString &text)
{
    for (int i = 0; i < m_views.size(); ++i)
        if (auto *ics = qobject_cast<IcsView *>(m_views[i])) {
            showSection(i);
            ics->setSearch(text);
        }
}

void ResearchPage::showSection(int index)
{
    if (index < 0 || index >= m_views.size())
        return;
    m_tabs->button(index)->setChecked(true);
    m_stack->setCurrentIndex(index);
    QSettings().setValue("research/section", index);
}

void ResearchPage::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    refreshQuote();
    m_quoteTimer.start();
}

void ResearchPage::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    m_quoteTimer.stop();
}

void ResearchPage::search(const QString &text)
{
    if (text.size() < 1)
        return;
    Client::instance()->search(text, this, [this, text](const QVector<SearchHit> &hits, const QString &) {
        if (m_edit->text().trimmed() != text)
            return;
        m_hits->clear();
        m_firstHit = hits.isEmpty() ? QString() : hits.first().symbol;
        for (const SearchHit &h : hits) {
            auto *item = new QStandardItem(QString("%1  —  %2  ·  %3").arg(h.symbol, h.name, h.exchange));
            item->setData(h.symbol, Qt::UserRole);
            m_hits->appendRow(item);
        }
        if (!hits.isEmpty() && m_edit->hasFocus())
            m_completer->complete();
    });
}

void ResearchPage::commit()
{
    QString text = m_edit->text().trimmed();
    // "AAPL  —  Apple Inc.  ·  NASDAQ" picked from the list.
    if (text.contains("  —  "))
        text = text.section("  —  ", 0, 0);
    static const QRegularExpression ticker("^[A-Za-z0-9.\\-=^]{1,12}$");
    if (ticker.match(text).hasMatch()) {
        setSymbol(text);
        return;
    }
    if (!m_firstHit.isEmpty())
        setSymbol(m_firstHit);
}

void ResearchPage::refreshQuote()
{
    const QString sym = m_symbol;
    if (sym.isEmpty() || !isVisible())
        return;
    Client::instance()->quotes({sym}, this, [this, sym](const QHash<QString, QJsonObject> &q, const QString &err) {
        if (sym != m_symbol)
            return;
        const QJsonObject o = q.value(sym);
        if (o.isEmpty()) {
            m_quote->setText(QString("<b>%1</b> <span style='color:%2'>%3</span>")
                                 .arg(sym.toHtmlEscaped(), Theme::current().TextFaint.name(),
                                      (err.isEmpty() ? tr("unknown symbol") : err).toHtmlEscaped()));
            return;
        }
        const QString cur = o.value("currency").toString();
        const double price = o.value("regularMarketPrice").toDouble(NaN), ch = o.value("regularMarketChange").toDouble(NaN),
                     pct = o.value("regularMarketChangePercent").toDouble(NaN) / 100;
        QString name = o.value("longName").toString();
        if (name.isEmpty())
            name = o.value("shortName").toString();
        const QString state = o.value("marketState").toString();
        m_quote->setText(QString("<b>%1 - %2</b>&nbsp;&nbsp;&nbsp;<b>%3</b>&nbsp;&nbsp;&nbsp;<span style='color:%4'>%5 (%6)</span>"
                                 "&nbsp;&nbsp;&nbsp;<span style='color:%7; font-size:11px'>%8</span>")
                             .arg(sym.toHtmlEscaped(), name.toHtmlEscaped(), money(price, cur), changeColor(ch).name(),
                                  money(ch, cur), percent(pct), Theme::current().TextFaint.name(),
                                  (state == "REGULAR" ? tr("market open") : tr("at close")) + " · " +
                                      o.value("fullExchangeName").toString().toHtmlEscaped()));
    });
}

QString ResearchPage::aiContext() const
{
    const QString section = m_sectionNames.value(m_tabs->checkedId());
    if (m_symbol.isEmpty()) return section;
    return section.isEmpty() ? m_symbol : m_symbol + ", " + section;
}
