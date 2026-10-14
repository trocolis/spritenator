// =============================================================================
// Document.h
//
// Um documento aberto: camadas e cenas (ver Sprite.h), a paleta, o caminho do
// arquivo, a seleção e o histórico de desfazer/refazer. Em documentos RGBA a
// paleta não descreve os pixels (fica a padrão, usada só em conversões).
//
// As ferramentas e operações editam a CÉLULA ATIVA (cena e camada atuais),
// exposta como canvas(); o editor exibe renderedImage(), a cena atual com
// todas as camadas visíveis compostas.
//
// Regra de ouro: TODA alteração feita pelo usuário passa por um comando de
// undo (ver gui/commands). Os métodos "apply*" abaixo são chamados pelos
// comandos e emitem os sinais que atualizam as views.
// =============================================================================
#pragma once

#include "Sprite.h"
#include "commands/UndoLabel.h"
#include "core/AnyImage.h"
#include "core/Image.h"
#include "core/Palette.h"
#include "core/Selection.h"

#include <QImage>
#include <QObject>
#include <QString>

#include <functional>

class QUndoStack;

namespace spritenator::gui {

/// Formatos de gravação ("Salvar como").
enum class SaveFormat {
    Image, ///< PNG/BMP: a cena atual, camadas mescladas
    Project, ///< Projeto Spritenator (.sprn): tudo, sem perdas
    Gif, ///< GIF animado: as cenas, camadas mescladas
    SpriteSheet, ///< PNG com as cenas lado a lado
    Frames, ///< um PNG por cena: nome_001.png, nome_002.png...
};

/// Formato e opções de gravação.
struct SaveOptions {
    SaveFormat format = SaveFormat::Image;
    int sheetColumns = 0; ///< colunas da sprite sheet (0 = todas numa linha)

    friend bool operator==(const SaveOptions&, const SaveOptions&) = default;
};

/// Documento de imagem aberto no editor.
class Document final : public QObject {
    Q_OBJECT

public:
    /// Cria um documento novo (sem arquivo): uma camada e uma cena.
    Document(core::AnyImage canvas, core::Palette palette, QObject* parent = nullptr);
    /// Cria um documento com a estrutura informada (ao abrir projetos e GIFs).
    Document(SpriteState state, QObject* parent = nullptr);
    ~Document() override;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    /// Abre um documento: projeto (.sprn), GIF animado (as cenas) ou imagem.
    /// Lança io::IoError.
    [[nodiscard]] static Document* load(const QString& path, QObject* parent = nullptr);

    // --- Acesso ao conteúdo (célula ativa) ---
    /// Imagem da célula ativa: é ela que as ferramentas editam.
    [[nodiscard]] const core::AnyImage& canvas() const;
    [[nodiscard]] const core::Palette& palette() const { return m_state.palette; }

    /// Modo de cor do documento (indexado ou RGBA; igual em todas as células).
    [[nodiscard]] core::ColorMode colorMode() const { return core::modeOf(canvas()); }

    /// Célula ativa indexada (só para documentos indexados; assert em debug).
    [[nodiscard]] const core::IndexedImage& indexedCanvas() const;

    /// A cena atual com as camadas visíveis compostas, pronta para exibição
    /// (atualizada incrementalmente).
    [[nodiscard]] const QImage& renderedImage() const { return m_rendered; }

    // --- Cenas e camadas ---
    [[nodiscard]] const SpriteState& state() const { return m_state; }
    [[nodiscard]] int frameCount() const { return static_cast<int>(m_state.frames.size()); }
    [[nodiscard]] int layerCount() const { return static_cast<int>(m_state.layers.size()); }
    [[nodiscard]] const std::vector<Layer>& layers() const { return m_state.layers; }
    [[nodiscard]] const std::vector<Frame>& frames() const { return m_state.frames; }
    [[nodiscard]] int currentFrame() const { return m_frame; }
    [[nodiscard]] int currentLayer() const { return m_layer; }
    /// Troca a cena/camada ativa (uma edição ao vivo em andamento é descartada).
    void setCurrentFrame(int frame);
    void setCurrentLayer(int layer);
    [[nodiscard]] CelKey activeCel() const;
    /// Imagem de uma célula qualquer.
    [[nodiscard]] const core::AnyImage& cel(int frame, int layer) const;
    /// Uma cena com as camadas visíveis compostas (miniaturas, papel de seda).
    [[nodiscard]] QImage composite(int frame) const;
    /// Uma cena com as camadas visíveis mescladas numa imagem do modo do
    /// documento (exportação). Indexada: vence a camada mais alta cujo pixel
    /// não é o índice 0.
    [[nodiscard]] core::AnyImage flattened(int frame) const;

    // --- Operações de linha do tempo (cada uma vira um passo de desfazer) ---
    void addFrame(); ///< cena vazia depois da atual
    void duplicateFrame();
    void removeFrame(); ///< não remove a última que sobrar
    void moveFrame(int offset); ///< -1 = para a esquerda, +1 = para a direita
    void setFrameDuration(int frame, int durationMs);
    void addLayer(); ///< camada vazia acima da atual
    void duplicateLayer();
    void removeLayer(); ///< não remove a última que sobrar
    void moveLayer(int offset); ///< +1 = para cima, -1 = para baixo
    /// Mescla a camada atual com a de baixo (em todas as cenas).
    void mergeLayerDown();
    void setLayerVisible(int layer, bool visible);
    void setLayerOpacity(int layer, int opacity);
    void renameLayer(int layer, const QString& name);

