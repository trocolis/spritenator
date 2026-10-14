// =============================================================================
// SelectionTools.h
//
// Ferramentas que criam seleções (nenhuma pinta pixels):
//   - RectSelectTool:    retângulo arrastando (Shift: quadrado);
//   - EllipseSelectTool: oval arrastando (Shift: círculo);
//   - PolygonSelectTool: ponto a ponto — cada clique é um vértice; fecha com
//                        clique duplo, clique no primeiro ponto, botão direito
//                        ou Enter; Backspace remove o último ponto;
//   - LassoSelectTool:   desenho livre do contorno (fecha ao soltar);
//   - MagicWandTool:     região de cor parecida (tolerância na sessão).
//
// Combinação com a seleção existente, decidida ao pressionar o botão:
// Shift soma, Ctrl subtrai; sem modificador, substitui.
// =============================================================================
#pragma once

#include "Tool.h"

#include "core/Selection.h"

namespace spritenator::gui {

class Document;

/// Base comum: modo de combinação e aplicação da forma ao documento.
class SelectionTool : public Tool {
public:
    [[nodiscard]] bool paints() const override { return false; }

protected:
    enum class Combine { Replace, Add, Subtract };

    /// Guarda o modo (pelos modificadores) e a seleção de partida.
    void beginShape(EditorSession& session);
    /// Aplica a forma ao documento ativo, combinada à seleção de partida.
    void applyShape(EditorSession& session, const core::Selection& shape) const;
    /// Restaura a seleção de partida (operação cancelada).
    void restoreBase(EditorSession& session) const;

    Combine m_combine = Combine::Replace;
    core::Selection m_base; ///< seleção antes da operação atual
};

/// Base das seleções arrastadas (retângulo e oval).
class DragSelectTool : public SelectionTool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void cancel(EditorSession& session) override;
    [[nodiscard]] bool isBusy() const override { return m_active; }

protected:
    /// A forma para o retângulo arrastado.
    [[nodiscard]] virtual core::Selection shapeFor(const core::Rect& area) const = 0;

private:
    /// Retângulo entre a âncora e o ponto (quadrado com Shift).
    [[nodiscard]] core::Rect areaTo(const EditorSession& session, core::Point point) const;

    bool m_active = false;
    bool m_dragged = false;
    core::Point m_anchor;
};

/// Seleção retangular.
class RectSelectTool final : public DragSelectTool {
protected:
    [[nodiscard]] core::Selection shapeFor(const core::Rect& area) const override;
};

/// Seleção oval.
class EllipseSelectTool final : public DragSelectTool {
protected:
    [[nodiscard]] core::Selection shapeFor(const core::Rect& area) const override;
};

/// Seleção ponto a ponto.
class PolygonSelectTool final : public SelectionTool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void hover(EditorSession& session, core::Point point) override;
    bool keyPress(EditorSession& session, int key) override;
    void cancel(EditorSession& session) override;
    [[nodiscard]] bool isBusy() const override { return !m_vertices.empty(); }
    [[nodiscard]] std::vector<core::Point> previewPath() const override;

private:
    /// Fecha o polígono e aplica a seleção.
    void finish(EditorSession& session);

    std::vector<core::Point> m_vertices;
    core::Point m_cursor; ///< ponto sob o mouse (elástico até ele)
};

/// Seleção livre (laço).
class LassoSelectTool final : public SelectionTool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    void cancel(EditorSession& session) override;
    [[nodiscard]] bool isBusy() const override { return !m_points.empty(); }
    [[nodiscard]] std::vector<core::Point> previewPath() const override { return m_points; }

private:
    std::vector<core::Point> m_points;
};

/// Varinha mágica.
class MagicWandTool final : public SelectionTool {
public:
    void press(EditorSession& session, core::Point point, Qt::MouseButton button) override;
    void move(EditorSession& session, core::Point point) override;
    void release(EditorSession& session, core::Point point) override;
    /// A região que seria selecionada (como o balde de tinta).
    [[nodiscard]] std::vector<core::Point> footprint(
        const EditorSession& session, core::Point point) const override;
};

/// A seleção que a varinha faria no documento ativo, num ponto.
[[nodiscard]] core::Selection magicWandSelection(const EditorSession& session, core::Point point);

} // namespace spritenator::gui
