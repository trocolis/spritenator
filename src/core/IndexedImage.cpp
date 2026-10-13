// =============================================================================
// IndexedImage.cpp
// =============================================================================
#include "IndexedImage.h"

#include <algorithm>
#include <stdexcept>

namespace spritenator::core {

IndexedImage::IndexedImage(Size size, ColorIndex fill)
    : m_size(size)
{
    if (size.isEmpty() || size.width > kMaxDimension || size.height > kMaxDimension) {
        throw std::invalid_argument("IndexedImage: tamanho inválido");
    }
    m_pixels.assign(static_cast<std::size_t>(size.area()), fill);
}

bool IndexedImage::setPixel(Point point, ColorIndex color, const WriteRules& rules)
{
    if (!contains(point)) {
        return false;
    }
    ColorIndex& target = m_pixels[offsetOf(point.x, point.y)];
    if (target == color || !rules.allows(target, color)) {
        return false;
    }
    target = color;
    return true;
}

void IndexedImage::fill(ColorIndex color)
{
    std::fill(m_pixels.begin(), m_pixels.end(), color);
}

void IndexedImage::fillRect(const Rect& area, ColorIndex color, const WriteRules& rules)
{
    const Rect clipped = area.intersected(bounds());
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            ColorIndex& target = m_pixels[offsetOf(x, y)];
            if (rules.allows(target, color)) {
                target = color;
            }
        }
    }
}

IndexedImage IndexedImage::copy(const Rect& area, ColorIndex outside) const
{
    IndexedImage result(area.size(), outside);
    // Só a parte que intersecta esta imagem é copiada; o resto fica "outside".
    const Rect source = area.intersected(bounds());
    for (int y = source.top(); y < source.bottom(); ++y) {
        const auto rowBegin = m_pixels.begin() + static_cast<std::ptrdiff_t>(offsetOf(source.x, y));
        std::copy(rowBegin, rowBegin + source.width,
            result.m_pixels.begin()
                + static_cast<std::ptrdiff_t>(result.offsetOf(source.x - area.x, y - area.y)));
    }
    return result;
}

void IndexedImage::paste(const IndexedImage& source, Point destination, const WriteRules& rules)
{
    // Região de destino efetivamente afetada (recortada aos nossos limites).
    const Rect target = Rect::fromPointAndSize(destination, source.size()).intersected(bounds());
    for (int y = target.top(); y < target.bottom(); ++y) {
        for (int x = target.left(); x < target.right(); ++x) {
            const ColorIndex incoming = source.pixel(x - destination.x, y - destination.y);
            ColorIndex& current = m_pixels[offsetOf(x, y)];
            if (rules.allows(current, incoming)) {
                current = incoming;
            }
        }
    }
}

IndexedImage IndexedImage::resizedCanvas(Size newSize, ColorIndex fill) const
{
    IndexedImage result(newSize, fill);
    result.paste(*this, { 0, 0 });
    return result;
}

ColorSet IndexedImage::usedColors() const
{
    ColorSet used;
    for (const ColorIndex value : m_pixels) {
        used.set(value);
    }
    return used;
}

void IndexedImage::remap(const std::array<ColorIndex, 256>& table)
{
    for (ColorIndex& value : m_pixels) {
        value = table[value];
    }
}

} // namespace spritenator::core
