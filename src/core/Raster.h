// =============================================================================
// Raster.h
//
// Algoritmos de rasterização (conversão de figuras geométricas em pixels).
// Todos trabalham com coordenadas inteiras e são "pixel perfect", como se
// espera de um editor de pixel art: nenhuma suavização (anti-aliasing).
//
// As funções chamam um "plotter" para cada pixel gerado, desacoplando o
// algoritmo de COMO o pixel é desenhado (cor, retícula, máscara, espessura).
// =============================================================================
#pragma once

#include "Geometry.h"

#include <functional>

namespace spritenator::core {

/// Função chamada para cada pixel gerado por um algoritmo de rasterização.
using Plotter = std::function<void(Point)>;

/// Reta entre "from" e "to" (inclusive) pelo algoritmo de Bresenham.
void plotLine(Point from, Point to, const Plotter& plot);

/// Retângulo delimitador dos pixels de uma reta (para cálculo de área suja).
[[nodiscard]] Rect lineBounds(Point from, Point to);

} // namespace spritenator::core
