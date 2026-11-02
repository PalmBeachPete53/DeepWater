#pragma once

#include <QWidget>

class CotReport;
class QButtonGroup;
class QStackedWidget;

// The Quant page: a sub-header with the sections (compare, correlation, risk & beta, COT, signals) and the
// section shown, which is remembered.
class QuantPage : public QWidget
{
    Q_OBJECT
public:
    explicit QuantPage(CotReport *cot, QWidget *parent = nullptr);

    void showSection(int index);

private:
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
};
