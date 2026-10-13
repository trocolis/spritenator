// =============================================================================
// Palette.cpp
// =============================================================================
#include "Palette.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace spritenator::core {

namespace {

/// As 16 cores clássicas do Windows (mesma ordem da paleta VGA do sistema).
constexpr std::array<Rgb, 16> kClassicColors = { {
    { 0, 0, 0 },
    { 128, 0, 0 },
    { 0, 128, 0 },
    { 128, 128, 0 },
    { 0, 0, 128 },
    { 128, 0, 128 },
    { 0, 128, 128 },
    { 192, 192, 192 },
    { 128, 128, 128 },
    { 255, 0, 0 },
    { 0, 255, 0 },
    { 255, 255, 0 },
    { 0, 0, 255 },
    { 255, 0, 255 },
    { 0, 255, 255 },
    { 255, 255, 255 },
} };

/// Converte um inteiro já limitado a [0, 255] em canal de 8 bits.
constexpr std::uint8_t toChannel(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

} // namespace

Palette::Palette(ColorDepth depth)
    : m_depth(depth)
{
}

Palette Palette::makeDefault(ColorDepth depth)
{
    Palette palette(depth);

    // As 16 primeiras entradas são sempre as cores clássicas.
    for (std::size_t index = 0; index < kClassicColors.size(); ++index) {
        palette.m_colors.at(index) = kClassicColors.at(index);
    }
    if (depth == ColorDepth::Bits4) {
        return palette;
    }

    // Entradas 16..231: cubo de cores 6x6x6 (níveis 0, 51, 102, ..., 255).
    std::size_t next = 16;
    for (int red = 0; red < 6; ++red) {
        for (int green = 0; green < 6; ++green) {
            for (int blue = 0; blue < 6; ++blue) {
                palette.m_colors.at(next++)
                    = { toChannel(red * 51), toChannel(green * 51), toChannel(blue * 51) };
            }
        }
    }

    // Entradas 232..255: rampa de 24 tons de cinza (8, 18, ..., 238).
    for (int step = 0; step < 24; ++step) {
        const std::uint8_t level = toChannel(8 + step * 10);
        palette.m_colors.at(next++) = { level, level, level };
    }
    return palette;
}

void Palette::checkIndex(int index) const
{
    if (!isValidIndex(index)) {
        throw std::out_of_range("Palette: índice de cor inválido: " + std::to_string(index));
    }
}

const Rgb& Palette::color(int index) const
{
    checkIndex(index);
    return m_colors.at(static_cast<std::size_t>(index));
}

void Palette::setColor(int index, const Rgb& color)
{
    checkIndex(index);
    m_colors.at(static_cast<std::size_t>(index)) = color;
}

void Palette::setDepth(ColorDepth depth)
{
    if (depth == m_depth) {
        return;
    }
    // Entradas que deixam de existir são zeradas para que, se a paleta voltar
    // a crescer, as novas cores comecem pretas (comportamento previsível).
    if (depth == ColorDepth::Bits4) {
        std::fill(m_colors.begin() + 16, m_colors.end(), Rgb { });
    }
    m_depth = depth;
}

ColorIndex Palette::nearestIndex(const Rgb& target, int first, int last) const
{
    first = std::clamp(first, 0, size() - 1);
    last = std::clamp(last, 0, size() - 1);
    if (first > last) {
        std::swap(first, last);
    }

    int bestIndex = first;
    int bestDistance = std::numeric_limits<int>::max();
    for (int index = first; index <= last; ++index) {
        const int distance
            = colorDistanceSquared(target, m_colors.at(static_cast<std::size_t>(index)));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
            if (distance == 0) {
                break; // cor exata: não há como melhorar
            }
        }
    }
    return static_cast<ColorIndex>(bestIndex);
}

void Palette::convertTo15Bit()
{
    for (int index = 0; index < size(); ++index) {
        auto& entry = m_colors.at(static_cast<std::size_t>(index));
        entry = entry.to15Bit();
    }
}

void Palette::interpolate(int from, int to)
{
    checkIndex(from);
    checkIndex(to);
    if (from > to) {
        std::swap(from, to);
    }
    const int span = to - from;
    if (span < 2) {
        return; // nada entre as extremidades
    }

    const Rgb start = color(from);
    const Rgb end = color(to);
    // Interpolação linear com arredondamento: cor = início + (fim - início) * t.
    const auto mix = [span](std::uint8_t a, std::uint8_t b, int step) {
        const int delta = static_cast<int>(b) - static_cast<int>(a);
        const int numerator = delta * step;
        // Arredonda para o inteiro mais próximo também para valores negativos.
        const int rounded = (numerator >= 0 ? numerator + span / 2 : numerator - span / 2) / span;
        return toChannel(static_cast<int>(a) + rounded);
    };
    for (int step = 1; step < span; ++step) {
        m_colors.at(static_cast<std::size_t>(from + step)) = { mix(start.red, end.red, step),
            mix(start.green, end.green, step), mix(start.blue, end.blue, step) };
    }
}

} // namespace spritenator::core
