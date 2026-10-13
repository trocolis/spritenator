// =============================================================================
// PenTool.cpp
// =============================================================================
#include "PenTool.h"

#include "core/Raster.h"
#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"

#include <QCoreApplication>

namespace spritenator::gui {

namespace {

/// Formato do pincel grosso: um "+" de 3x3 (aproxima um círculo de 3 px,
/// como a caneta de largura 3 do Windows usada pelo original).
constexpr core::Point kThickBrush[] = { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

} // namespace

PenTool::PenTool(bool thick)
    : m_thick(thick)
{
}

void PenTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        return;
    }
    // Esquerdo = cor de desenho; direito = cor de fundo.
    m_color = button == Qt::LeftButton ? session.drawColor() : session.backgroundColor();
    m_active = true;
    m_last = point;
    session.buffer()->beginEdit();
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
    session.buffer()->commitEdit(QCoreApplication::translate("PenTool", "Pen"));
}

void PenTool::cancel(EditorSession& session)
{
    m_active = false;
    Tool::cancel(session);
}

void PenTool::stroke(EditorSession& session, core::Point from, core::Point to) const
{
    const int margin = m_thick ? 1 : 0;
    session.buffer()->editLive([&](core::IndexedImage& image) {
        core::plotLine(from, to, [&](core::Point center) {
            if (m_thick) {
                for (const core::Point offset : kThickBrush) {
                    session.plot(image, center + offset, m_color);
                }
            } else {
                session.plot(image, center, m_color);
            }
        });
        // Área suja: retângulo da reta, expandido pela espessura do pincel.
        const core::Rect bounds = core::lineBounds(from, to);
        return core::Rect { bounds.x - margin, bounds.y - margin, bounds.width + 2 * margin,
            bounds.height + 2 * margin };
    });
}

} // namespace spritenator::gui
