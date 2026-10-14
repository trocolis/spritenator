// =============================================================================
// Brush.cpp
// =============================================================================
#include "Brush.h"

#include <algorithm>

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

} // namespace spritenator::core
