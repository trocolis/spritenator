// =============================================================================
// Transform.h
//
// Transformações geométricas sobre imagens indexadas (os 12 botões abaixo da
// área de desenho do original). Todas são funções PURAS: recebem uma imagem
// e devolvem uma nova, sem efeitos colaterais — fáceis de testar e de
// combinar com o sistema de desfazer.
//
// Exceto "scaled", todas preservam o tamanho da imagem: rotações acontecem em
// torno do centro e o que sai da área é descartado, com as lacunas
// preenchidas pela cor "fill" (normalmente a cor de fundo).
// =============================================================================
#pragma once

#include "IndexedImage.h"

namespace spritenator::core {

/// Desloca o conteúdo com "dx"/"dy" pixels, de forma circular: o que sai por
/// um lado volta pelo outro (útil para ajustar a posição de um quadro).
[[nodiscard]] IndexedImage shifted(const IndexedImage& image, int dx, int dy);

/// Espelha da esquerda para a direita.
[[nodiscard]] IndexedImage flippedHorizontally(const IndexedImage& image);

/// Espelha de cima para baixo.
[[nodiscard]] IndexedImage flippedVertically(const IndexedImage& image);

/// Simetria horizontal: a metade esquerda é copiada, espelhada, sobre a
/// metade direita (personagens vistos de frente ficam simétricos).
[[nodiscard]] IndexedImage mirroredHorizontally(const IndexedImage& image);

/// Simetria vertical: a metade de cima é copiada, espelhada, sobre a de baixo.
[[nodiscard]] IndexedImage mirroredVertically(const IndexedImage& image);

/// Rotação de 90 graus em torno do centro (horária ou anti-horária).
[[nodiscard]] IndexedImage rotated90(const IndexedImage& image, bool clockwise, ColorIndex fill);

/// Rotação livre (graus, sentido horário) por vizinho mais próximo.
[[nodiscard]] IndexedImage rotated(const IndexedImage& image, double degrees, ColorIndex fill);

/// Redimensiona o conteúdo para o novo tamanho (vizinho mais próximo).
[[nodiscard]] IndexedImage scaled(const IndexedImage& image, Size newSize);

/// "Preparar sombra": todo pixel diferente de "background" vira "shadow".
[[nodiscard]] IndexedImage shadowed(
    const IndexedImage& image, ColorIndex background, ColorIndex shadow);

} // namespace spritenator::core
