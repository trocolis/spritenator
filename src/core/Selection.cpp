// =============================================================================
// Selection.cpp
// =============================================================================
#include "Selection.h"

#include "Raster.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <variant>

namespace spritenator::core {

namespace {

/// Índice do byte de (x, y) num retângulo (coordenadas absolutas).
std::size_t offsetIn(const Rect& area, int x, int y)
{
    return static_cast<std::size_t>(y - area.y) * static_cast<std::size_t>(area.width)
        + static_cast<std::size_t>(x - area.x);
}

/// Retângulo envolvente de uma lista de pontos (vazio se a lista for vazia).
Rect boundsOfPoints(const std::vector<Point>& points)
{
    Rect bounds;
    for (const Point point : points) {
        bounds = bounds.united({ point.x, point.y, 1, 1 });
    }
    return bounds;
}

} // namespace

Selection::Selection(const Rect& area, std::vector<std::uint8_t> bits)
    : m_bounds(area)
    , m_bits(std::move(bits))
{
    normalize();
}

template <typename Predicate> Selection Selection::fromPredicate(const Rect& area, Predicate inside)
{
    if (area.isEmpty()) {
        return { };
    }
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(area.size().area()), 0);
    for (int y = area.top(); y < area.bottom(); ++y) {
        for (int x = area.left(); x < area.right(); ++x) {
            if (inside(Point { x, y })) {
                bits[offsetIn(area, x, y)] = 1;
            }
        }
    }
    return { area, std::move(bits) };
}

void Selection::normalize()
{
    if (m_bounds.isEmpty()) {
        *this = { };
        return;
    }
    // Menor retângulo com pixels selecionados.
    int left = m_bounds.right();
    int top = m_bounds.bottom();
    int right = m_bounds.left() - 1;
    int bottom = m_bounds.top() - 1;
    for (int y = m_bounds.top(); y < m_bounds.bottom(); ++y) {
        for (int x = m_bounds.left(); x < m_bounds.right(); ++x) {
            if (m_bits[offsetIn(m_bounds, x, y)] != 0) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < left) { // nada selecionado
        m_bounds = { };
        m_bits.clear();
        return;
    }
    const Rect tight = Rect::fromCorners({ left, top }, { right, bottom });
    if (tight == m_bounds) {
        return;
    }
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(tight.size().area()));
    for (int y = tight.top(); y < tight.bottom(); ++y) {
        for (int x = tight.left(); x < tight.right(); ++x) {
            bits[offsetIn(tight, x, y)] = m_bits[offsetIn(m_bounds, x, y)];
        }
    }
    m_bounds = tight;
    m_bits = std::move(bits);
}

// -----------------------------------------------------------------------------
// Formas
// -----------------------------------------------------------------------------
Selection Selection::rectangle(const Rect& area)
{
    if (area.isEmpty()) {
        return { };
    }
    return { area, std::vector<std::uint8_t>(static_cast<std::size_t>(area.size().area()), 1) };
}

Selection Selection::ellipse(const Rect& area)
{
    std::vector<Point> points;
    plotEllipse(area, true, [&](Point point) { points.push_back(point); });
    return fromPoints(points);
}

Selection Selection::fromPoints(const std::vector<Point>& points)
{
    const Rect bounds = boundsOfPoints(points);
    if (bounds.isEmpty()) {
        return { };
    }
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(bounds.size().area()), 0);
    for (const Point point : points) {
        bits[offsetIn(bounds, point.x, point.y)] = 1;
    }
    return { bounds, std::move(bits) };
}

Selection Selection::polygon(const std::vector<Point>& vertices)
{
    if (vertices.empty()) {
        return { };
    }
    // Contorno: as arestas, inclusive a que fecha o polígono.
    std::vector<Point> outline;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const Point from = vertices[index];
        const Point to = vertices[(index + 1) % vertices.size()];
        plotLine(from, to, [&](Point point) { outline.push_back(point); });
    }
    const Selection border = fromPoints(outline);
    if (vertices.size() < 3) {
        return border;
    }
    // Interior: centro do pixel dentro do polígono (regra par-ímpar). Os
    // vértices também ficam no centro dos pixels clicados.
    const auto inside = [&](Point point) {
        const double px = point.x + 0.5;
        const double py = point.y + 0.5;
        bool result = false;
        for (std::size_t i = 0, j = vertices.size() - 1; i < vertices.size(); j = i++) {
            const double xi = vertices[i].x + 0.5;
            const double yi = vertices[i].y + 0.5;
            const double xj = vertices[j].x + 0.5;
            const double yj = vertices[j].y + 0.5;
            if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) {
                result = !result;
            }
        }
        return result;
    };
    return fromPredicate(border.bounds(), inside).united(border);
}

