// =============================================================================
// FreeTransform.cpp
// =============================================================================
#include "FreeTransform.h"

#include "Color.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace spritenator::core {

namespace {

/// Tolerância das comparações (ângulos e cantos "inteiros").
constexpr double kEpsilon = 1e-9;

} // namespace

// -----------------------------------------------------------------------------
// Affine
// -----------------------------------------------------------------------------
Affine Affine::translation(double x, double y)
{
    return { 1, 0, 0, 1, x, y };
}

Affine Affine::scaling(double sx, double sy)
{
    return { sx, 0, 0, sy, 0, 0 };
}

Affine Affine::rotation(double degrees)
{
    // Ângulos retos exatos (sem resíduos de seno/cosseno).
    const double turns = degrees / 90.0;
    if (std::abs(turns - std::round(turns)) < kEpsilon) {
        const int quarter = ((static_cast<int>(std::lround(turns)) % 4) + 4) % 4;
        constexpr double kCos[] = { 1, 0, -1, 0 };
        constexpr double kSin[] = { 0, 1, 0, -1 };
        const double c = kCos[quarter];
        const double s = kSin[quarter];
        return { c, -s, s, c, 0, 0 };
    }
    const double radians = degrees * std::numbers::pi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    // Com y para baixo, esta matriz gira no sentido horário da tela.
    return { c, -s, s, c, 0, 0 };
}

Affine Affine::then(const Affine& next) const
{
    return { next.m11 * m11 + next.m12 * m21, next.m11 * m12 + next.m12 * m22,
        next.m21 * m11 + next.m22 * m21, next.m21 * m12 + next.m22 * m22,
        next.m11 * dx + next.m12 * dy + next.dx, next.m21 * dx + next.m22 * dy + next.dy };
}

std::optional<Affine> Affine::inverted() const
{
    const double determinant = m11 * m22 - m12 * m21;
    if (std::abs(determinant) < kEpsilon) {
        return std::nullopt;
    }
    const double i11 = m22 / determinant;
    const double i12 = -m12 / determinant;
    const double i21 = -m21 / determinant;
    const double i22 = m11 / determinant;
    return Affine { i11, i12, i21, i22, -(i11 * dx + i12 * dy), -(i21 * dx + i22 * dy) };
}

std::pair<double, double> Affine::map(double x, double y) const
{
    return { m11 * x + m12 * y + dx, m21 * x + m22 * y + dy };
}

// -----------------------------------------------------------------------------
// Parâmetros da transformação livre
// -----------------------------------------------------------------------------
bool TransformParams::isIdentity() const
{
    return *this == TransformParams { };
}

std::array<std::pair<double, double>, 4> transformedCorners(
    const Affine& transform, const Rect& bounds)
{
    const double left = bounds.left();
    const double top = bounds.top();
    const double right = bounds.right();
    const double bottom = bounds.bottom();
    return { transform.map(left, top), transform.map(right, top), transform.map(right, bottom),
        transform.map(left, bottom) };
}

Affine affineFor(const TransformParams& params, const Rect& bounds)
{
    const double cx = bounds.x + bounds.width / 2.0;
    const double cy = bounds.y + bounds.height / 2.0;
    Affine result = Affine::translation(-cx, -cy)
                        .then(Affine::scaling(params.sx, params.sy))
                        .then(Affine::rotation(params.angle))
                        .then(Affine::translation(cx + params.dx, cy + params.dy));

    // Ângulo reto: encaixa o canto mínimo na grade (ex.: girar 3x2 em torno
    // do centro deixaria os cantos em meio pixel).
    const double turns = params.angle / 90.0;
    if (std::abs(turns - std::round(turns)) < kEpsilon) {
        double minX = 0;
        double minY = 0;
        bool first = true;
        for (const auto& [x, y] : transformedCorners(result, bounds)) {
            minX = first ? x : std::min(minX, x);
            minY = first ? y : std::min(minY, y);
            first = false;
        }
        // Meio pixel arredonda sempre para o lado positivo (floor(v + 0.5)):
        // assim girar junto à borda de cima ou da esquerda não empurra a
        // seleção para fora da imagem.
        result.dx += std::floor(minX + 0.5) - minX;
        result.dy += std::floor(minY + 0.5) - minY;
    }
    return result;
}

// -----------------------------------------------------------------------------
// Pixels + máscara
// -----------------------------------------------------------------------------
template <typename Pixel>
Floating<Pixel> transformFloating(
    const Image<Pixel>& content, const Selection& mask, const Affine& transform, const Rect& clip)
{
    const auto inverse = transform.inverted();
    if (mask.isEmpty() || !inverse) {
        return { };
    }
    const Rect source = mask.bounds();
    // Retângulo de destino: envolve os cantos transformados.
    double minX = 0;
    double minY = 0;
    double maxX = 0;
    double maxY = 0;
    bool first = true;
    for (const auto& [x, y] : transformedCorners(transform, source)) {
        minX = first ? x : std::min(minX, x);
        minY = first ? y : std::min(minY, y);
        maxX = first ? x : std::max(maxX, x);
        maxY = first ? y : std::max(maxY, y);
        first = false;
    }
    const Rect target = Rect::fromCorners({ static_cast<int>(std::floor(minX + kEpsilon)),
                                              static_cast<int>(std::floor(minY + kEpsilon)) },
        { static_cast<int>(std::ceil(maxX - kEpsilon)) - 1,
            static_cast<int>(std::ceil(maxY - kEpsilon)) - 1 })
                            .intersected(clip);
    if (target.isEmpty()) {
        return { };
    }

    Image<Pixel> pixels(target.size());
    std::vector<Point> selected;
    for (int y = target.top(); y < target.bottom(); ++y) {
        for (int x = target.left(); x < target.right(); ++x) {
            const auto [sx, sy] = inverse->map(x + 0.5, y + 0.5);
            const Point from { static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)) };
            if (!mask.contains(from)) {
                continue;
            }
            pixels.setPixel({ x - target.x, y - target.y },
                content.pixel(from.x - source.x, from.y - source.y));
            selected.push_back({ x, y });
        }
    }
    Floating<Pixel> result;
    result.mask = Selection::fromPoints(selected);
    if (!result.mask.isEmpty()) {
        // Recorta os pixels ao retângulo exato da máscara.
        result.pixels = pixels.copy(result.mask.bounds().translated({ -target.x, -target.y }));
    }
    return result;
}

template Floating<ColorIndex> transformFloating(
    const Image<ColorIndex>&, const Selection&, const Affine&, const Rect&);
template Floating<Rgba> transformFloating(
    const Image<Rgba>&, const Selection&, const Affine&, const Rect&);

} // namespace spritenator::core
