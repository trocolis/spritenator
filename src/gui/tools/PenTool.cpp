// =============================================================================
// PenTool.cpp
// =============================================================================
#include "PenTool.h"

#include "core/Brush.h"
#include "core/Raster.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

void PenTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
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
    m_last = point;
    document->beginEdit();
    stroke(session, point, point);
}

void PenTool::move(EditorSession& session, core::Point point)
{
    if (!m_active || point == m_last) {
        return;
    }
    // Liga o ponto anterior ao atual: o traço fica contínuo mesmo com o
    // mouse movendo vários pixels entre dois eventos.
    stroke(session, m_last, point);
    m_last = point;
}

void PenTool::release(EditorSession& session, core::Point /*point*/)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (Document* document = session.activeDocument()) {
        document->commitEdit(UndoLabel { UndoOperation::Pen });
    }
}

void PenTool::cancel(EditorSession& session)
{
    m_active = false;
    Tool::cancel(session);
}

void PenTool::stroke(EditorSession& session, core::Point from, core::Point to) const
{
    Document* document = session.activeDocument();
    if (document == nullptr) {
        return;
    }
    // Ponta atual (formato e tamanho escolhidos na barra de opções).
    const std::vector<core::Point> brush
        = core::brushOffsets(session.brushShape(), session.brushSize());
    const core::Rect extent = core::brushExtent(session.brushSize());
    document->editLive([&](core::AnyImage& image) {
        core::plotLine(from, to, [&](core::Point center) {
            for (const core::Point offset : brush) {
                session.plot(image, center + offset, m_role);
            }
        });
        // Área suja: retângulo da reta, expandido pelo tamanho da ponta.
        const core::Rect bounds = core::lineBounds(from, to);
        return core::Rect { bounds.x + extent.x, bounds.y + extent.y,
            bounds.width + extent.width - 1, bounds.height + extent.height - 1 };
    });
}

std::vector<core::Point> PenTool::footprint(const EditorSession& session, core::Point point) const
{
    std::vector<core::Point> points;
    for (const core::Point offset : core::brushOffsets(session.brushShape(), session.brushSize())) {
        points.push_back(point + offset);
    }
    return points;
}

} // namespace spritenator::gui
