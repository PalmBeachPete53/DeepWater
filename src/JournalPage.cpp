#include "JournalPage.h"

#include "DeskWidgets.h"
#include "JournalData.h"
#include "QuantViews.h"
#include "ResearchData.h"
#include "Theme.h"
#include "desk/Instruments.h"
#include "desk/Risk.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

using namespace deskui;

namespace {

QComboBox *combo()
{
    auto *c = new QComboBox;
    c->setObjectName("resCombo");
    c->setCursor(Qt::PointingHandCursor);
    return c;
}

QPushButton *action(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setObjectName("qtChip");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QString money(double v, const QString &currency, int decimals = 2)
{
    return QString("%1 %2").arg(amount(v, decimals), currency);
}

// An axis's figures: 2.5k, not 3k, or two ticks can read the same.
QString axisMoney(double v)
{
    const double a = std::abs(v);
    if (a >= 1000)
        return QLocale().toString(v / 1000, 'f', a >= 100000 ? 0 : 1) + "k";
    return QLocale().toString(v, 'f', 0);
}

QString hours(double h)
{
    if (h < 1)
        return QObject::tr("%1 min").arg(qRound(h * 60));
    if (h < 48)
        return QObject::tr("%1 h").arg(QLocale().toString(h, 'f', 1));
    return QObject::tr("%1 d").arg(QLocale().toString(h / 24, 'f', 1));
}

// ------------------------------------------------------------------ the statement preview

class ImportDialog : public QDialog
{
    Q_OBJECT
public:
    ImportDialog(const QByteArray &bytes, const QString &fileName, QWidget *parent)
        : QDialog(parent)
        , m_bytes(bytes)
    {
        setWindowTitle(tr("Import trades"));
        setObjectName("helpWindow");
        setAttribute(Qt::WA_StyledBackground);
        resize(860, 560);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(16, 14, 16, 14);
        v->addWidget(quant::label(fileName, "qtTitle"));
        m_summary = quant::label({}, "qtCardLabel");
        m_summary->setWordWrap(true);
        v->addWidget(m_summary);

        m_columns = new QWidget;
        m_columnsLayout = new QFormLayout(m_columns);
        m_columnsLayout->setContentsMargins(0, 4, 0, 4);
        v->addWidget(m_columns);

        m_preview = table({tr("Symbol"), tr("Side"), tr("Volume"), tr("Opened"), tr("Closed"), tr("Open"), tr("Close"), tr("Costs"), tr("Profit")});
        m_preview->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        v->addWidget(m_preview, 1);

        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Import"));
        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        v->addWidget(m_buttons);

        m_result = desk::readStatement(bytes);
        if (m_result.format.contains("CSV") || !m_result.headers.isEmpty())
            buildColumns();
        fill();
    }

    QVector<desk::Trade> trades() const { return m_result.trades; }

private:
    void buildColumns()
    {
        const struct { desk::TradeField field; const char *name; } fields[] = {
            {desk::TradeField::Symbol, QT_TR_NOOP("Symbol")}, {desk::TradeField::Side, QT_TR_NOOP("Side")},
            {desk::TradeField::Volume, QT_TR_NOOP("Volume")}, {desk::TradeField::OpenTime, QT_TR_NOOP("Opened")},
            {desk::TradeField::CloseTime, QT_TR_NOOP("Closed")}, {desk::TradeField::Profit, QT_TR_NOOP("Profit")},
        };
        for (const auto &f : fields) {
            QComboBox *c = combo();
            c->addItem(tr("Not in the file"), -1);
            for (int i = 0; i < m_result.headers.size(); ++i)
                c->addItem(m_result.headers[i], i);
            c->setCurrentIndex(c->findData(m_result.mapping.value(int(f.field), -1)));
            m_columnsLayout->addRow(quant::label(tr(f.name), "qtFieldLabel"), c);
            connect(c, &QComboBox::currentIndexChanged, this, [this, field = f.field, c] {
                m_mapping[int(field)] = c->currentData().toInt();
                reread();
            });
        }
        m_mapping = m_result.mapping;
    }

    void reread()
    {
        QHash<int, int> mapping;
        for (auto it = m_mapping.begin(); it != m_mapping.end(); ++it)
            if (it.value() >= 0)
                mapping.insert(it.key(), it.value());
        const QStringList headers = m_result.headers;
        const QString format = m_result.format;
        m_result = desk::readCsv(QString::fromUtf8(m_bytes), mapping);
        m_result.format = format;
        m_result.headers = headers;
        fill();
    }

    void fill()
    {
        QString text;
        if (m_result.trades.isEmpty()) {
            text = m_result.error == "columns" ? tr("The columns weren't recognised: choose the symbol, a time and the profit below.")
                 : m_result.error == "empty" ? tr("The file is empty.")
                                             : tr("No closed trade was found. Export the account history from the terminal, as an HTML report or a CSV file.");
        } else {
            text = tr("%1: %2 trades found, %3 rows skipped (balance lines, orders, unreadable rows). A trade already imported is updated, not added twice.")
                       .arg(m_result.format).arg(m_result.trades.size()).arg(m_result.skipped);
        }
        m_summary->setText(text);
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_result.trades.isEmpty());
        m_preview->setRowCount(std::min<int>(50, m_result.trades.size()));
        const Palette &c = Theme::current();
        for (int i = 0; i < m_preview->rowCount(); ++i) {
            const desk::Trade &t = m_result.trades[i];
            m_preview->setItem(i, 0, cell(t.symbol, {}, Qt::AlignLeft, true));
            m_preview->setItem(i, 1, cell(t.isLong ? tr("Long") : tr("Short"), t.isLong ? c.Up : c.Down));
            m_preview->setItem(i, 2, cell(amount(t.volume)));
            m_preview->setItem(i, 3, cell(QLocale().toString(t.openTime.toLocalTime(), QLocale::ShortFormat)));
            m_preview->setItem(i, 4, cell(QLocale().toString(t.closeTime.toLocalTime(), QLocale::ShortFormat)));
            m_preview->setItem(i, 5, cell(amount(t.openPrice, 5)));
            m_preview->setItem(i, 6, cell(amount(t.closePrice, 5)));
            m_preview->setItem(i, 7, cell(amount(t.commission + t.swap)));
            m_preview->setItem(i, 8, cell(signedAmount(t.profit), t.profit >= 0 ? c.Up : c.Down));
        }
    }

    QByteArray m_bytes;
    desk::ImportResult m_result;
    QHash<int, int> m_mapping;
    QLabel *m_summary;
    QWidget *m_columns;
    QFormLayout *m_columnsLayout;
    QTableWidget *m_preview;
    QDialogButtonBox *m_buttons;
};

// ------------------------------------------------------------------ logging by hand

class TradeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TradeDialog(const desk::Trade &trade, QWidget *parent)
        : QDialog(parent)
        , m_trade(trade)
    {
        setWindowTitle(trade.id.isEmpty() ? tr("Log a trade") : tr("Edit the trade"));
        setObjectName("helpWindow");
        setAttribute(Qt::WA_StyledBackground);
        auto *form = new QFormLayout(this);
        form->setContentsMargins(18, 16, 18, 14);
        const auto number = [](double v, int decimals, double lo = -1e12) {
            auto *s = new QDoubleSpinBox;
            s->setObjectName("qtSpin");
            s->setDecimals(decimals);
            s->setRange(lo, 1e12);
            s->setValue(v);
            return s;
        };
        m_symbol = new QLineEdit(trade.symbol);
        m_symbol->setObjectName("qtSymbol");
        m_side = combo();
        m_side->addItem(tr("Long"), true);
        m_side->addItem(tr("Short"), false);
        m_side->setCurrentIndex(trade.isLong ? 0 : 1);
        m_volume = number(trade.volume, 2, 0);
        const QDateTime now = QDateTime::currentDateTime();
        m_open = new QDateTimeEdit(trade.openTime.isValid() ? trade.openTime.toLocalTime() : now);
        m_open->setDisplayFormat("yyyy-MM-dd HH:mm");
        m_closed = combo();
        m_closed->addItem(tr("Closed"), true);
        m_closed->addItem(tr("Still open"), false);
        m_closed->setCurrentIndex(trade.id.isEmpty() || trade.isClosed() ? 0 : 1);
        m_close = new QDateTimeEdit(trade.closeTime.isValid() ? trade.closeTime.toLocalTime() : now);
        m_close->setDisplayFormat("yyyy-MM-dd HH:mm");
        m_openPrice = number(trade.openPrice, 5, 0);
        m_closePrice = number(trade.closePrice, 5, 0);
        m_stop = number(trade.stopLoss, 5, 0);
        m_takeProfit = number(trade.takeProfit, 5, 0);
        m_costs = number(trade.commission + trade.swap, 2);
        m_profit = number(trade.profit, 2);
        m_strategy = new QLineEdit(trade.strategy);
        m_strategy->setObjectName("qtInput");
        m_notes = new QLineEdit(trade.notes);
        m_notes->setObjectName("qtInput");
        const auto row = [&](const QString &name, QWidget *w) { form->addRow(quant::label(name, "qtFieldLabel"), w); };
        row(tr("Symbol"), m_symbol);
        row(tr("Side"), m_side);
        row(tr("Volume (lots)"), m_volume);
        row(tr("Opened"), m_open);
        row(tr("State"), m_closed);
        row(tr("Closed"), m_close);
        row(tr("Open price"), m_openPrice);
        row(tr("Close price"), m_closePrice);
        row(tr("Stop loss"), m_stop);
        row(tr("Take profit"), m_takeProfit);
        row(tr("Commission and swap"), m_costs);
        row(tr("Profit"), m_profit);
        row(tr("Strategy"), m_strategy);
        row(tr("Notes"), m_notes);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
        const auto update = [this, buttons] {
            buttons->button(QDialogButtonBox::Save)->setEnabled(!m_symbol->text().trimmed().isEmpty());
            m_close->setEnabled(m_closed->currentData().toBool());
        };
        connect(m_symbol, &QLineEdit::textChanged, this, update);
        connect(m_closed, &QComboBox::currentIndexChanged, this, update);
        update();
    }

