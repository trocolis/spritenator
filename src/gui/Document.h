// =============================================================================
// Document.h
//
// Um documento aberto: o canvas (imagem indexada ou RGBA), sua paleta, o
// caminho do arquivo e o histórico de desfazer/refazer. Em documentos RGBA
// a paleta não descreve os pixels (fica a padrão, usada só em conversões).
//
// Regra de ouro: TODA alteração do canvas ou da paleta feita pelo usuário
// passa por um comando de undo (ver gui/commands). Os métodos "apply*" abaixo
// são chamados pelos comandos e emitem os sinais que atualizam as views.
// =============================================================================
#pragma once

#include "ImageTarget.h"
#include "commands/UndoLabel.h"
#include "core/AnyImage.h"
#include "core/Image.h"
#include "core/Palette.h"

#include <QImage>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QUndoStack;

namespace spritenator::gui {

/// Documento de imagem aberto no editor.
class Document final : public QObject, public ImageTarget {
    Q_OBJECT

public:
    /// Cria um documento novo (sem arquivo associado).
    Document(core::AnyImage canvas, core::Palette palette, QObject* parent = nullptr);
    ~Document() override;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    /// Abre um documento a partir de um arquivo. Lança io::IoError.
    [[nodiscard]] static Document* load(const QString& path, QObject* parent = nullptr);

    // --- Acesso ao conteúdo ---
    [[nodiscard]] const core::AnyImage& canvas() const { return m_canvas; }
    [[nodiscard]] const core::Palette& palette() const { return m_palette; }

    /// Modo de cor do documento (indexado ou RGBA).
    [[nodiscard]] core::ColorMode colorMode() const { return core::modeOf(m_canvas); }

    /// Canvas indexado (só para documentos indexados; assert em debug).
    [[nodiscard]] const core::IndexedImage& indexedCanvas() const;

    /// Imagem pronta para exibição (cache atualizado incrementalmente).
    [[nodiscard]] const QImage& renderedImage() const { return m_rendered; }

    // --- Arquivo ---
    [[nodiscard]] const QString& filePath() const { return m_filePath; }
    [[nodiscard]] bool hasFilePath() const { return !m_filePath.isEmpty(); }

    /// Nome curto para títulos de janela ("Untitled1" se não houver arquivo).
    [[nodiscard]] QString displayName() const;

    /// Grava no caminho informado (formato pela extensão) e passa a usá-lo
    /// como caminho do documento. Lança io::IoError.
    void saveAs(const QString& path);

    /// Grava no caminho atual. Lança io::IoError ou std::logic_error se o
    /// documento ainda não tiver caminho.
    void save();

    /// Recarrega o arquivo do disco descartando as alterações e o histórico.
    void revert();

    // --- Histórico ---
    [[nodiscard]] QUndoStack* undoStack() const { return m_undoStack; }

    /// Verdadeiro se houver alterações não gravadas.
    [[nodiscard]] bool isModified() const;

    // --- Edição ao vivo (usada pelas ferramentas) ---
    //
    // Ferramentas como o lápis alteram o canvas a cada movimento do mouse
    // (retorno imediato) e só ao soltar o botão registram UM passo de
    // desfazer com o "antes" e o "depois" da área tocada:
    //
    //   beginEdit()  -> guarda uma cópia (snapshot) do canvas
    //   editLive()   -> aplica alterações e acumula a área suja
    //   commitEdit() -> cria o RegionCommand (já aplicado)

    /// Função de edição ao vivo: altera a imagem e devolve o retângulo afetado.
    using LiveEdit = std::function<core::Rect(core::AnyImage&)>;

    void beginEdit();
    [[nodiscard]] bool isEditing() const { return m_editing; }
    void editLive(const LiveEdit& edit);
    /// Volta o canvas ao estado do início da edição (ferramentas de figura
    /// redesenham a figura inteira a cada movimento).
    void restoreSnapshot();
    /// Finaliza a edição; registra um passo de desfazer se algo mudou.
    void commitEdit(const UndoLabel& undoLabel);
    /// Desfaz a edição em andamento sem registrar nada.
    void cancelEdit();

    // --- Seleção retangular (coordenadas do canvas) ---
    [[nodiscard]] const std::optional<core::Rect>& selection() const { return m_selection; }
    void setSelection(const std::optional<core::Rect>& selection);
    /// Região de trabalho das operações: a seleção, ou o canvas inteiro.
    [[nodiscard]] core::Rect workArea() const;

    // --- Aplicação de alterações (usado pelos comandos de undo) ---

    /// Substitui uma região do canvas pelo conteúdo informado.
    void applyCanvasRegion(core::Point origin, const core::AnyImage& pixels);

    /// Substitui o canvas inteiro (pode mudar de tamanho).
    void applyCanvas(const core::AnyImage& canvas);

    /// Substitui a paleta inteira.
    void applyPalette(const core::Palette& palette);

    // --- ImageTarget (o alvo de edição de um documento é o seu canvas) ---
    [[nodiscard]] const core::AnyImage& targetImage() const override { return m_canvas; }
    void applyRegion(core::Point origin, const core::AnyImage& pixels) override
    {
        applyCanvasRegion(origin, pixels);
    }
    void applyImage(const core::AnyImage& image) override { applyCanvas(image); }

signals:
    /// A seleção mudou.
    void selectionChanged();
    /// Uma região do canvas mudou (coordenadas do canvas).
    void canvasChanged(const core::Rect& area);
    /// O tamanho do canvas mudou (implica redesenho completo).
    void canvasResized(const core::Size& size);
    /// A paleta mudou.
    void paletteChanged();
    /// O caminho do arquivo mudou (após "Salvar como").
    void filePathChanged(const QString& path);
    /// O estado "modificado" mudou.
    void modifiedChanged(bool modified);
    /// O modo de cor mudou (ex.: "Reverter" para um arquivo de outro modo).
    void colorModeChanged(core::ColorMode mode);

private:
    /// Reconstrói todo o cache de exibição.
    void rebuildRendered();

    core::AnyImage m_canvas;
    core::Palette m_palette;
    QImage m_rendered;
    QString m_filePath;
    QUndoStack* m_undoStack = nullptr;

    // Edição ao vivo em andamento.
    bool m_editing = false;
    core::AnyImage m_snapshot;
    core::Rect m_dirty;

    std::optional<core::Rect> m_selection;

    /// Número sequencial para nomes "Untitled1", "Untitled2"...
    int m_untitledNumber = 0;
};

} // namespace spritenator::gui
