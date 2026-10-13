// =============================================================================
// Raster.h
//
// Algoritmos de rasterização (conversão de figuras geométricas em pixels).
// Todos trabalham com coordenadas inteiras e são "pixel perfect", como se
// espera de um editor de pixel art: nenhuma suavização (anti-aliasing).
//
// As funções chamam um "plotter" para cada pixel gerado, desacoplando o
// algoritmo de COMO o pixel é desenhado (cor, retícula, máscara, espessura).
// Um mesmo pixel pode ser informado mais de uma vez; os plotters devem ser
// idempotentes (pintar duas vezes = pintar uma vez).
// =============================================================================
#pragma once

#include "Geometry.h"

#include <functional>
#include <vector>

namespace spritenator::core {

class IndexedImage;

/// Função chamada para cada pixel gerado por um algoritmo de rasterização.
using Plotter = std::function<void(Point)>;

/// Reta entre "from" e "to" (inclusive) pelo algoritmo de Bresenham.
void plotLine(Point from, Point to, const Plotter& plot);

/// Retângulo delimitador dos pixels de uma reta (para cálculo de área suja).
[[nodiscard]] Rect lineBounds(Point from, Point to);

/// Retângulo: contorno de 1 pixel ou preenchido.
void plotRectangle(const Rect& bounds, bool filled, const Plotter& plot);

/// Elipse inscrita no retângulo (contorno ou preenchida). Funciona com
/// larguras/alturas pares e ímpares, inclusive 1 pixel.
void plotEllipse(const Rect& bounds, bool filled, const Plotter& plot);

/// Retângulo de cantos arredondados. "cornerSize" é o diâmetro da elipse
/// usada nos cantos (limitado ao tamanho do retângulo).
void plotRoundRectangle(const Rect& bounds, int cornerSize, bool filled, const Plotter& plot);

/// Diâmetro de canto padrão para um retângulo arredondado deste tamanho.
[[nodiscard]] int defaultCornerSize(const Rect& bounds);

/// Região de preenchimento por inundação (balde de tinta): todos os pixels
/// 4-conectados a "seed" com a mesma cor dele. Devolve a lista de pixels;
/// vazia se "seed" estiver fora da imagem.
[[nodiscard]] std::vector<Point> floodRegion(const IndexedImage& image, Point seed);

} // namespace spritenator::core
