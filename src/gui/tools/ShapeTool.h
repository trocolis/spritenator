// =============================================================================
// ShapeTool.h
//
// Ferramentas de figuras geométricas: reta, retângulo, retângulo arredondado
// e elipse (contorno ou preenchidos). Enquanto o mouse é arrastado a figura
// é redesenhada do zero a cada movimento, sobre a imagem do início da edição
// (efeito "elástico"); ao soltar o botão ela é fixada e vira um passo de
// desfazer.
// =============================================================================
#pragma once

#include "Tool.h"

#include "core/Color.h"
#include "gui/commands/UndoLabel.h"

namespace spritenator::gui {

/// Tipos de figura desenhados pela ShapeTool.
enum class Shape { Line, Rectangle, RoundRectangle, Ellipse };

/// Ferramenta de figuras com efeito elástico.
class ShapeTool final : public Tool {
public:
    ShapeTool(Shape shape, bool filled);

    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void cancel(EditorSession& session) override;

private:
    /// Desenha a figura do ponto inicial até "current" (sobre o snapshot).
    void drawShape(EditorSession& session, core::Point current) const;

    /// Operação registrada no histórico (nome exibido no menu Desfazer).
    [[nodiscard]] UndoLabel undoLabel() const;

    Shape m_shape;
    bool m_filled;
    bool m_active = false;
    core::Point m_start;
    core::Point m_last;
    core::ColorIndex m_color = 0;
};

} // namespace spritenator::gui
