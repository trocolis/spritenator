// =============================================================================
// SimpleTools.h
//
// Ferramentas de interação curta, que não justificam arquivos próprios:
//   - FillTool:   balde de tinta (preenche a região contígua da mesma cor);
//   - SelectTool: seleção retangular arrastando o mouse;
//   - ZoomTool:   lupa (esquerdo amplia, direito reduz; tratada pelo editor
//                 de imagem, pois o zoom é de cada editor).
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

/// Seleção retangular. Um clique simples (sem arrastar) remove a seleção.
class SelectTool final : public Tool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void cancel(EditorSession& session) override;

private:
    bool m_active = false;
    bool m_dragged = false;
    core::Point m_anchor;
};

/// Lupa.
class ZoomTool final : public Tool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    [[nodiscard]] std::vector<core::Point> footprint(
        const EditorSession& session, core::Point point) const override;
};

} // namespace spritenator::gui
