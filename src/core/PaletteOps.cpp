// =============================================================================
// PaletteOps.cpp
// =============================================================================
#include "PaletteOps.h"

#include "Dithering.h"

#include <algorithm>
#include <limits>

namespace spritenator::core {

namespace {

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
        table.at(static_cast<std::size_t>(index))
            = nearestAllowedIndex(to, from.color(index), allowed);
    }
    return table;
}

IndexedImage ditherToPalette(const IndexedImage& image, const Palette& from, const Palette& to,
    const ColorSet& targets, const ColorSet& keep)
{
    const ColorSet allowed = effectiveTargets(to, targets);
    IndexedImage result = image;
    floydSteinberg(
        image.size(), to, allowed,
        [&](int x, int y) -> std::optional<Rgb> {
            const ColorIndex original = image.pixel(x, y);
            if (keep.test(original)) {
                return std::nullopt; // mantido como está (ex.: cor transparente)
            }
            return from.isValidIndex(original) ? from.color(original) : Rgb { };
        },
        [&](int x, int y, ColorIndex chosen) { result.setPixel({ x, y }, chosen); });
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