    desk::Trade trade() const
    {
        desk::Trade t = m_trade;
        t.symbol = m_symbol->text().trimmed().toUpper();
        t.isLong = m_side->currentData().toBool();
        t.volume = m_volume->value();
        t.openTime = m_open->dateTime().toUTC();
        t.closeTime = m_closed->currentData().toBool() ? m_close->dateTime().toUTC() : QDateTime();
        t.openPrice = m_openPrice->value();
        t.closePrice = m_closePrice->value();
        t.stopLoss = m_stop->value();
        t.takeProfit = m_takeProfit->value();
        t.commission = m_costs->value();
        t.swap = 0;
        t.profit = m_profit->value();
        t.strategy = m_strategy->text().trimmed();
        t.notes = m_notes->text().trimmed();
        return t;
    }

private:
    desk::Trade m_trade;
    QLineEdit *m_symbol, *m_strategy, *m_notes;
    QComboBox *m_side, *m_closed;
    QDoubleSpinBox *m_volume, *m_openPrice, *m_closePrice, *m_stop, *m_takeProfit, *m_costs, *m_profit;
    QDateTimeEdit *m_open, *m_close;
};

class AccountDialog : public QDialog
{
    Q_OBJECT
public:
    AccountDialog(const QString &title, const QString &name, const QString &currency, double balance, QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(title);
        setObjectName("helpWindow");
        setAttribute(Qt::WA_StyledBackground);
        auto *form = new QFormLayout(this);
        form->setContentsMargins(18, 16, 18, 14);
        m_name = new QLineEdit(name);
        m_name->setObjectName("qtInput");
        m_currency = combo();
        m_currency->setEditable(true);
        m_currency->addItems(desk::majorCurrencies());
        m_currency->setCurrentText(currency);
        m_balance = new QDoubleSpinBox;
        m_balance->setObjectName("qtSpin");
        m_balance->setRange(0, 1e12);
        m_balance->setDecimals(2);
        m_balance->setValue(balance);
        form->addRow(quant::label(tr("Name"), "qtFieldLabel"), m_name);
        form->addRow(quant::label(tr("Currency"), "qtFieldLabel"), m_currency);
        form->addRow(quant::label(tr("Starting balance"), "qtFieldLabel"), m_balance);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }
    QString name() const { return m_name->text().trimmed(); }
    QString currency() const { return m_currency->currentText().trimmed().toUpper(); }
    double balance() const { return m_balance->value(); }

private:
    QLineEdit *m_name;
    QComboBox *m_currency;
    QDoubleSpinBox *m_balance;
};

// ------------------------------------------------------------------ the bar above the journal's sections

