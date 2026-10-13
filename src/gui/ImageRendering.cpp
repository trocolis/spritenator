// =============================================================================
// ImageRendering.cpp
// =============================================================================
#include "ImageRendering.h"

#include <algorithm>

namespace spritenator::gui {

QList<QRgb> colorTableOf(const core::Palette& palette)
{
    QList<QRgb> table;
    table.reserve(palette.size());
    for (int index = 0; index < palette.size(); ++index) {
        const core::Rgb& color = palette.color(index);
        table.append(qRgb(color.red, color.green, color.blue));
    }
    // Tabela sempre com 256 entradas: assim índices fora de uma paleta de 16
    // cores (que não deveriam existir) aparecem pretos em vez de falhar.
    while (table.size() < core::Palette::kMaxColors) {
        table.append(qRgb(0, 0, 0));
    }
    return table;
}

QImage renderIndexed(const core::IndexedImage& image, const core::Palette& palette)
{
    if (image.isEmpty()) {
        return { };
    }
    QImage result(image.width(), image.height(), QImage::Format_Indexed8);
    result.setColorTable(colorTableOf(palette));
    updateRenderedRegion(result, image, image.bounds());
    return result;
}

void updateRenderedRegion(QImage& target, const core::IndexedImage& image, const core::Rect& area)
{
    const core::Rect clipped = area.intersected(image.bounds());
    const auto pixels = image.data();
    const auto width = static_cast<std::size_t>(image.width());
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        const auto rowStart
            = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(clipped.x);
        std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(rowStart), clipped.width,
            target.scanLine(y) + clipped.x);
    }
}

} // namespace spritenator::gui
