// =============================================================================
// CanvasEditor.h
//
// Editor de uma imagem: exibe o documento INTEIRO com zoom livre e repassa os
// eventos do mouse à ferramenta atual — a edição acontece diretamente sobre
// a imagem (não existe mais a área de desenho separada do CM1999).
//
// Navegação:
//   - roda do mouse: zoom (ancorado no ponteiro);
//   - Ctrl + roda: rola a imagem (com Shift, na horizontal);
//   - botão do meio arrastado: move a vista;
//   - lupa: esquerdo amplia, direito reduz (no ponto clicado).
//
// Sobre a imagem são desenhados: fundo xadrez (RGBA), grade principal (some
// quando ficaria densa demais), sub-grade, seleção e o DESTAQUE dos pixels
// que a ferramenta atual afetaria sob o mouse.
//
// É o widget de cada aba do QMdiArea e dono do seu Document.
// =============================================================================
#pragma once

#include "core/Geometry.h"

#include <QAbstractScrollArea>
#include <QImage>
#include <QPointF>

#include <optional>
#include <vector>

namespace spritenator::gui {

class Document;
class EditorSession;

/// Editor de imagem com zoom.
class CanvasEditor final : public QAbstractScrollArea {
    Q_OBJECT

public:
    /// Níveis usados por "aumentar/diminuir zoom" e pela roda (fatores).
    static constexpr double kZoomLevels[] = { 0.125, 0.25, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
    static constexpr double kMinZoom = 0.125;
    static constexpr double kMaxZoom = 32;

    /// Assume a posse do documento (passa a ser filho deste editor).
    CanvasEditor(Document* document, EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] Document* document() const { return m_document; }

    // --- Zoom ---
    /// Fator de zoom atual (1 = 100%).
    [[nodiscard]] double zoom() const { return m_zoom; }
    /// Define o zoom (limitado a [kMinZoom, kMaxZoom]). "anchor" é um ponto
    /// do viewport que deve continuar sobre o mesmo pixel; padrão: o centro.
    void setZoom(double zoom, std::optional<QPointF> anchor = std::nullopt);
    void zoomIn(std::optional<QPointF> anchor = std::nullopt);
    void zoomOut(std::optional<QPointF> anchor = std::nullopt);
    /// Maior zoom em que a imagem inteira cabe na área visível.
    void zoomToFit();

    // --- Coordenadas ---
    /// Pixel da imagem sob um ponto do viewport (pode estar fora da imagem).
    [[nodiscard]] core::Point imagePointAt(const QPointF& viewportPosition) const;
    /// Ponto do viewport correspondente a uma coordenada da imagem.
    [[nodiscard]] QPointF viewportPointOf(const QPointF& imagePosition) const;

    /// Pixels destacados sob o mouse (expostos para testes).
    [[nodiscard]] const std::vector<core::Point>& highlightedPixels() const { return m_highlight; }

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void zoomChanged(double zoom);
    /// O mouse passou sobre um pixel (para a barra de status).
    void pixelHovered(const core::Point& pixel);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// Canto superior esquerdo da imagem no viewport (centralizada se couber).
    [[nodiscard]] QPointF imageOrigin() const;
    /// Tamanho da imagem na tela, no zoom atual.
    [[nodiscard]] QSizeF scaledSize() const;
    /// Retângulo do viewport ocupado por uma região da imagem.
    [[nodiscard]] QRect viewportRectOf(const core::Rect& area) const;

    /// Aplica o zoom (sem mexer no ajuste automático).
    void applyZoom(double zoom, std::optional<QPointF> anchor);
    /// Zoom em que a imagem inteira cabe na área visível.
    [[nodiscard]] double fitZoom() const;

    /// Recalcula faixas e passos das barras de rolagem.
    void updateScrollBars();
    /// Recalcula o destaque para o pixel sob o mouse (ou o apaga). Sem
    /// "force", nada é feito se o pixel não mudou.
    void updateHighlight(std::optional<core::Point> pixel, bool force = false);
    /// Recalcula o destaque no pixel atual (algo além do mouse mudou).
    void refreshHighlight();
    /// Título da aba: "nome[*] (LxA[, RGBA])".
    void updateTitle();

    void paintGrids(QPainter& painter, const QRect& visible) const;
    void paintHighlight(QPainter& painter) const;

    Document* m_document;
    EditorSession* m_session;
    double m_zoom = 1;
    /// Enquanto o usuário não mexer no zoom, a imagem acompanha o tamanho
    /// da janela ("ajustar à janela" automático).
    bool m_autoFit = true;

    Qt::MouseButton m_activeButton = Qt::NoButton; ///< botão da ferramenta em uso
    bool m_panning = false; ///< arrastando com o botão do meio
    QPointF m_panStart; ///< posição do mouse no início do arraste
    QPoint m_panScrollStart; ///< rolagem no início do arraste
    double m_wheelRemainder = 0; ///< frações de "clique" da roda (touchpads)

    std::optional<QPointF> m_mousePosition; ///< último ponto do mouse no viewport
    std::optional<core::Point> m_hoverPixel;
    std::vector<core::Point> m_highlight;
    QImage m_highlightImage; ///< sobreposição do destaque (só a área envolvente)
    core::Rect m_highlightBounds;
};

} // namespace spritenator::gui
