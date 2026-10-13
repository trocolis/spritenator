// =============================================================================
// BmpWriter.h  (uso interno da camada io)
//
// Codificador de BMP indexado (1, 4 ou 8 bits por pixel). Foi escrito à mão
// porque o Qt sempre grava imagens indexadas com 8 bits; o original gravava
// imagens de 16 cores em 4 bits, e alguns programas (como o RPG Maker) leem
// esses arquivos.
// =============================================================================
#pragma once

#include "core/IndexedImage.h"
#include "core/Palette.h"

#include <QByteArray>

namespace spritenator::io::detail {

/// Codifica a imagem como um arquivo BMP completo (cabeçalhos + paleta +
/// pixels). A profundidade (1, 4 ou 8 bits) vem da paleta.
[[nodiscard]] QByteArray encodeBmp(const core::IndexedImage& image, const core::Palette& palette);

} // namespace spritenator::io::detail
