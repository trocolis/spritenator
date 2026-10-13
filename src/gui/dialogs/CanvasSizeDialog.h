// =============================================================================
// CanvasSizeDialog.h
//
// Diálogo "Tamanho do canvas" (TFormCanvasSize no original): nova largura e
// altura, com a opção de redimensionar o conteúdo junto ("expansão/redução de
// acordo com o canvas"). Sem a opção, o conteúdo fica ancorado no canto
// superior esquerdo e as áreas novas recebem a cor de fundo.
// =============================================================================
#pragma once

#include "core/Geometry.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace spritenator::gui {

/// Escolha do novo tamanho do canvas.
class CanvasSizeDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CanvasSizeDialog(core::Size current, QWidget* parent = nullptr);

    [[nodiscard]] core::Size newSize() const;
    /// Verdadeiro se o conteúdo deve ser esticado/encolhido para o novo tamanho.
    [[nodiscard]] bool scaleContents() const;

private:
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QCheckBox* m_scale = nullptr;
};

} // namespace spritenator::gui
