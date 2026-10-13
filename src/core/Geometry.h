// =============================================================================
// Geometry.h
//
// Tipos geométricos inteiros básicos (ponto, tamanho e retângulo) usados por
// todo o núcleo. São estruturas simples, constexpr e sem dependência do Qt,
// para que o "core" possa ser testado e reutilizado isoladamente.
//
// Convenção: retângulos são semiabertos — incluem (x, y) e excluem
// (x + width, y + height). Isso simplifica laços e interseções.
// =============================================================================
#pragma once

#include <algorithm>

namespace spritenator::core {

/// Ponto em coordenadas inteiras de pixel.
struct Point {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(const Point&, const Point&) = default;

    constexpr Point operator+(const Point& other) const { return { x + other.x, y + other.y }; }
    constexpr Point operator-(const Point& other) const { return { x - other.x, y - other.y }; }
};

/// Dimensões inteiras (largura x altura).
struct Size {
    int width = 0;
    int height = 0;

    friend constexpr bool operator==(const Size&, const Size&) = default;

    /// Verdadeiro se alguma dimensão for nula ou negativa.
    [[nodiscard]] constexpr bool isEmpty() const { return width <= 0 || height <= 0; }

    /// Área em pixels (0 se vazio).
    [[nodiscard]] constexpr long long area() const
    {
        return isEmpty() ? 0 : static_cast<long long>(width) * height;
    }
};

/// Retângulo semiaberto alinhado aos eixos.
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    friend constexpr bool operator==(const Rect&, const Rect&) = default;

    /// Cria um retângulo a partir de posição e tamanho.
    [[nodiscard]] static constexpr Rect fromPointAndSize(Point origin, Size size)
    {
        return { origin.x, origin.y, size.width, size.height };
    }

    /// Cria o menor retângulo que contém os dois pontos (ambos INCLUSIVOS),
    /// independentemente da ordem em que foram informados — útil para
    /// seleções feitas arrastando o mouse em qualquer direção.
    [[nodiscard]] static constexpr Rect fromCorners(Point a, Point b)
    {
        const int left = std::min(a.x, b.x);
        const int top = std::min(a.y, b.y);
        const int right = std::max(a.x, b.x);
        const int bottom = std::max(a.y, b.y);
        return { left, top, right - left + 1, bottom - top + 1 };
    }

    [[nodiscard]] constexpr int left() const { return x; }
    [[nodiscard]] constexpr int top() const { return y; }
    /// Primeira coluna FORA do retângulo.
    [[nodiscard]] constexpr int right() const { return x + width; }
    /// Primeira linha FORA do retângulo.
    [[nodiscard]] constexpr int bottom() const { return y + height; }

    [[nodiscard]] constexpr Point topLeft() const { return { x, y }; }
    [[nodiscard]] constexpr Size size() const { return { width, height }; }
    [[nodiscard]] constexpr bool isEmpty() const { return width <= 0 || height <= 0; }

    /// Verifica se o ponto está dentro do retângulo.
    [[nodiscard]] constexpr bool contains(Point point) const
    {
        return point.x >= x && point.y >= y && point.x < right() && point.y < bottom();
    }

    /// Verifica se o outro retângulo está inteiramente contido neste.
    [[nodiscard]] constexpr bool contains(const Rect& other) const
    {
        return !other.isEmpty() && other.x >= x && other.y >= y && other.right() <= right()
            && other.bottom() <= bottom();
    }

    /// Interseção de dois retângulos (vazio se não se tocarem).
    [[nodiscard]] constexpr Rect intersected(const Rect& other) const
    {
        const int left = std::max(x, other.x);
        const int top = std::max(y, other.y);
        const int rightEdge = std::min(right(), other.right());
        const int bottomEdge = std::min(bottom(), other.bottom());
        if (rightEdge <= left || bottomEdge <= top) {
            return { };
        }
        return { left, top, rightEdge - left, bottomEdge - top };
    }

    /// Menor retângulo que contém ambos (ignora retângulos vazios).
    [[nodiscard]] constexpr Rect united(const Rect& other) const
    {
        if (isEmpty()) {
            return other;
        }
        if (other.isEmpty()) {
            return *this;
        }
        const int left = std::min(x, other.x);
        const int top = std::min(y, other.y);
        const int rightEdge = std::max(right(), other.right());
        const int bottomEdge = std::max(bottom(), other.bottom());
        return { left, top, rightEdge - left, bottomEdge - top };
    }

    /// Retângulo deslocado pelo vetor informado.
    [[nodiscard]] constexpr Rect translated(Point offset) const
    {
        return { x + offset.x, y + offset.y, width, height };
    }
};

} // namespace spritenator::core