class JournalBar : public QFrame
{
    Q_OBJECT
public:
    explicit JournalBar(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("qtBar");
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(10, 6, 10, 6);
        h->setSpacing(8);
        m_account = combo();
        m_account->setMinimumWidth(150);
        m_year = combo();
        m_symbol = combo();
        m_side = combo();
        m_side->addItem(tr("Long and short"), 0);
        m_side->addItem(tr("Long"), 1);
        m_side->addItem(tr("Short"), -1);
        m_strategy = combo();
        h->addWidget(quant::label(tr("ACCOUNT"), "qtFieldLabel"));
        h->addWidget(m_account);
        auto *edit = action(tr("Edit"));
        auto *add = action("+");
        add->setToolTip(tr("New account"));
        auto *remove = action("×");
        remove->setToolTip(tr("Delete this account and its trades"));
        h->addWidget(edit);
        h->addWidget(add);
        h->addWidget(remove);
        h->addSpacing(10);
        h->addWidget(m_year);
        h->addWidget(m_symbol);
        h->addWidget(m_side);
        h->addWidget(m_strategy);
        h->addStretch(1);
        m_last = quant::label({}, "qtNote");
        h->addWidget(m_last);
        auto *log = action(tr("+ LOG TRADE"));
        auto *import = action(tr("IMPORT TRADES"));
        h->addWidget(log);
        h->addWidget(import);

        JournalStore *store = JournalStore::instance();
        connect(store, &JournalStore::changed, this, &JournalBar::sync);
        const auto changed = [this] {
            if (!m_syncing)
                emit filterChanged();
        };
        connect(m_year, &QComboBox::currentIndexChanged, this, changed);
        connect(m_symbol, &QComboBox::currentIndexChanged, this, changed);
        connect(m_side, &QComboBox::currentIndexChanged, this, changed);
        connect(m_strategy, &QComboBox::currentIndexChanged, this, changed);
        connect(m_account, &QComboBox::currentIndexChanged, this, [this, store](int i) {
            if (!m_syncing)
                store->setCurrent(i);
        });
        connect(edit, &QPushButton::clicked, this, [this, store] {
            const auto &a = store->account();
            AccountDialog d(tr("Edit the account"), a.name, a.currency, a.balance, this);
            if (d.exec() == QDialog::Accepted && !d.name().isEmpty())
                store->editAccount(d.name(), d.currency(), d.balance());
        });
        connect(add, &QPushButton::clicked, this, [this, store] {
            AccountDialog d(tr("New account"), {}, "USD", 10000, this);
            if (d.exec() == QDialog::Accepted && !d.name().isEmpty())
                store->addAccount(d.name(), d.currency(), d.balance());
        });
        connect(remove, &QPushButton::clicked, this, [this, store] {
            const auto &a = store->account();
            if (QMessageBox::question(this, tr("Delete the account"), tr("Delete “%1” and its %2 trades? This can't be undone.").arg(a.name).arg(a.trades.size()))
                == QMessageBox::Yes)
                store->removeAccount();
        });
        connect(log, &QPushButton::clicked, this, [this, store] {
            TradeDialog d({}, this);
            if (d.exec() == QDialog::Accepted)
                store->saveTrade(d.trade());
        });
        connect(import, &QPushButton::clicked, this, &JournalBar::import);
        sync();
    }

    desk::Filter filter() const
    {
        desk::Filter f;
        f.year = m_year->currentData().toInt();
        f.symbol = m_symbol->currentData().toString();
        f.side = m_side->currentData().toInt();
        f.strategy = m_strategy->currentData().toString();
        return f;
    }

signals:
    void filterChanged();

private:
    void import()
    {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import trades"), {}, tr("Statements (*.htm *.html *.csv *.txt);;All files (*)"));
        if (path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Import trades"), tr("The file can't be read: %1").arg(f.errorString()));
            return;
        }
        ImportDialog d(f.readAll(), QFileInfo(path).fileName(), this);
        if (d.exec() != QDialog::Accepted)
            return;
        int added = 0, updated = 0;
        JournalStore::instance()->importTrades(d.trades(), &added, &updated);
        m_last->setText(tr("%1 added, %2 updated").arg(added).arg(updated));
    }

    void sync()
    {
        JournalStore *store = JournalStore::instance();
        m_syncing = true;
        const desk::Filter keep = filter();
        m_account->clear();
        for (const auto &a : store->accounts())
            m_account->addItem(a.name);
        m_account->setCurrentIndex(store->current());
        m_year->clear();
        m_year->addItem(tr("All years"), 0);
        for (int y : store->years())
            m_year->addItem(QString::number(y), y);
        m_year->setCurrentIndex(std::max(0, m_year->findData(keep.year)));
        m_symbol->clear();
        m_symbol->addItem(tr("All instruments"), QString());
        for (const QString &s : store->symbols())
            m_symbol->addItem(s, s);
        m_symbol->setCurrentIndex(std::max(0, m_symbol->findData(keep.symbol)));
        m_strategy->clear();
        m_strategy->addItem(tr("All strategies"), QString());
        for (const QString &s : store->strategies())
            m_strategy->addItem(s, s);
        m_strategy->setCurrentIndex(std::max(0, m_strategy->findData(keep.strategy)));
        if (store->account().lastImport.isValid())
            m_last->setText(tr("Last import %1").arg(QLocale().toString(store->account().lastImport.toLocalTime(), QLocale::ShortFormat)));
        else
            m_last->clear();
        m_syncing = false;
        emit filterChanged();
    }

    QComboBox *m_account, *m_year, *m_symbol, *m_side, *m_strategy;
    QLabel *m_last;
    bool m_syncing = false;
};

// ------------------------------------------------------------------ Dashboard

class DashboardView : public quant::View
{
    Q_OBJECT
public:
    explicit DashboardView(JournalBar *bar, QWidget *parent = nullptr)
        : View(parent)
        , m_bar(bar)
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        row->addWidget(tile(tr("Net P&L"), &m_net, &m_netSub), 1);
        row->addWidget(tile(tr("Trade win %"), &m_win, &m_winSub), 1);
        row->addWidget(tile(tr("Profit factor"), &m_pf, &m_pfSub), 1);
        row->addWidget(tile(tr("Max drawdown"), &m_dd, &m_ddSub), 1);
        row->addWidget(tile(tr("Avg win / loss"), &m_ratio, &m_ratioSub), 1);
        row->addWidget(tile(tr("Expectancy / trade"), &m_exp, &m_expSub), 1);
        content()->addLayout(row);

        auto *mid = new QHBoxLayout;
        mid->setSpacing(10);
        QVBoxLayout *body;
        auto *scoreCard = quant::card(tr("Performance score"), &body);
        m_radar = new RadarChart;
        m_radar->setMinimumHeight(240);
        body->addWidget(m_radar);
        m_score = quant::label("—", "qtBigValue");
        m_score->setAlignment(Qt::AlignCenter);
        body->addWidget(m_score);
        auto *scoreNote = quant::label(tr("This app's own mean of the six measures above, each on a plain scale: not a standard."), "qtNote");
        scoreNote->setWordWrap(true);
        scoreNote->setAlignment(Qt::AlignCenter);
        body->addWidget(scoreNote);
        mid->addWidget(scoreCard, 1);
        auto *equityCard = quant::card(tr("Cumulative net P&L"), &body);
        m_equity = new quant::MultiLineChart;
        m_equity->setSignedArea(true);
        m_equity->setMinimumHeight(290);
        body->addWidget(m_equity);
        mid->addWidget(equityCard, 2);
        content()->addLayout(mid);

        auto *low = new QHBoxLayout;
        low->setSpacing(10);
        auto *monthCard = quant::card(tr("Month P&L"), &body);
        m_months = new BarsChart;
        body->addWidget(m_months);
        low->addWidget(monthCard, 1);
        auto *quarterCard = quant::card(tr("Quarter P&L"), &body);
        m_quarters = new BarsChart;
        body->addWidget(m_quarters);
        low->addWidget(quarterCard, 1);
        auto *lastCard = quant::card(tr("Last trades P&L"), &body);
        m_last = new BarsChart;
        body->addWidget(m_last);
        low->addWidget(lastCard, 1);
        content()->addLayout(low);

