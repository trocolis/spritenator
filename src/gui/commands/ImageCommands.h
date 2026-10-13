// =============================================================================
// ImageCommands.h
//
// Comandos de desfazer/refazer (padrão Command, via QUndoCommand) para
// alterações de imagem e de paleta.
//
// Estratégia de memória: o comando de região guarda apenas o retângulo
// afetado (antes e depois), e não a imagem inteira.
// =============================================================================
#pragma once

#include "UndoLabel.h"
#include "core/IndexedImage.h"
#include "core/Palette.h"

#include <QUndoCommand>

namespace spritenator::gui {

class Document;
class ImageTarget;

/// Base dos comandos do Spritenator: guarda a operação (UndoLabel) para que
/// o nome exibido no menu possa ser retraduzido quando o idioma muda.
class LabeledCommand : public QUndoCommand {
public:
    explicit LabeledCommand(const UndoLabel& label);

    [[nodiscard]] const UndoLabel& label() const { return m_label; }

    /// Reaplica o nome no idioma atual.
    void retranslate();

private:
    UndoLabel m_label;
};

/// Troca o conteúdo de uma região retangular de um ImageTarget.
class RegionCommand final : public LabeledCommand {
public:
    /// "before"/"after" devem ter o mesmo tamanho. Se "alreadyApplied" for
    /// verdadeiro, o primeiro redo() (chamado pelo QUndoStack::push) não faz
    /// nada — útil quando a edição já foi aplicada ao vivo (ex.: traço do lápis).
    RegionCommand(ImageTarget* target, core::Point origin, core::IndexedImage before,
        core::IndexedImage after, const UndoLabel& label, bool alreadyApplied = false);

    void undo() override;
    void redo() override;

private:
    ImageTarget* m_target;
    core::Point m_origin;
    core::IndexedImage m_before;
    core::IndexedImage m_after;
    bool m_skipNextRedo;
};

/// Troca a imagem inteira de um ImageTarget (ex.: redimensionar, limpar tudo).
class ReplaceImageCommand final : public LabeledCommand {
public:
    ReplaceImageCommand(ImageTarget* target, core::IndexedImage before, core::IndexedImage after,
        const UndoLabel& label);

    void undo() override;
    void redo() override;

private:
    ImageTarget* m_target;
    core::IndexedImage m_before;
    core::IndexedImage m_after;
};

/// Troca a paleta de um documento. Comandos consecutivos com o mesmo
/// "mergeKey" (>= 0) são fundidos: arrastar um controle deslizante de cor
/// gera um único passo de desfazer, e não centenas.
class PaletteCommand final : public LabeledCommand {
public:
    PaletteCommand(Document* document, core::Palette before, core::Palette after,
        const UndoLabel& label, int mergeKey = -1);

    void undo() override;
    void redo() override;
    [[nodiscard]] int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    /// Identificador de tipo para mergeWith (qualquer valor único).
    static constexpr int kCommandId = 0x5041; // 'PA'

    Document* m_document;
    core::Palette m_before;
    core::Palette m_after;
    int m_mergeKey;
};

/// Troca canvas e paleta de um documento ao mesmo tempo (ex.: carregar uma
/// paleta com remapeamento das cores da imagem).
class DocumentStateCommand final : public LabeledCommand {
public:
    DocumentStateCommand(Document* document, core::IndexedImage beforeImage,
        core::Palette beforePalette, core::IndexedImage afterImage, core::Palette afterPalette,
        const UndoLabel& label);

    void undo() override;
    void redo() override;

private:
    Document* m_document;
    core::IndexedImage m_beforeImage;
    core::Palette m_beforePalette;
    core::IndexedImage m_afterImage;
    core::Palette m_afterPalette;
};

} // namespace spritenator::gui
