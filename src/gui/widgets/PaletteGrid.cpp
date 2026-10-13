// =============================================================================
// PaletteGrid.cpp
// =============================================================================
#include "PaletteGrid.h"

#include "gui/ImageRendering.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace spritenator::gui {

namespace {

constexpr int kColumns = 16;
constexpr int kPreferredCell = 18;
constexpr int kMinimumCell = 11;

} // namespace

PaletteGrid::PaletteGrid(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void PaletteGrid::setColorPalette(const core::Palette& palette)
{
    m_palette = palette;
    m_visibleCount = palette.size();
    setSelection(std::min(m_anchor, m_visibleCount - 1), std::min(m_current, m_visibleCount - 1));
    updateGeometry();
    update();
}

void PaletteGrid::setVisibleCount(int count)
{
    m_visibleCount = std::clamp(count, 1, m_palette.size());
    updateGeometry();
    update();
}

void PaletteGrid::setSelection(int anchor, int current)
{
    anchor = std::clamp(anchor, 0, m_visibleCount - 1);
    current = std::clamp(current, 0, m_visibleCount - 1);
    if (anchor != m_anchor || current != m_current) {
        m_anchor = anchor;
        m_current = current;
        emit selectionChanged();
        update();
    }
}

int PaletteGrid::rows() const
{
    return (m_visibleCount + kColumns - 1) / kColumns;
}

int PaletteGrid::cellSize() const
{
    // A célula precisa caber tanto na largura quanto na altura disponíveis.
    const int byWidth = (width() - 1) / kColumns;
    const int byHeight = (height() - 1) / std::max(1, rows());
    return std::clamp(std::min(byWidth, byHeight), 4, kPreferredCell);
}

QSize PaletteGrid::sizeHint() const
{
    return { kColumns * kPreferredCell + 1, rows() * kPreferredCell + 1 };
}

QSize PaletteGrid::minimumSizeHint() const
{
    return { kColumns * kMinimumCell + 1, rows() * kMinimumCell + 1 };
}

QRect PaletteGrid::cellRect(int index) const
{
    const int size = cellSize();
    return { (index % kColumns) * size, (index / kColumns) * size, size, size };
}

int PaletteGrid::indexAt(const QPoint& position) const
{
    const int size = cellSize();
    if (position.x() < 0 || position.y() < 0 || position.x() >= kColumns * size) {
        return -1;
    }
    const int index = (position.y() / size) * kColumns + position.x() / size;
    return index < m_visibleCount ? index : -1;
}

void PaletteGrid::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    // Desabilitada (ex.: nenhum arquivo carregado ainda): área vazia.
    if (!isEnabled()) {
        painter.fillRect(rect(), palette().color(QPalette::Window));
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }
    for (int index = 0; index < m_visibleCount; ++index) {
        const QRect cell = cellRect(index);
        painter.fillRect(cell, toQColor(m_palette.color(index)));
        painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
        painter.drawRect(cell);
    }
    // Intervalo selecionado: moldura dupla em cada célula.
    painter.setBrush(Qt::NoBrush);
    for (int index = selectionFirst(); index <= selectionLast(); ++index) {
        const QRect cell = cellRect(index);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(cell);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(cell.adjusted(1, 1, -1, -1));
    }
}

void PaletteGrid::mousePressEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (index < 0 || event->button() != Qt::LeftButton) {
        return;
    }
    // Shift estende o intervalo a partir da âncora atual.
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        setSelection(m_anchor, index);
    } else {
        setSelection(index, index);
    }
}

void PaletteGrid::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0 && event->button() == Qt::LeftButton) {
        emit colorDoubleClicked(index);
    }
}

} // namespace spritenator::gui
