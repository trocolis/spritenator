// =============================================================================
// Image.h
//
// Imagem genérica: uma matriz 2D de pixels de um tipo qualquer. O editor
// trabalha com dois tipos de pixel:
//
//   - ColorIndex (1 byte): imagens INDEXADAS (2, 16 ou 256 cores); o pixel é
//     um índice da paleta, e a cor real vive na paleta do documento;
//   - Rgba (4 bytes): imagens em CORES REAIS com transparência.
//
// Os algoritmos (figuras, preenchimento, transformações) são escritos uma
// única vez sobre Image<Pixel> e servem para os dois modos. Os apelidos
// IndexedImage e RgbaImage tornam o uso no restante do código mais legível.
//
// A implementação fica neste cabeçalho porque é um template: o compilador
// precisa vê-la para gerar o código de cada tipo de pixel.
// =============================================================================
#pragma once

#include "Color.h"
#include "Geometry.h"
#include "Palette.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace spritenator::core {

// -----------------------------------------------------------------------------
// Regras de escrita
// -----------------------------------------------------------------------------

/// Regras aplicadas ao escrever pixels sobre uma imagem existente. Cada tipo
/// de pixel tem a sua especialização.
template <typename Pixel> struct WriteRulesFor;

/// Regras para imagens indexadas. Reproduzem duas funções do original:
///  - "Transparent Colour Effect": pixels de origem com a cor transparente
///    não são copiados;
///  - "Mask": pixels de DESTINO cuja cor está protegida não são alterados.
template <> struct WriteRulesFor<ColorIndex> {
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

/// Regras para imagens RGBA. Não há paleta nem máscara de cores: o "efeito
/// de cor transparente" ignora os pixels de origem totalmente transparentes.
template <> struct WriteRulesFor<Rgba> {
    /// Pixels de origem com alfa 0 não são copiados.
    bool skipTransparent = false;

    [[nodiscard]] bool allows(const Rgba& /*current*/, const Rgba& incoming) const
    {
        return !(skipTransparent && incoming.alpha == 0);
    }
};

/// Nome histórico das regras indexadas (usado em todo o editor).
using WriteRules = WriteRulesFor<ColorIndex>;
using RgbaWriteRules = WriteRulesFor<Rgba>;

// -----------------------------------------------------------------------------
// Image<Pixel>
// -----------------------------------------------------------------------------

/// Matriz 2D de pixels.
template <typename Pixel> class Image {
public:
    using PixelType = Pixel;
    using Rules = WriteRulesFor<Pixel>;

    /// Maior dimensão aceita (igual ao limite dos diálogos do original).
    static constexpr int kMaxDimension = 8192;

    /// Cria uma imagem vazia (0x0).
    Image() = default;

    /// Cria uma imagem do tamanho informado, preenchida com "fill".
    /// Lança std::invalid_argument se o tamanho for inválido.
    explicit Image(Size size, Pixel fill = Pixel { })
        : m_size(size)
    {
        if (size.isEmpty() || size.width > kMaxDimension || size.height > kMaxDimension) {
            throw std::invalid_argument("Image: tamanho inválido");
        }
        m_pixels.assign(static_cast<std::size_t>(size.area()), fill);
    }

    [[nodiscard]] int width() const { return m_size.width; }
    [[nodiscard]] int height() const { return m_size.height; }
    [[nodiscard]] Size size() const { return m_size; }
    [[nodiscard]] Rect bounds() const { return { 0, 0, m_size.width, m_size.height }; }
    [[nodiscard]] bool isEmpty() const { return m_size.isEmpty(); }

    /// Verifica se o ponto está dentro da imagem.
    [[nodiscard]] bool contains(Point point) const { return bounds().contains(point); }

    /// Lê um pixel SEM verificação em builds de release (assert em debug).
    /// Use em laços internos quando o ponto já foi validado.
    [[nodiscard]] Pixel pixel(int x, int y) const
    {
        assert(contains({ x, y }));
        return m_pixels[offsetOf(x, y)];
    }

    /// Lê um pixel com verificação: devolve "fallback" se estiver fora.
    [[nodiscard]] Pixel pixelOr(Point point, Pixel fallback) const
    {
        return contains(point) ? m_pixels[offsetOf(point.x, point.y)] : fallback;
    }

    /// Escreve um pixel, ignorando silenciosamente pontos fora da imagem
    /// (recorte automático, conveniente para ferramentas de desenho).
    /// Devolve true se o pixel foi efetivamente alterado.
    bool setPixel(Point point, Pixel color, const Rules& rules = { })
    {
        if (!contains(point)) {
            return false;
        }
        Pixel& target = m_pixels[offsetOf(point.x, point.y)];
        if (target == color || !rules.allows(target, color)) {
            return false;
        }
        target = color;
        return true;
    }

    /// Preenche a imagem toda com uma cor.
    void fill(Pixel color) { std::fill(m_pixels.begin(), m_pixels.end(), color); }

    /// Preenche um retângulo (recortado aos limites da imagem).
    void fillRect(const Rect& area, Pixel color, const Rules& rules = { })
    {
        const Rect clipped = area.intersected(bounds());
        for (int y = clipped.top(); y < clipped.bottom(); ++y) {
            for (int x = clipped.left(); x < clipped.right(); ++x) {
                Pixel& target = m_pixels[offsetOf(x, y)];
                if (rules.allows(target, color)) {
                    target = color;
                }
            }
        }
    }

    /// Copia uma região para uma nova imagem do tamanho de "area". Partes da
    /// região fora desta imagem são preenchidas com "outside".
    [[nodiscard]] Image copy(const Rect& area, Pixel outside = Pixel { }) const
    {
        Image result(area.size(), outside);
        // Só a parte que intersecta esta imagem é copiada; o resto fica "outside".
        const Rect source = area.intersected(bounds());
        for (int y = source.top(); y < source.bottom(); ++y) {
            const auto rowBegin
                = m_pixels.begin() + static_cast<std::ptrdiff_t>(offsetOf(source.x, y));
            std::copy(rowBegin, rowBegin + source.width,
                result.m_pixels.begin()
                    + static_cast<std::ptrdiff_t>(result.offsetOf(source.x - area.x, y - area.y)));
        }
        return result;
    }

    /// Desenha "source" nesta imagem com o canto superior esquerdo em
    /// "destination", respeitando as regras de escrita. Partes fora dos
    /// limites são descartadas.
    void paste(const Image& source, Point destination, const Rules& rules = { })
    {
        // Região de destino efetivamente afetada (recortada aos nossos limites).
        const Rect target
            = Rect::fromPointAndSize(destination, source.size()).intersected(bounds());
        for (int y = target.top(); y < target.bottom(); ++y) {
            for (int x = target.left(); x < target.right(); ++x) {
                const Pixel incoming = source.pixel(x - destination.x, y - destination.y);
                Pixel& current = m_pixels[offsetOf(x, y)];
                if (rules.allows(current, incoming)) {
                    current = incoming;
                }
            }
        }
    }

    /// Nova imagem com outro tamanho; o conteúdo é mantido ancorado no canto
    /// superior esquerdo e as áreas novas recebem "fill".
    [[nodiscard]] Image resizedCanvas(Size newSize, Pixel fill = Pixel { }) const
    {
        Image result(newSize, fill);
        result.paste(*this, { 0, 0 });
        return result;
    }

    /// Conjunto das cores efetivamente usadas pelos pixels (só indexadas).
    [[nodiscard]] ColorSet usedColors() const
        requires std::same_as<Pixel, ColorIndex>
    {
        ColorSet used;
        for (const ColorIndex value : m_pixels) {
            used.set(value);
        }
        return used;
    }

    /// Substitui cada índice "i" por "table[i]" (remapeamento de paleta; só
    /// indexadas).
    void remap(const std::array<ColorIndex, 256>& table)
        requires std::same_as<Pixel, ColorIndex>
    {
        for (ColorIndex& value : m_pixels) {
            value = table[value];
        }
    }

    /// Acesso de leitura aos pixels (linha a linha, sem preenchimento).
    [[nodiscard]] std::span<const Pixel> data() const { return m_pixels; }

    /// Acesso de escrita aos pixels (para I/O). Tamanho = largura * altura.
    [[nodiscard]] std::span<Pixel> data() { return m_pixels; }

    friend bool operator==(const Image&, const Image&) = default;

private:
    /// Deslocamento linear do pixel (x, y) no vetor.
    [[nodiscard]] std::size_t offsetOf(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_size.width)
            + static_cast<std::size_t>(x);
    }

    Size m_size { };
    std::vector<Pixel> m_pixels;
};

/// Imagem indexada: cada pixel é um índice da paleta (1 byte).
using IndexedImage = Image<ColorIndex>;

/// Imagem em cores reais com transparência (4 bytes por pixel).
using RgbaImage = Image<Rgba>;

} // namespace spritenator::core
