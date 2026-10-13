// =============================================================================
// Transform.cpp
// =============================================================================
#include "Transform.h"

#include <cmath>
#include <numbers>

namespace spritenator::core {

namespace {

/// Módulo sempre não negativo (o operador % do C++ pode devolver negativos).
int wrap(int value, int modulus)
{
    const int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

/// Aplica uma função "origem de cada pixel de destino" (mapeamento inverso):
/// para cada (x, y) do resultado, "sourceOf" informa de onde ler. Pontos de
/// origem fora da imagem recebem "fill". O mapeamento inverso garante que
/// nenhum pixel de destino fique sem valor (sem "buracos").
template <typename SourceOf>
IndexedImage remapPixels(const IndexedImage& image, ColorIndex fill, SourceOf&& sourceOf)
{
    IndexedImage result(image.size(), fill);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Point source = sourceOf(x, y);
            if (image.contains(source)) {
                result.setPixel({ x, y }, image.pixel(source.x, source.y));
            }
        }
    }
    return result;
}

} // namespace

IndexedImage shifted(const IndexedImage& image, int dx, int dy)
{
    if (image.isEmpty()) {
        return image;
    }
    return remapPixels(image, 0, [&](int x, int y) {
        return Point { wrap(x - dx, image.width()), wrap(y - dy, image.height()) };
    });
}

IndexedImage flippedHorizontally(const IndexedImage& image)
{
    return remapPixels(image, 0, [&](int x, int y) { return Point { image.width() - 1 - x, y }; });
}

IndexedImage flippedVertically(const IndexedImage& image)
{
    return remapPixels(image, 0, [&](int x, int y) { return Point { x, image.height() - 1 - y }; });
}

IndexedImage mirroredHorizontally(const IndexedImage& image)
{
    // Colunas da metade direita leem a coluna espelhada da metade esquerda.
    // Com largura ímpar, a coluna central é mantida.
    const int half = image.width() / 2;
    return remapPixels(image, 0, [&](int x, int y) {
        const bool rightHalf = x >= image.width() - half;
        return Point { rightHalf ? image.width() - 1 - x : x, y };
    });
}

IndexedImage mirroredVertically(const IndexedImage& image)
{
    const int half = image.height() / 2;
    return remapPixels(image, 0, [&](int x, int y) {
        const bool bottomHalf = y >= image.height() - half;
        return Point { x, bottomHalf ? image.height() - 1 - y : y };
    });
}

IndexedImage rotated90(const IndexedImage& image, bool clockwise, ColorIndex fill)
{
    // Rotação exata em torno do centro. Trabalhamos em coordenadas dobradas
    // (2x) para que centros "entre pixels" (tamanhos pares) sejam inteiros.
    const int centerX2 = image.width() - 1; // 2 * centro x
    const int centerY2 = image.height() - 1; // 2 * centro y
    return remapPixels(image, fill, [&](int x, int y) {
        const int relX2 = 2 * x - centerX2;
        const int relY2 = 2 * y - centerY2;
        // Inverso da rotação horária é a anti-horária, e vice-versa.
        const int srcRelX2 = clockwise ? relY2 : -relY2;
        const int srcRelY2 = clockwise ? -relX2 : relX2;
        const int sumX = srcRelX2 + centerX2;
        const int sumY = srcRelY2 + centerY2;
        // Se a soma for ímpar, o ponto cai entre pixels (só ocorre quando
        // largura e altura têm paridades diferentes): arredondamos para baixo.
        return Point { static_cast<int>(std::floor(sumX / 2.0)),
            static_cast<int>(std::floor(sumY / 2.0)) };
    });
}

IndexedImage rotated(const IndexedImage& image, double degrees, ColorIndex fill)
{
    const double radians = degrees * std::numbers::pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    const double centerX = (image.width() - 1) / 2.0;
    const double centerY = (image.height() - 1) / 2.0;
    return remapPixels(image, fill, [&](int x, int y) {
        // Rotação inversa (anti-horária) do ponto de destino em torno do centro.
        const double relX = x - centerX;
        const double relY = y - centerY;
        const double sourceX = cosine * relX + sine * relY + centerX;
        const double sourceY = -sine * relX + cosine * relY + centerY;
        return Point { static_cast<int>(std::lround(sourceX)),
            static_cast<int>(std::lround(sourceY)) };
    });
}

IndexedImage scaled(const IndexedImage& image, Size newSize)
{
    IndexedImage result(newSize);
    for (int y = 0; y < newSize.height; ++y) {
        // Amostra no centro de cada pixel de destino, evitando o viés que
        // descartaria sempre a última linha/coluna ao reduzir.
        const int sourceY
            = static_cast<int>((2LL * y + 1) * image.height() / (2LL * newSize.height));
        for (int x = 0; x < newSize.width; ++x) {
            const int sourceX
                = static_cast<int>((2LL * x + 1) * image.width() / (2LL * newSize.width));
            result.setPixel({ x, y }, image.pixel(sourceX, sourceY));
        }
    }
    return result;
}

IndexedImage shadowed(const IndexedImage& image, ColorIndex background, ColorIndex shadow)
{
    IndexedImage result = image;
    for (ColorIndex& value : result.data()) {
        if (value != background) {
            value = shadow;
        }
    }
    return result;
}

} // namespace spritenator::core
