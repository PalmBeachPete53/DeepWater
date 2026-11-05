#pragma once

#include "AiRead.h"

#include <QColor>
#include <QVector>
#include <QWidget>

// Minimal line chart. In "detailed" mode it adds an area fill, a baseline at
// the first value, and high/low labels.
class Sparkline : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;
    explicit Sparkline(QWidget *parent = nullptr);

    void setValues(const QVector<double> &values);
    void setColor(const QColor &color);
    void setDetailed(bool detailed);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<double> m_values;
    QColor m_color;
    bool m_detailed = false;
};
