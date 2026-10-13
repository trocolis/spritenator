// =============================================================================
// AnyImage.h
//
// Imagem de qualquer modo de cor: indexada OU RGBA. É o tipo usado pelo
// documento, pela área de desenho e pelos comandos de desfazer, que não
// precisam saber de antemão com qual tipo de pixel estão lidando.
//
// Por ser um std::variant, o compilador obriga a tratar os dois casos sempre
// que o tipo do pixel importa (std::visit); as funções abaixo cobrem as
// operações comuns que valem para qualquer modo.
// =============================================================================
#pragma once

#include "Image.h"
#include "Palette.h"

#include <variant>

namespace spritenator::core {

/// Modo de cor de uma imagem.
enum class ColorMode {
    Indexed, ///< pixels são índices de paleta (2, 16 ou 256 cores)
    Rgba, ///< pixels são cores reais com transparência (32 bits)
};

/// Imagem indexada ou RGBA.
using AnyImage = std::variant<IndexedImage, RgbaImage>;

/// Modo de cor da imagem.
[[nodiscard]] inline ColorMode modeOf(const AnyImage& image)
{
    return std::holds_alternative<RgbaImage>(image) ? ColorMode::Rgba : ColorMode::Indexed;
}

/// Tamanho da imagem, qualquer que seja o modo.
[[nodiscard]] inline Size sizeOf(const AnyImage& image)
{
    return std::visit([](const auto& concrete) { return concrete.size(); }, image);
}

/// Retângulo (0, 0, largura, altura) da imagem.
[[nodiscard]] inline Rect boundsOf(const AnyImage& image)
{
    return Rect::fromPointAndSize({ 0, 0 }, sizeOf(image));
}

/// Imagem vazia (0x0) no modo informado.
[[nodiscard]] inline AnyImage emptyImage(ColorMode mode)
{
    return mode == ColorMode::Rgba ? AnyImage(RgbaImage { }) : AnyImage(IndexedImage { });
}

/// Nova imagem do modo informado, toda com o pixel "vazio" do modo
/// (índice 0 ou RGBA transparente).
[[nodiscard]] inline AnyImage blankImage(ColorMode mode, Size size)
{
    return mode == ColorMode::Rgba ? AnyImage(RgbaImage(size)) : AnyImage(IndexedImage(size));
}

/// Copia uma região (partes fora da imagem recebem o pixel "vazio").
[[nodiscard]] inline AnyImage copyRegion(const AnyImage& image, const Rect& area)
{
    return std::visit([&](const auto& concrete) -> AnyImage { return concrete.copy(area); }, image);
}

/// Cola "source" em "target" na posição informada, sem regras de escrita.
/// Os dois precisam ser do mesmo modo (lança std::invalid_argument se não).
void pasteImage(AnyImage& target, const AnyImage& source, Point destination);

/// Converte a imagem para o modo informado, usando a paleta quando um dos
/// lados é indexado (ver ColorConversion). Mesma cópia se o modo já bate.
[[nodiscard]] AnyImage convertImage(const AnyImage& image, ColorMode mode, const Palette& palette);

} // namespace spritenator::core
