// =============================================================================
// GridLayout.h
//
// Configuração das grades exibidas sobre a imagem (diálogo "Grades"):
//
//   - grade PRINCIPAL: por padrão 1x1 px, ou seja, a grade de pixels; pode
//     ser configurada (ex.: 2x2 para trabalhar em "pixels dobrados");
//   - SUB-GRADE: o "tamanho do personagem" herdado do Character Maker 1999
//     (ex.: 24x32 nos charsets do RPG Maker 2000). Delimita os quadros de
//     animação e é usada pela janela de animação.
// =============================================================================
#pragma once

#include "Geometry.h"

namespace spritenator::core {

/// Espaçamentos das grades (em pixels da imagem).
struct GridLayout {
    /// Grade principal (padrão: um pixel).
    Size mainGrid { 1, 1 };

    /// Sub-grade / célula de personagem.
    Size subGrid { 24, 32 };

    friend constexpr bool operator==(const GridLayout&, const GridLayout&) = default;

    /// Célula da sub-grade que contém o ponto (sempre do tamanho da célula).
    [[nodiscard]] constexpr Rect subGridCellAt(Point point) const { return cellAt(point, subGrid); }

    /// Célula de uma grade de espaçamento "step" que contém o ponto.
    [[nodiscard]] static constexpr Rect cellAt(Point point, Size step)
    {
        // Divisão "para baixo" (floor) correta também para coordenadas negativas.
        const auto floorDiv = [](int value, int divisor) constexpr {
            const int quotient = value / divisor;
            return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
        };
        const Point origin { floorDiv(point.x, step.width) * step.width,
            floorDiv(point.y, step.height) * step.height };
        return Rect::fromPointAndSize(origin, step);
    }
};

} // namespace spritenator::core
