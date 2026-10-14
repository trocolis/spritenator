// =============================================================================
// Quantize.h
//
// Redução de cores por "corte mediano" (median cut): divide repetidamente o
// conjunto de cores ao meio, pelo canal de maior variação, até chegar ao
// número desejado de grupos; cada grupo vira a média das suas cores.
// Usada ao gravar GIF a partir de imagens RGBA (o GIF tem no máximo 256 cores).
// =============================================================================
#pragma once

#include "Color.h"

#include <vector>

namespace spritenator::core {

/// Paleta de no máximo "maxColors" cores que representa "colors" (pixels, com
/// repetições: cores frequentes pesam mais). Se já houver poucas cores
/// distintas, elas voltam exatamente.
[[nodiscard]] std::vector<Rgb> medianCut(const std::vector<Rgb>& colors, int maxColors);

/// Índice da cor mais próxima (distância perceptual de Color.h).
[[nodiscard]] int nearestColor(const std::vector<Rgb>& palette, const Rgb& color);

} // namespace spritenator::core
