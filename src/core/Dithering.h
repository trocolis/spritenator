// =============================================================================
// Dithering.h
//
// Busca da cor mais próxima numa paleta e pontilhamento por difusão de erro
// (Floyd–Steinberg), escritos de forma genérica: a ORIGEM das cores é uma
// função (pode vir de uma imagem indexada + paleta, ou de uma imagem RGBA) e
// o DESTINO também (quem chama decide onde gravar o índice escolhido).
//
// Usado por PaletteOps (trocar a paleta de uma imagem indexada) e por
// ColorConversion (converter RGBA em indexada).
// =============================================================================
#pragma once

#include "Geometry.h"
#include "Palette.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

namespace spritenator::core {

/// Índice da cor de "palette" mais parecida com "target", considerando
/// apenas os índices marcados em "allowed" (que não deve estar vazio).
[[nodiscard]] inline ColorIndex nearestAllowedIndex(
    const Palette& palette, const Rgb& target, const ColorSet& allowed)
{
    int bestIndex = 0;
    int bestDistance = std::numeric_limits<int>::max();
    for (int index = 0; index < palette.size(); ++index) {
        if (!allowed.test(static_cast<std::size_t>(index))) {
            continue;
        }
        const int distance = colorDistanceSquared(target, palette.color(index));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return static_cast<ColorIndex>(bestIndex);
}

/// Pontilhamento de Floyd–Steinberg.
///
///   - colorAt(x, y) -> std::optional<Rgb>: cor original do pixel; nullopt
///     indica um pixel a ser ignorado (não recebe índice nem espalha erro);
///   - emit(x, y, ColorIndex): recebe o índice escolhido para o pixel.
///
/// O erro de cada pixel (cor desejada - cor obtida) é distribuído aos
/// vizinhos ainda não processados com os pesos clássicos 7/16, 3/16, 5/16 e
/// 1/16, o que reproduz tons intermediários com as cores disponíveis.
template <typename ColorAt, typename Emit>
void floydSteinberg(
    Size size, const Palette& palette, const ColorSet& allowed, ColorAt&& colorAt, Emit&& emit)
{
    // Erro acumulado por canal (multiplicado por 16), linha atual e seguinte.
    struct Error {
        int red = 0;
        int green = 0;
        int blue = 0;
    };
    const auto width = static_cast<std::size_t>(size.width);
    std::vector<Error> current(width + 2);
    std::vector<Error> next(width + 2);
    const auto clampChannel
        = [](int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); };

    for (int y = 0; y < size.height; ++y) {
        std::fill(next.begin(), next.end(), Error { });
        for (int x = 0; x < size.width; ++x) {
            const std::optional<Rgb> source = colorAt(x, y);
            if (!source) {
                continue;
            }
            // Índice +1: as posições 0 e width+1 são "margens" sem efeito.
            const auto column = static_cast<std::size_t>(x) + 1;
            const Error& error = current[column];
            const Rgb wanted { clampChannel(source->red + error.red / 16),
                clampChannel(source->green + error.green / 16),
                clampChannel(source->blue + error.blue / 16) };
            const ColorIndex chosen = nearestAllowedIndex(palette, wanted, allowed);
            emit(x, y, chosen);

            const Rgb obtained = palette.color(chosen);
            const int errorRed = wanted.red - obtained.red;
            const int errorGreen = wanted.green - obtained.green;
            const int errorBlue = wanted.blue - obtained.blue;
            const auto spread = [&](Error& target, int weight) {
                target.red += errorRed * weight;
                target.green += errorGreen * weight;
                target.blue += errorBlue * weight;
            };
            spread(current[column + 1], 7);
            spread(next[column - 1], 3);
            spread(next[column], 5);
            spread(next[column + 1], 1);
        }
        std::swap(current, next);
    }
}

} // namespace spritenator::core
