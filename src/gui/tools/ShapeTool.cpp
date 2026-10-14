// =============================================================================
// ShapeTool.cpp
// =============================================================================
#include "ShapeTool.h"

#include "core/Brush.h"
#include "core/Raster.h"
#include "gui/Document.h"
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
    Document* document = session.activeDocument();
    if (document == nullptr) {
        return;
    }
    m_role = colorRoleFor(button); // esquerdo = primária; direito = secundária
    m_active = true;
    m_start = point;
    m_last = point;
    document->beginEdit();
    drawShape(session, point);
}

void ShapeTool::move(EditorSession& session, core::Point point)
{
    if (!m_active || point == m_last) {
        return;
    }
    m_last = point;
    // Efeito elástico: apaga a figura anterior e desenha a nova.
    Document* document = session.activeDocument();
    if (document == nullptr) {
        return;
    }
    document->restoreSnapshot();
    drawShape(session, point);
}

void ShapeTool::release(EditorSession& session, core::Point /*point*/)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (Document* document = session.activeDocument()) {
        document->commitEdit(undoLabel());
    }
}

void ShapeTool::cancel(EditorSession& session)
{
    m_active = false;
    Tool::cancel(session);
}

void ShapeTool::drawShape(EditorSession& session, core::Point current) const
{
    Document* document = session.activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::Rect bounds = core::Rect::fromCorners(m_start, current);
    // 1. O contorno de 1 px da figura (o "caminho" da ponta).
    std::vector<core::Point> outline;
    const core::Plotter collect = [&](core::Point point) { outline.push_back(point); };
    switch (m_shape) {
    case Shape::Line: core::plotLine(m_start, current, collect); break;
    case Shape::Rectangle: core::plotRectangle(bounds, false, collect); break;
    case Shape::RoundRectangle:
        core::plotRoundRectangle(bounds, core::defaultCornerSize(bounds), false, collect);
        break;
    case Shape::Ellipse: core::plotEllipse(bounds, false, collect); break;
    }
    // 2. O traço: a ponta do lápis carimbada ao longo do contorno.
    const int size = session.brushSize();
    std::vector<core::Point> pixels = core::strokePixels(outline, session.brushShape(), size);
    // 3. Preenchidas: o interior também.
    if (m_filled) {
        const core::Plotter fill = [&](core::Point point) { pixels.push_back(point); };
        switch (m_shape) {
        case Shape::Line: break;
        case Shape::Rectangle: core::plotRectangle(bounds, true, fill); break;
        case Shape::RoundRectangle:
            core::plotRoundRectangle(bounds, core::defaultCornerSize(bounds), true, fill);
            break;
        case Shape::Ellipse: core::plotEllipse(bounds, true, fill); break;
        }
    }
    const core::Rect extent = core::brushExtent(size);
    document->editLive([&](core::AnyImage& image) {
        for (const core::Point point : pixels) {
            session.plot(image, point, m_role);
        }
        // Os dois cantos, aumentados pela ponta.
        return core::Rect { bounds.x + extent.x, bounds.y + extent.y,
            bounds.width + extent.width - 1, bounds.height + extent.height - 1 };
    });
}

std::vector<core::Point> ShapeTool::footprint(const EditorSession& session, core::Point point) const
{
    // Antes de começar: a ponta no ponto inicial.
    std::vector<core::Point> points;
    for (const core::Point offset : core::brushOffsets(session.brushShape(), session.brushSize())) {
        points.push_back(point + offset);
    }
    return points;
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
