// =============================================================================
// Quantize.cpp
// =============================================================================
#include "Quantize.h"

#include <algorithm>
#include <map>

namespace spritenator::core {

namespace {

/// Cor distinta e quantas vezes aparece.
struct Entry {
    Rgb color;
    long long count = 0;
};

/// Canal (0 = R, 1 = G, 2 = B) de uma cor.
int channel(const Rgb& color, int index)
{
    return index == 0 ? color.red : (index == 1 ? color.green : color.blue);
}

/// Faixa de valores do canal num grupo.
int rangeOf(const std::vector<Entry>& box, int index)
{
    const auto [low, high]
        = std::minmax_element(box.begin(), box.end(), [index](const Entry& a, const Entry& b) {
              return channel(a.color, index) < channel(b.color, index);
          });
    return channel(high->color, index) - channel(low->color, index);
}

} // namespace

std::vector<Rgb> medianCut(const std::vector<Rgb>& colors, int maxColors)
{
    // Cores distintas com a frequência de cada uma.
    std::map<std::uint32_t, long long> counts;
    for (const Rgb& color : colors) {
        ++counts[color.toPacked()];
    }
    std::vector<Entry> entries;
    for (const auto& [packed, count] : counts) {
        entries.push_back({ Rgb::fromPacked(packed), count });
    }
    if (maxColors <= 0 || entries.empty()) {
        return { };
    }
    if (static_cast<int>(entries.size()) <= maxColors) {
        std::vector<Rgb> exact;
        for (const Entry& entry : entries) {
            exact.push_back(entry.color);
        }
        return exact;
    }

    std::vector<std::vector<Entry>> boxes { std::move(entries) };
    while (static_cast<int>(boxes.size()) < maxColors) {
        // Grupo com a maior faixa (em qualquer canal) que ainda pode dividir.
        int best = -1;
        int bestChannel = 0;
        int bestRange = 0;
        for (std::size_t index = 0; index < boxes.size(); ++index) {
            if (boxes[index].size() < 2) {
                continue;
            }
            for (int c = 0; c < 3; ++c) {
                const int range = rangeOf(boxes[index], c);
                if (range > bestRange) {
                    best = static_cast<int>(index);
                    bestChannel = c;
                    bestRange = range;
                }
            }
        }
        if (best < 0) {
            break; // nada mais a dividir
        }
        std::vector<Entry>& box = boxes[static_cast<std::size_t>(best)];
        std::sort(box.begin(), box.end(), [bestChannel](const Entry& a, const Entry& b) {
            return channel(a.color, bestChannel) < channel(b.color, bestChannel);
        });
        // Mediana ponderada pela frequência.
        long long total = 0;
        for (const Entry& entry : box) {
            total += entry.count;
        }
        long long accumulated = 0;
        std::size_t split = 1;
        for (std::size_t index = 0; index + 1 < box.size(); ++index) {
            accumulated += box[index].count;
            split = index + 1;
            if (accumulated * 2 >= total) {
                break;
            }
        }
        std::vector<Entry> upper(box.begin() + static_cast<std::ptrdiff_t>(split), box.end());
        box.resize(split);
        boxes.push_back(std::move(upper));
    }

    std::vector<Rgb> palette;
    for (const auto& box : boxes) {
        long long red = 0;
        long long green = 0;
        long long blue = 0;
        long long total = 0;
        for (const Entry& entry : box) {
            red += entry.color.red * entry.count;
            green += entry.color.green * entry.count;
            blue += entry.color.blue * entry.count;
            total += entry.count;
        }
        palette.push_back({ static_cast<std::uint8_t>(red / total),
            static_cast<std::uint8_t>(green / total), static_cast<std::uint8_t>(blue / total) });
    }
    return palette;
}

int nearestColor(const std::vector<Rgb>& palette, const Rgb& color)
{
    int best = 0;
    int bestDistance = -1;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const int distance = colorDistanceSquared(palette[index], color);
        if (bestDistance < 0 || distance < bestDistance) {
            best = static_cast<int>(index);
            bestDistance = distance;
        }
    }
    return best;
}

} // namespace spritenator::core
