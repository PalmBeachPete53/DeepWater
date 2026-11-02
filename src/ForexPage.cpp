#include "ForexPage.h"

#include "FxData.h"
#include "MacroData.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolTip>
#include <QVBoxLayout>

namespace {

constexpr int SparkDays = 90;

int decimalsFor(const FxPair &p)
{
    return p.quote == "JPY" ? 2 : 4;
}

QColor mix(const QColor &a, const QColor &b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

QLabel *sectionLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setObjectName("sectionLabel");
    return l;
}

} // namespace

// ---------------------------------------------------------------- FxPairCard

FxPairCard::FxPairCard(FxData *fx, PolicyRates *rates, int pair, QWidget *parent)
    : QWidget(parent)
    , m_fx(fx)
    , m_rates(rates)
    , m_pair(pair)
{
    setMinimumSize(170, 118);
}

void FxPairCard::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();
    const FxPair &pair = m_fx->pairs()[m_pair];
    const QLocale loc;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(ui.ControlBorder, 1));
    p.setBrush(ui.Panel);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

    const double pad = 14, w = width() - 2 * pad;
    QFont f = font();
    f.setPointSizeF(12.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 10, w, 20), Qt::AlignLeft | Qt::AlignVCenter, pair.label());

    const QVector<double> s = m_fx->series(m_pair);
    if (s.isEmpty())
        return;
    const double pct = m_fx->changePct(m_pair);
    const QColor trend = pct >= 0 ? ui.Up : ui.Down;

    f.setPointSizeF(11);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(trend);
    p.drawText(QRectF(pad, 10, w, 20), Qt::AlignRight | Qt::AlignVCenter,
               QString("%1%2%").arg(pct >= 0 ? "+" : "", loc.toString(pct, 'f', 2)));

    f.setPointSizeF(19);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 32, w, 28), Qt::AlignLeft | Qt::AlignVCenter,
               loc.toString(s.last(), 'f', decimalsFor(pair)));

    // Carry: policy rate of the base currency minus that of the quote currency.
    if (m_rates->hasRate(pair.base) && m_rates->hasRate(pair.quote)) {
        const double carry = m_rates->current(pair.base) - m_rates->current(pair.quote);
        QFont cf = font();
        cf.setPointSizeF(10);
        p.setFont(cf);
        p.setPen(ui.TextDim);
        p.drawText(QRectF(pad, 32, w, 28), Qt::AlignRight | Qt::AlignVCenter,
                   tr("carry %1%2 pp").arg(carry >= 0 ? "+" : "", loc.toString(carry, 'f', 2)));
    }

    // 90-day sparkline
    const int from = std::max<int>(0, s.size() - SparkDays);
    const auto [lo, hi] = std::minmax_element(s.begin() + from, s.end());
    const double span = *hi - *lo > 1e-12 ? *hi - *lo : 1;
    const QRectF spark(pad, 66, w, height() - 66 - 12);
    QPainterPath path;
    for (int i = from; i < s.size(); ++i) {
        const QPointF pt(spark.left() + spark.width() * (i - from) / std::max(1, int(s.size()) - 1 - from),
                         spark.bottom() - (s[i] - *lo) / span * spark.height());
        if (i == from) path.moveTo(pt); else path.lineTo(pt);
    }
    const QColor line = s.last() >= s[from] ? ui.Up : ui.Down;
    p.setPen(QPen(line, 1.4));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

// ---------------------------------------------------------------- PolicyRateCard

PolicyRateCard::PolicyRateCard(PolicyRates *rates, int bank, QWidget *parent)
    : QWidget(parent)
    , m_rates(rates)
    , m_bank(bank)
{
    setMinimumSize(170, 132);
}

