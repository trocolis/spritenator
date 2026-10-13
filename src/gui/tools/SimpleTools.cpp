// =============================================================================
// SimpleTools.cpp
// =============================================================================
#include "SimpleTools.h"

#include "core/Raster.h"
#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

// -----------------------------------------------------------------------------
// FillTool
// -----------------------------------------------------------------------------
void FillTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        return;
    }
    const core::ColorIndex color
        = button == Qt::LeftButton ? session.drawColor() : session.backgroundColor();
    DrawBuffer* buffer = session.buffer();

    // A região é calculada ANTES de pintar, sobre a imagem original: com tons
    // (retícula) a região pintada não é uniforme e não serve de referência.
    const std::vector<core::Point> region = core::floodRegion(buffer->image(), point);
    if (region.empty()) {
        return;
    }

    buffer->beginEdit();
    buffer->editLive([&](core::IndexedImage& image) {
        core::Rect bounds;
        for (const core::Point pixel : region) {
            session.plot(image, pixel, color);
            bounds = bounds.united({ pixel.x, pixel.y, 1, 1 });
        }
        return bounds;
    });
    buffer->commitEdit(UndoLabel { UndoOperation::Fill });
}

void FillTool::move(EditorSession& /*session*/, core::Point /*point*/)
{
}

void FillTool::release(EditorSession& /*session*/, core::Point /*point*/)
{
}

// -----------------------------------------------------------------------------
// SelectTool
// -----------------------------------------------------------------------------
void SelectTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        return;
    }
    m_active = true;
    m_dragged = false;
    m_anchor = point;
    session.buffer()->setSelection(core::Rect::fromCorners(point, point));
}

void SelectTool::move(EditorSession& session, core::Point point)
{
    if (!m_active) {
        return;
    }
    m_dragged = m_dragged || point != m_anchor;
    session.buffer()->setSelection(core::Rect::fromCorners(m_anchor, point));
}

void SelectTool::release(EditorSession& session, core::Point /*point*/)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (!m_dragged) {
        session.buffer()->setSelection(std::nullopt); // clique simples: limpa
    }
}

void SelectTool::cancel(EditorSession& session)
{
    m_active = false;
    session.buffer()->setSelection(std::nullopt);
}

// -----------------------------------------------------------------------------
// ZoomTool
// -----------------------------------------------------------------------------
void ZoomTool::press(EditorSession& session, core::Point /*point*/, Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        session.zoomIn();
    } else if (button == Qt::RightButton) {
        session.zoomOut();
    }
}

void ZoomTool::move(EditorSession& /*session*/, core::Point /*point*/)
{
}

void ZoomTool::release(EditorSession& /*session*/, core::Point /*point*/)
{
}

} // namespace spritenator::gui
