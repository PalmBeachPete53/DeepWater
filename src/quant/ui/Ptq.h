#pragma once

// Port of public/js/ptq.js: the "Check with PTQ" box under a tester result and the PTQ tab, in
// PTQ's own brand look (near-black ground with glows, Instrument Sans/Serif, Fragment Mono, pills).
// The terminal only reads from PTQ and can only make it do less (stop, disarm).

#include <QJsonObject>

#include <functional>

class QWidget;

namespace qtm {

class App;
namespace panels { class Body; }

namespace ptq {

// ptqStatus(force): GET /api/ptq/status, cached for 15 s. `done` gets the status object
// ({ connected: false, reason } when the request fails).
void ptqStatus(QObject *context, bool force, std::function<void(const QJsonObject &)> done);

// mountPtqCheck(el, ctx, tried): fills `el` (a widget with a QVBoxLayout, emptied first).
// ctx = { strategy, symbol, tf, name }.
void mountPtqCheck(QWidget *el, App *app, const QJsonObject &ctx, bool tried = false);

// renderPtqTab(el, app, tried): the PTQ bottom tab (refreshes itself every 15 s while shown).
void renderPtqTab(panels::Body *el, App *app, bool tried = false);

} // namespace ptq
} // namespace qtm
