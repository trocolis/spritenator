// =============================================================================
// Brush.h
//
// Ponta do lápis: formato (quadrada ou redonda) e tamanho em pixels. A ponta
// é descrita pelos deslocamentos dos pixels pintados em relação ao pixel sob
// o mouse.
//
// Tamanhos pares não têm pixel central: o mouse fica no pixel de cima e da
// esquerda do centro (como no Aseprite). A ponta redonda é um círculo em
// pixels: no tamanho 3 vira um "+", sem os cantos.
// =============================================================================
#pragma once

#include "Geometry.h"

#include <vector>

namespace spritenator::core {

/// Formato da ponta do lápis.
enum class BrushShape { Square, Round };

/// Tamanho mínimo e máximo da ponta (pixels).
inline constexpr int kMinBrushSize = 1;
inline constexpr int kMaxBrushSize = 64;

/// Deslocamentos dos pixels de uma ponta (tamanho limitado a 1..64).
[[nodiscard]] std::vector<Point> brushOffsets(BrushShape shape, int size);

/// Retângulo ocupado pela ponta em torno do pixel (0, 0).
[[nodiscard]] Rect brushExtent(int size);

} // namespace spritenator::core
