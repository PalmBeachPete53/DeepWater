#pragma once

// Text drawn like the browser draws it. On macOS Qt adds tracking and drops kerning for the system
// font, so a word comes out a few percent wider or narrower than in Chrome; for the painted parts of
// the UI (top bar and draw bar buttons, the chart legend, drawing labels) the text is shaped with
// CoreText (same advances and kerning as the browser) and the glyphs drawn by Qt at those positions.
// Elsewhere (and on other systems) this falls back to QFontMetricsF / QPainter::drawText.

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QString>

class QPainter;

namespace qtm::text {

double width(const QFont &font, const QString &s);
// fillText with the alphabetic baseline at `origin`.
void draw(QPainter &p, const QPointF &origin, const QFont &font, const QString &s, const QColor &color);
// Ascent and descent the browser uses for a line box (font metrics).
double ascent(const QFont &font);
double descent(const QFont &font);

} // namespace qtm::text
