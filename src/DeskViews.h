#pragma once

#include "CalendarData.h"
#include "DeskData.h"
#include "DeskPage.h"
#include "DeskWidgets.h"
#include "MacroData.h"
#include "QuantViews.h"
#include "ResearchData.h"
#include "Theme.h"
#include "desk/Instruments.h"
#include "desk/Market.h"

#include <QComboBox>
#include <QPushButton>

// What the Desk's sections share.
namespace deskui {

inline QComboBox *deskCombo()
{
    auto *c = new QComboBox;
    c->setObjectName("resCombo");
    c->setCursor(Qt::PointingHandCursor);
    return c;
}

inline QPushButton *deskAction(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setObjectName("qtChip");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

// Asks the AI side panel to read this section. The prompt holds no data, only what to read and how to answer.
inline QPushButton *askAiButton(const Context &c, const QString &text, const QString &prompt)
{
    QPushButton *b = deskAction(text);
    QObject::connect(b, &QPushButton::clicked, c.page, [page = c.page, prompt] { emit page->askAi(prompt); });
    return b;
}

// The price decimals a market is usually quoted with.
inline int priceDecimals(double price)
{
    if (price >= 1000) return 2;
    if (price >= 20) return 2;
    if (price >= 5) return 3;
    return 5;
}

} // namespace deskui
