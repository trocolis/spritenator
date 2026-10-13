// =============================================================================
// TextInputDialog.h
//
// Diálogo "Entrada de texto" (TFormInputText no original): fonte, tamanho,
// estilos e o texto a desenhar, com prévia ampliada do resultado em pixels.
// =============================================================================
#pragma once

#include <QDialog>
#include <QFont>

class QCheckBox;
class QFontComboBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;

namespace spritenator::gui {

/// Escolha de texto e fonte para desenhar na área de desenho.
class TextInputDialog final : public QDialog {
    Q_OBJECT

public:
    explicit TextInputDialog(QWidget* parent = nullptr);

    [[nodiscard]] QString text() const;
    [[nodiscard]] QFont selectedFont() const;

private:
    /// Atualiza a prévia (texto rasterizado sem suavização).
    void updatePreview();

    QFontComboBox* m_fontName = nullptr;
    QSpinBox* m_size = nullptr;
    QCheckBox* m_bold = nullptr;
    QCheckBox* m_italic = nullptr;
    QCheckBox* m_underline = nullptr;
    QCheckBox* m_strikeOut = nullptr;
    QPlainTextEdit* m_text = nullptr;
    QLabel* m_preview = nullptr;
};

} // namespace spritenator::gui
