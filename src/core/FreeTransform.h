// =============================================================================
// FreeTransform.h
//
// Transformação livre de uma seleção: os pixels selecionados (e a própria
// máscara) passam por uma transformação afim — mover, escalar, girar,
// espelhar — como na "transformação livre" dos editores modernos.
//
// A amostragem é por vizinho mais próximo (sem suavização, como a pixel art
// pede): para cada pixel do destino, o centro dele é levado de volta pela
// transformação inversa até a origem. Quando o ângulo é múltiplo de 90° e a
// escala é inteira, o resultado é exato (espelhar e girar não perdem pixels).
//
// Coordenadas: as do canvas, com (x, y) no CANTO superior esquerdo do pixel;
// o centro do pixel (x, y) é (x + 0.5, y + 0.5).
// =============================================================================
#pragma once

#include "Image.h"
#include "Selection.h"

#include <array>
#include <optional>
#include <utility>

namespace spritenator::core {

/// Transformação afim 2D: x' = m11*x + m12*y + dx ; y' = m21*x + m22*y + dy.
struct Affine {
    double m11 = 1;
    double m12 = 0;
    double m21 = 0;
    double m22 = 1;
    double dx = 0;
    double dy = 0;

    [[nodiscard]] static Affine translation(double x, double y);
    [[nodiscard]] static Affine scaling(double sx, double sy);
    /// Rotação em graus, no sentido HORÁRIO da tela (y para baixo).
    [[nodiscard]] static Affine rotation(double degrees);

    /// Esta transformação seguida de "next".
    [[nodiscard]] Affine then(const Affine& next) const;
    [[nodiscard]] std::optional<Affine> inverted() const;
    [[nodiscard]] std::pair<double, double> map(double x, double y) const;

    friend bool operator==(const Affine&, const Affine&) = default;
};

/// Parâmetros de uma transformação livre, relativos à seleção ORIGINAL:
/// escala (negativa = espelhada) e rotação em torno do centro dela, e depois
/// um deslocamento. É a forma usada pela interface (alças, teclas).
struct TransformParams {
    double dx = 0; ///< deslocamento do centro, em pixels
    double dy = 0;
    double angle = 0; ///< graus, sentido horário
    double sx = 1; ///< escala horizontal (negativa: espelhada)
    double sy = 1; ///< escala vertical

    [[nodiscard]] bool isIdentity() const;
    friend bool operator==(const TransformParams&, const TransformParams&) = default;
};

/// Transformação afim correspondente aos parâmetros, para uma seleção com o
/// retângulo envolvente "bounds". Com ângulo múltiplo de 90°, o deslocamento
/// é ajustado para o resultado cair na grade de pixels.
[[nodiscard]] Affine affineFor(const TransformParams& params, const Rect& bounds);

/// Cantos (superior esquerdo, superior direito, inferior direito, inferior
/// esquerdo) do retângulo "bounds" transformado.
[[nodiscard]] std::array<std::pair<double, double>, 4> transformedCorners(
    const Affine& transform, const Rect& bounds);

/// Resultado de transformar pixels + máscara.
template <typename Pixel> struct Floating {
    Image<Pixel> pixels; ///< cobre exatamente mask.bounds()
    Selection mask;
};

/// Transforma os pixels selecionados. "content" tem o tamanho de
/// mask.bounds() (a cópia daquela região do canvas). Só são calculados os
/// pixels de destino dentro de "clip" (normalmente o canvas).
template <typename Pixel>
[[nodiscard]] Floating<Pixel> transformFloating(
    const Image<Pixel>& content, const Selection& mask, const Affine& transform, const Rect& clip);

} // namespace spritenator::core
