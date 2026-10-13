// =============================================================================
// Transform.h
//
// Transformações geométricas sobre imagens (os 12 botões abaixo da área de
// desenho do original). São templates: valem para imagens indexadas e RGBA
// (instanciadas explicitamente para os dois tipos em Transform.cpp). Todas são funções PURAS:
// recebem uma imagem e devolvem uma nova, sem efeitos colaterais — fáceis de testar e de combinar
// com o sistema de desfazer.
//
// Exceto "scaled", todas preservam o tamanho da imagem: rotações acontecem em
// torno do centro e o que sai da área é descartado, com as lacunas
// preenchidas pela cor "fill" (normalmente a cor de fundo).
// =============================================================================
#pragma once

#include "Image.h"

#include <type_traits>

namespace spritenator::core {

/// Desloca o conteúdo com "dx"/"dy" pixels, de forma circular: o que sai por
/// um lado volta pelo outro (útil para ajustar a posição de um quadro).
template <typename Pixel>
[[nodiscard]] Image<Pixel> shifted(const Image<Pixel>& image, int dx, int dy);

/// Espelha da esquerda para a direita.
template <typename Pixel> [[nodiscard]] Image<Pixel> flippedHorizontally(const Image<Pixel>& image);

/// Espelha de cima para baixo.
template <typename Pixel> [[nodiscard]] Image<Pixel> flippedVertically(const Image<Pixel>& image);

/// Simetria horizontal: a metade esquerda é copiada, espelhada, sobre a
/// metade direita (personagens vistos de frente ficam simétricos).
template <typename Pixel>
[[nodiscard]] Image<Pixel> mirroredHorizontally(const Image<Pixel>& image);

/// Simetria vertical: a metade de cima é copiada, espelhada, sobre a de baixo.
template <typename Pixel> [[nodiscard]] Image<Pixel> mirroredVertically(const Image<Pixel>& image);

/// Rotação de 90 graus em torno do centro (horária ou anti-horária). O
/// tipo do preenchimento não participa da dedução do template: assim
/// "rotated90(imagem, true, 0)" funciona com um literal.
template <typename Pixel>
[[nodiscard]] Image<Pixel> rotated90(
    const Image<Pixel>& image, bool clockwise, std::type_identity_t<Pixel> fill);

/// Rotação livre (graus, sentido horário) por vizinho mais próximo.
template <typename Pixel>
[[nodiscard]] Image<Pixel> rotated(
    const Image<Pixel>& image, double degrees, std::type_identity_t<Pixel> fill);

/// Redimensiona o conteúdo para o novo tamanho (vizinho mais próximo).
template <typename Pixel>
[[nodiscard]] Image<Pixel> scaled(const Image<Pixel>& image, Size newSize);

/// "Preparar sombra" (indexada): todo pixel diferente de "background" vira
/// "shadow".
[[nodiscard]] IndexedImage shadowed(
    const IndexedImage& image, ColorIndex background, ColorIndex shadow);

/// "Preparar sombra" (RGBA): todo pixel não transparente recebe as
/// componentes de cor de "shadow", mantendo o próprio alfa (o contorno
/// semitransparente continua suave).
[[nodiscard]] RgbaImage shadowed(const RgbaImage& image, const Rgba& shadow);

} // namespace spritenator::core
