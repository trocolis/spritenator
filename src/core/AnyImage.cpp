// =============================================================================
// AnyImage.cpp
// =============================================================================
#include "AnyImage.h"

#include "ColorConversion.h"

#include <stdexcept>

namespace spritenator::core {

void pasteImage(AnyImage& target, const AnyImage& source, Point destination)
{
    if (target.index() != source.index()) {
        throw std::invalid_argument("pasteImage: imagens de modos de cor diferentes");
    }
    std::visit(
        [&](auto& concrete) {
            using Concrete = std::decay_t<decltype(concrete)>;
            concrete.paste(std::get<Concrete>(source), destination);
        },
        target);
}

AnyImage convertImage(const AnyImage& image, ColorMode mode, const Palette& palette)
{
    if (modeOf(image) == mode) {
        return image;
    }
    if (mode == ColorMode::Rgba) {
        // Indexada -> RGBA: exata; o índice 0 (fundo/transparente do RPG
        // Maker) continua opaco — a conversão não inventa transparência.
        return toRgba(std::get<IndexedImage>(image), palette);
    }
    // RGBA -> indexada: pixels quase transparentes viram o índice 0.
    return toIndexed(std::get<RgbaImage>(image), palette);
}

} // namespace spritenator::core