void PolicyRateCard::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();
    const PolicyRates::Bank &bank = m_rates->banks()[m_bank];
    const QLocale loc;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(ui.ControlBorder, 1));
    p.setBrush(ui.Panel);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

    const double pad = 14, w = width() - 2 * pad;
    QFont f = font();
    f.setPointSizeF(12.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 10, w, 20), Qt::AlignLeft | Qt::AlignVCenter, bank.currency);
    f.setWeight(QFont::Normal);
    f.setPointSizeF(11);
    p.setFont(f);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(pad, 10, w, 20), Qt::AlignRight | Qt::AlignVCenter, bank.name);

    const QVector<PolicyRates::Point> h = m_rates->history(bank.currency);
    if (h.isEmpty())
        return;
    f.setPointSizeF(19);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 32, w, 28), Qt::AlignLeft | Qt::AlignVCenter,
               QString("%1%").arg(loc.toString(h.last().rate, 'f', 2)));

    // Last move, in basis points.
    QDate when;
    double delta = 0;
    f.setPointSizeF(10);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    if (m_rates->lastChange(bank.currency, &when, &delta)) {
        p.setPen(delta < 0 ? ui.Down : ui.Up);
        p.drawText(QRectF(pad, 60, w, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   tr("%1%2 bp on %3").arg(delta > 0 ? "+" : "").arg(qRound(delta * 100))
                       .arg(loc.toString(when, QLocale::ShortFormat)));
    } else {
        p.setPen(ui.TextDim);
        p.drawText(QRectF(pad, 60, w, 16), Qt::AlignLeft | Qt::AlignVCenter, tr("unchanged over 3 years"));
    }
    const double year = m_rates->changeOver(bank.currency, 365);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(pad, 60, w, 16), Qt::AlignRight | Qt::AlignVCenter,
               tr("1Y %1%2 bp").arg(year > 0 ? "+" : "").arg(qRound(year * 100)));

    // Step chart over the full history, on a shared 0-based scale.
    const QRectF chart(pad, 82, w, height() - 82 - 12);
    double hi = 0.5, lo = 0;
    for (const auto &pt : h) {
        hi = std::max(hi, pt.rate);
        lo = std::min(lo, pt.rate);
    }
    const qint64 t0 = h.first().date.toJulianDay(), t1 = std::max(t0 + 1, h.last().date.toJulianDay());
    auto pos = [&](const QDate &d, double r) {
        return QPointF(chart.left() + chart.width() * (d.toJulianDay() - t0) / double(t1 - t0),
                       chart.bottom() - (r - lo) / (hi - lo) * chart.height());
    };
    QPainterPath path;
    path.moveTo(pos(h.first().date, h.first().rate));
    for (int i = 1; i < h.size(); ++i) {
        if (h[i].rate != h[i - 1].rate)
            path.lineTo(pos(h[i].date, h[i - 1].rate));
        path.lineTo(pos(h[i].date, h[i].rate));
    }
    p.setPen(QPen(ui.Accent, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

// ---------------------------------------------------------------- PositioningCard

PositioningCard::PositioningCard(CotData *cot, int contract, QWidget *parent)
    : QWidget(parent)
    , m_cot(cot)
    , m_contract(contract)
{
    setMinimumSize(200, 168);
}

void PositioningCard::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();
    const CotData::Contract &c = m_cot->contracts()[m_contract];
    const QLocale loc;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(ui.ControlBorder, 1));
    p.setBrush(ui.Panel);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

    const double pad = 14, w = width() - 2 * pad;
    QFont f = font();
    f.setPointSizeF(12.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 10, w, 20), Qt::AlignLeft | Qt::AlignVCenter, tr("%1 vs USD").arg(c.currency));

    const QVector<CotData::Week> weeks = m_cot->weeks(c.currency);
    if (weeks.isEmpty())
        return;
    const CotData::Week &last = weeks.last();
    const CotData::Week *prev = weeks.size() > 1 ? &weeks[weeks.size() - 2] : nullptr;
    f.setPointSizeF(10.5);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(pad, 10, w, 20), Qt::AlignRight | Qt::AlignVCenter, loc.toString(last.date, QLocale::ShortFormat));

    // Two rows: asset managers (structural) and leveraged funds (speculative).
    const QColor amColor = ui.Accent;
    const QColor lfColor = ui.Down.lighter(110);
    auto row = [&](double y, const QString &label, const QColor &color, double pct, double prevPct) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(pad + 4, y + 9), 3.5, 3.5);
        QFont lf = font();
        lf.setPointSizeF(10.5);
        p.setFont(lf);
        p.setPen(ui.TextDim);
        p.drawText(QRectF(pad + 14, y, w, 18), Qt::AlignLeft | Qt::AlignVCenter, label);
        QFont vf = font();
        vf.setPointSizeF(12);
        vf.setWeight(QFont::DemiBold);
        p.setFont(vf);
        p.setPen(pct >= 0 ? ui.Up : ui.Down);
        const QString value = QString("%1%2%").arg(pct >= 0 ? "+" : "", loc.toString(pct, 'f', 1));
        const double dw = 70;
        p.drawText(QRectF(pad, y, w - dw, 18), Qt::AlignRight | Qt::AlignVCenter, value);
        if (prev) {
            const double d = pct - prevPct;
            p.setFont(lf);
            p.setPen(ui.TextDim);
            p.drawText(QRectF(pad, y, w, 18), Qt::AlignRight | Qt::AlignVCenter,
                       tr("%1%2 /wk").arg(d >= 0 ? "+" : "", loc.toString(d, 'f', 1)));
        }
    };
    row(36, tr("Asset managers"), amColor, last.assetMgrPct(), prev ? prev->assetMgrPct() : 0);
    row(58, tr("Leveraged funds"), lfColor, last.levFundsPct(), prev ? prev->levFundsPct() : 0);

    // 52-week history of both nets, % of open interest, with a zero line.
    const QRectF chart(pad, 86, w, height() - 86 - 12);
    double lim = 5;
    for (const auto &wk : weeks)
        lim = std::max({lim, std::abs(wk.assetMgrPct()), std::abs(wk.levFundsPct())});
    auto y = [&](double v) { return chart.center().y() - v / lim * chart.height() / 2; };
    p.setPen(QPen(ui.PanelLine, 1, Qt::DashLine));
    p.drawLine(QPointF(chart.left(), y(0)), QPointF(chart.right(), y(0)));
    for (const auto &[color, am] : {std::pair(lfColor, false), std::pair(amColor, true)}) {
        QPainterPath path;
        for (int i = 0; i < weeks.size(); ++i) {
            const QPointF pt(chart.left() + chart.width() * i / std::max(1, int(weeks.size()) - 1),
                             y(am ? weeks[i].assetMgrPct() : weeks[i].levFundsPct()));
            if (i == 0) path.moveTo(pt); else path.lineTo(pt);
        }
        p.setPen(QPen(color, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
}

// ---------------------------------------------------------------- CorrelationMatrix

CorrelationMatrix::CorrelationMatrix(FxData *fx, QWidget *parent)
    : QWidget(parent)
    , m_fx(fx)
{
    setMouseTracking(true);
    setMinimumHeight(sizeHint().height());
    recompute();
}

QSize CorrelationMatrix::sizeHint() const
{
    const int n = m_fx->pairs().size();
    return QSize(90 + n * 64, 30 + n * 38);
}

void CorrelationMatrix::setWindow(int returns)
{
    m_window = returns;
    recompute();
}

void CorrelationMatrix::recompute()
{
    m_matrix = m_fx->correlation(m_window, &m_used);
    update();
}

QRectF CorrelationMatrix::cellRect(int row, int col) const
{
    const int n = m_fx->pairs().size();
    const double labelW = 84, headerH = 28;
    const double cellW = std::max(40.0, (width() - labelW) / n);
    const double cellH = std::max(28.0, (height() - headerH) / n);
    return QRectF(labelW + col * cellW, headerH + row * cellH, cellW, cellH);
}

void CorrelationMatrix::paintEvent(QPaintEvent *)
{
    if (m_matrix.isEmpty())
        return;
    const Palette &ui = Theme::current();
    const QLocale loc;
    const int n = m_fx->pairs().size();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = font();
    f.setPointSizeF(10.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);

    // Column and row labels
    for (int i = 0; i < n; ++i) {
        const bool hot = i == m_hoverRow || i == m_hoverCol;
        p.setPen(hot ? ui.Text : ui.TextDim);
        const QRectF top = cellRect(0, i);
        p.drawText(QRectF(top.left(), 0, top.width(), 26), Qt::AlignCenter, m_fx->pairs()[i].label());
        const QRectF left = cellRect(i, 0);
        p.drawText(QRectF(0, left.top(), 78, left.height()), Qt::AlignRight | Qt::AlignVCenter,
                   m_fx->pairs()[i].label());
    }

    // Cells: diverging colors, negative toward Down and positive toward Up.
    f.setWeight(QFont::Normal);
    f.setPointSizeF(10.5);
    p.setFont(f);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            const double v = m_matrix[r][c];
            const QRectF cell = cellRect(r, c).adjusted(1.5, 1.5, -1.5, -1.5);
            QColor fill = r == c ? ui.Hover : mix(ui.Panel, v >= 0 ? ui.Up : ui.Down, std::abs(v) * 0.85);
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawRoundedRect(cell, 4, 4);
            if (r == m_hoverRow || c == m_hoverCol) {
                QColor ring = ui.Text;
                ring.setAlpha(r == m_hoverRow && c == m_hoverCol ? 220 : 60);
                p.setPen(QPen(ring, 1));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(cell, 4, 4);
            }
            // Text contrast: strong cells get the background color as text.
            p.setPen(r == c ? ui.TextFaint : (std::abs(v) > 0.6 ? ui.Background : ui.Text));
            p.drawText(cell, Qt::AlignCenter, loc.toString(v, 'f', 2));
        }
    }
}

