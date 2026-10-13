// =============================================================================
// IndexedImage.h
//
// Imagem indexada: uma matriz de índices de paleta (1 byte por pixel). É a
// estrutura central do editor: o canvas de cada documento e o buffer da área
// de desenho (Draw Area) são instâncias desta classe.
//
// A imagem não guarda a paleta; quem a exibe (documento) associa as duas.
// =============================================================================
#pragma once

#include "Color.h"
#include "Geometry.h"
#include "Palette.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace spritenator::core {

/// Regras aplicadas ao escrever pixels sobre uma imagem existente.
///
/// Reproduz duas funções do original:
///  - "Transparent Colour Effect": pixels de origem com a cor transparente
///    não são copiados;
///  - "Mask": pixels de DESTINO cuja cor está protegida não são alterados.
struct WriteRules {
    /// Índice tratado como transparente na ORIGEM (nenhum se vazio).
    std::optional<ColorIndex> transparentIndex;
    /// Cores protegidas no DESTINO (nenhuma se nulo). Não é dono do conjunto.
    const ColorSet* protectedColors = nullptr;

    /// Decide se o pixel de destino "current" pode receber a cor "incoming".
    [[nodiscard]] bool allows(ColorIndex current, ColorIndex incoming) const
    {
        if (transparentIndex && incoming == *transparentIndex) {
            return false;
        }
        if (protectedColors != nullptr && protectedColors->test(current)) {
            return false;
        }
        return true;
    }
};

/// Matriz 2D de índices de cor.
class IndexedImage {
public:
    /// Maior dimensão aceita (igual ao limite dos diálogos do original).
    static constexpr int kMaxDimension = 8192;

    /// Cria uma imagem vazia (0x0).
    IndexedImage() = default;

    /// Cria uma imagem do tamanho informado, preenchida com "fill".
    /// Lança std::invalid_argument se o tamanho for inválido.
    explicit IndexedImage(Size size, ColorIndex fill = 0);

    [[nodiscard]] int width() const { return m_size.width; }
    [[nodiscard]] int height() const { return m_size.height; }
    [[nodiscard]] Size size() const { return m_size; }
    [[nodiscard]] Rect bounds() const { return { 0, 0, m_size.width, m_size.height }; }
    [[nodiscard]] bool isEmpty() const { return m_size.isEmpty(); }

    /// Verifica se o ponto está dentro da imagem.
    [[nodiscard]] bool contains(Point point) const { return bounds().contains(point); }

    /// Lê um pixel SEM verificação em builds de release (assert em debug).
    /// Use em laços internos quando o ponto já foi validado.
    [[nodiscard]] ColorIndex pixel(int x, int y) const
    {
        assert(contains({ x, y }));
        return m_pixels[offsetOf(x, y)];
    }

    /// Lê um pixel com verificação: devolve "fallback" se estiver fora.
    [[nodiscard]] ColorIndex pixelOr(Point point, ColorIndex fallback) const
    {
        return contains(point) ? m_pixels[offsetOf(point.x, point.y)] : fallback;
    }

    /// Escreve um pixel, ignorando silenciosamente pontos fora da imagem
    /// (recorte automático, conveniente para ferramentas de desenho).
    /// Devolve true se o pixel foi efetivamente alterado.
    bool setPixel(Point point, ColorIndex color, const WriteRules& rules = { });

    /// Preenche a imagem toda com uma cor.
    void fill(ColorIndex color);

    /// Preenche um retângulo (recortado aos limites da imagem).
    void fillRect(const Rect& area, ColorIndex color, const WriteRules& rules = { });

    /// Copia uma região para uma nova imagem do tamanho de "area". Partes da
    /// região fora desta imagem são preenchidas com "outside".
    [[nodiscard]] IndexedImage copy(const Rect& area, ColorIndex outside = 0) const;

    /// Desenha "source" nesta imagem com o canto superior esquerdo em
    /// "destination", respeitando as regras de escrita. Partes fora dos
    /// limites são descartadas.
    void paste(const IndexedImage& source, Point destination, const WriteRules& rules = { });

    /// Nova imagem com outro tamanho; o conteúdo é mantido ancorado no canto
    /// superior esquerdo e as áreas novas recebem "fill".
    [[nodiscard]] IndexedImage resizedCanvas(Size newSize, ColorIndex fill = 0) const;

    /// Conjunto das cores efetivamente usadas pelos pixels.
    [[nodiscard]] ColorSet usedColors() const;

    /// Substitui cada índice "i" por "table[i]" (remapeamento de paleta).
    void remap(const std::array<ColorIndex, 256>& table);

    /// Acesso de leitura aos bytes (linha a linha, sem preenchimento).
    [[nodiscard]] std::span<const ColorIndex> data() const { return m_pixels; }

    /// Acesso de escrita aos bytes (para I/O). Tamanho = largura * altura.
    [[nodiscard]] std::span<ColorIndex> data() { return m_pixels; }

    friend bool operator==(const IndexedImage&, const IndexedImage&) = default;

private:
    /// Deslocamento linear do pixel (x, y) no vetor.
    [[nodiscard]] std::size_t offsetOf(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_size.width)
            + static_cast<std::size_t>(x);
    }

    Size m_size { };
    std::vector<ColorIndex> m_pixels;
};

} // namespace spritenator::core