        auto *more = new QHBoxLayout;
        more->setSpacing(10);
        more->addWidget(tile(tr("Sharpe-like ratio"), &m_sharpe, &m_sharpeSub), 1);
        more->addWidget(tile(tr("Sortino-like ratio"), &m_sortino, &m_sortinoSub), 1);
        more->addWidget(tile(tr("Winning streak"), &m_streak, &m_streakSub), 1);
        more->addWidget(tile(tr("Average holding time"), &m_hold, &m_holdSub), 1);
        more->addWidget(tile(tr("Recovery factor"), &m_recovery, &m_recoverySub), 1);
        content()->addLayout(more);

        m_openCard = quant::card(tr("Open positions"), &body);
        m_open = table({tr("Symbol"), tr("Side"), tr("Volume"), tr("Opened"), tr("Open"), tr("Live"), tr("Unrealised P&L")});
        body->addWidget(m_open);
        m_openNote = quant::label({}, "qtNote");
        m_openNote->setWordWrap(true);
        body->addWidget(m_openNote);
        content()->addWidget(m_openCard);
        content()->addStretch(1);

        connect(bar, &JournalBar::filterChanged, this, &DashboardView::fill);
        m_refresh = new QTimer(this);
        m_refresh->setInterval(60 * 1000);
        connect(m_refresh, &QTimer::timeout, this, &DashboardView::fillOpen);
    }

protected:
    void load() override
    {
        fill();
        m_refresh->start();
    }

private:
    void fill()
    {
        JournalStore *store = JournalStore::instance();
        const auto &account = store->account();
        const QString cur = account.currency;
        const QVector<desk::Trade> trades = desk::applyFilter(account.trades, m_bar->filter());
        const desk::JournalStats s = desk::computeStats(trades, account.balance);
        setStatus(s.closed ? QString() : tr("No closed trade yet: import a statement of MetaTrader 4, MetaTrader 5 or cTrader, or log a trade by hand."));

        setSigned(m_net, money(s.net, cur), s.net);
        m_netSub->setText(tr("%1 closed trades").arg(s.closed));
        m_win->setText(s.closed ? QString("%1%").arg(amount(s.winRate)) : "—");
        m_winSub->setText(tr("%1 won, %2 flat, %3 lost").arg(s.wins).arg(s.flat).arg(s.losses));
        m_pf->setText(s.closed ? amount(s.profitFactor) : "—");
        m_pfSub->setText(tr("%1 won for %2 lost").arg(amount(s.grossWin), amount(s.grossLoss)));
        setSigned(m_dd, s.closed ? money(-s.maxDrawdown, cur) : "—", -s.maxDrawdown);
        m_ddSub->setText(s.closed ? tr("%1% of the balance at its peak").arg(amount(s.maxDrawdownPct)) : QString());
        m_ratio->setText(s.closed ? amount(s.winLossRatio) : "—");
        m_ratioSub->setText(tr("%1 average win, %2 average loss").arg(amount(s.avgWin), amount(s.avgLoss)));
        setSigned(m_exp, s.closed ? money(s.expectancy, cur) : "—", s.expectancy);
        m_expSub->setText(tr("net per trade"));

        m_radar->setData({tr("Win %"), tr("Profit factor"), tr("Avg win/loss"), tr("Recovery"), tr("Drawdown"), tr("Consistency")}, s.radar);
        m_score->setText(s.closed ? QString("%1 / 100").arg(amount(s.score, 1)) : "—");

        QVector<QDate> dates;
        QVector<double> values;
        for (const desk::EquityPoint &e : s.equity) {
            dates << e.time.date();
            values << e.cumulative;
        }
        m_equity->setData(dates, {{tr("Cumulative"), Theme::current().Accent, values}}, [](double v) { return axisMoney(v); });

        const auto buckets = [](const QVector<desk::Bucket> &list, QStringList *labels, QVector<double> *out) {
            for (const desk::Bucket &b : list) {
                *labels << b.label;
                *out << b.net;
            }
        };
        const quant::Format f = [](double v) { return axisMoney(v); };
        QStringList labels;
        QVector<double> nets;
        buckets(s.byMonth, &labels, &nets);
        m_months->setData(labels, nets, f);
        labels.clear();
        nets.clear();
        buckets(s.byQuarter, &labels, &nets);
        m_quarters->setData(labels, nets, f);
        labels.clear();
        nets.clear();
        for (int i = std::max<int>(0, s.equity.size() - 40); i < s.equity.size(); ++i) {
            labels << s.equity[i].time.date().toString("MM/dd");
            nets << s.equity[i].tradeNet;
        }
        m_last->setData(labels, nets, f);

        m_sharpe->setText(s.closed ? amount(s.sharpeLike) : "—");
        m_sharpeSub->setText(tr("mean over spread of the trades' results, not annualised"));
        m_sortino->setText(s.closed ? amount(s.sortinoLike) : "—");
        m_sortinoSub->setText(tr("the same with the losses' spread only"));
        m_streak->setText(s.closed ? QString::number(s.longestWinStreak) : "—");
        m_streakSub->setText(tr("longest losing streak: %1").arg(s.longestLossStreak));
        m_hold->setText(s.closed ? hours(s.avgHoldHours) : "—");
        m_holdSub->setText(tr("from open to close"));
        m_recovery->setText(s.closed ? amount(s.recoveryFactor) : "—");
        m_recoverySub->setText(tr("net P&L over the largest drawdown"));
        fillOpen();
    }

    // Open positions at the terminal's own quotes.
    void fillOpen()
    {
        JournalStore *store = JournalStore::instance();
        QVector<desk::Trade> open;
        for (const desk::Trade &t : desk::applyFilter(store->account().trades, m_bar->filter()))
            if (!t.isClosed())
                open << t;
        m_openCard->setVisible(!open.isEmpty());
        if (open.isEmpty())
            return;
        QStringList symbols;
        const QString accountCurrency = store->account().currency;
        for (const desk::Trade &t : open)
            if (const desk::Instrument *i = desk::findInstrument(t.symbol)) {
                symbols << i->symbol;
                if (i->quote != accountCurrency)
                    symbols << i->quote + accountCurrency + "=X";
            }
        symbols.removeDuplicates();
        research::Client::instance()->quotes(symbols, 60, this, [this, open, accountCurrency](const QHash<QString, QJsonObject> &q, const QString &error) {
            const Palette &c = Theme::current();
            m_open->setRowCount(open.size());
            double total = 0;
            int priced = 0;
            for (int r = 0; r < open.size(); ++r) {
                const desk::Trade &t = open[r];
                const desk::Instrument *i = desk::findInstrument(t.symbol);
                double live = research::NaN, pnl = research::NaN;
                if (i) {
                    live = research::raw(q.value(i->symbol), "regularMarketPrice");
                    const double fx = i->quote == accountCurrency ? 1.0 : research::raw(q.value(i->quote + accountCurrency + "=X"), "regularMarketPrice");
                    pnl = (live - t.openPrice) * (t.isLong ? 1 : -1) * t.volume * i->contractSize * fx + t.commission + t.swap;
                }
                m_open->setItem(r, 0, cell(t.symbol, {}, Qt::AlignLeft, true));
                m_open->setItem(r, 1, cell(t.isLong ? tr("Long") : tr("Short"), t.isLong ? c.Up : c.Down));
                m_open->setItem(r, 2, cell(amount(t.volume)));
                m_open->setItem(r, 3, cell(QLocale().toString(t.openTime.toLocalTime(), QLocale::ShortFormat)));
                m_open->setItem(r, 4, cell(amount(t.openPrice, 5)));
                m_open->setItem(r, 5, cell(amount(live, 5)));
                m_open->setItem(r, 6, cell(signedAmount(pnl), std::isnan(pnl) ? c.TextDim : pnl >= 0 ? c.Up : c.Down, Qt::AlignCenter, true));
                if (!std::isnan(pnl)) {
                    total += pnl;
                    ++priced;
                }
            }
            fitHeight(m_open);
            m_openNote->setText(error.isEmpty()
                                    ? tr("From the same delayed quotes as the rest of the terminal, refreshed every minute: %1 %2 over %3 of %4 positions, with the contract sizes of a standard account.")
                                          .arg(signedAmount(total), accountCurrency).arg(priced).arg(open.size())
                                    : tr("Quotes unavailable: %1").arg(error));
        });
    }

    JournalBar *m_bar;
    QLabel *m_net, *m_netSub, *m_win, *m_winSub, *m_pf, *m_pfSub, *m_dd, *m_ddSub, *m_ratio, *m_ratioSub, *m_exp, *m_expSub;
    QLabel *m_sharpe, *m_sharpeSub, *m_sortino, *m_sortinoSub, *m_streak, *m_streakSub, *m_hold, *m_holdSub, *m_recovery, *m_recoverySub;
    QLabel *m_score, *m_openNote;
    RadarChart *m_radar;
    quant::MultiLineChart *m_equity;
    BarsChart *m_months, *m_quarters, *m_last;
    QFrame *m_openCard;
    QTableWidget *m_open;
    QTimer *m_refresh;
};

