// =============================================================================
// Brush.cpp
// =============================================================================
#include "Brush.h"

#include <algorithm>
#include <cstdint>

namespace spritenator::core {

Rect brushExtent(int size)
{
    size = std::clamp(size, kMinBrushSize, kMaxBrushSize);
    // De -(size-1)/2 até size/2: em tamanhos pares, um pixel a mais para a
    // direita e para baixo.
    const int low = -((size - 1) / 2);
    return { low, low, size, size };
}

std::vector<Point> brushOffsets(BrushShape shape, int size)
{
    const Rect extent = brushExtent(size);
    size = extent.width;
    // Centro geométrico da ponta e raio "pixel art": um quarto de pixel a
    // menos que a metade, para os cantos não ficarem quadrados demais.
    const double center = extent.x + (size - 1) / 2.0;
    const double radius = size / 2.0 - 0.25;
    std::vector<Point> offsets;
    for (int dy = extent.top(); dy < extent.bottom(); ++dy) {
        for (int dx = extent.left(); dx < extent.right(); ++dx) {
            const double ox = dx - center;
            const double oy = dy - center;
            if (shape == BrushShape::Square || size <= 2 || ox * ox + oy * oy <= radius * radius) {
                offsets.push_back({ dx, dy });
            }
        }
    }
    return offsets;
}

std::vector<Point> strokePixels(const std::vector<Point>& path, BrushShape shape, int size)
{
    if (path.empty()) {
        return { };
    }
    const std::vector<Point> brush = brushOffsets(shape, size);
    const Rect extent = brushExtent(size);
    // Área coberta: o envolvente do caminho, aumentado pela ponta.
    Rect area;
    for (const Point point : path) {
        area = area.united(extent.translated(point));
    }
    // Marca cada pixel uma vez (os carimbos se sobrepõem muito).
    std::vector<std::uint8_t> covered(static_cast<std::size_t>(area.size().area()), 0);
    std::vector<Point> pixels;
    for (const Point point : path) {
        for (const Point offset : brush) {
            const Point pixel = point + offset;
            const auto index
                = static_cast<std::size_t>(pixel.y - area.y) * static_cast<std::size_t>(area.width)
                + static_cast<std::size_t>(pixel.x - area.x);
            if (covered[index] == 0) {
                covered[index] = 1;
                pixels.push_back(pixel);
            }
        }
    }
    return pixels;
}

} // namespace spritenator::core
