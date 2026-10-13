// =============================================================================
// DrawBuffer.h
//
// O buffer da área de desenho (Draw Area). No Character Maker 1999 a edição
// acontece num quadro separado do canvas: o usuário traz uma célula do canvas
// para cá, desenha ampliado e depois grava o quadro de volta. O buffer é
// único e compartilhado entre todos os documentos, o que permite copiar
// quadros de um canvas para outro.
//
// Edição "ao vivo": ferramentas como o lápis alteram o buffer a cada
// movimento do mouse (feedback imediato) e só ao soltar o botão a edição é
// registrada no histórico como UM comando de undo:
//
//   beginEdit()  -> guarda uma cópia (snapshot) da imagem
//   editLive()   -> aplica alterações e acumula a área suja
//   commitEdit() -> cria o RegionCommand com o "antes" e o "depois"
// =============================================================================
#pragma once

#include "ImageTarget.h"
#include "commands/UndoLabel.h"
#include "core/Image.h"
#include "core/Palette.h"

#include <QImage>
#include <QObject>

#include <functional>
#include <optional>

class QUndoStack;

namespace spritenator::gui {

/// Imagem editada na área de desenho, com histórico próprio.
class DrawBuffer final : public QObject, public ImageTarget {
    Q_OBJECT

public:
    /// Função de edição ao vivo: altera a imagem e devolve o retângulo afetado.
    using LiveEdit = std::function<core::Rect(core::IndexedImage&)>;

    DrawBuffer(core::Size size, QObject* parent = nullptr);
    ~DrawBuffer() override;

    DrawBuffer(const DrawBuffer&) = delete;
    DrawBuffer& operator=(const DrawBuffer&) = delete;

    [[nodiscard]] const core::IndexedImage& image() const { return m_image; }
    [[nodiscard]] const QImage& renderedImage() const { return m_rendered; }
    [[nodiscard]] QUndoStack* undoStack() const { return m_undoStack; }

    /// Paleta usada apenas para EXIBIR o buffer (a do documento ativo).
    void setPalette(const core::Palette& palette);

    /// Substitui o conteúdo inteiro registrando um passo de desfazer.
    void replace(const core::IndexedImage& image, const UndoLabel& undoLabel);

    // --- Edição ao vivo ---
    void beginEdit();
    [[nodiscard]] bool isEditing() const { return m_editing; }
    void editLive(const LiveEdit& edit);
    /// Volta a imagem ao estado do início da edição (usado por ferramentas
    /// de forma, que redesenham a figura inteira a cada movimento).
    void restoreSnapshot();
    /// Finaliza a edição; registra um comando se algo mudou.
    void commitEdit(const UndoLabel& undoLabel);
    /// Desfaz a edição em andamento sem registrar nada.
    void cancelEdit();
    /// Imagem do início da edição em andamento (vazia se não houver edição).
    [[nodiscard]] const core::IndexedImage& snapshot() const { return m_snapshot; }

    // --- Seleção retangular (coordenadas do buffer) ---
    [[nodiscard]] const std::optional<core::Rect>& selection() const { return m_selection; }
    void setSelection(const std::optional<core::Rect>& selection);

    /// Região de trabalho: a seleção (se houver) ou o buffer inteiro.
    [[nodiscard]] core::Rect workArea() const;

    // --- ImageTarget ---
    [[nodiscard]] const core::IndexedImage& targetImage() const override { return m_image; }
    void applyRegion(core::Point origin, const core::IndexedImage& pixels) override;
    void applyImage(const core::IndexedImage& image) override;

signals:
    /// Região alterada (coordenadas do buffer).
    void changed(const core::Rect& area);
    /// O tamanho do buffer mudou.
    void resized(const core::Size& size);
    /// A seleção mudou.
    void selectionChanged();

private:
    /// Atualiza o cache de exibição e notifica a região alterada.
    void notifyChanged(const core::Rect& area);

    core::IndexedImage m_image;
    QImage m_rendered;
    core::Palette m_palette;
    QUndoStack* m_undoStack = nullptr;

    bool m_editing = false;
    core::IndexedImage m_snapshot;
    core::Rect m_dirty;

    std::optional<core::Rect> m_selection;
};

} // namespace spritenator::gui
