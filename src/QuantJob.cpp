#include "QuantJob.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

namespace quant {

JobBar::JobBar(JobRunner *runner, QWidget *parent)
    : QFrame(parent)
{
    setObjectName("qtJobBar");
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(10);
    m_bar = new QProgressBar;
    m_bar->setObjectName("qtProgress");
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(8);
    h->addWidget(m_bar, 1);
    m_text = new QLabel;
    m_text->setObjectName("qtCardLabel");
    m_text->setMinimumWidth(220);
    h->addWidget(m_text);
    m_cancel = new QPushButton(tr("Cancel"));
    m_cancel->setObjectName("qtSeg");
    m_cancel->setCursor(Qt::PointingHandCursor);
    h->addWidget(m_cancel);
    hide();
    connect(runner, &JobRunner::started, this, [this] {
        m_bar->setValue(0);
        m_text->setText(tr("Starting…"));
        show();
    });
    connect(runner, &JobRunner::progress, this, [this](int percent, const QString &text) {
        m_bar->setValue(percent);
        m_text->setText(text);
    });
    connect(runner, &JobRunner::finished, this, &QWidget::hide);
    connect(m_cancel, &QPushButton::clicked, runner, &JobRunner::cancel);
}

} // namespace quant
