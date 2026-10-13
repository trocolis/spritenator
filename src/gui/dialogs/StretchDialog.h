// =============================================================================
// StretchDialog.h
//
// Diálogo "Esticar" (TFormStretch no original): novo tamanho do conteúdo da
// região de trabalho, com opção de manter a proporção.
// =============================================================================
#pragma once

#include "core/Geometry.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace spritenator::gui {

/// Escolha do novo tamanho para "Esticar".
class StretchDialog final : public QDialog {
    Q_OBJECT

public:
    /// "current" é o tamanho atual (valor inicial e base da proporção).
    explicit StretchDialog(core::Size current, QWidget* parent = nullptr);

    [[nodiscard]] core::Size newSize() const;

private:
    core::Size m_original;
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QCheckBox* m_keepAspect = nullptr;
    bool m_updating = false;
};

} // namespace spritenator::gui
