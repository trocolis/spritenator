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
#include "Image.h"

#include <functional>
#include <vector>

namespace spritenator::core {

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
/// vazia se "seed" estiver fora da imagem. Funciona para qualquer tipo de
/// pixel (índice ou RGBA): "mesma cor" é igualdade exata do pixel.
template <typename Pixel>
[[nodiscard]] std::vector<Point> floodRegion(const Image<Pixel>& image, Point seed)
{
    std::vector<Point> region;
    if (!image.contains(seed)) {
        return region;
    }
    const Pixel target = image.pixel(seed.x, seed.y);
    const int width = image.width();
    std::vector<bool> visited(static_cast<std::size_t>(image.size().area()), false);
    const auto indexOf = [width](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
    };
    const auto matches = [&](int x, int y) {
        return image.contains({ x, y }) && !visited[indexOf(x, y)] && image.pixel(x, y) == target;
    };

    // Algoritmo por varredura de linhas ("scanline fill"): para cada semente,
    // expande à esquerda e à direita e empilha as linhas de cima e de baixo.
    // Evita a recursão profunda (e o estouro de pilha) da versão ingênua.
    std::vector<Point> stack { seed };
    while (!stack.empty()) {
        const Point current = stack.back();
        stack.pop_back();
        if (!matches(current.x, current.y)) {
            continue;
        }
        int left = current.x;
        while (matches(left - 1, current.y)) {
            --left;
        }
        int right = current.x;
        while (matches(right + 1, current.y)) {
            ++right;
        }
        for (int x = left; x <= right; ++x) {
            visited[indexOf(x, current.y)] = true;
            region.push_back({ x, current.y });
            for (const int neighbourY : { current.y - 1, current.y + 1 }) {
                if (matches(x, neighbourY)) {
                    stack.push_back({ x, neighbourY });
                }
            }
        }
    }
    return region;
}

} // namespace spritenator::core
