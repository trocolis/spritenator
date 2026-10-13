// =============================================================================
// ShapeTool.cpp
// =============================================================================
#include "ShapeTool.h"

#include "core/Raster.h"
#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

ShapeTool::ShapeTool(Shape shape, bool filled)
    : m_shape(shape)
    , m_filled(filled)
{
}

void ShapeTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        return;
    }
    m_color = button == Qt::LeftButton ? session.drawColor() : session.backgroundColor();
    m_active = true;
    m_start = point;
    m_last = point;
    session.buffer()->beginEdit();
    drawShape(session, point);
}

void ShapeTool::move(EditorSession& session, core::Point point)
{
    if (!m_active || point == m_last) {
        return;
    }
    m_last = point;
    // Efeito elástico: apaga a figura anterior e desenha a nova.
    session.buffer()->restoreSnapshot();
    drawShape(session, point);
}

void ShapeTool::release(EditorSession& session, core::Point /*point*/)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    session.buffer()->commitEdit(undoLabel());
}

void ShapeTool::cancel(EditorSession& session)
{
    m_active = false;
    Tool::cancel(session);
}

void ShapeTool::drawShape(EditorSession& session, core::Point current) const
{
    session.buffer()->editLive([&](core::IndexedImage& image) {
        const core::Plotter plot = [&](core::Point point) { session.plot(image, point, m_color); };
        const core::Rect bounds = core::Rect::fromCorners(m_start, current);
        switch (m_shape) {
        case Shape::Line: core::plotLine(m_start, current, plot); break;
        case Shape::Rectangle: core::plotRectangle(bounds, m_filled, plot); break;
        case Shape::RoundRectangle:
            core::plotRoundRectangle(bounds, core::defaultCornerSize(bounds), m_filled, plot);
            break;
        case Shape::Ellipse: core::plotEllipse(bounds, m_filled, plot); break;
        }
        return bounds; // todas as figuras cabem no retângulo dos dois cantos
    });
}

UndoLabel ShapeTool::undoLabel() const
{
    switch (m_shape) {
    case Shape::Line: return { UndoOperation::Line };
    case Shape::Rectangle:
        return { m_filled ? UndoOperation::FilledRectangle : UndoOperation::Rectangle };
    case Shape::RoundRectangle:
        return { m_filled ? UndoOperation::FilledRoundedRectangle
                          : UndoOperation::RoundedRectangle };
    case Shape::Ellipse:
        return { m_filled ? UndoOperation::FilledEllipse : UndoOperation::Ellipse };
    }
    return { UndoOperation::Line };
}

} // namespace spritenator::gui
