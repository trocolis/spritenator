// =============================================================================
// ActionCatalog.h
//
// Catálogo central de todas as ações (QAction) da janela principal.
//
// No original em Delphi existia um TActionList com cada comando, seu texto,
// atalho, dica e ícone; menus e botões apenas referenciavam essas ações. Este
// catálogo reproduz o mesmo princípio: a ação é criada UMA vez aqui e depois
// reutilizada por menus, barras de ferramentas e pelo Integrator, garantindo
// que estado (habilitada/marcada) fique sempre consistente.
// =============================================================================
#pragma once

#include <QKeySequence>
#include <QList>
#include <QObject>

#include <array>
#include <cstddef>

class QAction;

namespace spritenator::gui {

/// Identificadores de todas as ações conhecidas pela aplicação.
enum class ActionId : std::size_t {
    // --- Arquivo ---
    FileNew,
    FileOpen,
    FileRevert,
    FileSave,
    FileSaveAs,
    FileClose,
    FileSetBackupFolder,
    FileExit,
    // --- Editar ---
    EditUndo,
    EditRedo,
    EditCut,
    EditCopy,
    EditPaste,
    EditClear,
    EditSelectAll,
    EditDrawText,
    EditShadow,
    EditShortcuts,
    // --- Seleção ---
    SelectNone,
    SelectInvert,
    SelectRotateClockwise,
    SelectRotateCounterClockwise,
    SelectFlipHorizontal,
    SelectFlipVertical,
    SelectApplyTransform,
    SelectMoveLeft,
    SelectMoveRight,
    SelectMoveUp,
    SelectMoveDown,
    SelectMoveLeftFast,
    SelectMoveRightFast,
    SelectMoveUpFast,
    SelectMoveDownFast,
    SelectOutlineLeft,
    SelectOutlineRight,
    SelectOutlineUp,
    SelectOutlineDown,
    // --- Exibir ---
    ViewGrids,
    ViewMainGrid,
    ViewSubGrid,
    ViewZoomIn,
    ViewZoomOut,
    ViewZoomActual,
    ViewZoomFit,
    ViewAnimation,
    // --- Canvas ---
    CanvasResize,
    CanvasClearAll,
    // --- Paleta ---
    PaletteTransparent,
    PaletteLoad,
    PaletteSave,
    PaletteEdit,
    PaletteClearMask,
    PaletteInvertMask,
    PaletteAutoMask,
    PaletteMaskMode,
    PaletteGradation,
    // --- Janela ---
    WindowReverseCanvas,
    // --- Ajuda ---
    HelpContents,
    HelpAbout,

    Count ///< Sentinela: quantidade total de ações (não é uma ação).
};

/// Quantidade de ações, para dimensionar arrays.
inline constexpr std::size_t kActionCount = static_cast<std::size_t>(ActionId::Count);

/// Cria e mantém todas as ações da aplicação.
class ActionCatalog final : public QObject {
    Q_OBJECT

public:
    /// Cria todas as ações; o pai (normalmente a MainWindow) é dono delas.
    explicit ActionCatalog(QObject* parent);

    /// Devolve a ação correspondente ao identificador (nunca nula).
    [[nodiscard]] QAction* action(ActionId id) const;

    /// Atalho sintático: catalog[ActionId::FileNew].
    [[nodiscard]] QAction* operator[](ActionId id) const { return action(id); }

    /// Reaplica textos e dicas no idioma atual (chamado na criação e quando o
    /// idioma muda com o programa aberto).
    void retranslate();

    // --- Atalhos configuráveis ---
    /// Nome estável da ação, usado para gravar o atalho nas preferências.
    [[nodiscard]] static QString settingsKey(ActionId id);
    /// Atalho padrão da ação.
    [[nodiscard]] static QKeySequence defaultShortcut(ActionId id);
    /// Ações cujo atalho só vale com o editor de imagem em foco (as setas,
    /// que de outro modo roubariam as teclas de caixas numéricas e listas).
    [[nodiscard]] QList<QAction*> editorActions() const;

private:
    std::array<QAction*, kActionCount> m_actions { };
};

} // namespace spritenator::gui
