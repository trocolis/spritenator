// =============================================================================
// PenTool.h
//
// Lápis fino (1 pixel) e grosso (3 pixels). Botão esquerdo desenha com a cor
// de desenho; botão direito, com a cor de fundo — como no original.
// =============================================================================
#pragma once

#include "Tool.h"

#include "core/Color.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

/// Desenho à mão livre.
class PenTool final : public Tool {
public:
    /// "thick" = pincel de 3 pixels de largura (formato de "+").
    explicit PenTool(bool thick);

    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void cancel(EditorSession& session) override;

private:
    /// Desenha um segmento de reta com o pincel atual.
    void stroke(EditorSession& session, core::Point from, core::Point to) const;

    bool m_thick;
    bool m_active = false;
    core::Point m_last;
    ColorRole m_role = ColorRole::Primary; ///< cor primária ou secundária
};

} // namespace spritenator::gui
