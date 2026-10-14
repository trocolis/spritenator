// =============================================================================
// SimpleTools.cpp
// =============================================================================
#include "SimpleTools.h"

#include "core/Raster.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"

#include <variant>

namespace spritenator::gui {

// -----------------------------------------------------------------------------
// FillTool
// -----------------------------------------------------------------------------
void FillTool::press(EditorSession& session, core::Point point, Qt::MouseButton button)
{
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        return;
    }
    const ColorRole role = colorRoleFor(button);
    Document* document = session.activeDocument();
    if (document == nullptr) {
        return;
    }

    // A região é calculada ANTES de pintar, sobre a imagem original: com tons
    // (retícula) a região pintada não é uniforme e não serve de referência.
    const std::vector<core::Point> region = std::visit(
        [&](const auto& image) { return core::floodRegion(image, point); }, document->canvas());
    if (region.empty()) {
        return;
    }

    document->beginEdit();
    document->editLive([&](core::AnyImage& image) {
        core::Rect bounds;
        for (const core::Point pixel : region) {
            session.plot(image, pixel, role);
            bounds = bounds.united({ pixel.x, pixel.y, 1, 1 });
        }
        return bounds;
    });
    document->commitEdit(UndoLabel { UndoOperation::Fill });
}

void FillTool::move(EditorSession& /*session*/, core::Point /*point*/)
{
}

void FillTool::release(EditorSession& /*session*/, core::Point /*point*/)
{
}

// -----------------------------------------------------------------------------
// Pegada do balde: a região que seria preenchida
// -----------------------------------------------------------------------------
std::vector<core::Point> FillTool::footprint(const EditorSession& session, core::Point point) const
{
    const Document* document = session.activeDocument();
    if (document == nullptr) {
        return { };
    }
    std::vector<core::Point> region = std::visit(
        [&](const auto& image) { return core::floodRegion(image, point); }, document->canvas());
    // Limite de segurança: em imagens enormes, destacar milhões de pixels a
    // cada movimento do mouse tornaria a interface lenta.
    constexpr std::size_t kMaxHighlighted = 262144; // 512 x 512
    if (region.size() > kMaxHighlighted) {
        return { point };
    }
    return region;
}

} // namespace spritenator::gui
