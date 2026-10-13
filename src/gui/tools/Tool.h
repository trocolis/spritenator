// =============================================================================
// Tool.h
//
// Interface das ferramentas da área de desenho (padrão Strategy). A área de
// desenho (DrawAreaView) não sabe COMO cada ferramenta funciona: apenas
// converte eventos de mouse para coordenadas do buffer e repassa à
// ferramenta atual da EditorSession.
// =============================================================================
#pragma once

#include "core/Geometry.h"

#include <Qt>

class QPainter;

namespace spritenator::gui {

class EditorSession;

/// Identificadores das ferramentas (mesma ordem dos botões do original).
enum class ToolKind {
    Select, ///< seleção retangular
    Zoom, ///< lupa: esquerdo amplia, direito reduz
    Fill, ///< balde de tinta (preenchimento por inundação)
    PenThin, ///< lápis de 1 pixel
    PenThick, ///< lápis de 3 pixels
    Line, ///< reta
    Rectangle, ///< retângulo (contorno)
    RoundRectangle, ///< retângulo de cantos arredondados (contorno)
    Ellipse, ///< elipse (contorno)
    FilledRectangle, ///< retângulo preenchido
    FilledRoundRectangle, ///< retângulo de cantos arredondados preenchido
    FilledEllipse, ///< elipse preenchida
};

/// Ferramenta de desenho.
class Tool {
public:
    virtual ~Tool() = default;

    /// Botão pressionado sobre o pixel "point" do buffer.
    virtual void press(EditorSession& session, core::Point point, Qt::MouseButton button) = 0;

    /// Mouse arrastado (com botão pressionado) até "point".
    virtual void move(EditorSession& session, core::Point point) = 0;

    /// Botão solto em "point": a operação é concluída.
    virtual void release(EditorSession& session, core::Point point) = 0;

    /// Operação interrompida (Esc, troca de ferramenta, perda do mouse).
    virtual void cancel(EditorSession& session);

    /// Desenha elementos temporários sobre a área de desenho (ex.: retângulo
    /// de seleção em andamento). "scale" é o zoom atual.
    virtual void paintOverlay(QPainter& painter, int scale) const;

protected:
    Tool() = default;
    Tool(const Tool&) = default;
    Tool& operator=(const Tool&) = default;
};

} // namespace spritenator::gui
