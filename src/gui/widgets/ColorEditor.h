// =============================================================================
// ColorEditor.h
//
// Painel de ajuste de cores: os mostradores das cores primária (desenho) e
// secundária (fundo) e as barras de canal (as ScrollBars do Integrator do
// original).
//
// As barras editam o mostrador ATIVO — clicar num mostrador o torna ativo:
//   - modo indexado: barras R, G, B editam a entrada da paleta da cor ativa
//     (arrastar uma barra gera um único passo de desfazer);
//   - modo RGBA: barras R, G, B e A editam a cor RGBA ativa diretamente.
// =============================================================================
#pragma once

#include "gui/EditorSession.h"

#include <QWidget>

#include <array>

class QLabel;
class QSlider;
class QSpinBox;

namespace spritenator::gui {

class ColorSwatch;

/// Mostradores de cor + barras R, G, B (e A no modo RGBA).
class ColorEditor final : public QWidget {
    Q_OBJECT

public:
    /// Canais editáveis (A só aparece no modo RGBA).
    enum Channel { Red, Green, Blue, Alpha, ChannelCount };

    explicit ColorEditor(EditorSession* session, QWidget* parent = nullptr);

    /// Mostradores e barras (expostos para testes de interação).
    [[nodiscard]] ColorSwatch* swatch(ColorRole role) const;
    [[nodiscard]] QSlider* slider(Channel channel) const;

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Reaplica dicas e rótulos no idioma atual.
    void retranslate();

    /// Atualiza mostradores e barras a partir do estado da sessão.
    void refresh();

    /// Aplica o valor de um canal à cor ativa (paleta ou RGBA, conforme o modo).
    void applyChannel(Channel channel, int value);

    EditorSession* m_session;
    ColorSwatch* m_primarySwatch = nullptr;
    ColorSwatch* m_secondarySwatch = nullptr;
    std::array<QSlider*, ChannelCount> m_sliders { };
    std::array<QSpinBox*, ChannelCount> m_spins { };
    std::array<QLabel*, ChannelCount> m_channelLabels { };
    bool m_updating = false; ///< evita laços sinal -> atualização -> sinal
};

} // namespace spritenator::gui
