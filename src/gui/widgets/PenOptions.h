// =============================================================================
// PenOptions.h
//
// Barra de opções da ponta, exibida abaixo das ferramentas com o lápis ou uma
// figura (reta, retângulos, elipse) ativa: formato da ponta (quadrada ou
// redonda) e tamanho (1 a 64 pixels). O estado vive na EditorSession.
// =============================================================================
#pragma once

#include <QWidget>

class QLabel;
class QSlider;
class QSpinBox;
class QToolButton;

namespace spritenator::gui {

class EditorSession;

/// Opções da ponta do lápis.
class PenOptions final : public QWidget {
    Q_OBJECT

public:
    explicit PenOptions(EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] QToolButton* squareButton() const { return m_square; }
    [[nodiscard]] QToolButton* roundButton() const { return m_round; }
    [[nodiscard]] QSpinBox* sizeBox() const { return m_spin; }

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslate();
    /// Reflete a sessão (valores e visibilidade).
    void syncFromSession();

    EditorSession* m_session;
    QLabel* m_tipLabel = nullptr;
    QToolButton* m_square = nullptr;
    QToolButton* m_round = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QSlider* m_slider = nullptr;
    QSpinBox* m_spin = nullptr;
};

} // namespace spritenator::gui
