// =============================================================================
// Raster.cpp
// =============================================================================
#include "Raster.h"

#include <cstdlib>

namespace spritenator::core {

void plotLine(Point from, Point to, const Plotter& plot)
{
    // Bresenham generalizado para todos os octantes: o erro acumulado decide
    // quando avançar no eixo secundário. Usa apenas aritmética inteira.
    const int deltaX = std::abs(to.x - from.x);
    const int deltaY = -std::abs(to.y - from.y);
    const int stepX = from.x < to.x ? 1 : -1;
    const int stepY = from.y < to.y ? 1 : -1;
    int error = deltaX + deltaY;

    Point current = from;
    while (true) {
        plot(current);
        if (current == to) {
            break;
        }
        const int doubled = 2 * error;
        if (doubled >= deltaY) {
            error += deltaY;
            current.x += stepX;
        }
        if (doubled <= deltaX) {
            error += deltaX;
            current.y += stepY;
        }
    }
}

Rect lineBounds(Point from, Point to)
{
    return Rect::fromCorners(from, to);
}

} // namespace spritenator::core
