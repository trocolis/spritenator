// =============================================================================
// GridDialog.h
//
// Diálogo "Grades": espaçamento da grade principal (padrão 1x1, a grade de
// pixels) e da sub-grade (o "tamanho do personagem" do CM1999, ex.: 24x32).
// =============================================================================
#pragma once

#include "core/GridLayout.h"

#include <QDialog>

class QComboBox;

namespace spritenator::gui {

/// Edição dos espaçamentos das grades.
class GridDialog final : public QDialog {
    Q_OBJECT

public:
    explicit GridDialog(const core::GridLayout& layout, QWidget* parent = nullptr);

    /// Configuração escolhida (válida após accept()).
    [[nodiscard]] core::GridLayout layout() const;

private:
    /// Valida os campos antes de fechar com OK.
    void acceptIfValid();

    QComboBox* m_mainWidth = nullptr;
    QComboBox* m_mainHeight = nullptr;
    QComboBox* m_subWidth = nullptr;
    QComboBox* m_subHeight = nullptr;
};

} // namespace spritenator::gui
