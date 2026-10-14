// =============================================================================
// UndoLabel.h
//
// Nomes das operações exibidos no menu Desfazer/Refazer ("Desfazer Lápis").
//
// Os comandos guardam QUAL operação representam (UndoOperation), e não o
// texto já traduzido. Assim, quando o idioma muda com o programa aberto,
// todo o histórico é retraduzido (ver retranslateUndoHistory) — sem isso o
// menu mostraria nomes antigos no idioma anterior.
// =============================================================================
#pragma once

#include <QString>

class QUndoGroup;

namespace spritenator::gui {

/// Todas as operações que geram um passo de desfazer.
enum class UndoOperation {
    // Ferramentas
    Pen,
    Fill,
    Line,
    Rectangle,
    FilledRectangle,
    RoundedRectangle,
    FilledRoundedRectangle,
    Ellipse,
    FilledEllipse,
    // Transferência canvas <-> área de desenho
    GetCell,
    PutCell,
    TransferAll,
    // Edição
    Paste,
    Clear,
    PrepareShadow,
    Text,
    // Transformações
    ShiftUp,
    ShiftRight,
    ShiftDown,
    ShiftLeft,
    FlipVertically,
    FlipHorizontally,
    VerticalSymmetry,
    HorizontalSymmetry,
    RotateRight,
    RotateLeft,
    FreeRotation,
    Stretch,
    TransformSelection, ///< transformação livre da seleção (mover, escalar...)
    // Linha do tempo
    AddFrame,
    DuplicateFrame,
    DeleteFrame,
    MoveFrame,
    FrameDuration,
    AddLayer,
    DuplicateLayer,
    DeleteLayer,
    MoveLayer,
    MergeLayer,
    LayerProperties, ///< nome ou visibilidade
    LayerOpacity,
    // Canvas e grade
    CanvasSize,
    ClearAll,
    CharacterSize,
    // Paleta
    EditColor, ///< usa o argumento: índice da cor
    Gradation,
    LoadPalette,
    EditPalette,
};

/// Rótulo de um passo de desfazer: a operação e um argumento opcional.
struct UndoLabel {
    UndoOperation operation;
    int argument = -1; ///< substitui "%1" no nome, quando >= 0

    /// Nome traduzido no idioma atual (ex.: "Editar cor 12").
    [[nodiscard]] QString text() const;
};

/// Retraduz os nomes de todos os comandos de todas as pilhas do grupo.
void retranslateUndoHistory(const QUndoGroup* group);

} // namespace spritenator::gui