// ------------------------------------------------------------------ Trade log

class TradeLogView : public quant::View
{
    Q_OBJECT
public:
    explicit TradeLogView(JournalBar *bar, QWidget *parent = nullptr)
        : View(parent)
        , m_bar(bar)
    {
        QVBoxLayout *body;
        auto *c = quant::card(tr("Trades"), &body);
        m_table = table({tr("Closed"), tr("Symbol"), tr("Side"), tr("Volume"), tr("Open"), tr("Close"), tr("Held"), tr("Costs"), tr("Profit"), tr("Net"), tr("Strategy"), tr("Notes"), {}});
        m_table->setMouseTracking(true);
        body->addWidget(m_table);
        content()->addWidget(c);
        content()->addStretch(1);
        connect(bar, &JournalBar::filterChanged, this, &TradeLogView::fill);
    }

protected:
    void load() override { fill(); }

private:
    void fill()
    {
        const Palette &c = Theme::current();
        QVector<desk::Trade> trades = desk::applyFilter(JournalStore::instance()->account().trades, m_bar->filter());
        std::sort(trades.begin(), trades.end(), [](const desk::Trade &a, const desk::Trade &b) {
            return (a.isClosed() ? a.closeTime : a.openTime) > (b.isClosed() ? b.closeTime : b.openTime);
        });
        setStatus(trades.isEmpty() ? tr("No trade matches these filters.") : QString());
        m_table->setRowCount(trades.size());
        for (int r = 0; r < trades.size(); ++r) {
            const desk::Trade &t = trades[r];
            m_table->setItem(r, 0, cell(t.isClosed() ? QLocale().toString(t.closeTime.toLocalTime(), QLocale::ShortFormat) : tr("open"), t.isClosed() ? QColor() : c.Accent));
            m_table->setItem(r, 1, cell(t.symbol, {}, Qt::AlignLeft, true));
            m_table->setItem(r, 2, cell(t.isLong ? tr("Long") : tr("Short"), t.isLong ? c.Up : c.Down));
            m_table->setItem(r, 3, cell(amount(t.volume)));
            m_table->setItem(r, 4, cell(amount(t.openPrice, 5)));
            m_table->setItem(r, 5, cell(t.isClosed() ? amount(t.closePrice, 5) : "—"));
            m_table->setItem(r, 6, cell(t.isClosed() ? hours(t.holdHours()) : "—"));
            m_table->setItem(r, 7, cell(amount(t.commission + t.swap)));
            m_table->setItem(r, 8, cell(signedAmount(t.profit), t.profit >= 0 ? c.Up : c.Down));
            m_table->setItem(r, 9, cell(signedAmount(t.net()), t.net() >= 0 ? c.Up : c.Down, Qt::AlignCenter, true));
            m_table->setItem(r, 10, cell(t.strategy));
            m_table->setItem(r, 11, cell(t.notes, c.TextDim, Qt::AlignLeft));
            auto *buttons = new QWidget;
            auto *h = new QHBoxLayout(buttons);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(2);
            auto *edit = new QPushButton(tr("Edit"));
            edit->setObjectName("qtIconButton");
            edit->setCursor(Qt::PointingHandCursor);
            auto *del = new QPushButton("×");
            del->setObjectName("qtIconButton");
            del->setCursor(Qt::PointingHandCursor);
            del->setToolTip(tr("Delete this trade"));
            h->addWidget(edit);
            h->addWidget(del);
            connect(edit, &QPushButton::clicked, this, [this, t] {
                TradeDialog d(t, this);
                if (d.exec() == QDialog::Accepted)
                    JournalStore::instance()->saveTrade(d.trade());
            });
            connect(del, &QPushButton::clicked, this, [this, id = t.id, symbol = t.symbol] {
                if (QMessageBox::question(this, tr("Delete the trade"), tr("Delete this %1 trade?").arg(symbol)) == QMessageBox::Yes)
                    JournalStore::instance()->removeTrade(id);
            });
            m_table->setCellWidget(r, 12, buttons);
        }
        fitColumns(m_table, 11);
        fitHeight(m_table);
    }

    JournalBar *m_bar;
    QTableWidget *m_table;
};

// ------------------------------------------------------------------ Advanced details

