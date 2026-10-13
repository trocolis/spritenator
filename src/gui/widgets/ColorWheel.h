// =============================================================================
// ColorWheel.h
//
// Seletor de cor "anel + triângulo" (modelo HSV), como nos editores GTK/GIMP:
//
//   - o ANEL externo escolhe o matiz (hue), girando pelo espectro de cores;
//   - o TRIÂNGULO interno escolhe saturação e valor: seus vértices são a cor
//     pura do matiz (S=1, V=1), o branco (S=0, V=1) e o preto (V=0). O
//     triângulo gira com o matiz, de modo que o vértice da cor pura aponta
//     para o matiz escolhido no anel.
//
// O botão esquerdo emite a cor para a primária; o direito, para a
// secundária (ver colorPicked). O widget trabalha só com RGB/HSV: o alfa é
// controlado à parte (barra A do ColorEditor).
// =============================================================================
#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QWidget>

namespace spritenator::gui {

/// Geometria do triângulo HSV (funções puras, expostas para testes).
namespace wheel {

/// Ponto do triângulo em coordenadas baricêntricas: pesos dos vértices cor
/// pura (hueWeight), branco (whiteWeight) e preto (blackWeight); soma = 1.
struct Barycentric {
    double hueWeight = 0;
    double whiteWeight = 0;
    double blackWeight = 0;
};

/// Saturação e valor (0..1) correspondentes a um ponto do triângulo.
struct SaturationValue {
    double saturation = 0;
    double value = 0;
};

/// Ponto do triângulo -> saturação/valor. Pontos fora do triângulo (pesos
/// negativos) são projetados para dentro.
[[nodiscard]] SaturationValue toSaturationValue(Barycentric point);

/// Saturação/valor -> ponto do triângulo (inverso de toSaturationValue).
[[nodiscard]] Barycentric toBarycentric(SaturationValue color);

/// Coordenadas baricêntricas de "point" no triângulo (a, b, c) =
/// (cor pura, branco, preto).
[[nodiscard]] Barycentric barycentricOf(QPointF point, QPointF a, QPointF b, QPointF c);

} // namespace wheel

/// Seletor de cor anel + triângulo.
class ColorWheel final : public QWidget {
    Q_OBJECT

public:
    explicit ColorWheel(QWidget* parent = nullptr);

    /// Cor exibida (o alfa é ignorado). Não emite colorPicked.
    void setColor(const QColor& color);
    [[nodiscard]] QColor color() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override { return width; }

    /// Vértices atuais do triângulo (cor pura, branco, preto), em pixels.
    /// Expostos para testes e para cálculos de interação.
    void triangleVertices(QPointF& hueVertex, QPointF& whiteVertex, QPointF& blackVertex) const;

    /// Ponto do anel (meio da espessura) correspondente a um matiz.
    [[nodiscard]] QPointF ringPointForHue(int hue) const;

signals:
    /// O usuário escolheu uma cor com o mouse ("button" = esquerdo/direito).
    void colorPicked(const QColor& color, Qt::MouseButton button);

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /// Parte do widget sendo arrastada.
    enum class DragTarget { None, Ring, Triangle };

    /// Reaplica a dica no idioma atual.
    void retranslate();

    [[nodiscard]] QPointF center() const;
    [[nodiscard]] double outerRadius() const;
    [[nodiscard]] double innerRadius() const;

    /// Atualiza matiz ou saturação/valor a partir da posição do mouse.
    void pickAt(const QPointF& position);
    /// Gera a imagem do triângulo (refeita quando o matiz ou o tamanho muda).
    void rebuildTriangleImage();

    int m_hue = 0; ///< 0..359 (mantido mesmo para cinzas)
    double m_saturation = 0; ///< 0..1
    double m_value = 0; ///< 0..1
    DragTarget m_drag = DragTarget::None;
    Qt::MouseButton m_button = Qt::NoButton;
    QImage m_triangleImage; ///< cache do triângulo colorido
};

} // namespace spritenator::gui
