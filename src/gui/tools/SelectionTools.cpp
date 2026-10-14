// =============================================================================
// SelectionTools.cpp
// =============================================================================
#include "SelectionTools.h"

#include "gui/Document.h"
#include "gui/EditorSession.h"

#include <algorithm>
#include <cstdlib>

namespace spritenator::gui {

// -----------------------------------------------------------------------------
// SelectionTool (base)
// -----------------------------------------------------------------------------
void SelectionTool::beginShape(EditorSession& session)
{
    const Qt::KeyboardModifiers modifiers = session.inputModifiers();
    if (modifiers.testFlag(Qt::ControlModifier)) {
        m_combine = Combine::Subtract;
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
        m_combine = Combine::Add;
    } else {
        m_combine = Combine::Replace;
    }
    const Document* document = session.activeDocument();
    m_base = document != nullptr ? document->selection() : core::Selection { };
}

void SelectionTool::applyShape(EditorSession& session, const core::Selection& shape) const
{
    Document* document = session.activeDocument();
    if (document == nullptr) {
        return;
    }
    switch (m_combine) {
    case Combine::Replace: document->setSelection(shape); break;
    case Combine::Add: document->setSelection(m_base.united(shape)); break;
    case Combine::Subtract: document->setSelection(m_base.subtracted(shape)); break;
    }
}

void SelectionTool::restoreBase(EditorSession& session) const
{
    if (Document* document = session.activeDocument()) {
        document->setSelection(m_base);
    }
}

// -----------------------------------------------------------------------------
// DragSelectTool: retângulo e oval
// -----------------------------------------------------------------------------
void DragSelectTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if ((button != Qt::LeftButton && button != Qt::RightButton)
        || session.activeDocument() == nullptr) {
        return;
    }
    beginShape(session);
    m_active = true;
    m_dragged = false;
    m_anchor = point;
}

core::Rect DragSelectTool::areaTo(const EditorSession& session, core::Point point) const
{
    if (session.inputModifiers().testFlag(Qt::ShiftModifier)) {
        // Shift: quadrado (ou círculo), do tamanho do maior lado arrastado.
        const int side = std::max(std::abs(point.x - m_anchor.x), std::abs(point.y - m_anchor.y));
        point = { m_anchor.x + (point.x < m_anchor.x ? -side : side),
            m_anchor.y + (point.y < m_anchor.y ? -side : side) };
    }
    return core::Rect::fromCorners(m_anchor, point);
}

void DragSelectTool::move(EditorSession& session, core::Point point)
{
    if (!m_active) {
        return;
    }
    m_dragged = m_dragged || point != m_anchor;
    if (m_dragged) {
        applyShape(session, shapeFor(areaTo(session, point)));
    }
}

void DragSelectTool::release(EditorSession& session, core::Point point)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (m_dragged) {
        applyShape(session, shapeFor(areaTo(session, point)));
    } else if (m_combine == Combine::Replace) {
        // Clique simples (sem somar/subtrair): desfaz a seleção.
        if (Document* document = session.activeDocument()) {
            document->clearSelection();
        }
    }
}

void DragSelectTool::cancel(EditorSession& session)
{
    if (m_active) {
        m_active = false;
        restoreBase(session);
    }
}

core::Selection RectSelectTool::shapeFor(const core::Rect& area) const
{
    return core::Selection::rectangle(area);
}

core::Selection EllipseSelectTool::shapeFor(const core::Rect& area) const
{
    return core::Selection::ellipse(area);
}

// -----------------------------------------------------------------------------
// PolygonSelectTool: ponto a ponto
// -----------------------------------------------------------------------------
void PolygonSelectTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (session.activeDocument() == nullptr) {
        return;
    }
    if (button == Qt::RightButton) {
        if (!m_vertices.empty()) {
            finish(session); // botão direito fecha o polígono
        }
        return;
    }
    if (button != Qt::LeftButton) {
        return;
    }
    if (m_vertices.empty()) {
        beginShape(session); // o modo vale para o polígono inteiro
    } else if (point == m_vertices.back()
        || (m_vertices.size() >= 3 && point == m_vertices.front())) {
        // Clique duplo (mesmo ponto do último vértice) ou clique no primeiro
        // ponto: fecha.
        finish(session);
        return;
    }
    m_vertices.push_back(point);
    m_cursor = point;
}

