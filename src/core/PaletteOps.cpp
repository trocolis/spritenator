// =============================================================================
// PaletteOps.cpp
// =============================================================================
#include "PaletteOps.h"

#include <algorithm>
#include <limits>

namespace spritenator::core {

namespace {

/// Cor mais próxima de "target" entre os índices permitidos de "palette".
ColorIndex nearestAllowed(const Palette& palette, const Rgb& target, const ColorSet& targets)
{
    int bestIndex = 0;
    int bestDistance = std::numeric_limits<int>::max();
    for (int index = 0; index < palette.size(); ++index) {
        if (!targets.test(static_cast<std::size_t>(index))) {
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

/// Restringe o conjunto de destinos aos índices válidos (vazio = todos).
ColorSet effectiveTargets(const Palette& palette, const ColorSet& targets)
{
    const ColorSet valid = allColorsOf(palette);
    const ColorSet restricted = targets.none() ? valid : (targets & valid);
    return restricted.none() ? valid : restricted; // nunca deixa sem opções
}

} // namespace

RemapTable identityRemap()
{
    RemapTable table { };
    for (std::size_t index = 0; index < table.size(); ++index) {
        table[index] = static_cast<ColorIndex>(index);
    }
    return table;
}

ColorSet allColorsOf(const Palette& palette)
{
    ColorSet colors;
    for (int index = 0; index < palette.size(); ++index) {
        colors.set(static_cast<std::size_t>(index));
    }
    return colors;
}

ColorSet systemColors()
{
    ColorSet colors;
    for (std::size_t index = 0; index < 10; ++index) {
        colors.set(index); // 0..9
        colors.set(246 + index); // 246..255
    }
    return colors;
}

RemapTable nearestRemap(const Palette& from, const Palette& to, const ColorSet& targets)
{
    const ColorSet allowed = effectiveTargets(to, targets);
    RemapTable table = identityRemap();
    for (int index = 0; index < from.size(); ++index) {
        table.at(static_cast<std::size_t>(index)) = nearestAllowed(to, from.color(index), allowed);
    }
    return table;
}

IndexedImage ditherToPalette(const IndexedImage& image, const Palette& from, const Palette& to,
    const ColorSet& targets, const ColorSet& keep)
{
    const ColorSet allowed = effectiveTargets(to, targets);
    IndexedImage result = image;
    const int width = image.width();

    // Erro acumulado por canal, para a linha atual e a próxima.
    struct Error {
        int red = 0;
        int green = 0;
        int blue = 0;
    };
    std::vector<Error> current(static_cast<std::size_t>(width) + 2);
    std::vector<Error> next(static_cast<std::size_t>(width) + 2);
    const auto clampChannel
        = [](int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); };

    for (int y = 0; y < image.height(); ++y) {
        std::fill(next.begin(), next.end(), Error { });
        for (int x = 0; x < width; ++x) {
            const ColorIndex original = image.pixel(x, y);
            if (keep.test(original)) {
                continue; // mantido como está (ex.: cor transparente)
            }
            // Índice +1: as posições 0 e width+1 são "margens" sem efeito.
            const Error& error = current[static_cast<std::size_t>(x) + 1];
            const Rgb source = from.isValidIndex(original) ? from.color(original) : Rgb { };
            const Rgb wanted { clampChannel(source.red + error.red / 16),
                clampChannel(source.green + error.green / 16),
                clampChannel(source.blue + error.blue / 16) };
            const ColorIndex chosen = nearestAllowed(to, wanted, allowed);
            result.setPixel({ x, y }, chosen);

            // Distribui o erro (x16) segundo os pesos clássicos 7/3/5/1.
            const Rgb obtained = to.color(chosen);
            const int errRed = wanted.red - obtained.red;
            const int errGreen = wanted.green - obtained.green;
            const int errBlue = wanted.blue - obtained.blue;
            const auto spread = [&](Error& target, int weight) {
                target.red += errRed * weight;
                target.green += errGreen * weight;
                target.blue += errBlue * weight;
            };
            const auto column = static_cast<std::size_t>(x) + 1;
            spread(current[column + 1], 7);
            spread(next[column - 1], 3);
            spread(next[column], 5);
            spread(next[column + 1], 1);
        }
        std::swap(current, next);
    }
    return result;
}

CompactResult removeUnusedColors(const Palette& palette, const ColorSet& used)
{
    CompactResult result { Palette(palette.depth()), identityRemap(), 0 };
    int next = 0;
    for (int index = 0; index < palette.size(); ++index) {
        if (used.test(static_cast<std::size_t>(index))) {
            result.palette.setColor(next, palette.color(index));
            result.remap.at(static_cast<std::size_t>(index)) = static_cast<ColorIndex>(next);
            ++next;
        }
    }
    result.removedCount = palette.size() - next;
    return result;
}

RemapTable swapRemap(int a, int b)
{
    RemapTable table = identityRemap();
    std::swap(table.at(static_cast<std::size_t>(a)), table.at(static_cast<std::size_t>(b)));
    return table;
}

Palette mergeColors(const Palette& palette, const std::vector<Rgb>& colors, int sourceStart,
    int count, int destination)
{
    Palette result = palette;
    for (int offset = 0; offset < count; ++offset) {
        const int source = sourceStart + offset;
        const int target = destination + offset;
        if (source < 0 || source >= static_cast<int>(colors.size())
            || !result.isValidIndex(target)) {
            continue;
        }
        result.setColor(target, colors.at(static_cast<std::size_t>(source)));
    }
    return result;
}

IndexedImage applyPalette(const IndexedImage& image, const Palette& oldPalette,
    const Palette& newPalette, const PaletteApplyOptions& options)
{
    if (options.method == RemapMethod::KeepIndices) {
        return image;
    }

    // Destinos permitidos e índices preservados conforme as opções.
    ColorSet targets = allColorsOf(newPalette);
    ColorSet keep;
    if (options.avoidSystemColors && newPalette.size() == Palette::kMaxColors) {
        targets &= ~systemColors();
    }
    if (options.keepTransparentZero) {
        targets.reset(0);
        keep.set(0);
    }

    if (options.method == RemapMethod::Dither) {
        return ditherToPalette(image, oldPalette, newPalette, targets, keep);
    }

    RemapTable table = nearestRemap(oldPalette, newPalette, targets);
    if (options.keepTransparentZero) {
        table[0] = 0;
    }
    IndexedImage result = image;
    result.remap(table);
    return result;
}

} // namespace spritenator::core