class AdvancedView : public quant::View
{
    Q_OBJECT
public:
    explicit AdvancedView(JournalBar *bar, QWidget *parent = nullptr)
        : View(parent)
        , m_bar(bar)
    {
        QVBoxLayout *body;
        const auto grid = [&](std::initializer_list<std::pair<QString, QWidget *>> items) {
            auto *row = new QHBoxLayout;
            row->setSpacing(10);
            for (const auto &[title, chart] : items) {
                auto *c = quant::card(title, &body);
                body->addWidget(chart);
                row->addWidget(c, 1);
            }
            content()->addLayout(row);
        };
        m_drawdown = new quant::MultiLineChart;
        m_drawdown->setSignedArea(true);
        m_drawdown->setMinimumHeight(200);
        m_timeOfDay = new DotsChart;
        m_duration = new DotsChart;
        m_duration->setLogX(true);
        grid({{tr("Drawdown"), m_drawdown}, {tr("Result by time of day"), m_timeOfDay}, {tr("Result by holding time"), m_duration}});
        m_weekday = new HBarsChart;
        m_best = new HBarsChart;
        m_symbols = new HBarsChart;
        grid({{tr("P&L by weekday"), m_weekday}, {tr("Top 5 trades"), m_best}, {tr("Top 5 symbols, P&L"), m_symbols}});
        m_counts = new HBarsChart;
        m_counts->setNeutralColor(true);
        m_worst = new HBarsChart;
        m_symbolCounts = new HBarsChart;
        m_symbolCounts->setNeutralColor(true);
        grid({{tr("Trades by weekday"), m_counts}, {tr("Bottom 5 trades"), m_worst}, {tr("Trades, top 5 symbols"), m_symbolCounts}});
        content()->addStretch(1);
        connect(bar, &JournalBar::filterChanged, this, &AdvancedView::fill);
    }

protected:
    void load() override { fill(); }

private:
    void fill()
    {
        const auto &account = JournalStore::instance()->account();
        const QVector<desk::Trade> trades = desk::applyFilter(account.trades, m_bar->filter());
        const desk::JournalStats s = desk::computeStats(trades, account.balance);
        setStatus(s.closed ? tr("%1 closed trades.").arg(s.closed) : tr("No closed trade matches these filters."));
        const quant::Format money = [](double v) { return axisMoney(v); };

        QVector<QDate> dates;
        QVector<double> dd;
        for (const desk::EquityPoint &e : s.equity) {
            dates << e.time.date();
            dd << e.drawdown;
        }
        m_drawdown->setData(dates, {{tr("Drawdown"), Theme::current().Down, dd}}, money);

        QVector<double> hour, net, held;
        QStringList tips;
        for (const desk::Trade &t : trades) {
            if (!t.isClosed())
                continue;
            const QDateTime local = t.openTime.toLocalTime();
            hour << local.time().hour() + local.time().minute() / 60.0;
            net << t.net();
            held << std::max(t.holdHours(), 0.02);
            tips << tr("%1 · %2 %3\nOpened %4\nHeld %5\n%6")
                        .arg(t.symbol, t.isLong ? tr("Long") : tr("Short"), amount(t.volume), QLocale().toString(local, QLocale::ShortFormat), hours(t.holdHours()), signedAmount(t.net()));
        }
        m_timeOfDay->setXRange(0, 24);
        m_timeOfDay->setData(hour, net, [](double h) { return QString("%1:00").arg(qRound(h), 2, 10, QChar('0')); }, money, tips);
        m_duration->setData(held, net, [](double h) { return hours(h); }, money, tips);

        const QStringList days {tr("Mon"), tr("Tue"), tr("Wed"), tr("Thu"), tr("Fri"), tr("Sat"), tr("Sun")};
        QStringList dayLabels;
        QVector<double> dayNet, dayCount;
        for (int i = 0; i < s.byWeekday.size(); ++i)
            if (s.byWeekday[i].count || i < 5) {
                dayLabels << days[i];
                dayNet << s.byWeekday[i].net;
                dayCount << s.byWeekday[i].count;
            }
        m_weekday->setData(dayLabels, dayNet, money);
        m_counts->setData(dayLabels, dayCount, [](double v) { return QString::number(qRound(v)); });

        const auto tradeBars = [&](HBarsChart *chart, const QVector<desk::Trade> &list) {
            QStringList labels, lineTips;
            QVector<double> values;
            for (const desk::Trade &t : list) {
                labels << QString("%1 %2").arg(t.symbol, t.closeTime.date().toString("MM/dd"));
                values << t.net();
                lineTips << tr("%1 · %2 %3\nOpened %4\nHeld %5\n%6")
                                .arg(t.symbol, t.isLong ? tr("Long") : tr("Short"), amount(t.volume), QLocale().toString(t.openTime.toLocalTime(), QLocale::ShortFormat), hours(t.holdHours()), signedAmount(t.net()));
            }
            chart->setData(labels, values, money, lineTips);
        };
        tradeBars(m_best, s.best);
        tradeBars(m_worst, s.worst);

        QStringList names;
        QVector<double> symNet, symCount;
        for (int i = 0; i < std::min<int>(5, s.bySymbol.size()); ++i) {
            names << s.bySymbol[i].label;
            symNet << s.bySymbol[i].net;
            symCount << s.bySymbol[i].count;
        }
        m_symbols->setData(names, symNet, money);
        m_symbolCounts->setData(names, symCount, [](double v) { return QString::number(qRound(v)); });
    }

    JournalBar *m_bar;
    quant::MultiLineChart *m_drawdown;
    DotsChart *m_timeOfDay, *m_duration;
    HBarsChart *m_weekday, *m_best, *m_symbols, *m_counts, *m_worst, *m_symbolCounts;
};

// ------------------------------------------------------------------ Risk calculator

// Stop, entry and target on one line.
class TradeMap : public QWidget
{
    Q_OBJECT
public:
    explicit TradeMap(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(76);
    }
    void setLevels(double stop, double entry, double target, int decimals)
    {
        m_stop = stop;
        m_entry = entry;
        m_target = target;
        m_decimals = decimals;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!(m_stop > 0) || !(m_entry > 0))
            return;
        const Palette &c = Theme::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double target = m_target > 0 ? m_target : m_entry + (m_entry - m_stop);
        const double lo = std::min({m_stop, m_entry, target}), hi = std::max({m_stop, m_entry, target});
        const double span = hi > lo ? hi - lo : 1, left = 34, right = width() - 34.0, y = 34;
        const auto x = [&](double v) { return left + (v - lo) / span * (right - left); };
        p.setPen(Qt::NoPen);
        p.setBrush(c.Down);
        p.drawRect(QRectF(QPointF(std::min(x(m_stop), x(m_entry)), y - 3), QPointF(std::max(x(m_stop), x(m_entry)), y + 3)));
        if (m_target > 0) {
            p.setBrush(c.Up);
            p.drawRect(QRectF(QPointF(std::min(x(target), x(m_entry)), y - 3), QPointF(std::max(x(target), x(m_entry)), y + 3)));
        }
        const auto mark = [&](double v, const QString &name, const QColor &col) {
            p.setPen(QPen(col, 2));
            p.drawLine(QPointF(x(v), y - 13), QPointF(x(v), y + 13));
            p.setFont(quant::chartFont(9, true));
            p.drawText(QRectF(x(v) - 50, 0, 100, 18), Qt::AlignCenter, name);
            p.setPen(c.Text);
            p.setFont(quant::chartFont(10));
            p.drawText(QRectF(x(v) - 50, y + 16, 100, 18), Qt::AlignCenter, QLocale().toString(v, 'f', m_decimals));
        };
        mark(m_stop, tr("STOP"), c.Down);
        mark(m_entry, tr("ENTRY"), c.Text);
        if (m_target > 0)
            mark(target, tr("TARGET"), c.Up);
    }

