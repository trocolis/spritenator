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
//
// Sobre a imagem são desenhados: fundo xadrez (RGBA), grade principal (some
// quando ficaria densa demais), sub-grade, seleção e o DESTAQUE dos pixels
// que a ferramenta atual afetaria sob o mouse.
//
// É o widget de cada aba do QMdiArea e dono do seu Document.
// =============================================================================
#pragma once

#include "core/FreeTransform.h"
#include "core/Geometry.h"

#include <QAbstractScrollArea>
#include <QImage>
#include <QLine>
#include <QList>
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

    // --- Alças de transformação da seleção (expostas para testes) ---
    /// O que um clique do botão esquerdo em "position" faria com a seleção.
    enum class TransformHit { None, Move, Scale, Rotate };
    [[nodiscard]] TransformHit transformHitAt(const QPointF& position) const;
    /// Cantos da caixa de transformação na tela (superior esquerdo, superior
    /// direito, inferior direito, inferior esquerdo); vazio se não há caixa.
    [[nodiscard]] QList<QPointF> transformBox() const;
    /// Pontos das 8 alças na tela: os 4 cantos e os meios dos 4 lados.
    [[nodiscard]] QList<QPointF> transformHandles() const;

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
    /// Papel de seda (se ligado): cenas vizinhas translúcidas.
    void paintOnionSkin(QPainter& painter, const QRectF& target, const QRect& source);
    void paintSelection(QPainter& painter) const;
    /// Contorno em construção da ferramenta (polígono, laço).
    void paintToolPath(QPainter& painter) const;
    /// Recalcula as bordas da seleção (cache usado na pintura).
    void updateSelectionEdges();
    /// Redesenha se o contorno em construção da ferramenta mudou.
    void refreshToolPath();

    // --- Alças de transformação ---
    /// Coordenadas da imagem (com frações) de um ponto do viewport.
    [[nodiscard]] QPointF imageCoordinatesAt(const QPointF& viewportPosition) const;
    /// Verdadeiro se a caixa de transformação deve aparecer.
    [[nodiscard]] bool showsTransformBox() const;
    /// Índice da alça sob o ponto (0..7) ou -1.
    [[nodiscard]] int handleAt(const QPointF& position) const;
    void paintTransformBox(QPainter& painter) const;
    /// Começa a arrastar (mover, escalar ou girar); false se não há o que fazer.
    bool beginTransformDrag(const QPointF& position, Qt::KeyboardModifiers modifiers);
    void continueTransformDrag(const QPointF& position, Qt::KeyboardModifiers modifiers);
    /// Cursor conforme o que está sob o mouse (alças, rotação, mover).
    void updateTransformCursor(const QPointF& position);

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
    QList<QLine> m_selectionEdges; ///< bordas da seleção (coordenadas da imagem)
    int m_onionFrame = -1; ///< cena para a qual o papel de seda foi composto
    QImage m_onionPrevious; ///< cena anterior (papel de seda)
    QImage m_onionNext; ///< próxima cena (papel de seda)
    bool m_hadToolPath = false;
    std::optional<core::Point> m_hoverPixel;

    // Arraste das alças de transformação.
    TransformHit m_drag = TransformHit::None;
    int m_dragHandle = -1; ///< alça arrastada (escala)
    int m_dragDirX = 0; ///< lado da alça no eixo local x (-1, 0 ou 1)
    int m_dragDirY = 0; ///< lado da alça no eixo local y (-1, 0 ou 1)
    QPointF m_dragStart; ///< ponto inicial, em coordenadas da imagem
    core::TransformParams m_dragParams; ///< parâmetros no início do arraste
    std::vector<core::Point> m_highlight;
    QImage m_highlightImage; ///< sobreposição do destaque (só a área envolvente)
    core::Rect m_highlightBounds;
};

} // namespace spritenator::gui
