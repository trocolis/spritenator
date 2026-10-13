// =============================================================================
// ColorConversion.h
//
// Conversões entre imagens indexadas e RGBA. São usadas quando conteúdo
// passa entre documentos de modos diferentes (a área de desenho é
// compartilhada) e ao colar imagens externas.
//
//   indexada -> RGBA: exata (cada índice vira a cor da paleta, opaca);
//   RGBA -> indexada: com perdas — pixels quase transparentes viram o índice
//                     transparente (0) e os demais, a cor mais próxima da
//                     paleta (opcionalmente com pontilhamento).
// =============================================================================
#pragma once

#include "Image.h"
#include "Palette.h"

#include <cstdint>
#include <optional>

namespace spritenator::core {

/// Converte uma imagem indexada em RGBA usando a paleta. Se
/// "transparentIndex" for informado, os pixels com esse índice ficam
/// totalmente transparentes (alfa 0); os demais, opacos.
[[nodiscard]] RgbaImage toRgba(const IndexedImage& image, const Palette& palette,
    std::optional<ColorIndex> transparentIndex = std::nullopt);

/// Opções da conversão RGBA -> indexada.
struct IndexingOptions {
    /// Alfa mínimo para um pixel ser considerado visível.
    std::uint8_t alphaThreshold = 128;
    /// Índice que recebe os pixels abaixo do limite de alfa.
    ColorIndex transparentIndex = 0;
    /// Usar pontilhamento (Floyd–Steinberg) em vez da cor mais próxima.
    bool dither = false;
};

/// Converte uma imagem RGBA em indexada com a paleta informada.
[[nodiscard]] IndexedImage toIndexed(
    const RgbaImage& image, const Palette& palette, const IndexingOptions& options = { });

} // namespace spritenator::core
