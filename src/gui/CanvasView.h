// =============================================================================
// CanvasView.h
//
// Widget que desenha o canvas de um documento em escala inteira (1x ou 2x).
// Equivale ao PaintBox da "TChildForm" do original.
//
// Também exibe o "cursor de célula": um retângulo do tamanho do personagem
// que acompanha o mouse, indicando qual quadro será trazido para a área de
// desenho (botão direito) ou gravado a partir dela (botão esquerdo).
// =============================================================================
#pragma once

#include "core/CharacterLayout.h"
#include "core/Geometry.h"

#include <QWidget>

#include <optional>

namespace spritenator::gui {

class Document;

/// Exibe um documento e reporta cliques em células.
class CanvasView final : public QWidget {
    Q_OBJECT

public:
    explicit CanvasView(Document* document, QWidget* parent = nullptr);

    [[nodiscard]] Document* document() const { return m_document; }

    /// Fator de ampliação da exibição (1 ou 2).
    [[nodiscard]] int scale() const { return m_scale; }
    void setScale(int scale);

    /// Configuração da grade de personagens (tamanho do cursor de célula).
    void setCharacterLayout(const core::CharacterLayout& layout);

    /// Célula atualmente sob o cursor (se o mouse estiver sobre o canvas).
    [[nodiscard]] std::optional<core::Rect> hoveredCell() const { return m_hoveredCell; }

    /// Define a célula destacada (usado também pela navegação por teclado).
    void setHoveredCell(const std::optional<core::Rect>& cell);

    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// Botão esquerdo numa célula: gravar a área de desenho nela.
    void putRequested(const core::Rect& cell);
    /// Botão direito numa célula: trazer a célula para a área de desenho.
    void getRequested(const core::Rect& cell);
    /// O mouse passou sobre um pixel do canvas (para a barra de status).
    void pixelHovered(const core::Point& pixel);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    /// Converte uma posição do widget em pixel do canvas.
    [[nodiscard]] core::Point toCanvas(const QPointF& position) const;

    /// Converte um retângulo do canvas em retângulo do widget.
    [[nodiscard]] QRect toWidget(const core::Rect& area) const;

    /// Recalcula o tamanho fixo do widget (canvas x escala).
    void updateGeometryFromDocument();

    Document* m_document = nullptr;
    int m_scale = 1;
    core::CharacterLayout m_layout;
    std::optional<core::Rect> m_hoveredCell;
};

} // namespace spritenator::gui
