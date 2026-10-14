// =============================================================================
// NewImageDialog.h
//
// Diálogo "Nova imagem": largura, altura e cores, em duas seções — RGBA, ou
// Indexada com 256, 16 ou 2 cores (e a opção de copiar a paleta da imagem
// ativa). Em vez de predefinições, oferece o HISTÓRICO das combinações usadas
// antes (tamanho + modo de cor) e abre com a mais recente.
// =============================================================================
#pragma once

#include "gui/AppSettings.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QRadioButton;
class QSpinBox;
class QWidget;

namespace spritenator::gui {

/// Diálogo de criação de uma imagem nova.
class NewImageDialog final : public QDialog {
    Q_OBJECT

public:
    /// "canCopyPalette" habilita a opção de copiar a paleta atual;
    /// "history" são as escolhas anteriores, da mais recente à mais antiga.
    NewImageDialog(
        bool canCopyPalette, const QList<NewImageEntry>& history, QWidget* parent = nullptr);

    /// Escolha atual (tamanho + modo de cor), a ser gravada no histórico.
    [[nodiscard]] NewImageEntry entry() const;

    [[nodiscard]] core::Size imageSize() const;
    [[nodiscard]] core::ColorMode colorMode() const;
    /// Profundidade escolhida (só tem sentido no modo indexado).
    [[nodiscard]] core::ColorDepth colorDepth() const;
    [[nodiscard]] bool copyCurrentPalette() const;

    /// Combo do histórico (exposto para testes).
    [[nodiscard]] QComboBox* historyCombo() const { return m_history; }

    /// Texto de uma entrada do histórico, ex.: "72 × 128 · RGBA".
    [[nodiscard]] static QString entryLabel(const NewImageEntry& entry);

private:
    /// Preenche tamanho e cores com uma entrada.
    void applyEntry(const NewImageEntry& entry);
    /// Habilita as opções da seção Indexada conforme o modo escolhido.
    void updateIndexedOptions();
    /// Mantém o combo coerente: mostra a entrada igual à escolha atual, ou
    /// nenhuma (texto de orientação) se os valores foram editados.
    void syncHistorySelection();

    QList<NewImageEntry> m_entries;
    bool m_canCopyPalette = false;

    QComboBox* m_history = nullptr;
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QRadioButton* m_rgba = nullptr;
    QRadioButton* m_indexed = nullptr;
    QWidget* m_indexedOptions = nullptr;
    QRadioButton* m_colors256 = nullptr;
    QRadioButton* m_colors16 = nullptr;
    QRadioButton* m_colors2 = nullptr;
    QCheckBox* m_copyPalette = nullptr;
};

} // namespace spritenator::gui
