// =============================================================================
// ColorEditor.h
//
// Painel com as amostras das cores de desenho/fundo e os controles R, G, B
// que editam a cor de desenho na paleta (as três ScrollBars do Integrator do
// original). Arrastar um controle gera um único passo de desfazer.
// =============================================================================
#pragma once

#include <QWidget>

#include <array>

class QLabel;
class QSlider;
class QSpinBox;

namespace spritenator::gui {

class EditorSession;

/// Amostras de cor + edição RGB da cor de desenho.
class ColorEditor final : public QWidget {
    Q_OBJECT

public:
    explicit ColorEditor(EditorSession* session, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Reaplica dicas e rótulos no idioma atual.
    void retranslate();

    /// Atualiza amostras e controles a partir do estado da sessão.
    void refresh();

    /// Aplica o valor de um canal (0 = R, 1 = G, 2 = B) à paleta.
    void applyChannel(int channel, int value);

    EditorSession* m_session;
    QLabel* m_drawSwatch = nullptr;
    QLabel* m_backgroundSwatch = nullptr;
    std::array<QSlider*, 3> m_sliders { };
    std::array<QSpinBox*, 3> m_spins { };
    std::array<QLabel*, 3> m_channelLabels { };
    bool m_updating = false; ///< evita laços sinal -> atualização -> sinal
};

} // namespace spritenator::gui
