// =============================================================================
// PenTool.h
//
// Lápis: desenho à mão livre com a ponta escolhida na barra de opções
// (quadrada ou redonda, de 1 a 64 pixels). Botão esquerdo desenha com a cor
// primária; botão direito, com a secundária.
// =============================================================================
#pragma once

#include "Tool.h"

#include "core/Color.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

/// Desenho à mão livre.
class PenTool final : public Tool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void cancel(EditorSession& session) override;
    [[nodiscard]] std::vector<core::Point> footprint(
        const EditorSession& session, core::Point point) const override;

private:
    /// Desenha um segmento de reta com o pincel atual.
    void stroke(EditorSession& session, core::Point from, core::Point to) const;

    bool m_active = false;
    core::Point m_last;
    ColorRole m_role = ColorRole::Primary; ///< cor primária ou secundária
};

} // namespace spritenator::gui