// -----------------------------------------------------------------------------
// Consulta
// -----------------------------------------------------------------------------
bool Selection::contains(Point point) const
{
    return m_bounds.contains(point) && m_bits[offsetIn(m_bounds, point.x, point.y)] != 0;
}

bool Selection::isRectangular() const
{
    return !isEmpty() && std::all_of(m_bits.begin(), m_bits.end(), [](auto bit) { return bit; });
}

long long Selection::pixelCount() const
{
    return std::count(m_bits.begin(), m_bits.end(), std::uint8_t { 1 });
}

std::vector<Point> Selection::points() const
{
    std::vector<Point> result;
    for (int y = m_bounds.top(); y < m_bounds.bottom(); ++y) {
        for (int x = m_bounds.left(); x < m_bounds.right(); ++x) {
            if (m_bits[offsetIn(m_bounds, x, y)] != 0) {
                result.push_back({ x, y });
            }
        }
    }
    return result;
}

// -----------------------------------------------------------------------------
// Combinações
// -----------------------------------------------------------------------------
Selection Selection::united(const Selection& other) const
{
    if (other.isEmpty()) {
        return *this;
    }
    if (isEmpty()) {
        return other;
    }
    return fromPredicate(m_bounds.united(other.m_bounds),
        [&](Point point) { return contains(point) || other.contains(point); });
}

Selection Selection::subtracted(const Selection& other) const
{
    if (isEmpty() || other.isEmpty()) {
        return *this;
    }
    return fromPredicate(
        m_bounds, [&](Point point) { return contains(point) && !other.contains(point); });
}

Selection Selection::intersected(const Rect& area) const
{
    const Rect common = m_bounds.intersected(area);
    if (common == m_bounds) {
        return *this;
    }
    return fromPredicate(common, [&](Point point) { return contains(point); });
}

Selection Selection::inverted(const Rect& area) const
{
    return fromPredicate(area, [&](Point point) { return !contains(point); });
}

Selection Selection::translated(Point offset) const
{
    Selection result = *this;
    result.m_bounds = m_bounds.translated(offset);
    return result;
}

// -----------------------------------------------------------------------------
// Varinha mágica
// -----------------------------------------------------------------------------
Selection magicWand(
    const AnyImage& image, const Palette& palette, Point seed, int tolerance, bool contiguous)
{
    const Rect bounds = boundsOf(image);
    if (!bounds.contains(seed)) {
        return { };
    }
    // Cor RGBA de cada pixel (indexada: a cor da paleta, sempre opaca).
    const auto colorAt = [&](Point point) -> Rgba {
        if (const auto* indexed = std::get_if<IndexedImage>(&image)) {
            return Rgba::opaque(palette.color(indexed->pixel(point.x, point.y)));
        }
        return std::get<RgbaImage>(image).pixel(point.x, point.y);
    };
    const Rgba reference = colorAt(seed);
    const int limit = std::clamp(tolerance, 0, 100) * 255 / 100;
    const auto similar = [&](Point point) {
        const Rgba color = colorAt(point);
        if (color.alpha == 0 && reference.alpha == 0) {
            return true; // transparentes são todos iguais, qualquer que seja o RGB
        }
        const std::array differences
            = { std::abs(color.red - reference.red), std::abs(color.green - reference.green),
                  std::abs(color.blue - reference.blue), std::abs(color.alpha - reference.alpha) };
        return *std::max_element(differences.begin(), differences.end()) <= limit;
    };

    std::vector<Point> points;
    if (!contiguous) { // todos os pixels parecidos da imagem
        for (int y = 0; y < bounds.height; ++y) {
            for (int x = 0; x < bounds.width; ++x) {
                if (similar({ x, y })) {
                    points.push_back({ x, y });
                }
            }
        }
        return Selection::fromPoints(points);
    }

    // Região 4-conectada (pilha explícita: sem recursão profunda).
    std::vector<std::uint8_t> visited(static_cast<std::size_t>(bounds.size().area()), 0);
    std::vector<Point> stack { seed };
    while (!stack.empty()) {
        const Point point = stack.back();
        stack.pop_back();
        if (!bounds.contains(point) || visited[offsetIn(bounds, point.x, point.y)] != 0) {
            continue;
        }
        visited[offsetIn(bounds, point.x, point.y)] = 1;
        if (!similar(point)) {
            continue;
        }
        points.push_back(point);
        stack.push_back({ point.x + 1, point.y });
        stack.push_back({ point.x - 1, point.y });
        stack.push_back({ point.x, point.y + 1 });
        stack.push_back({ point.x, point.y - 1 });
    }
    return Selection::fromPoints(points);
}

} // namespace spritenator::core