void CorrelationMatrix::mouseMoveEvent(QMouseEvent *e)
{
    const int n = m_fx->pairs().size();
    int row = -1, col = -1;
    for (int r = 0; r < n && row < 0; ++r)
        for (int c = 0; c < n; ++c)
            if (cellRect(r, c).contains(e->position())) { row = r; col = c; break; }
    if (row != m_hoverRow || col != m_hoverCol) {
        m_hoverRow = row;
        m_hoverCol = col;
        update();
    }
    if (row >= 0 && row != col && !m_matrix.isEmpty()) {
        QToolTip::showText(e->globalPosition().toPoint(),
                           tr("%1 vs %2: %3 over %4 daily returns")
                               .arg(m_fx->pairs()[row].label(), m_fx->pairs()[col].label(),
                                    QLocale().toString(m_matrix[row][col], 'f', 2))
                               .arg(m_used),
                           this);
    } else {
        QToolTip::hideText();
    }
}

void CorrelationMatrix::leaveEvent(QEvent *)
{
    m_hoverRow = m_hoverCol = -1;
    update();
}

// ---------------------------------------------------------------- ForexPage

ForexPage::ForexPage(FxData *fx, PolicyRates *rates, CotData *cot, QWidget *parent)
    : QWidget(parent)
    , m_fx(fx)
    , m_rates(rates)
    , m_cot(cot)
{
    setObjectName("sidePanel");
    setAttribute(Qt::WA_StyledBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName("panelContent");
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(32, 26, 32, 32);
    l->setSpacing(6);

    auto *title = new QLabel(tr("Forex"));
    title->setObjectName("title");
    l->addWidget(title);
    m_source = new QLabel;
    m_source->setObjectName("dim");
    m_source->setWordWrap(true);
    l->addWidget(m_source);
    l->addSpacing(14);

    l->addWidget(sectionLabel(tr("Main pairs")));
    auto *grid = new QGridLayout;
    grid->setSpacing(12);
    for (int i = 0; i < m_fx->pairs().size(); ++i) {
        auto *card = new FxPairCard(m_fx, m_rates, i);
        grid->addWidget(card, i / 5, i % 5);
        m_cards.push_back(card);
    }
    l->addLayout(grid);
    l->addSpacing(18);

    // Policy rates
    l->addWidget(sectionLabel(tr("Policy rates")));
    m_ratesSource = new QLabel;
    m_ratesSource->setObjectName("dim");
    m_ratesSource->setWordWrap(true);
    l->addWidget(m_ratesSource);
    l->addSpacing(4);
    auto *ratesGrid = new QGridLayout;
    ratesGrid->setSpacing(12);
    for (int i = 0; i < m_rates->banks().size(); ++i) {
        auto *card = new PolicyRateCard(m_rates, i);
        ratesGrid->addWidget(card, i / 5, i % 5);
        m_repaint.push_back(card);
    }
    l->addLayout(ratesGrid);
    l->addSpacing(18);

    // Capital flows: futures positioning
    l->addWidget(sectionLabel(tr("Capital flows · futures positioning")));
    m_flowsSource = new QLabel;
    m_flowsSource->setObjectName("dim");
    m_flowsSource->setWordWrap(true);
    l->addWidget(m_flowsSource);
    l->addSpacing(4);
    auto *flowsGrid = new QGridLayout;
    flowsGrid->setSpacing(12);
    for (int i = 0; i < m_cot->contracts().size(); ++i) {
        auto *card = new PositioningCard(m_cot, i);
        flowsGrid->addWidget(card, i / 4, i % 4);
        m_repaint.push_back(card);
    }
    l->addLayout(flowsGrid);
    l->addSpacing(18);

    // Correlation matrix with its window selector.
    auto *header = new QHBoxLayout;
    header->addWidget(sectionLabel(tr("Correlation of daily returns")));
    header->addStretch();
    auto *windows = new QButtonGroup(this);
    const struct { const char *label; int returns; } choices[] = {
        {QT_TR_NOOP("1M"), 21}, {QT_TR_NOOP("3M"), 63}, {QT_TR_NOOP("6M"), 126}, {QT_TR_NOOP("1Y"), 252}};
    m_matrix = new CorrelationMatrix(m_fx);
    for (const auto &choice : choices) {
        auto *b = new QPushButton(tr(choice.label));
        b->setObjectName("segButton");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(choice.returns == 63);
        windows->addButton(b);
        header->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, returns = choice.returns] {
            m_matrix->setWindow(returns);
            refresh();
        });
    }
    l->addLayout(header);
    l->addSpacing(6);
    l->addWidget(m_matrix);
    m_matrixNote = new QLabel;
    m_matrixNote->setObjectName("dim");
    m_matrixNote->setWordWrap(true);
    l->addWidget(m_matrixNote);
    l->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);

    connect(m_fx, &FxData::updated, this, [this] {
        m_matrix->reload();
        refresh();
    });
    connect(m_rates, &PolicyRates::updated, this, &ForexPage::refresh);
    connect(m_cot, &CotData::updated, this, &ForexPage::refresh);
    refresh();
}

