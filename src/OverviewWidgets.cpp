#include "OverviewWidgets.h"

#include "QuantCharts.h"
#include "ResearchCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimeZone>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace overview {

namespace {

bool ok(double v) { return std::isfinite(v); }

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

QString signedPct(double v, int decimals = 2)
{
    if (!ok(v))
        return "—";
    return QString("%1%2%").arg(v > 0 ? "+" : "").arg(v, 0, 'f', decimals);
}

QColor signColor(double v)
{
    const Palette &c = Theme::current();
    return !ok(v) ? c.TextFaint : v > 0 ? c.Up : v < 0 ? c.Down : c.TextDim;
}

} // namespace

// ---------------------------------------------------------------- Panel

Panel::Panel(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setObjectName("ovPanel");
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 4);
    v->setSpacing(0);
    auto *headBox = new QWidget;
    headBox->setObjectName("ovPanelHead");
    headBox->setAttribute(Qt::WA_StyledBackground);
    headBox->setFixedHeight(26);
    m_head = new QHBoxLayout(headBox);
    m_head->setContentsMargins(8, 0, 6, 0);
    m_head->setSpacing(6);
    m_title = new QLabel(title.toUpper());
    m_title->setObjectName("ovTitle");
    m_note = new QLabel;
    m_note->setObjectName("ovNote");
    m_note->hide();
    m_head->addWidget(m_title);
    m_head->addWidget(m_note);
    m_head->addStretch(1);
    v->addWidget(headBox);
    m_body = new QVBoxLayout;
    m_body->setContentsMargins(0, 2, 0, 0);
    m_body->setSpacing(0);
    v->addLayout(m_body, 1);
}

void Panel::setTitle(const QString &title)
{
    m_title->setText(title.toUpper());
}

void Panel::setNote(const QString &note)
{
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
}

QWidget *periodTabs(const QStringList &labels, QButtonGroup **group, int checked)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(1);
    auto *g = new QButtonGroup(w);
    g->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i]);
        b->setObjectName("ovTab");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);   // a focused button would scroll the page to it
        b->setChecked(i == checked);
        g->addButton(b, i);
        h->addWidget(b);
    }
    *group = g;
    return w;
}

// ---------------------------------------------------------------- Table

