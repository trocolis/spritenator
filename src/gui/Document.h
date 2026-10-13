// =============================================================================
// Document.h
//
// Um documento aberto: o canvas (imagem indexada), sua paleta, o caminho do
// arquivo e o histórico de desfazer/refazer.
//
// Regra de ouro: TODA alteração do canvas ou da paleta feita pelo usuário
// passa por um comando de undo (ver gui/commands). Os métodos "apply*" abaixo
// são chamados pelos comandos e emitem os sinais que atualizam as views.
// =============================================================================
#pragma once

#include "core/IndexedImage.h"
#include "core/Palette.h"

#include <QImage>
#include <QObject>
#include <QString>

class QUndoStack;

namespace spritenator::gui {

/// Documento de imagem indexada aberto no editor.
class Document final : public QObject {
    Q_OBJECT

public:
    /// Cria um documento novo (sem arquivo associado).
    Document(core::IndexedImage canvas, core::Palette palette, QObject* parent = nullptr);
    ~Document() override;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    /// Abre um documento a partir de um arquivo. Lança io::IoError.
    [[nodiscard]] static Document* load(const QString& path, QObject* parent = nullptr);

    // --- Acesso ao conteúdo ---
    [[nodiscard]] const core::IndexedImage& canvas() const { return m_canvas; }
    [[nodiscard]] const core::Palette& palette() const { return m_palette; }

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

    // --- Aplicação de alterações (usado pelos comandos de undo) ---

    /// Substitui uma região do canvas pelo conteúdo informado.
    void applyCanvasRegion(core::Point origin, const core::IndexedImage& pixels);

    /// Substitui o canvas inteiro (pode mudar de tamanho).
    void applyCanvas(const core::IndexedImage& canvas);

    /// Substitui a paleta inteira.
    void applyPalette(const core::Palette& palette);

signals:
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

private:
    /// Reconstrói todo o cache de exibição.
    void rebuildRendered();

    core::IndexedImage m_canvas;
    core::Palette m_palette;
    QImage m_rendered;
    QString m_filePath;
    QUndoStack* m_undoStack = nullptr;

    /// Número sequencial para nomes "Untitled1", "Untitled2"...
    int m_untitledNumber = 0;
};

} // namespace spritenator::gui
