// =============================================================================
// ImageRendering.h
//
// Conversões entre as imagens do núcleo e QImage para exibição:
//   - indexada -> Format_Indexed8: trocar a paleta é barato (só a tabela de
//     cores muda);
//   - RGBA -> Format_ARGB32: as views desenham um fundo xadrez por baixo,
//     para que a transparência fique visível.
// Atualizar uma região copia somente os pixels afetados.
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/Image.h"
#include "core/Palette.h"

#include <QColor>
#include <QImage>
#include <QList>

class QPainter;
class QRect;

namespace spritenator::gui {

/// Tabela de cores do Qt correspondente à paleta (tamanho = palette.size()).
[[nodiscard]] QList<QRgb> colorTableOf(const core::Palette& palette);

/// Cria um QImage indexado com os pixels e a paleta informados.
[[nodiscard]] QImage renderIndexed(const core::IndexedImage& image, const core::Palette& palette);

/// Copia para "target" apenas a região alterada da imagem indexada. O QImage
/// precisa ter o mesmo tamanho da imagem.
void updateRenderedRegion(QImage& target, const core::IndexedImage& image, const core::Rect& area);

/// Cria um QImage para qualquer modo (a paleta só é usada se indexada).
[[nodiscard]] QImage renderImage(const core::AnyImage& image, const core::Palette& palette);

/// Atualiza a região alterada de um QImage criado por renderImage.
void updateRenderedRegion(QImage& target, const core::AnyImage& image, const core::Rect& area);

/// Pinta o fundo xadrez cinza-claro/branco que indica transparência.
/// "cellSize" é o lado de cada casa, em pixels de tela.
void paintTransparencyGrid(QPainter& painter, const QRect& area, int cellSize = 8);

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

[[nodiscard]] inline QColor toQColor(const core::Rgba& color)
{
    return { color.red, color.green, color.blue, color.alpha };
}

[[nodiscard]] inline core::Rgba toRgba(const QColor& color)
{
    return { static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
        static_cast<std::uint8_t>(color.blue()), static_cast<std::uint8_t>(color.alpha()) };
}

} // namespace spritenator::gui
