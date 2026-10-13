// =============================================================================
// Tone.h
//
// Padrões de retícula ("tons") do Character Maker 1999. Ao desenhar com um
// tom, apenas os pixels cobertos pelo padrão recebem a cor — o restante fica
// intacto. É a forma clássica de simular meios-tons em pixel art.
//
// Os 8 padrões (A..H) foram extraídos pixel a pixel dos ícones originais.
// Todos têm período 4x4; a cobertura vai de 16/16 (A, sólido) a 2/16 (H).
//
// O "deslocamento de tom" (tone shift) muda a origem do padrão, permitindo
// por exemplo alternar entre as duas fases de um xadrez.
// =============================================================================
#pragma once

#include "Geometry.h"

#include <array>
#include <cstdint>

namespace spritenator::core {

/// Padrões de tom disponíveis.
enum class Tone : std::uint8_t { A, B, C, D, E, F, G, H };

/// Quantidade de tons.
inline constexpr int kToneCount = 8;

/// Quantidade de deslocamentos distintos (4 colunas x 4 linhas).
inline constexpr int kToneShiftCount = 16;

namespace detail {

/// Cada padrão: 4 linhas de 4 bits; o bit 3 é a coluna 0 (esquerda).
/// Ex.: 0b1101 = "##.#".
inline constexpr std::array<std::array<std::uint8_t, 4>, kToneCount> kTonePatterns = { {
    { 0b1111, 0b1111, 0b1111, 0b1111 }, // A: ####/####/####/####  (16/16)
    { 0b1111, 0b1101, 0b1111, 0b0111 }, // B: ####/##.#/####/.###  (14/16)
    { 0b1111, 0b0101, 0b1111, 0b0101 }, // C: ####/.#.#/####/.#.#  (12/16)
    { 0b1110, 0b0101, 0b1011, 0b0101 }, // D: ###./.#.#/#.##/.#.#  (10/16)
    { 0b1010, 0b0101, 0b1010, 0b0101 }, // E: #.#./.#.#/#.#./.#.#  ( 8/16)
    { 0b1010, 0b0100, 0b1010, 0b0001 }, // F: #.#./.#../#.#./...#  ( 6/16)
    { 0b1010, 0b0000, 0b1010, 0b0000 }, // G: #.#./..../#.#./....  ( 4/16)
    { 0b1000, 0b0000, 0b0010, 0b0000 }, // H: #.../..../..#./....  ( 2/16)
} };

} // namespace detail

/// Verifica se o padrão "tone", deslocado por "shift" (0..15), cobre o pixel.
[[nodiscard]] constexpr bool toneCovers(Tone tone, Point point, int shift = 0)
{
    // Deslocamento: coluna = shift % 4, linha = shift / 4.
    const int column = ((point.x - shift % 4) % 4 + 4) % 4; // módulo sempre positivo
    const int row = ((point.y - shift / 4) % 4 + 4) % 4;
    const std::uint8_t bits = detail::kTonePatterns.at(static_cast<std::size_t>(tone))
                                  .at(static_cast<std::size_t>(row));
    return ((bits >> (3 - column)) & 1U) != 0;
}

} // namespace spritenator::core
