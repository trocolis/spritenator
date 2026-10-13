// =============================================================================
// ImageRendering.h
//
// Conversões entre o modelo indexado do núcleo e QImage para exibição. O
// QImage resultante é Format_Indexed8: trocar a paleta é barato (apenas a
// tabela de cores muda) e atualizar uma região copia somente as linhas
// afetadas.
// =============================================================================
#pragma once

#include "core/Image.h"
#include "core/Palette.h"

#include <QColor>
#include <QImage>
#include <QList>

namespace spritenator::gui {

/// Tabela de cores do Qt correspondente à paleta (tamanho = palette.size()).
[[nodiscard]] QList<QRgb> colorTableOf(const core::Palette& palette);

/// Cria um QImage indexado com os pixels e a paleta informados.
[[nodiscard]] QImage renderIndexed(const core::IndexedImage& image, const core::Palette& palette);

/// Copia para "target" apenas a região alterada da imagem indexada. O QImage
/// precisa ter o mesmo tamanho da imagem.
void updateRenderedRegion(QImage& target, const core::IndexedImage& image, const core::Rect& area);

/// Conversões simples de cor entre núcleo e Qt.
[[nodiscard]] inline QColor toQColor(const core::Rgb& color)
{
    return { color.red, color.green, color.blue };
}

[[nodiscard]] inline core::Rgb toRgb(const QColor& color)
{
    return { static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
        static_cast<std::uint8_t>(color.blue()) };
}

} // namespace spritenator::gui
