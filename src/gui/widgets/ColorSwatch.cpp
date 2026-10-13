// =============================================================================
// ColorSwatch.cpp
// =============================================================================
#include "ColorSwatch.h"

#include "gui/ImageRendering.h"

#include <QMouseEvent>
#include <QPainter>

namespace spritenator::gui {

ColorSwatch::ColorSwatch(QWidget* parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ColorSwatch::setColor(const QColor& color)
{
    if (color != m_color) {
        m_color = color;
        update();
    }
}

void ColorSwatch::setText(const QString& text)
{
    if (text != m_text) {
        m_text = text;
        update();
    }
}

void ColorSwatch::setActive(bool active)
{
    if (active != m_active) {
        m_active = active;
        update();
    }
}

QSize ColorSwatch::sizeHint() const
{
    return { 64, 30 };
}

QSize ColorSwatch::minimumSizeHint() const
{
    return { 32, 24 };
}

void ColorSwatch::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const QRect area = rect().adjusted(3, 3, -3, -3);

    // Xadrez por baixo: só aparece através de cores translúcidas.
    if (m_color.alpha() < 255) {
        paintTransparencyGrid(painter, area, 6);
    }
    painter.fillRect(area, m_color);

    if (!m_text.isEmpty()) {
        // Texto em preto ou branco conforme a luminosidade (e o xadrez claro).
        const bool dark = m_color.alpha() >= 128 && m_color.lightness() <= 128;
        painter.setPen(dark ? Qt::white : Qt::black);
        painter.drawText(area, Qt::AlignCenter, m_text);
    }

    // Moldura: destaque na cor de seleção do sistema quando ativo.
    painter.setBrush(Qt::NoBrush);
    if (m_active) {
        painter.setPen(QPen(palette().color(QPalette::Highlight), 3));
        painter.drawRect(rect().adjusted(1, 1, -2, -2));
    } else {
        painter.setPen(QPen(palette().color(QPalette::Mid), 1));
        painter.drawRect(area.adjusted(0, 0, -1, -1));
    }
}

void ColorSwatch::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

} // namespace spritenator::gui
