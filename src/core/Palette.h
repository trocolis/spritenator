// =============================================================================
// Palette.h
//
// Paleta de cores de uma imagem indexada: 2 cores (1 bit), 16 cores (4 bits)
// ou 256 cores (8 bits).
// =============================================================================
#pragma once

#include "Color.h"

#include <array>
#include <bitset>

namespace spritenator::core {

/// Profundidade de cor suportada pelas imagens indexadas.
enum class ColorDepth {
    Bits1, ///< 2 cores (monocromática)
    Bits4, ///< 16 cores
    Bits8, ///< 256 cores
};

/// Bits por pixel correspondentes a uma profundidade (1, 4 ou 8).
[[nodiscard]] constexpr int bitsPerPixelOf(ColorDepth depth)
{
    switch (depth) {
    case ColorDepth::Bits1: return 1;
    case ColorDepth::Bits4: return 4;
    case ColorDepth::Bits8: return 8;
    }
    return 8;
}

/// Quantidade de cores correspondente a uma profundidade (2, 16 ou 256).
[[nodiscard]] constexpr int colorCountOf(ColorDepth depth)
{
    return 1 << bitsPerPixelOf(depth);
}

/// Conjunto de índices de paleta (um bit por cor). Usado para máscaras de
/// proteção e para levantamento de cores utilizadas numa imagem.
using ColorSet = std::bitset<256>;

/// Paleta com 16 ou 256 entradas RGB.
class Palette {
public:
    /// Quantidade máxima de cores de qualquer paleta.
    static constexpr int kMaxColors = 256;

    /// Cria uma paleta preta com a profundidade informada.
    explicit Palette(ColorDepth depth = ColorDepth::Bits8);

    /// Paleta padrão para novas imagens:
    ///  - 16 cores: as 16 cores clássicas do Windows/VGA;
    ///  - 256 cores: 16 cores clássicas + cubo 6x6x6 + rampa de 24 cinzas.
    [[nodiscard]] static Palette makeDefault(ColorDepth depth);

    [[nodiscard]] ColorDepth depth() const { return m_depth; }

    /// Número de entradas válidas (16 ou 256).
    [[nodiscard]] int size() const { return colorCountOf(m_depth); }

    /// Verifica se o índice pertence à paleta.
    [[nodiscard]] bool isValidIndex(int index) const { return index >= 0 && index < size(); }

    /// Cor no índice informado. Lança std::out_of_range se inválido.
    [[nodiscard]] const Rgb& color(int index) const;

    /// Altera a cor no índice informado. Lança std::out_of_range se inválido.
    void setColor(int index, const Rgb& color);

    /// Altera a profundidade. Ao reduzir para 16 cores, as entradas acima de
    /// 15 são descartadas; ao aumentar, as novas entradas ficam pretas.
    void setDepth(ColorDepth depth);

    /// Índice da cor da paleta mais parecida com a cor informada,
    /// considerando apenas o intervalo [first, last] (inclusivo, limitado ao
    /// tamanho da paleta). Em empate vence o menor índice.
    [[nodiscard]] ColorIndex nearestIndex(const Rgb& target, int first = 0, int last = 255) const;

    /// Converte todas as cores para a precisão de 15 bits (ver Rgb::to15Bit).
    void convertTo15Bit();

    /// Preenche as entradas entre "from" e "to" (inclusive, em qualquer
    /// ordem) com uma interpolação linear entre as cores das extremidades.
    /// Equivale ao botão "Gradation" do original.
    void interpolate(int from, int to);

    friend bool operator==(const Palette&, const Palette&) = default;

private:
    /// Valida um índice e lança std::out_of_range se for inválido.
    void checkIndex(int index) const;

    std::array<Rgb, kMaxColors> m_colors { };
    ColorDepth m_depth = ColorDepth::Bits8;
};

} // namespace spritenator::core
