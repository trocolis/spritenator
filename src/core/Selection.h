// =============================================================================
// Selection.h
//
// Seleção de forma livre: uma máscara de pixels (selecionado ou não). Guarda
// apenas o retângulo envolvente e, dentro dele, um byte por pixel — deslocar
// a seleção é só mudar o retângulo.
//
// Formas: retângulo, oval, polígono (ponto a ponto ou desenho livre) e
// conjuntos de pontos (varinha mágica). Combinações: somar, subtrair,
// inverter e recortar a uma área. A representação é sempre "normalizada":
// o retângulo envolvente é o menor que contém todos os pixels selecionados,
// então duas seleções iguais têm sempre os mesmos dados (operator==).
// =============================================================================
#pragma once

#include "AnyImage.h"
#include "Geometry.h"
#include "Palette.h"

#include <cstdint>
#include <vector>

namespace spritenator::core {

/// Máscara de seleção de forma livre (vazia = nada selecionado).
class Selection {
public:
    Selection() = default;

    // --- Formas ---
    [[nodiscard]] static Selection rectangle(const Rect& area);
    /// Oval inscrito no retângulo (os mesmos pixels da elipse preenchida).
    [[nodiscard]] static Selection ellipse(const Rect& area);
    /// Polígono fechado pelos vértices: interior (centros dos pixels, regra
    /// par-ímpar) mais o próprio contorno, para formas finas não sumirem.
    [[nodiscard]] static Selection polygon(const std::vector<Point>& vertices);
    /// Exatamente os pixels informados.
    [[nodiscard]] static Selection fromPoints(const std::vector<Point>& points);

    // --- Consulta ---
    [[nodiscard]] bool isEmpty() const { return m_bounds.isEmpty(); }
    /// Menor retângulo que contém todos os pixels selecionados.
    [[nodiscard]] const Rect& bounds() const { return m_bounds; }
    [[nodiscard]] bool contains(Point point) const;
    /// Verdadeiro se todos os pixels do retângulo envolvente estão selecionados.
    [[nodiscard]] bool isRectangular() const;
    [[nodiscard]] long long pixelCount() const;
    /// Todos os pixels selecionados (linha a linha).
    [[nodiscard]] std::vector<Point> points() const;

    // --- Combinações (o resultado é uma seleção nova) ---
    [[nodiscard]] Selection united(const Selection& other) const;
    [[nodiscard]] Selection subtracted(const Selection& other) const;
    /// Só a parte dentro da área.
    [[nodiscard]] Selection intersected(const Rect& area) const;
    /// Dentro da área, tudo o que NÃO está selecionado.
    [[nodiscard]] Selection inverted(const Rect& area) const;
    [[nodiscard]] Selection translated(Point offset) const;

    friend bool operator==(const Selection&, const Selection&) = default;

private:
    /// Monta a seleção a partir de um retângulo e dos bytes dele (normaliza).
    Selection(const Rect& area, std::vector<std::uint8_t> bits);
    /// Monta a seleção com os pixels da área para os quais inside(p) vale.
    template <typename Predicate>
    [[nodiscard]] static Selection fromPredicate(const Rect& area, Predicate inside);

    /// Reduz o retângulo ao mínimo que contém os pixels selecionados.
    void normalize();

    Rect m_bounds;
    std::vector<std::uint8_t> m_bits; ///< m_bounds.width * m_bounds.height (1 = selecionado)
};

/// Varinha mágica: pixels de cor parecida com a do ponto "seed".
///
/// "tolerance" vai de 0 (só a mesma cor) a 100 (qualquer cor); a distância é
/// a maior diferença entre os canais R, G, B (e A, em RGBA). Em imagens
/// indexadas são comparadas as CORES da paleta, não os índices. Com
/// "contiguous", só a região 4-conectada ao ponto; senão, a imagem toda.
[[nodiscard]] Selection magicWand(
    const AnyImage& image, const Palette& palette, Point seed, int tolerance, bool contiguous);

} // namespace spritenator::core
