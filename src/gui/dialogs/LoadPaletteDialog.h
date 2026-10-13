// =============================================================================
// LoadPaletteDialog.h
//
// Diálogo "Carregar paleta" (TFormLoadPalette no original):
//   - arquivo de paleta (ou imagem) com prévia das cores lidas;
//   - posição de leitura: tudo, ou "N cores a partir de X na posição Y";
//   - como tratar os pixels (manter índices, cor aproximada, difusão de erro);
//   - não usar as 20 cores do sistema; tratar o índice 0 como transparente.
// =============================================================================
#pragma once

#include "core/Palette.h"
#include "core/PaletteOps.h"

#include <QDialog>

#include <vector>

class QCheckBox;
class QLabel;
class QRadioButton;
class QSpinBox;

namespace spritenator::gui {

class PaletteGrid;

/// Escolha e configuração de uma paleta a carregar.
class LoadPaletteDialog final : public QDialog {
    Q_OBJECT

public:
    /// "current" é a paleta atual do documento (base da mesclagem).
    LoadPaletteDialog(
        const core::Palette& current, const QString& startDirectory, QWidget* parent = nullptr);

    /// Paleta resultante (atual mesclada com as cores lidas).
    [[nodiscard]] core::Palette resultPalette() const;
    /// Opções de remapeamento dos pixels.
    [[nodiscard]] core::PaletteApplyOptions applyOptions() const;
    /// Arquivo escolhido.
    [[nodiscard]] const QString& filePath() const { return m_filePath; }

private:
    /// Abre o seletor de arquivos e lê as cores.
    void browse();
    /// Atualiza a prévia e habilita/desabilita controles.
    void refresh();

    core::Palette m_current;
    std::vector<core::Rgb> m_loaded;
    QString m_filePath;
    QString m_startDirectory;

    QLabel* m_fileLabel = nullptr;
    PaletteGrid* m_loadedGrid = nullptr;
    PaletteGrid* m_resultGrid = nullptr;
    QRadioButton* m_readAll = nullptr;
    QRadioButton* m_readRange = nullptr;
    QSpinBox* m_from = nullptr;
    QSpinBox* m_count = nullptr;
    QSpinBox* m_destination = nullptr;
    QRadioButton* m_keepIndices = nullptr;
    QRadioButton* m_nearest = nullptr;
    QRadioButton* m_dither = nullptr;
    QCheckBox* m_avoidSystem = nullptr;
    QCheckBox* m_transparentZero = nullptr;
};

} // namespace spritenator::gui
