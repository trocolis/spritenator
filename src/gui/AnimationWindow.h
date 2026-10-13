// =============================================================================
// AnimationWindow.h
//
// Janela de prévia de animação (F2). O usuário monta uma lista de quadros a
// partir das células do canvas e a prévia os exibe em laço, lendo SEMPRE o
// canvas atual do documento ativo — editar o desenho atualiza a animação ao
// vivo.
//
// Como adicionar quadros: traga uma célula para a área de desenho (botão
// direito no canvas) e clique em "Add Cell", ou em "Walk Cycle" para gerar
// a sequência 1-2-3-2 do RPG Maker a partir dessa célula.
// =============================================================================
#pragma once

#include "core/Animation.h"

#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;

namespace spritenator::gui {

class EditorSession;

/// Área que desenha o quadro atual da animação.
class AnimationPreview final : public QWidget {
    Q_OBJECT

public:
    AnimationPreview(EditorSession* session, const core::Animation* animation, QWidget* parent);

    void setFramePosition(std::size_t position);
    [[nodiscard]] std::size_t framePosition() const { return m_position; }
    void setScale(int scale);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    EditorSession* m_session;
    const core::Animation* m_animation;
    std::size_t m_position = 0;
    int m_scale = 2;
};

/// Janela de animação (não modal).
class AnimationWindow final : public QWidget {
    Q_OBJECT

public:
    explicit AnimationWindow(EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] const core::Animation& animation() const { return m_animation; }

    /// Adiciona a última célula transferida para a área de desenho.
    void addLastCell();
    /// Adiciona o ciclo de caminhada a partir da última célula transferida.
    void addWalkCycle();

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Reaplica todos os textos no idioma atual.
    void retranslate();
    /// Texto exibido na lista para o quadro da linha informada.
    [[nodiscard]] QString frameLabel(int row) const;

    void removeSelectedFrame();
    void clearFrames();
    void advanceFrame();
    /// Reconstrói a lista exibida e (re)inicia a reprodução se houver quadros.
    void refreshFrames();
    void updatePlayback();

    EditorSession* m_session;
    core::Animation m_animation;

    AnimationPreview* m_preview = nullptr;
    QListWidget* m_frameList = nullptr;
    QSpinBox* m_wait = nullptr;
    QComboBox* m_scale = nullptr;
    QPushButton* m_addCell = nullptr;
    QPushButton* m_walkCycle = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QGroupBox* m_framesBox = nullptr;
    QLabel* m_waitLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QTimer* m_timer = nullptr;
};

} // namespace spritenator::gui
