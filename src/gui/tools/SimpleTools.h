// =============================================================================
// SimpleTools.h
//
// Ferramentas de interação curta, que não justificam arquivos próprios.
// Hoje só o balde de tinta (FillTool), que preenche a região contígua da
// mesma cor.
// =============================================================================
#pragma once

#include "Tool.h"

namespace spritenator::gui {

/// Balde de tinta.
class FillTool final : public Tool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    /// A região contígua que seria preenchida.
    [[nodiscard]] std::vector<core::Point> footprint(
        const EditorSession& session, core::Point point) const override;
};

} // namespace spritenator::gui
