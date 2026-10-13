// =============================================================================
// CharacterSizeDialog.h
//
// Diálogo "Tamanho do personagem" (TFormCharaSize no original): tamanho da
// célula, espaçamento da sub-grade e opções de exibição/navegação.
// =============================================================================
#pragma once

#include "core/CharacterLayout.h"

#include <QDialog>

class QCheckBox;
class QComboBox;

namespace spritenator::gui {

/// Edição da grade de personagens.
class CharacterSizeDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CharacterSizeDialog(const core::CharacterLayout& layout, QWidget* parent = nullptr);

    /// Configuração escolhida (válida após accept()).
    [[nodiscard]] core::CharacterLayout layout() const;

private:
    /// Valida os campos antes de fechar com OK.
    void acceptIfValid();

    QComboBox* m_cellWidth = nullptr;
    QComboBox* m_cellHeight = nullptr;
    QComboBox* m_subGridWidth = nullptr;
    QComboBox* m_subGridHeight = nullptr;
    QCheckBox* m_doubleCanvas = nullptr;
    QCheckBox* m_halfCellSteps = nullptr;
    QCheckBox* m_keepMouseCursor = nullptr;
};

} // namespace spritenator::gui
