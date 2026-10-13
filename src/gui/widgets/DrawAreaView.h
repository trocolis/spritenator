// =============================================================================
// DrawAreaView.h
//
// A área de desenho ampliada (TDrawArea no original). Exibe o DrawBuffer no
// zoom atual, com grade de pixels (grade principal), sub-grade e contorno de
// células, a seleção e os elementos temporários da ferramenta atual.
//
// Os eventos de mouse são convertidos em coordenadas do buffer e repassados
// à ferramenta ativa da EditorSession (padrão Strategy).
// =============================================================================
#pragma once

#include "core/Geometry.h"

#include <QWidget>

namespace spritenator::gui {

class EditorSession;

/// Área de desenho ampliada.
class DrawAreaView final : public QWidget {
    Q_OBJECT

public:
    explicit DrawAreaView(EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// Pixel do buffer sob o mouse (para a barra de status).
    void pixelHovered(const core::Point& pixel);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// Converte uma posição do widget em pixel do buffer.
    [[nodiscard]] core::Point toBuffer(const QPointF& position) const;

    /// Ajusta o tamanho do widget ao buffer x zoom.
    void updateSize();

    /// Desenha as linhas de grade visíveis dentro de "dirty".
    void paintGrids(QPainter& painter, const QRect& dirty) const;

    EditorSession* m_session;
    Qt::MouseButton m_activeButton = Qt::NoButton; ///< botão do traço em andamento
};

} // namespace spritenator::gui