private:
    double m_stop = 0, m_entry = 0, m_target = 0;
    int m_decimals = 5;
};

class RiskView : public quant::View
{
    Q_OBJECT
public:
    explicit RiskView(QWidget *parent = nullptr)
        : View(parent)
    {
        auto *columns = new QHBoxLayout;
        columns->setSpacing(10);
        QVBoxLayout *body;
        auto *form = quant::card(tr("Trade parameters"), &body);
        form->setMaximumWidth(330);
        form->setMinimumWidth(270);
        const auto field = [&](const QString &name, QWidget *w) {
            body->addWidget(quant::label(name, "qtFieldLabel"));
            body->addWidget(w);
            body->addSpacing(4);
        };
        const auto number = [](int decimals) {
            auto *s = new QDoubleSpinBox;
            s->setObjectName("qtSpin");
            s->setDecimals(decimals);
            s->setRange(0, 1e12);
            s->setKeyboardTracking(true);
            return s;
        };
        m_instrument = combo();
        m_instrument->setEditable(true);
        m_instrument->setInsertPolicy(QComboBox::NoInsert);
        for (const desk::Instrument &i : desk::instruments())
            m_instrument->addItem(i.label, i.id);
        field(tr("Instrument"), m_instrument);
        m_currency = combo();
        m_currency->setEditable(true);
        m_currency->addItems(desk::majorCurrencies());
        field(tr("Account currency"), m_currency);
        m_balance = number(2);
        field(tr("Account balance"), m_balance);
        auto *riskRow = new QHBoxLayout;
        riskRow->setSpacing(6);
        m_risk = number(2);
        riskRow->addWidget(m_risk, 1);
        QButtonGroup *unit;
        riskRow->addWidget(quant::segments({"%", tr("Amount")}, &unit));
        m_unit = unit;
        body->addWidget(quant::label(tr("Risk per trade"), "qtFieldLabel"));
        body->addLayout(riskRow);
        body->addSpacing(4);
        m_entry = number(5);
        field(tr("Entry price"), m_entry);
        m_stop = number(5);
        field(tr("Stop loss"), m_stop);
        m_target = number(5);
        field(tr("Take profit (optional)"), m_target);
        auto *prefill = action(tr("Use the live price"));
        body->addWidget(prefill);
        m_quoteNote = quant::label({}, "qtNote");
        m_quoteNote->setWordWrap(true);
        body->addWidget(m_quoteNote);
        body->addStretch(1);
        columns->addWidget(form);

        auto *right = new QVBoxLayout;
        right->setSpacing(10);
        auto *top = new QHBoxLayout;
        top->setSpacing(10);
        top->addWidget(tile(tr("Lot size"), &m_lots, &m_lotsSub), 1);
        top->addWidget(tile(tr("Position size"), &m_units, &m_side), 1);
        top->addWidget(tile(tr("Stop distance"), &m_distance, &m_distanceSub), 1);
        right->addLayout(top);
        auto *mid = new QHBoxLayout;
        mid->setSpacing(10);
        mid->addWidget(tile(tr("Risk"), &m_riskValue, &m_riskSub), 1);
        mid->addWidget(tile(tr("Reward"), &m_reward, &m_rewardSub), 1);
        right->addLayout(mid);
        auto *mapCard = quant::card(tr("Trade map"), &body);
        m_map = new TradeMap;
        body->addWidget(m_map);
        right->addWidget(mapCard);
        auto *ratioCard = quant::card(tr("Risk / reward ratio"), &body);
        m_ratio = quant::label("—", "qtBigValue");
        m_ratio->setAlignment(Qt::AlignCenter);
        m_ratio->setStyleSheet("font-size: 46px;");
        body->addWidget(m_ratio);
        m_grade = quant::label({}, "qtKicker");
        m_grade->setAlignment(Qt::AlignCenter);
        body->addWidget(m_grade);
        m_breakeven = quant::label({}, "qtCardLabel");
        m_breakeven->setAlignment(Qt::AlignCenter);
        body->addWidget(m_breakeven);
        right->addWidget(ratioCard);
        m_message = quant::label({}, "qtNote");
        m_message->setWordWrap(true);
        right->addWidget(m_message);
        right->addStretch(1);
        columns->addLayout(right, 1);
        content()->addLayout(columns);
        content()->addStretch(1);

        QSettings s;
        m_instrument->setCurrentIndex(std::max(0, m_instrument->findData(s.value("journal/risk/instrument", "EURUSD"))));
        m_currency->setCurrentText(s.value("journal/risk/currency", "USD").toString());
        m_balance->setValue(s.value("journal/risk/balance", 10000).toDouble());
        m_risk->setValue(s.value("journal/risk/risk", 1).toDouble());
        m_unit->button(s.value("journal/risk/unit", 0).toInt())->setChecked(true);

        for (QDoubleSpinBox *sp : {m_balance, m_risk, m_entry, m_stop, m_target})
            connect(sp, &QDoubleSpinBox::valueChanged, this, &RiskView::recompute);
        connect(m_unit, &QButtonGroup::idClicked, this, &RiskView::recompute);
        connect(m_currency, &QComboBox::currentTextChanged, this, &RiskView::fetchQuotes);
        connect(m_instrument, &QComboBox::currentIndexChanged, this, [this] {
            fetchQuotes();
            m_prefillOnQuote = true;
        });
        connect(prefill, &QPushButton::clicked, this, [this] {
            m_prefillOnQuote = true;
            fetchQuotes();
        });
    }

protected:
    void load() override
    {
        m_prefillOnQuote = true;
        fetchQuotes();
    }

private:
    const desk::Instrument *instrument() const { return desk::findInstrument(m_instrument->currentData().toString()); }

    void fetchQuotes()
    {
        const desk::Instrument *i = instrument();
        if (!i)
            return;
        const QString account = m_currency->currentText().trimmed().toUpper();
        QStringList symbols {i->symbol};
        const QString fxSymbol = i->quote + account + "=X";
        const bool needFx = i->quote != account;
        if (needFx)
            symbols << fxSymbol;
        m_quoteNote->setText(tr("Loading the live rate…"));
        research::Client::instance()->quotes(symbols, 60, this, [this, sym = i->symbol, fxSymbol, needFx](const QHash<QString, QJsonObject> &q, const QString &error) {
            const desk::Instrument *now = instrument();
            if (!now || now->symbol != sym)
                return;
            m_live = research::raw(q.value(sym), "regularMarketPrice");
            m_conversion = needFx ? research::raw(q.value(fxSymbol), "regularMarketPrice") : 1.0;
            if (!std::isnan(m_live) && m_prefillOnQuote) {
                m_prefillOnQuote = false;
                const int decimals = decimalsFor(m_live);
                const double distance = m_live * 0.005;
                const QSignalBlocker a(m_entry), b(m_stop), c(m_target);
                for (QDoubleSpinBox *sp : {m_entry, m_stop, m_target})
                    sp->setDecimals(decimals);
                m_entry->setValue(m_live);
                m_stop->setValue(m_live - distance);
                m_target->setValue(m_live + distance * 2);
            }
            m_quoteNote->setText(!error.isEmpty() && std::isnan(m_live)
                                     ? tr("Live rate unavailable: %1. Type the levels by hand.").arg(error)
                                     : tr("Levels prefilled from the live price, yours to change. Quotes are about 15 minutes delayed."));
            recompute();
        });
        recompute();
    }