void PolygonSelectTool::move(EditorSession& session, core::Point point)
{
    hover(session, point);
}

void PolygonSelectTool::release(EditorSession& /*session*/, core::Point /*point*/)
{
}

void PolygonSelectTool::hover(EditorSession& /*session*/, core::Point point)
{
    m_cursor = point;
}

bool PolygonSelectTool::keyPress(EditorSession& session, int key)
{
    if (m_vertices.empty()) {
        return false;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        finish(session);
        return true;
    }
    if (key == Qt::Key_Backspace) {
        m_vertices.pop_back();
        return true;
    }
    return false;
}

void PolygonSelectTool::cancel(EditorSession& /*session*/)
{
    m_vertices.clear(); // a seleção só muda ao fechar: nada a restaurar
}

std::vector<core::Point> PolygonSelectTool::previewPath() const
{
    std::vector<core::Point> path = m_vertices;
    if (!path.empty()) {
        path.push_back(m_cursor); // elástico até o mouse
    }
    return path;
}

void PolygonSelectTool::finish(EditorSession& session)
{
    const std::vector<core::Point> vertices = std::move(m_vertices);
    m_vertices.clear();
    applyShape(session, core::Selection::polygon(vertices));
}

// -----------------------------------------------------------------------------
// LassoSelectTool: desenho livre
// -----------------------------------------------------------------------------
void LassoSelectTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if ((button != Qt::LeftButton && button != Qt::RightButton)
        || session.activeDocument() == nullptr) {
        return;
    }
    beginShape(session);
    m_points = { point };
}

void LassoSelectTool::move(EditorSession& /*session*/, core::Point point)
{
    if (!m_points.empty() && point != m_points.back()) {
        m_points.push_back(point);
    }
}

void LassoSelectTool::release(EditorSession& session, core::Point point)
{
    if (m_points.empty()) {
        return;
    }
    move(session, point);
    const std::vector<core::Point> points = std::move(m_points);
    m_points.clear();
    if (points.size() == 1 && m_combine == Combine::Replace) {
        if (Document* document = session.activeDocument()) {
            document->clearSelection(); // clique simples: desfaz a seleção
        }
        return;
    }
    applyShape(session, core::Selection::polygon(points));
}

void LassoSelectTool::cancel(EditorSession& /*session*/)
{
    m_points.clear();
}

// -----------------------------------------------------------------------------
// MagicWandTool
// -----------------------------------------------------------------------------
core::Selection magicWandSelection(const EditorSession& session, core::Point point)
{
    const Document* document = session.activeDocument();
    if (document == nullptr) {
        return { };
    }
    return core::magicWand(document->canvas(), document->palette(), point, session.wandTolerance(),
        session.wandContiguous());
}

void MagicWandTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        return;
    }
    beginShape(session);
    applyShape(session, magicWandSelection(session, point));
}

void MagicWandTool::move(EditorSession& /*session*/, core::Point /*point*/)
{
}

void MagicWandTool::release(EditorSession& /*session*/, core::Point /*point*/)
{
}

std::vector<core::Point> MagicWandTool::footprint(
    const EditorSession& session, core::Point point) const
{
    // Limite de segurança (como no balde): regiões enormes destacariam
    // milhões de pixels a cada movimento do mouse.
    constexpr long long kMaxHighlighted = 262144; // 512 x 512
    const core::Selection selection = magicWandSelection(session, point);
    if (selection.pixelCount() > kMaxHighlighted) {
        return { point };
    }
    return selection.points();
}

} // namespace spritenator::gui
