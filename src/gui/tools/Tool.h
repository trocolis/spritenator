// =============================================================================
// Tool.h
//
// Interface das ferramentas (padrão Strategy). O editor de imagem
// (CanvasEditor) não sabe COMO cada ferramenta funciona: apenas converte
// eventos de mouse para coordenadas da imagem e repassa à ferramenta atual
// da EditorSession. As ferramentas editam o documento ativo da sessão.
// =============================================================================
#pragma once

#include "core/Geometry.h"

#include <Qt>

#include <vector>

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

    /// Botão pressionado sobre o pixel "point" da imagem.
    virtual void press(EditorSession& session, core::Point point, Qt::MouseButton button) = 0;

    /// Mouse arrastado (com botão pressionado) até "point".
    virtual void move(EditorSession& session, core::Point point) = 0;

    /// Botão solto em "point": a operação é concluída.
    virtual void release(EditorSession& session, core::Point point) = 0;

    /// Operação interrompida (Esc, troca de ferramenta, perda do mouse).
    virtual void cancel(EditorSession& session);

    /// Pixels que a ferramenta afetaria se o botão fosse pressionado em
    /// "point" (antes de filtrar tom e máscara). Exibidos em destaque quando o
    /// mouse passa sobre a imagem. Padrão: o próprio pixel.
    [[nodiscard]] virtual std::vector<core::Point> footprint(
        const EditorSession& session, core::Point point) const;

protected:
    Tool() = default;
    Tool(const Tool&) = default;
    Tool& operator=(const Tool&) = default;
};

} // namespace spritenator::gui