Table::Table(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void Table::setColumns(const QVector<Column> &columns, bool header)
{
    m_columns = columns;
    m_header = header;
    updateGeometry();
    update();
}

void Table::setRows(const QVector<Row> &rows)
{
    const bool resized = rows.size() != m_rows.size();
    m_rows = rows;
    if (!rows.isEmpty())
        m_message.clear();
    if (m_hover >= m_rows.size())
        m_hover = -1;
    if (resized)
        updateGeometry();
    update();
}

void Table::setMessage(const QString &message)
{
    m_message = message;
    update();
}

void Table::setMinimumRows(int rows)
{
    m_minRows = rows;
    updateGeometry();
}

QSize Table::sizeHint() const
{
    const int rows = std::max<int>(m_minRows, std::max<int>(m_rows.size(), 1));
    return {240, headerHeight() + rows * rowHeight() + 2};
}

QSize Table::minimumSizeHint() const
{
    return {160, sizeHint().height()};
}

QVector<int> Table::columnWidths() const
{
    const int pad = 8;
    const int avail = std::max(0, width() - 2 * pad);
    int fixed = 0;
    double stretch = 0;
    for (const Column &c : m_columns) {
        fixed += c.fixed;
        if (!c.fixed)
            stretch += c.stretch;
    }
    const int rest = std::max(0, avail - fixed);
    QVector<int> out;
    for (const Column &c : m_columns)
        out << (c.fixed ? c.fixed : stretch > 0 ? int(rest * c.stretch / stretch) : 0);
    return out;
}

int Table::rowAt(int y) const
{
    const int r = (y - headerHeight()) / rowHeight();
    return y >= headerHeight() && r >= 0 && r < m_rows.size() ? r : -1;
}

void Table::paintEvent(QPaintEvent *)
{
    const Palette &c = Theme::current();
    QPainter p(this);
    const QVector<int> widths = columnWidths();
    const int pad = 8, gap = 6;

    if (m_header) {
        p.setFont(quant::chartFont(9, true));
        p.setPen(c.TextFaint);
        int x = pad;
        for (int i = 0; i < m_columns.size(); ++i) {
            const QRect r(x, 0, widths[i], headerHeight());
            p.drawText(r.adjusted(0, 0, -gap / 2, 0), int(m_columns[i].align) | Qt::AlignVCenter,
                       m_columns[i].title.toUpper());
            x += widths[i];
        }
        p.setPen(withAlpha(c.PanelLine, 255));
        p.drawLine(pad, headerHeight() - 1, width() - pad, headerHeight() - 1);
    }

    if (m_rows.isEmpty()) {
        if (!m_message.isEmpty()) {
            p.setFont(quant::chartFont(11));
            p.setPen(c.TextFaint);
            p.drawText(rect().adjusted(pad, headerHeight() + 4, -pad, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                       m_message);
        }
        return;
    }

    const QFont regular = quant::chartFont(11), bold = quant::chartFont(11, true);
    const QFontMetrics fm(regular), fmBold(bold);
    for (int r = 0; r < m_rows.size(); ++r) {
        const Row &row = m_rows[r];
        const int y = headerHeight() + r * rowHeight();
        if (r == m_hover && !row.symbol.isEmpty())
            p.fillRect(QRect(0, y, width(), rowHeight()), c.Hover);
        if (row.marker.isValid())
            p.fillRect(QRect(3, y + 2, 2, rowHeight() - 4), row.marker);
        int x = pad;
        for (int i = 0; i < m_columns.size() && i < row.cells.size(); ++i) {
            const Cell &cell = row.cells[i];
            const QRect r(x, y, widths[i] - gap, rowHeight());
            if (ok(cell.bar)) {
                const int h = 6, w = r.width();
                const QRect track(r.left(), r.center().y() - h / 2 + 1, w, h);
                p.fillRect(track, withAlpha(c.TextFaint, 40));
                const int len = int(std::round(std::clamp(std::abs(cell.bar), 0.0, 1.0) * w));
                if (len > 0)
                    p.fillRect(QRect(track.left(), track.top(), len, h), cell.bar >= 0 ? c.Up : c.Down);
            } else if (!cell.text.isEmpty()) {
                p.setFont(cell.bold ? bold : regular);
                p.setPen(cell.color.isValid() ? cell.color : c.Text);
                const QString text = (cell.bold ? fmBold : fm).elidedText(cell.text, Qt::ElideRight, r.width());
                p.drawText(r, int(m_columns[i].align) | Qt::AlignVCenter, text);
            }
            x += widths[i];
        }
    }
}

void Table::mouseMoveEvent(QMouseEvent *e)
{
    const int r = rowAt(int(e->position().y()));
    if (r != m_hover) {
        m_hover = r;
        setCursor(r >= 0 && !m_rows[r].symbol.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void Table::mousePressEvent(QMouseEvent *e)
{
    const int r = rowAt(int(e->position().y()));
    if (e->button() == Qt::LeftButton && r >= 0 && !m_rows[r].symbol.isEmpty())
        emit symbolClicked(m_rows[r].symbol);
}

void Table::leaveEvent(QEvent *)
{
    m_hover = -1;
    unsetCursor();
    update();
}

bool Table::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        const int r = rowAt(he->pos().y());
        if (r >= 0 && !m_rows[r].tooltip.isEmpty())
            QToolTip::showText(he->globalPos(), m_rows[r].tooltip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

// ---------------------------------------------------------------- PerfChart

PerfChart::PerfChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PerfChart::setData(const QVector<qint64> &times, const QVector<Series> &series, const QString &format)
{
    QHash<QString, bool> visible;
    for (const Series &s : std::as_const(m_series))
        visible.insert(s.name, s.visible);
    m_times = times;
    m_series = series;
    for (Series &s : m_series)
        s.visible = visible.value(s.name, s.visible);
    m_format = format;
    m_message.clear();
    m_hover = -1;
    update();
}

void PerfChart::setSeriesVisible(const QString &name, bool visible)
{
    for (Series &s : m_series)
        if (s.name == name)
            s.visible = visible;
    update();
}

void PerfChart::setMessage(const QString &message)
{
    m_message = message;
    update();
}

void PerfChart::paintEvent(QPaintEvent *)
{
    const Palette &c = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int n = int(m_times.size());
    if (n < 2) {
        p.setFont(quant::chartFont(11));
        p.setPen(c.TextFaint);
        p.drawText(rect(), Qt::AlignCenter, m_message.isEmpty() ? tr("Loading…") : m_message);
        return;
    }

    // Range of the visible series, zero included.
    double lo = 0, hi = 0;
    for (const Series &s : m_series)
        if (s.visible)
            for (double v : s.values)
                if (ok(v)) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
    if (hi - lo < 1e-9) {
        hi += 0.1;
        lo -= 0.1;
    }
    const double padY = (hi - lo) * 0.08;
    lo -= padY;
    hi += padY;
    const QVector<double> ticks = research::niceTicks(lo, hi, 6);
    if (!ticks.isEmpty()) {
        lo = std::min(lo, ticks.first());
        hi = std::max(hi, ticks.last());
    }

    const QFont small = quant::chartFont(9), label = quant::chartFont(9, true);
    const QRectF plot(10, 10, width() - 10 - 96, height() - 10 - 22);
    auto X = [&](int i) { return plot.left() + plot.width() * i / double(n - 1); };
    auto Y = [&](double v) { return plot.bottom() - plot.height() * (v - lo) / (hi - lo); };

    // Grid and the % axis on the right.
    p.setFont(small);
    const int decimals = (hi - lo) < 2 ? 1 : 0;
    for (double t : ticks) {
        const double y = Y(t);
        p.setPen(QPen(c.GridLine, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(c.TextFaint);
        p.drawText(QRectF(width() - 46, y - 7, 42, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1%").arg(t, 0, 'f', decimals));
    }

    // Time labels, at least 90 px apart.
    const QTimeZone ny("America/New_York");
    const int step = std::max(1, int(std::ceil(n / std::max(1.0, plot.width() / 90.0))));
    for (int i = 0; i < n; i += step) {
        const double x = X(i);
        p.setPen(QPen(withAlpha(c.GridLine, 120), 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(c.TextFaint);
        const QString text = QLocale().toString(QDateTime::fromSecsSinceEpoch(m_times[i], ny), m_format);
        p.drawText(i == 0 ? QRectF(x, plot.bottom() + 4, 90, 14) : QRectF(x - 45, plot.bottom() + 4, 90, 14),
                   (i == 0 ? Qt::AlignLeft : Qt::AlignHCenter) | Qt::AlignTop, text);
    }

    // The envelope: green from zero up to the highest series, red from zero down to the lowest.
    QVector<double> top(n, 0), bottom(n, 0);
    int last = -1;
    for (int i = 0; i < n; ++i) {
        bool any = false;
        for (const Series &s : m_series)
            if (s.visible && i < s.values.size() && ok(s.values[i])) {
                top[i] = std::max(top[i], s.values[i]);
                bottom[i] = std::min(bottom[i], s.values[i]);
                any = true;
            }
        if (any)
            last = i;
    }
    if (last > 0) {
        QPainterPath up, down;
        up.moveTo(X(0), Y(0));
        down.moveTo(X(0), Y(0));
        for (int i = 0; i <= last; ++i) {
            up.lineTo(X(i), Y(top[i]));
            down.lineTo(X(i), Y(bottom[i]));
        }
        up.lineTo(X(last), Y(0));
        down.lineTo(X(last), Y(0));
        up.closeSubpath();
        down.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(c.Up, 70));
        p.drawPath(up);
        p.setBrush(withAlpha(c.Down, 70));
        p.drawPath(down);
        p.setBrush(Qt::NoBrush);
    }
    p.setPen(QPen(withAlpha(c.TextDim, 150), 1));
    p.drawLine(QPointF(plot.left(), Y(0)), QPointF(plot.right(), Y(0)));

    // Lines, then their names at their ends (pushed apart so they don't overlap).
    struct End {
        double y;
        QString name;
        QColor color;
    };
    QVector<End> ends;
    for (const Series &s : m_series) {
        if (!s.visible)
            continue;
        QPainterPath path;
        bool started = false;
        int end = -1;
        for (int i = 0; i < n && i < s.values.size(); ++i) {
            if (!ok(s.values[i]))
                continue;
            const QPointF pt(X(i), Y(s.values[i]));
            if (!started)
                path.moveTo(pt);
            else
                path.lineTo(pt);
            started = true;
            end = i;
        }
        p.setPen(QPen(s.color, 1.6));
        p.drawPath(path);
        if (end >= 0)
            ends.push_back({Y(s.values[end]), s.name, s.color});
    }
    std::sort(ends.begin(), ends.end(), [](const End &a, const End &b) { return a.y < b.y; });
    for (int i = 1; i < ends.size(); ++i)
        ends[i].y = std::max(ends[i].y, ends[i - 1].y + 12);
    p.setFont(label);
    for (const End &e : ends) {
        p.setPen(e.color);
        p.drawText(QRectF(plot.right() + 6, e.y - 7, 44, 14), Qt::AlignLeft | Qt::AlignVCenter, e.name);
    }

    // Crosshair.
    if (m_hover >= 0 && m_hover < n) {
        const double x = X(m_hover);
        p.setPen(QPen(withAlpha(c.Text, 110), 1, Qt::DashLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        QStringList lines;
        QVector<QColor> colors;
        lines << QLocale().toString(QDateTime::fromSecsSinceEpoch(m_times[m_hover], ny),
                                    m_format.contains("HH") ? "ddd MMM d, HH:mm" : "ddd MMM d, yyyy");
        colors << c.TextDim;
        for (const Series &s : m_series)
            if (s.visible && m_hover < s.values.size() && ok(s.values[m_hover])) {
                lines << QString("%1  %2").arg(s.name, -4).arg(signedPct(s.values[m_hover]));
                colors << s.color;
                p.setPen(Qt::NoPen);
                p.setBrush(s.color);
                p.drawEllipse(QPointF(x, Y(s.values[m_hover])), 3, 3);
                p.setBrush(Qt::NoBrush);
            }
        p.setFont(small);
        const QFontMetrics fm(small);
        int w = 0;
        for (const QString &l : lines)
            w = std::max(w, fm.horizontalAdvance(l));
        const QRectF box(x + 10 + w + 16 > plot.right() ? x - 10 - w - 16 : x + 10, plot.top() + 4, w + 16,
                         lines.size() * 14 + 8);
        p.setPen(c.TooltipBorder);
        p.setBrush(c.Tooltip);
        p.drawRoundedRect(box, 3, 3);
        for (int i = 0; i < lines.size(); ++i) {
            p.setPen(colors[i]);
            p.drawText(QRectF(box.left() + 8, box.top() + 4 + i * 14, w + 4, 14), Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
        }
    }
}

void PerfChart::mouseMoveEvent(QMouseEvent *e)
{
    const int n = int(m_times.size());
    if (n < 2)
        return;
    const double left = 10, w = width() - 10 - 96;
    const double x = e->position().x();
    const int i = x < left || x > left + w ? -1 : int(std::round((x - left) / w * (n - 1)));
    if (i != m_hover) {
        m_hover = i;
        update();
    }
}

void PerfChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

// ---------------------------------------------------------------- StatsView

StatsView::StatsView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(240, 230);
}

void StatsView::setData(const QVector<Stat> &stats, const QVector<Correlation> &correlations)
{
    m_stats = stats;
    m_correlations = correlations;
    update();
}

void StatsView::paintEvent(QPaintEvent *)
{
    const Palette &c = Theme::current();
    QPainter p(this);
    const QFont name = quant::chartFont(10, true), small = quant::chartFont(10), value = quant::chartFont(10, true);
    const int pad = 8, gap = 6;
    const int cardW = (width() - 2 * pad - gap) / 2, cardH = 80;

    for (int i = 0; i < m_stats.size(); ++i) {
        const Stat &s = m_stats[i];
        const QRect card(pad + (i % 2) * (cardW + gap), pad + (i / 2) * (cardH + gap), cardW, cardH);
        p.fillRect(card, withAlpha(c.Background, 140));
        p.setPen(c.PanelLine);
        p.drawRect(card.adjusted(0, 0, -1, -1));
        p.setFont(name);
        p.setPen(s.color);
        p.drawText(card.adjusted(8, 4, -8, 0), Qt::AlignLeft | Qt::AlignTop, s.name);
        const struct {
            QString label;
            double v;
            bool signedValue;
        } rows[] {{tr("Ret:"), s.ret, true}, {tr("High:"), s.high, true}, {tr("Low:"), s.low, true}, {tr("Vol:"), s.vol, false}};
        for (int r = 0; r < 4; ++r) {
            const QRect line(card.left() + 8, card.top() + 20 + r * 14, card.width() - 16, 14);
            p.setFont(small);
            p.setPen(c.TextDim);
            p.drawText(line, Qt::AlignLeft | Qt::AlignVCenter, rows[r].label);
            p.setFont(value);
            p.setPen(rows[r].signedValue ? signColor(rows[r].v) : c.Text);
            p.drawText(line, Qt::AlignRight | Qt::AlignVCenter,
                       rows[r].signedValue ? signedPct(rows[r].v)
                                           : ok(rows[r].v) ? QString("%1%").arg(rows[r].v, 0, 'f', 2) : "—");
        }
    }

    const int top = pad + ((int(m_stats.size()) + 1) / 2) * (cardH + gap) + 4;
    if (m_correlations.isEmpty())
        return;
    p.setFont(quant::chartFont(9, true));
    p.setPen(c.TextSection);
    p.drawText(QRect(pad, top, width() - 2 * pad, 14), Qt::AlignLeft | Qt::AlignVCenter, tr("CORRELATIONS"));
    for (int i = 0; i < m_correlations.size(); ++i) {
        const Correlation &k = m_correlations[i];
        const QRect line(pad + (i % 2) * (cardW + gap), top + 18 + (i / 2) * 15, cardW, 15);
        p.setFont(small);
        p.setPen(c.TextDim);
        p.drawText(line, Qt::AlignLeft | Qt::AlignVCenter, QString("%1/%2").arg(k.a, k.b));
        p.setFont(value);
        p.setPen(!ok(k.value) ? c.TextFaint : c.Text);
        p.drawText(line.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter,
                   ok(k.value) ? QString::number(k.value, 'f', 3) : "—");
    }
}

} // namespace overview
