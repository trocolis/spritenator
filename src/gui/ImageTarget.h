// =============================================================================
// ImageTarget.h
//
// Interface comum a tudo que contém uma imagem indexada editável: o canvas
// de um Document e o buffer da área de desenho (DrawBuffer). Os comandos de
// undo trabalham com esta interface e, por isso, servem para os dois.
// =============================================================================
#pragma once

#include "core/IndexedImage.h"

namespace spritenator::gui {

/// Alvo de edição de imagem (não é dono de nada; apenas expõe operações).
class ImageTarget {
public:
    virtual ~ImageTarget() = default;

    /// Imagem atual.
    [[nodiscard]] virtual const core::IndexedImage& targetImage() const = 0;

    /// Substitui uma região pelo conteúdo informado (sem regras de escrita).
    virtual void applyRegion(core::Point origin, const core::IndexedImage& pixels) = 0;

    /// Substitui a imagem inteira (pode mudar de tamanho).
    virtual void applyImage(const core::IndexedImage& image) = 0;

protected:
    ImageTarget() = default;
    ImageTarget(const ImageTarget&) = default;
    ImageTarget& operator=(const ImageTarget&) = default;
};

} // namespace spritenator::gui
