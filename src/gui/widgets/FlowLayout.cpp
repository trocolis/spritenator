// =============================================================================
// FlowLayout.cpp
// =============================================================================
#include "FlowLayout.h"

#include <QWidget>

#include <algorithm>

namespace spritenator::gui {

FlowLayout::FlowLayout(QWidget* parent, int spacing)
    : QLayout(parent)
{
    setContentsMargins(0, 0, 0, 0);
    setSpacing(spacing);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem* item = takeAt(0)) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
}

int FlowLayout::count() const
{
    return static_cast<int>(m_items.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    return m_items.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return { };
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

QSize FlowLayout::minimumSize() const
{
    // No mínimo, o maior item (cada um numa linha).
    QSize size;
    for (const QLayoutItem* item : m_items) {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize FlowLayout::sizeHint() const
{
    // Ideal: todos numa linha só.
    int width = 0;
    int height = 0;
    for (const QLayoutItem* item : m_items) {
        width += item->sizeHint().width() + (width > 0 ? spacing() : 0);
        height = std::max(height, item->sizeHint().height());
    }
    const QMargins margins = contentsMargins();
    return { width + margins.left() + margins.right(), height + margins.top() + margins.bottom() };
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const
{
    const QMargins margins = contentsMargins();
    const QRect area
        = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int x = area.x();
    int y = area.y();
    int lineHeight = 0;
    for (QLayoutItem* item : m_items) {
        if (item->isEmpty()) { // widgets ocultos não ocupam espaço
            continue;
        }
        const QSize size = item->sizeHint();
        // Não cabe nesta linha (e ela já tem algo): quebra.
        if (x + size.width() > area.right() + 1 && lineHeight > 0) {
            x = area.x();
            y += lineHeight + spacing();
            lineHeight = 0;
        }
        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), size));
        }
        x += size.width() + spacing();
        lineHeight = std::max(lineHeight, size.height());
    }
    return y + lineHeight - rect.y() + margins.bottom();
}

} // namespace spritenator::gui
