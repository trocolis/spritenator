// =============================================================================
// TimelinePanel.h
//
// Linha do tempo (painel inferior), com duas barras:
//   - CENAS: miniaturas em ordem; nova, duplicar, apagar, mover; duração da
//     cena atual; reproduzir/pausar (o editor mostra a animação em laço) e
//     papel de seda;
//   - CAMADAS: da de cima para a de baixo; caixa = visível; clique duplo
//     renomeia; nova, duplicar, apagar, subir, descer, mesclar com a de
//     baixo; opacidade (só em RGBA).
//
// Age sempre sobre o documento ativo da sessão. Os comandos são as ações do
// catálogo (com atalhos configuráveis e também no menu Animação).
// =============================================================================
#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QSlider;
class QSpinBox;
class QTimer;

namespace spritenator::gui {

class ActionCatalog;
class Document;
class EditorSession;

/// Painel inferior com as barras de cenas e de camadas.
class TimelinePanel final : public QWidget {
    Q_OBJECT

public:
    TimelinePanel(EditorSession* session, ActionCatalog* actions, QWidget* parent = nullptr);

    /// Habilita as ações conforme o documento ativo (chamado também pela
    /// janela principal depois de atualizar as demais ações).
    void updateActions();

    /// Verdadeiro durante a reprodução.
    [[nodiscard]] bool isPlaying() const;

    // --- Expostos para testes ---
    [[nodiscard]] QListWidget* frameList() const { return m_frames; }
    [[nodiscard]] QListWidget* layerList() const { return m_layers; }
    [[nodiscard]] QSpinBox* durationBox() const { return m_duration; }
    [[nodiscard]] QSlider* opacitySlider() const { return m_opacity; }

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslate();
    /// Passa a acompanhar o documento ativo.
    void attach(Document* document);
    /// Reconstrói as duas listas a partir do documento.
    void rebuild();
    /// Seleções, duração e opacidade refletem a cena/camada atual.
    void syncCurrent();
    /// Atualiza a miniatura de uma cena.
    void refreshThumbnail(int frame);
    /// Miniaturas pendentes (o desenho na cena atual é agrupado).
    void refreshPendingThumbnails();

    /// Antes de mudar de cena/camada ou de estrutura: aplica a transformação
    /// da seleção e interrompe a ferramenta.
    void finishPendingWork();
    void playNextFrame();
    void setPlaying(bool playing);

    /// Linha da lista de camadas <-> índice da camada (a de cima vem antes).
    [[nodiscard]] int rowForLayer(int layer) const;
    [[nodiscard]] int layerForRow(int row) const;

    EditorSession* m_session;
    ActionCatalog* m_actions;
    QPointer<Document> m_document;
    bool m_updating = false; ///< evita realimentação lista <-> documento

    QLabel* m_framesLabel = nullptr;
    QLabel* m_layersLabel = nullptr;
    QLabel* m_durationLabel = nullptr;
    QLabel* m_opacityLabel = nullptr;
    QListWidget* m_frames = nullptr;
    QListWidget* m_layers = nullptr;
    QSpinBox* m_duration = nullptr;
    QSlider* m_opacity = nullptr;
    QTimer* m_playTimer = nullptr;
    QTimer* m_thumbnailTimer = nullptr;
};

} // namespace spritenator::gui
