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
#include "core/AnyImage.h"
#include "core/Image.h"
#include "core/Palette.h"
#include "gui/Sprite.h"

#include <QUndoCommand>

namespace spritenator::gui {

class Document;

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

/// Troca o conteúdo de uma região retangular de uma célula do documento.
/// Desfazer e refazer tornam essa célula a ativa (levam à cena dela).
class RegionCommand final : public LabeledCommand {
public:
    /// "before"/"after" devem ter o mesmo tamanho. Se "alreadyApplied" for
    /// verdadeiro, o primeiro redo() (chamado pelo QUndoStack::push) não faz
    /// nada — útil quando a edição já foi aplicada ao vivo (ex.: traço do lápis).
    RegionCommand(Document* document, const CelKey& cel, core::Point origin, core::AnyImage before,
        core::AnyImage after, const UndoLabel& label, bool alreadyApplied = false);

    void undo() override;
    void redo() override;

private:
    Document* m_document;
    CelKey m_cel;
    core::Point m_origin;
    core::AnyImage m_before;
    core::AnyImage m_after;
    bool m_skipNextRedo;
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

/// Troca o conteúdo inteiro do documento (cenas, camadas, células e paleta):
/// operações de linha do tempo, redimensionar o canvas, remapear a paleta.
/// As células são compartilhadas entre os dois estados, então guardar o
/// "antes" e o "depois" custa pouco.
class SpriteStateCommand final : public LabeledCommand {
public:
    /// Com "mergeable", comandos seguidos da mesma operação no mesmo
    /// documento viram um só passo (ex.: arrastar a opacidade ou a duração).
    SpriteStateCommand(Document* document, SpriteState before, SpriteState after,
        const UndoLabel& label, bool mergeable = false);

    void undo() override;
    void redo() override;
    [[nodiscard]] int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    static constexpr int kCommandId = 0x5353; // 'SS'

    Document* m_document;
    bool m_mergeable;
    SpriteState m_before;
    SpriteState m_after;
};

} // namespace spritenator::gui