    static int decimalsFor(double price)
    {
        if (price >= 1000) return 2;
        if (price >= 20) return 3;
        return 5;
    }

    void recompute()
    {
        const desk::Instrument *i = instrument();
        if (!i)
            return;
        const Palette &c = Theme::current();
        const QString account = m_currency->currentText().trimmed().toUpper();
        QSettings s;
        s.setValue("journal/risk/instrument", i->id);
        s.setValue("journal/risk/currency", account);
        s.setValue("journal/risk/balance", m_balance->value());
        s.setValue("journal/risk/risk", m_risk->value());
        s.setValue("journal/risk/unit", m_unit->checkedId());

        desk::RiskInput in;
        in.balance = m_balance->value();
        in.risk = m_risk->value();
        in.riskIsPercent = m_unit->checkedId() == 0;
        in.entry = m_entry->value();
        in.stop = m_stop->value();
        in.target = m_target->value();
        in.contractSize = i->contractSize;
        in.quoteToAccount = i->quote == account ? 1.0 : m_conversion;
        const desk::RiskResult r = desk::computeRisk(in);
        m_map->setLevels(in.stop, in.entry, in.target, m_entry->decimals());
        if (!r.valid) {
            for (QLabel *l : {m_lots, m_units, m_distance, m_riskValue, m_reward, m_ratio})
                l->setText("—");
            m_grade->clear();
            m_breakeven->clear();
            const QHash<QString, QString> why {{"balance", tr("Enter the account's balance.")},
                                               {"risk", tr("Enter what you accept to lose.")},
                                               {"stop", tr("The stop must differ from the entry.")},
                                               {"conversion", tr("The rate from %1 to %2 isn't known yet.").arg(i->quote, account)},
                                               {"target", tr("The target must be on the other side of the entry from the stop.")}};
            m_message->setText(why.value(r.error));
            m_message->setStyleSheet(QString("color: %1").arg(c.Down.name()));
            return;
        }
        m_message->setStyleSheet({});
        m_message->setText(tr("Position size = risk ÷ (stop distance × value of the quote currency). 1 lot of %1 = %2 units, the size of a standard account: check your broker's. Rate from %3 to %4: %5.")
                               .arg(i->label, QLocale().toString(i->contractSize, 'f', 0), i->quote, account, QLocale().toString(in.quoteToAccount, 'f', 5)));
        m_lots->setText(QLocale().toString(r.lots, 'f', 2));
        m_lotsSub->setText(tr("lots"));
        m_units->setText(QLocale().toString(r.units, 'f', r.units < 100 ? 2 : 0));
        m_side->setText(r.isLong ? tr("LONG: units") : tr("SHORT: units"));
        m_side->setStyleSheet(QString("color: %1").arg((r.isLong ? c.Up : c.Down).name()));
        m_distance->setText(QLocale().toString(r.stopDistance, 'f', m_entry->decimals()));
        m_distanceSub->setText(pct(r.stopDistance / in.entry * 100).remove('+') + tr(" of the price"));
        m_riskValue->setText(QLocale().toString(r.riskAmount, 'f', 2));
        m_riskValue->setStyleSheet(QString("color: %1").arg(c.Down.name()));
        m_riskSub->setText(tr("%1, %2% of the balance").arg(account, QLocale().toString(r.riskPercent, 'f', 2)));
        if (r.hasTarget) {
            m_reward->setText(QLocale().toString(r.rewardAmount, 'f', 2));
            m_reward->setStyleSheet(QString("color: %1").arg(c.Up.name()));
            m_rewardSub->setText(tr("%1 at the take profit").arg(account));
            m_ratio->setText(QString("1 : %1").arg(QLocale().toString(r.rewardRisk, 'f', 2)));
            const desk::RiskGrade g = desk::gradeOf(r.rewardRisk);
            m_grade->setText(g == desk::RiskGrade::Poor ? tr("POOR") : g == desk::RiskGrade::Thin ? tr("THIN") : g == desk::RiskGrade::Acceptable ? tr("ACCEPTABLE") : tr("STRONG"));
            m_breakeven->setText(tr("Breakeven win rate: %1%").arg(QLocale().toString(r.breakevenWinRate, 'f', 0)));
        } else {
            m_reward->setText("—");
            m_rewardSub->setText(tr("no take profit"));
            m_ratio->setText("—");
            m_grade->clear();
            m_breakeven->clear();
        }
    }

    QComboBox *m_instrument, *m_currency;
    QDoubleSpinBox *m_balance, *m_risk, *m_entry, *m_stop, *m_target;
    QButtonGroup *m_unit;
    QLabel *m_quoteNote, *m_lots, *m_lotsSub, *m_units, *m_side, *m_distance, *m_distanceSub, *m_riskValue, *m_riskSub, *m_reward, *m_rewardSub;
    QLabel *m_ratio, *m_grade, *m_breakeven, *m_message;
    TradeMap *m_map;
    double m_live = research::NaN, m_conversion = research::NaN;
    bool m_prefillOnQuote = false;
};

} // namespace

JournalPage::JournalPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("quantPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(quant::mono(12));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(36);
    auto *sh = new QHBoxLayout(sub);
    sh->setContentsMargins(6, 0, 12, 0);
    sh->setSpacing(2);
    m_tabs = new QButtonGroup(this);
    const QStringList names {tr("Dashboard"), tr("Trade log"), tr("Advanced details"), tr("Risk calculator")};
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
    sh->addStretch(1);
    v->addWidget(sub);

    auto *barHolder = new QWidget;
    auto *bh = new QVBoxLayout(barHolder);
    bh->setContentsMargins(12, 10, 12, 0);
    auto *bar = new JournalBar;
    bh->addWidget(bar);
    v->addWidget(barHolder);

    m_stack = new QStackedWidget;
    m_stack->addWidget(new DashboardView(bar));
    m_stack->addWidget(new TradeLogView(bar));
    m_stack->addWidget(new AdvancedView(bar));
    m_stack->addWidget(new RiskView);
    v->addWidget(m_stack, 1);

    connect(m_tabs, &QButtonGroup::idClicked, this, &JournalPage::showSection);
    connect(m_tabs, &QButtonGroup::idClicked, barHolder, [barHolder](int index) { barHolder->setVisible(index != 3); });
    const int section = std::clamp(QSettings().value("journal/section", 0).toInt(), 0, m_stack->count() - 1);
    barHolder->setVisible(section != 3);
    showSection(section);
}

void JournalPage::showSection(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_tabs->button(index)->setChecked(true);
    m_stack->setCurrentIndex(index);
    QSettings().setValue("journal/section", index);
}

#include "JournalPage.moc"
