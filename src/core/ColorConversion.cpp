// =============================================================================
// ColorConversion.cpp
// =============================================================================
#include "ColorConversion.h"

#include "Dithering.h"
#include "PaletteOps.h"

namespace spritenator::core {

RgbaImage toRgba(
    const IndexedImage& image, const Palette& palette, std::optional<ColorIndex> transparentIndex)
{
    // Tabela de conversão índice -> Rgba (256 entradas; fora da paleta = preto).
    std::array<Rgba, 256> table { };
    for (int index = 0; index < 256; ++index) {
        const Rgb color = palette.isValidIndex(index) ? palette.color(index) : Rgb { };
        table.at(static_cast<std::size_t>(index)) = Rgba::opaque(color);
    }
    if (transparentIndex) {
        table.at(*transparentIndex).alpha = 0;
    }

    RgbaImage result(image.size());
    auto output = result.data();
    const auto input = image.data();
    for (std::size_t offset = 0; offset < input.size(); ++offset) {
        output[offset] = table.at(input[offset]);
    }
    return result;
}

IndexedImage toIndexed(
    const RgbaImage& image, const Palette& palette, const IndexingOptions& options)
{
    IndexedImage result(image.size(), options.transparentIndex);
    const ColorSet allowed = allColorsOf(palette);
    const auto visible = [&](int x, int y) -> std::optional<Rgb> {
        const Rgba pixel = image.pixel(x, y);
        if (pixel.alpha < options.alphaThreshold) {
            return std::nullopt; // fica com o índice transparente
        }
        return pixel.rgb();
    };

    if (options.dither) {
        floydSteinberg(image.size(), palette, allowed, visible,
            [&](int x, int y, ColorIndex chosen) { result.setPixel({ x, y }, chosen); });
        return result;
    }
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (const std::optional<Rgb> color = visible(x, y)) {
                result.setPixel({ x, y }, nearestAllowedIndex(palette, *color, allowed));
            }
        }
    }
    return result;
}

} // namespace spritenator::core
