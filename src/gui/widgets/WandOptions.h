// =============================================================================
// WandOptions.h
//
// Barra de opções da varinha mágica, exibida abaixo das ferramentas apenas
// quando a varinha está ativa: tolerância de cor (0 a 100%) e "Contígua"
// (só a região conectada ao ponto clicado). O estado vive na EditorSession.
// =============================================================================
#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QSlider;
class QSpinBox;

namespace spritenator::gui {

class EditorSession;

/// Opções da varinha mágica.
class WandOptions final : public QWidget {
    Q_OBJECT

public:
    explicit WandOptions(EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] QSlider* toleranceSlider() const { return m_slider; }
    [[nodiscard]] QCheckBox* contiguousBox() const { return m_contiguous; }

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslate();
    /// Reflete a sessão (valores e visibilidade).
    void syncFromSession();

    EditorSession* m_session;
    QLabel* m_label = nullptr;
    QSlider* m_slider = nullptr;
    QSpinBox* m_spin = nullptr;
    QCheckBox* m_contiguous = nullptr;
};

} // namespace spritenator::gui
