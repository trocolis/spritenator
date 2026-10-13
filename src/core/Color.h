// =============================================================================
// Color.h
//
// Representação de uma cor RGB de 8 bits por canal e utilidades associadas.
// As imagens do Spritenator são indexadas: cada pixel guarda um ÍNDICE de
// paleta; a cor real (Rgb) vive somente na paleta.
// =============================================================================
#pragma once

#include <cstdint>

namespace spritenator::core {

/// Índice de cor na paleta (0..255).
using ColorIndex = std::uint8_t;

/// Cor RGB com 8 bits por canal.
struct Rgb {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend constexpr bool operator==(const Rgb&, const Rgb&) = default;

    /// Empacota a cor no formato 0x00RRGGBB.
    [[nodiscard]] constexpr std::uint32_t toPacked() const
    {
        return (static_cast<std::uint32_t>(red) << 16U) | (static_cast<std::uint32_t>(green) << 8U)
            | static_cast<std::uint32_t>(blue);
    }

    /// Cria uma cor a partir do formato 0x00RRGGBB (o byte alto é ignorado).
    [[nodiscard]] static constexpr Rgb fromPacked(std::uint32_t packed)
    {
        return { static_cast<std::uint8_t>((packed >> 16U) & 0xFFU),
            static_cast<std::uint8_t>((packed >> 8U) & 0xFFU),
            static_cast<std::uint8_t>(packed & 0xFFU) };
    }

    /// Reduz a cor à precisão de 15 bits (5 bits por canal), como faz o
    /// RPG Maker 2000/2003 internamente. O resultado é reexpandido para 8 bits
    /// replicando os bits altos, para que 31 -> 255 e 0 -> 0.
    [[nodiscard]] constexpr Rgb to15Bit() const
    {
        const auto quantize = [](std::uint8_t channel) constexpr {
            const auto fiveBits = static_cast<unsigned>(channel) >> 3U;
            return static_cast<std::uint8_t>((fiveBits << 3U) | (fiveBits >> 2U));
        };
        return { quantize(red), quantize(green), quantize(blue) };
    }
};

/// Distância euclidiana ao quadrado entre duas cores, com pesos aproximando a
/// sensibilidade do olho humano (verde > vermelho > azul). Usada para
/// encontrar a cor mais próxima na paleta. O quadrado evita a raiz.
[[nodiscard]] constexpr int colorDistanceSquared(const Rgb& a, const Rgb& b)
{
    const int deltaRed = static_cast<int>(a.red) - static_cast<int>(b.red);
    const int deltaGreen = static_cast<int>(a.green) - static_cast<int>(b.green);
    const int deltaBlue = static_cast<int>(a.blue) - static_cast<int>(b.blue);
    return 3 * deltaRed * deltaRed + 4 * deltaGreen * deltaGreen + 2 * deltaBlue * deltaBlue;
}

} // namespace spritenator::core
