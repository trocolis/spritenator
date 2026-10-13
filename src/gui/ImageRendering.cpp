// =============================================================================
// ImageRendering.cpp
// =============================================================================
#include "ImageRendering.h"

#include <QPainter>

#include <algorithm>
#include <variant>

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

QImage renderImage(const core::AnyImage& image, const core::Palette& palette)
{
    if (const auto* indexed = std::get_if<core::IndexedImage>(&image)) {
        return renderIndexed(*indexed, palette);
    }
    const auto& rgba = std::get<core::RgbaImage>(image);
    if (rgba.isEmpty()) {
        return { };
    }
    QImage result(rgba.width(), rgba.height(), QImage::Format_ARGB32);
    updateRenderedRegion(result, image, rgba.bounds());
    return result;
}

void updateRenderedRegion(QImage& target, const core::AnyImage& image, const core::Rect& area)
{
    if (const auto* indexed = std::get_if<core::IndexedImage>(&image)) {
        updateRenderedRegion(target, *indexed, area);
        return;
    }
    const auto& rgba = std::get<core::RgbaImage>(image);
    const core::Rect clipped = area.intersected(rgba.bounds());
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(target.scanLine(y));
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            line[x] = rgba.pixel(x, y).toArgb32();
        }
    }
}

void paintTransparencyGrid(QPainter& painter, const QRect& area, int cellSize)
{
    // Casas alinhadas à origem do widget (não à área), para que repinturas
    // parciais continuem o mesmo padrão sem "emendas".
    static const QColor kLight(255, 255, 255);
    static const QColor kDark(204, 204, 204);
    painter.fillRect(area, kLight);
    const int firstColumn = area.left() / cellSize;
    const int firstRow = area.top() / cellSize;
    for (int row = firstRow; row * cellSize <= area.bottom(); ++row) {
        for (int column = firstColumn + ((firstColumn + row) % 2 == 0 ? 1 : 0);
            column * cellSize <= area.right(); column += 2) {
            painter.fillRect(
                QRect(column * cellSize, row * cellSize, cellSize, cellSize).intersected(area),
                kDark);
        }
    }
}

} // namespace spritenator::gui
