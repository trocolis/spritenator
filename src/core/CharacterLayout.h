// =============================================================================
// CharacterLayout.h
//
// Descreve como o canvas é dividido em "células de personagem".
//
// No Character Maker 1999 o canvas (ex.: 72x128) é visto como uma grade de
// quadros do tamanho do personagem (ex.: 24x32). A área de desenho edita UM
// quadro por vez: clicar com o botão direito numa célula do canvas traz o
// quadro para a área de desenho; com o esquerdo, grava o quadro de volta.
// Esta estrutura concentra a matemática dessa grade.
// =============================================================================
#pragma once

#include "Geometry.h"

#include <algorithm>

namespace spritenator::core {

/// Configuração da grade de personagens (diálogo "Character Size").
struct CharacterLayout {
    /// Tamanho de uma célula (quadro) de personagem, em pixels.
    Size cellSize { 24, 32 };

    /// Espaçamento da sub-grade, em pixels (linhas auxiliares mais finas).
    Size subGridSize { 8, 8 };

    /// Exibe o canvas com o dobro do tamanho ("Based on the usual 2x size").
    bool doubleCanvas = false;

    /// Permite posicionar o cursor de células em passos de MEIA célula.
    bool halfCellSteps = false;

    /// Mantém o cursor padrão do mouse (não troca o formato sobre o canvas).
    bool keepMouseCursor = false;

    friend constexpr bool operator==(const CharacterLayout&, const CharacterLayout&) = default;

    /// Passo de movimentação do cursor de células (célula ou meia célula).
    [[nodiscard]] constexpr Size stepSize() const
    {
        if (!halfCellSteps) {
            return cellSize;
        }
        return { std::max(1, cellSize.width / 2), std::max(1, cellSize.height / 2) };
    }

    /// Retângulo da célula que contém o ponto do canvas, alinhado ao passo
    /// atual. O resultado tem sempre o tamanho de uma célula inteira.
    [[nodiscard]] constexpr Rect cellAt(Point canvasPoint) const
    {
        const Size step = stepSize();
        // Divisão "para baixo" (floor) correta também para coordenadas negativas.
        const auto floorDiv = [](int value, int divisor) constexpr {
            const int quotient = value / divisor;
            return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
        };
        const Point origin { floorDiv(canvasPoint.x, step.width) * step.width,
            floorDiv(canvasPoint.y, step.height) * step.height };
        return Rect::fromPointAndSize(origin, cellSize);
    }

    /// Célula vizinha deslocada "columns"/"rows" passos (pode sair do canvas).
    [[nodiscard]] constexpr Rect neighbour(const Rect& cell, int columns, int rows) const
    {
        const Size step = stepSize();
        return cell.translated({ columns * step.width, rows * step.height });
    }
};

} // namespace spritenator::core
