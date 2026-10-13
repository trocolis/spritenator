// =============================================================================
// PaletteEditDialog.h
//
// Editor de paleta (TFormEditPalette no original). Trabalha sobre CÓPIAS da
// paleta e do canvas; nada muda no documento até o OK, quando a MainWindow
// aplica o resultado como um único passo de desfazer.
//
// Operações que reorganizam a paleta (trocar cores, remover não usadas)
// também remapeiam os pixels, para que a imagem mantenha a aparência.
// =============================================================================
#pragma once

#include "core/Image.h"
#include "core/Palette.h"
#include "core/PaletteOps.h"

#include <QDialog>

#include <vector>

class QLabel;

namespace spritenator::gui {

class PaletteGrid;

/// Edição interativa de uma paleta.
class PaletteEditDialog final : public QDialog {
    Q_OBJECT

public:
    PaletteEditDialog(
        const core::Palette& palette, const core::IndexedImage& canvas, QWidget* parent = nullptr);

    /// Resultados (válidos após accept()).
    [[nodiscard]] const core::Palette& palette() const { return m_palette; }
    [[nodiscard]] const core::IndexedImage& canvas() const { return m_canvas; }
    /// Remapeamento acumulado aplicado aos pixels (para atualizar também o
    /// buffer da área de desenho).
    [[nodiscard]] const core::RemapTable& remap() const { return m_remap; }

private:
    void editColor(int index);
    void copyColors();
    void pasteColors();
    void swapColors();
    void makeGradient();
    void removeUnused();
    void convertTo15Bit();
    void resetToDefault();

    /// Aplica uma tabela de remapeamento ao canvas e acumula no total.
    void applyRemap(const core::RemapTable& table);
    /// Atualiza a grade e o texto informativo.
    void refresh();

    core::Palette m_palette;
    core::IndexedImage m_canvas;
    core::RemapTable m_remap;
    std::vector<core::Rgb> m_copiedColors;

    PaletteGrid* m_grid = nullptr;
    QLabel* m_info = nullptr;
};

} // namespace spritenator::gui