    /// Função aplicada a cada célula por transformCels (recebe também o
    /// índice da camada: a de baixo às vezes é tratada diferente).
    using CelFunction = std::function<core::AnyImage(const core::AnyImage& cel, int layer)>;
    /// Aplica uma função a TODAS as células e troca a paleta, como um passo
    /// de desfazer (redimensionar o canvas, remapear cores da paleta...).
    void transformCels(
        const CelFunction& function, const core::Palette& palette, const UndoLabel& undoLabel);

    // --- Arquivo ---
    [[nodiscard]] const QString& filePath() const { return m_filePath; }
    [[nodiscard]] bool hasFilePath() const { return !m_filePath.isEmpty(); }
    /// Formato usado por save() (o da última gravação ou abertura).
    [[nodiscard]] const SaveOptions& saveOptions() const { return m_saveOptions; }
    /// Verdadeiro se o arquivo atual pode ser reaberto por "Reverter"
    /// (sprite sheet e imagens separadas não voltam como cenas).
    [[nodiscard]] bool canRevert() const;

    /// Nome curto para títulos de janela ("Untitled1" se não houver arquivo).
    [[nodiscard]] QString displayName() const;

    /// Grava no caminho e formato informados e passa a usá-los. Lança
    /// io::IoError.
    void saveAs(const QString& path, const SaveOptions& options);
    /// Formato pela extensão: .sprn = projeto, .gif = GIF, senão imagem.
    void saveAs(const QString& path);

    /// Grava no caminho atual. Lança io::IoError ou std::logic_error se o
    /// documento ainda não tiver caminho.
    void save();

    /// Recarrega o arquivo do disco descartando as alterações e o histórico.
    void revert();

    /// Formato de gravação sugerido pela extensão do caminho.
    [[nodiscard]] static SaveFormat formatForPath(const QString& path);

    // --- Histórico ---
    [[nodiscard]] QUndoStack* undoStack() const { return m_undoStack; }

    /// Verdadeiro se houver alterações não gravadas.
    [[nodiscard]] bool isModified() const;

    // --- Edição ao vivo (usada pelas ferramentas) ---
    //
    // Ferramentas como o lápis alteram a célula ativa a cada movimento do
    // mouse (retorno imediato) e só ao soltar o botão registram UM passo de
    // desfazer com o "antes" e o "depois" da área tocada:
    //
    //   beginEdit()  -> guarda uma cópia (snapshot) da célula ativa
    //   editLive()   -> aplica alterações e acumula a área suja
    //   commitEdit() -> cria o RegionCommand (já aplicado)

    /// Função de edição ao vivo: altera a imagem e devolve o retângulo afetado.
    using LiveEdit = std::function<core::Rect(core::AnyImage&)>;

    void beginEdit();
    [[nodiscard]] bool isEditing() const { return m_editing; }
    void editLive(const LiveEdit& edit);
    /// Volta a célula ao estado do início da edição (ferramentas de figura
    /// redesenham a figura inteira a cada movimento).
    void restoreSnapshot();
    /// Finaliza a edição; registra um passo de desfazer se algo mudou.
    void commitEdit(const UndoLabel& undoLabel);
    /// Desfaz a edição em andamento sem registrar nada.
    void cancelEdit();

    // --- Seleção de forma livre (coordenadas do canvas, comum às cenas) ---
    /// Seleção atual (vazia = nada selecionado).
    [[nodiscard]] const core::Selection& selection() const { return m_selection; }
    [[nodiscard]] bool hasSelection() const { return !m_selection.isEmpty(); }
    /// Define a seleção (recortada ao canvas).
    void setSelection(const core::Selection& selection);
    /// Atalho: seleção retangular.
    void setSelection(const core::Rect& area) { setSelection(core::Selection::rectangle(area)); }
    void clearSelection() { setSelection(core::Selection { }); }
    /// Região de trabalho das operações: o retângulo envolvente da seleção,
    /// ou o canvas inteiro.
    [[nodiscard]] core::Rect workArea() const;

    // --- Aplicação de alterações (usado pelos comandos de undo) ---
    /// Substitui uma região de uma célula (que passa a ser a ativa).
    void applyCelRegion(const CelKey& key, core::Point origin, const core::AnyImage& pixels);
    /// Substitui todo o conteúdo (estrutura, células e paleta).
    void applyState(const SpriteState& state);
    /// Substitui a paleta inteira.
    void applyPalette(const core::Palette& palette);

signals:
    /// A seleção mudou.
    void selectionChanged();
    /// Uma região da cena exibida mudou (coordenadas do canvas).
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
    /// Cenas ou camadas foram criadas, removidas, movidas ou alteradas
    /// (nome, visibilidade, opacidade, duração).
    void structureChanged();
    /// A cena ou a camada ativa mudou.
    void currentCelChanged(int frame, int layer);

private:
    /// Célula ativa, separada dos instantâneos (cópia ao escrever).
    core::AnyImage& mutableCanvas();
    /// Registra a troca de estado como um passo de desfazer.
    void changeState(SpriteState after, UndoOperation operation, bool mergeable = false);
    /// Novo identificador de cena/camada.
    int takeId(SpriteState& state) const;
    /// Recompõe uma região (ou tudo) da cena atual em m_rendered.
    void updateComposite(const core::Rect& area);
    void rebuildRendered();
    /// Limita a cena/camada atual ao que existe.
    void clampCurrent();

    SpriteState m_state;
    int m_frame = 0;
    int m_layer = 0;
    QImage m_rendered;
    QString m_filePath;
    SaveOptions m_saveOptions;
    QUndoStack* m_undoStack = nullptr;

    // Edição ao vivo em andamento.
    bool m_editing = false;
    core::AnyImage m_snapshot;
    core::Rect m_dirty;

    core::Selection m_selection;

    /// Número sequencial para nomes "Untitled1", "Untitled2"...
    int m_untitledNumber = 0;
};

} // namespace spritenator::gui
