// =============================================================================
// TimelinePanel.h
//
// Linha do tempo (painel inferior), unificada como no Pixelorama:
//
//   [camadas: nova, apagar, subir, descer, duplicar, mesclar]   [cenas: nova,
//   apagar, duplicar, mover | primeira, anterior, reproduzir, próxima,
//   última | papel de seda, vai e volta, FPS]
//   ┌ olho │ cadeado │ Opacidade ┬ 1  2  3 ... (cenas)
//   │  👁  │   🔒   │ Camada 2  ┤ ▢  ▣  ▢     (células)
//   │  👁  │   🔒   │ Camada 1  ┤ ▢  ▢  ▢
//
// A grade (TimelineGrid) só desenha e avisa os cliques; este painel aplica as
// mudanças ao documento ativo da sessão. Os comandos são as ações do catálogo
// (com atalhos configuráveis e também no menu Animação).
//
// Reprodução: todas as cenas com a mesma duração, definida em FPS; repete do
// começo ou vai e volta ("ping-pong").
// =============================================================================
#pragma once

#include <QPointer>
#include <QWidget>

class QSpinBox;
class QTimer;

namespace spritenator::gui {

class ActionCatalog;
class Document;
class EditorSession;
class TimelineGrid;

/// Painel inferior com a grade de camadas x cenas.
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
    [[nodiscard]] TimelineGrid* grid() const { return m_grid; }
    [[nodiscard]] QSpinBox* fpsBox() const { return m_fps; }
    [[nodiscard]] QSpinBox* opacityBox() const { return m_opacity; }

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslate();
    /// Passa a acompanhar o documento ativo.
    void attach(Document* document);
    /// FPS, opacidade e ações refletem a cena/camada atual.
    void syncCurrent();
    /// Antes de mudar de cena/camada ou de estrutura: aplica a transformação
    /// da seleção e interrompe a ferramenta.
    void finishPendingWork();
    void playNextFrame();
    void setPlaying(bool playing);
    /// Ícone do botão de repetição (repetir do começo ou vai e volta).
    void updateLoopIcon();

    EditorSession* m_session;
    ActionCatalog* m_actions;
    QPointer<Document> m_document;
    bool m_updating = false; ///< evita realimentação controles <-> documento
    int m_direction = 1; ///< sentido da reprodução no vai e volta

    TimelineGrid* m_grid = nullptr;
    QSpinBox* m_fps = nullptr;
    QSpinBox* m_opacity = nullptr;
    QTimer* m_playTimer = nullptr;
};

} // namespace spritenator::gui