void ForexPage::refresh()
{
    const QLocale loc;
    const QVector<QDate> &dates = m_fx->dates();
    QString source = tr("European Central Bank reference rates via frankfurter.dev: one fix per business day "
                        "around 16:00 CET, no intraday data.");
    if (!dates.isEmpty())
        source += ' ' + tr("Latest fix: %1.").arg(loc.toString(dates.last(), QLocale::LongFormat));
    else if (!m_fx->errorString().isEmpty())
        source += ' ' + tr("Could not load rates: %1").arg(m_fx->errorString());
    else
        source += ' ' + tr("Loading…");
    m_source->setText(source);

    m_matrixNote->setText(
        tr("Pearson correlation of daily log returns over the last %1 fixes. Quote convention matters: "
           "EUR/USD and USD/CHF both measure the dollar, from opposite sides, so they correlate negatively.")
            .arg(m_matrix->observations()));
    for (FxPairCard *c : std::as_const(m_cards))
        c->update();
    for (QWidget *w : std::as_const(m_repaint))
        w->update();

    QString rates = tr("Bank for International Settlements (BIS), daily series as reported by each central bank "
                       "(for the US, the midpoint of the Fed funds target range). Pair cards show the carry: base "
                       "minus quote policy rate.");
    if (!m_rates->errorString().isEmpty())
        rates += ' ' + tr("Could not load: %1").arg(m_rates->errorString());
    m_ratesSource->setText(rates);

    QString flows = tr("CFTC Traders in Financial Futures, CME contracts vs USD, released every Friday for the "
                       "previous Tuesday. Net position as % of open interest: asset managers (funds, pensions) "
                       "and leveraged funds (hedge funds, CTAs). The weekly change shows money moving in or out.");
    if (!m_cot->errorString().isEmpty())
        flows += ' ' + tr("Could not load: %1").arg(m_cot->errorString());
    m_flowsSource->setText(flows);
}
