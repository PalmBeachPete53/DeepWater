#pragma once

// Port of quant-terminal/public/lib/seasonals.js. Returns the same object shape as JS
// seasonals(bars), as JSON ({ years: [...], monthly: [...], average: [...], ... }).

#include "Bar.h"

#include <QJsonObject>

namespace qtm {

QJsonObject seasonals(const Bars &bars);

} // namespace qtm
