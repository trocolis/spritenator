// =============================================================================
// Raster.cpp
// =============================================================================
#include "Raster.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <utility>

namespace spritenator::core {

namespace {

/// Recebe, a cada passo do algoritmo da elipse, as colunas esquerda/direita
/// e as linhas superior/inferior de quatro pixels simétricos.
using QuadrantEmitter = std::function<void(int left, int right, int upper, int lower)>;

/// Elipse inscrita no retângulo [x0..x1] x [y0..y1] (inclusivo), pelo
/// algoritmo de Alois Zingl ("A Rasterizing Algorithm for Drawing Curves",
/// 2012). Trabalha só com inteiros e trata corretamente diâmetros pares,
/// ímpares e elipses muito achatadas.
void rasterizeEllipse(int x0, int y0, int x1, int y1, const QuadrantEmitter& emit)
{
    long long a = std::abs(x1 - x0);
    const long long b = std::abs(y1 - y0);
    long long b1 = b & 1; // altura ímpar?
    long long dx = 4 * (1 - a) * b * b; // incrementos de erro
    long long dy = 4 * (b1 + 1) * a * a;
    long long err = dx + dy + b1 * a * a; // erro do primeiro passo

    if (x0 > x1) {
        x0 = x1;
        x1 += static_cast<int>(a);
    }
    if (y0 > y1) {
        y0 = y1;
    }
    y0 += static_cast<int>((b + 1) / 2); // linha inferior inicial (metade de baixo)
    y1 = y0 - static_cast<int>(b1); // linha superior inicial (metade de cima)
    a *= 8 * a;
    b1 = 8 * b * b;

    do {
        emit(x0, x1, y1, y0);
        const long long e2 = 2 * err;
        if (e2 <= dy) { // passo em y
            ++y0;
            --y1;
            dy += a;
            err += dy;
        }
        if (e2 >= dx || 2 * err > dy) { // passo em x
            ++x0;
            --x1;
            dx += b1;
            err += dx;
        }
    } while (x0 <= x1);

    // Elipses achatadas (largura 1-2) terminam cedo demais: completa as pontas.
    while (y0 - y1 < b) {
        emit(x0 - 1, x1 + 1, y1, y0);
        ++y0;
        --y1;
    }
}

/// Plota cada ponto e, se "filled", preenche cada linha entre o menor e o
/// maior x encontrados nela (válido para figuras convexas).
void plotConvex(const std::vector<Point>& outline, bool filled, const Plotter& plot)
{
    if (!filled) {
        for (const Point point : outline) {
            plot(point);
        }
        return;
    }
    std::map<int, std::pair<int, int>> spans; // linha -> (menor x, maior x)
    for (const Point point : outline) {
        auto [it, inserted] = spans.try_emplace(point.y, point.x, point.x);
        if (!inserted) {
            it->second.first = std::min(it->second.first, point.x);
            it->second.second = std::max(it->second.second, point.x);
        }
    }
    for (const auto& [y, span] : spans) {
        for (int x = span.first; x <= span.second; ++x) {
            plot({ x, y });
        }
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Reta
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Retângulo
// -----------------------------------------------------------------------------
void plotRectangle(const Rect& bounds, bool filled, const Plotter& plot)
{
    if (bounds.isEmpty()) {
        return;
    }
    const int lastX = bounds.right() - 1;
    const int lastY = bounds.bottom() - 1;
    for (int y = bounds.top(); y <= lastY; ++y) {
        const bool edgeRow = y == bounds.top() || y == lastY;
        if (filled || edgeRow) {
            for (int x = bounds.left(); x <= lastX; ++x) {
                plot({ x, y });
            }
        } else {
            plot({ bounds.left(), y });
            plot({ lastX, y });
        }
    }
}

// -----------------------------------------------------------------------------
// Elipse
// -----------------------------------------------------------------------------
void plotEllipse(const Rect& bounds, bool filled, const Plotter& plot)
{
    if (bounds.isEmpty()) {
        return;
    }
    // Com 1-2 pixels em alguma dimensão a "elipse" é um retângulo cheio.
    if (bounds.width <= 2 || bounds.height <= 2) {
        plotRectangle(bounds, true, plot);
        return;
    }
    std::vector<Point> outline;
    rasterizeEllipse(bounds.left(), bounds.top(), bounds.right() - 1, bounds.bottom() - 1,
        [&](int left, int right, int upper, int lower) {
            outline.push_back({ right, lower });
            outline.push_back({ left, lower });
            outline.push_back({ left, upper });
            outline.push_back({ right, upper });
        });
    plotConvex(outline, filled, plot);
}

// -----------------------------------------------------------------------------
// Retângulo arredondado
// -----------------------------------------------------------------------------
int defaultCornerSize(const Rect& bounds)
{
    // Dois terços do lado menor, limitados a 16 px: em pixel art cantos
    // menores mal aparecem, e em figuras grandes o resultado continua
    // parecendo um retângulo, e não uma elipse.
    constexpr int kMinCorner = 3;
    constexpr int kMaxCorner = 16;
    return std::clamp(std::min(bounds.width, bounds.height) * 2 / 3, kMinCorner, kMaxCorner);
}

void plotRoundRectangle(const Rect& bounds, int cornerSize, bool filled, const Plotter& plot)
{
    if (bounds.isEmpty()) {
        return;
    }
    const int corner = std::min({ cornerSize, bounds.width, bounds.height });
    if (corner <= 2 || bounds.width <= 2 || bounds.height <= 2) {
        plotRectangle(bounds, filled, plot);
        return;
    }

    // Os quatro cantos são os quadrantes de uma elipse "corner x corner"
    // desenhada no canto superior esquerdo; os quadrantes da direita/de baixo
    // são deslocados até os cantos correspondentes.
    const int shiftX = bounds.width - corner;
    const int shiftY = bounds.height - corner;
    std::vector<Point> outline;
    rasterizeEllipse(bounds.left(), bounds.top(), bounds.left() + corner - 1,
        bounds.top() + corner - 1, [&](int left, int right, int upper, int lower) {
            outline.push_back({ left, upper });
            outline.push_back({ right + shiftX, upper });
            outline.push_back({ left, lower + shiftY });
            outline.push_back({ right + shiftX, lower + shiftY });
        });

    // Lados retos: completam a primeira/última linha e coluna.
    const int lastX = bounds.right() - 1;
    const int lastY = bounds.bottom() - 1;
    const int sideStartX = bounds.left() + corner / 2;
    const int sideEndX = lastX - corner / 2;
    const int sideStartY = bounds.top() + corner / 2;
    const int sideEndY = lastY - corner / 2;
    for (int x = sideStartX; x <= sideEndX; ++x) {
        outline.push_back({ x, bounds.top() });
        outline.push_back({ x, lastY });
    }
    for (int y = sideStartY; y <= sideEndY; ++y) {
        outline.push_back({ bounds.left(), y });
        outline.push_back({ lastX, y });
    }
    plotConvex(outline, filled, plot);
}

} // namespace spritenator::core
