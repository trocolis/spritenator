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
    SelectEllipse, ///< seleção oval
    SelectPolygon, ///< seleção ponto a ponto (polígono)
    SelectLasso, ///< seleção livre (laço)
    MagicWand, ///< varinha mágica: região de cor parecida
    Fill, ///< balde de tinta (preenchimento por inundação)
    Pen, ///< lápis (ponta e tamanho configuráveis)
    Line, ///< reta
    Rectangle, ///< retângulo (contorno)
    RoundRectangle, ///< retângulo de cantos arredondados (contorno)
    Ellipse, ///< elipse (contorno)
    FilledRectangle, ///< retângulo preenchido
    FilledRoundRectangle, ///< retângulo de cantos arredondados preenchido
    FilledEllipse, ///< elipse preenchida
};

/// Verdadeiro para as ferramentas que criam seleções (elas não pintam).
[[nodiscard]] constexpr bool isSelectionTool(ToolKind kind)
{
    return kind == ToolKind::Select || kind == ToolKind::SelectEllipse
        || kind == ToolKind::SelectPolygon || kind == ToolKind::SelectLasso
        || kind == ToolKind::MagicWand;
}

/// Verdadeiro para as ferramentas que desenham com a ponta do lápis
/// (formato e tamanho da barra de opções): o lápis e as figuras.
[[nodiscard]] constexpr bool usesBrush(ToolKind kind)
{
    return kind == ToolKind::Pen || kind == ToolKind::Line || kind == ToolKind::Rectangle
        || kind == ToolKind::RoundRectangle || kind == ToolKind::Ellipse
        || kind == ToolKind::FilledRectangle || kind == ToolKind::FilledRoundRectangle
        || kind == ToolKind::FilledEllipse;
}

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

    /// Mouse movido SEM botão pressionado (ex.: o "elástico" do polígono).
    virtual void hover(EditorSession& session, core::Point point);

    /// Tecla pressionada com o editor em foco (Enter, Backspace...).
    /// Devolve true se a ferramenta tratou a tecla.
    virtual bool keyPress(EditorSession& session, int key);

    /// Operação interrompida (Esc, troca de ferramenta, perda do mouse).
    virtual void cancel(EditorSession& session);

    /// Verdadeiro enquanto há uma operação em andamento que o Esc cancela
    /// (ex.: um polígono ainda aberto).
    [[nodiscard]] virtual bool isBusy() const;

    /// Contorno em construção, exibido pelo editor como linha (polígono e
    /// laço). Vazio para as demais ferramentas.
    [[nodiscard]] virtual std::vector<core::Point> previewPath() const;

    /// Verdadeiro se a ferramenta pinta pixels: a pegada é filtrada pelo
    /// tom, pela máscara e pela seleção. As de seleção não pintam.
    [[nodiscard]] virtual bool paints() const;

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
