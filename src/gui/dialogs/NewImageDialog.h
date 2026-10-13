// =============================================================================
// NewImageDialog.h
//
// Diálogo "Novo" (TFormFileNew no original): largura, altura, número de
// cores e a opção de copiar a paleta da imagem ativa. Acrescenta predefinições
// de tamanho do RPG Maker 2000 citadas no manual original.
// =============================================================================
#pragma once

#include "core/Geometry.h"
#include "core/Palette.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QRadioButton;
class QSpinBox;

namespace spritenator::gui {

/// Diálogo de criação de uma imagem nova.
class NewImageDialog final : public QDialog {
    Q_OBJECT

public:
    /// "canCopyPalette" habilita a opção de copiar a paleta atual.
    NewImageDialog(bool canCopyPalette, QWidget* parent = nullptr);

    [[nodiscard]] core::Size imageSize() const;
    [[nodiscard]] core::ColorDepth colorDepth() const;
    [[nodiscard]] bool copyCurrentPalette() const;

private:
    /// Aplica a predefinição escolhida no combo às caixas de tamanho.
    void applyPreset(int index);

    QComboBox* m_presets = nullptr;
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QRadioButton* m_colors256 = nullptr;
    QRadioButton* m_colors16 = nullptr;
    QCheckBox* m_copyPalette = nullptr;
};

} // namespace spritenator::gui
