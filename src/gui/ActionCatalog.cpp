// =============================================================================
// ActionCatalog.cpp
//
// Os atalhos de teclado reproduzem os do Character Maker 1999 (extraídos do
// TActionList do formulário principal). Os textos seguem a tradução inglesa
// do original, revisada.
//
// Todas as ações são descritas numa única tabela declarativa (actionSpecs).
// Os textos ficam marcados com QT_TRANSLATE_NOOP (com o contexto que tr()
// usaria nesta classe) — visíveis para o lupdate, mas traduzidos só em
// retranslate(). Assim a mesma tabela serve para criar
// as ações e para retraduzi-las quando o idioma muda com o programa aberto.
// =============================================================================
#include "ActionCatalog.h"

#include "IconProvider.h"

#include <QAction>

#include <cassert>
#include <vector>

namespace spritenator::gui {

namespace {

/// Descrição completa de uma ação.
struct ActionSpec {
    ActionId id;
    const char* text; ///< texto do menu (com "&" do acelerador)
    const char* statusTip; ///< dica exibida na barra de status e no tooltip
    const char* icon; ///< nome lógico do ícone (nulo = sem ícone)
    QKeySequence shortcut;
    bool checkable = false;
};

/// Tabela de todas as ações, na ordem de ActionId. Construída na primeira
/// chamada (os atalhos-padrão do sistema exigem a QGuiApplication criada).
const std::vector<ActionSpec>& actionSpecs()
{
    using Seq = QKeySequence;
    static const std::vector<ActionSpec> specs = {
        // --- Arquivo ---
        { ActionId::FileNew, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&New..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Create a new image"), "file_new",
            Seq(Seq::New) },
        { ActionId::FileOpen, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Open..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Open an existing image"),
            "file_open", Seq(Seq::Open) },
        { ActionId::FileRevert, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Revert"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Discard all changes and reload the image from disk"),
            nullptr, Seq(Qt::CTRL | Qt::Key_R) },
        { ActionId::FileSave, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Save"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Save the current image"),
            "file_save", Seq(Seq::Save) },
        { ActionId::FileSaveAs, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Save &As..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Save the current image with another name or format"),
            nullptr, Seq(Qt::Key_F12) },
        { ActionId::FileClose, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Close"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Close the current image"),
            nullptr, Seq(Qt::CTRL | Qt::Key_F4) },
        { ActionId::FileSetBackupFolder,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Set &Backup Folder..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Choose the folder where backups are written automatically on save"),
            nullptr, Seq() },
        { ActionId::FileExit, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "E&xit"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Quit the application"), nullptr,
            Seq(Qt::ALT | Qt::Key_F4) },
        // --- Editar ---
        { ActionId::EditUndo, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Undo"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Undo the last operation"),
            "edit_undo", Seq(Qt::CTRL | Qt::Key_Z) },
        { ActionId::EditRedo, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Redo"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Redo the last undone operation"),
            "edit_redo", Seq(Qt::CTRL | Qt::SHIFT | Qt::Key_Z) },
        { ActionId::EditCut, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Cu&t"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the selection to the clipboard"),
            "edit_cut", Seq(Qt::CTRL | Qt::Key_X) },
        { ActionId::EditCopy, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Copy"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Copy the selection to the clipboard"),
            "edit_copy", Seq(Qt::CTRL | Qt::Key_C) },
        { ActionId::EditPaste, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Paste"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Paste the clipboard contents into the image"),
            "edit_paste", Seq(Qt::CTRL | Qt::Key_V) },
        { ActionId::EditClear, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "C&lear"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Erase the selection without copying it"),
            nullptr, Seq(Qt::Key_Delete) },
        { ActionId::EditSelectAll,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Select &All"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Select the whole image"), nullptr,
            Seq(Qt::CTRL | Qt::Key_A) },
        { ActionId::EditDrawText,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Text &Input..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Render text into the image"),
            "edit_draw_text", Seq(Qt::CTRL | Qt::Key_T) },
        { ActionId::EditShortcuts,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Keyboard Shortcuts..."),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Change the keyboard shortcut of any command"),
            nullptr, Seq() },
        { ActionId::EditShadow,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Prepare &Shadow"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Replace every non-background color with the drawing color"),
            nullptr, Seq(Qt::CTRL | Qt::Key_Q) },
        // --- Seleção ---
        { ActionId::SelectNone, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Deselect"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Remove the selection"), nullptr,
            Seq(Qt::CTRL | Qt::SHIFT | Qt::Key_A) },
        { ActionId::SelectInvert,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Invert Selection"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Select everything that is not selected"),
            nullptr, Seq(Qt::CTRL | Qt::Key_I) },
        { ActionId::SelectRotateClockwise,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Rotate Right"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Rotate the selection 90 degrees clockwise"),
            "rotate_cw", Seq(Qt::Key_R) },
        { ActionId::SelectRotateCounterClockwise,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Rotate &Left"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Rotate the selection 90 degrees counterclockwise"),
            "rotate_ccw", Seq(Qt::SHIFT | Qt::Key_R) },
        { ActionId::SelectFlipHorizontal,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Flip &Horizontally"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Mirror the selection from left to right"),
            "flip_horizontal", Seq(Qt::Key_E) },
        { ActionId::SelectFlipVertical,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Flip &Vertically"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Mirror the selection from top to bottom"),
            "flip_vertical", Seq(Qt::SHIFT | Qt::Key_E) },
        { ActionId::SelectApplyTransform,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "A&pply Transformation"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Apply the pending move, scale or rotation of the selection (Enter in the image)"),
            nullptr, Seq() },
        // Mover: setas (1 px), Shift+setas (8 px) e Ctrl+setas (só o contorno).
        { ActionId::SelectMoveLeft,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Left"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the selected pixels one pixel to the left"),
            nullptr, Seq(Qt::Key_Left) },
        { ActionId::SelectMoveRight,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Right"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the selected pixels one pixel to the right"),
            nullptr, Seq(Qt::Key_Right) },
        { ActionId::SelectMoveUp,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Up"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the selected pixels one pixel up"),
            nullptr, Seq(Qt::Key_Up) },
        { ActionId::SelectMoveDown,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Down"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the selected pixels one pixel down"),
            nullptr, Seq(Qt::Key_Down) },
        { ActionId::SelectMoveLeftFast,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Left (8 px)"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the selected pixels eight pixels to the left"),
            nullptr, Seq(Qt::SHIFT | Qt::Key_Left) },
        { ActionId::SelectMoveRightFast,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Right (8 px)"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the selected pixels eight pixels to the right"),
            nullptr, Seq(Qt::SHIFT | Qt::Key_Right) },
        { ActionId::SelectMoveUpFast,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Up (8 px)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the selected pixels eight pixels up"),
            nullptr, Seq(Qt::SHIFT | Qt::Key_Up) },
        { ActionId::SelectMoveDownFast,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Selection Down (8 px)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the selected pixels eight pixels down"),
            nullptr, Seq(Qt::SHIFT | Qt::Key_Down) },
        { ActionId::SelectOutlineLeft,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Outline Left"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move only the selection outline one pixel to the left"),
            nullptr, Seq(Qt::CTRL | Qt::Key_Left) },
        { ActionId::SelectOutlineRight,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Outline Right"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move only the selection outline one pixel to the right"),
            nullptr, Seq(Qt::CTRL | Qt::Key_Right) },
        { ActionId::SelectOutlineUp,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Outline Up"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move only the selection outline one pixel up"),
            nullptr, Seq(Qt::CTRL | Qt::Key_Up) },
        { ActionId::SelectOutlineDown,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Outline Down"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move only the selection outline one pixel down"),
            nullptr, Seq(Qt::CTRL | Qt::Key_Down) },
        // --- Ferramentas ---
        { ActionId::PenSizeDecrease,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Smaller Tip"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Decrease the size of the tip (pen and shapes) by one pixel"),
            nullptr, Seq(Qt::Key_BracketLeft) },
        { ActionId::PenSizeIncrease,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Larger Tip"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Increase the size of the tip (pen and shapes) by one pixel"),
            nullptr, Seq(Qt::Key_BracketRight) },
        // --- Animação: cenas e camadas ---
        { ActionId::FrameNew, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&New Scene"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Add an empty scene after the current one"),
            "frame_new", Seq(Qt::ALT | Qt::Key_N), false },
        { ActionId::FrameDuplicate,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Duplicate Scene"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Add a copy of the current scene after it"),
            "frame_duplicate", Seq(Qt::ALT | Qt::Key_D), false },
        { ActionId::FrameDelete,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Dele&te Scene"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Delete the current scene"),
            "frame_delete", Seq(), false },
        { ActionId::FrameMoveLeft,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Scene &Left"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the current scene one position to the left"),
            "frame_move_left", Seq(), false },
        { ActionId::FrameMoveRight,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Scene &Right"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the current scene one position to the right"),
            "frame_move_right", Seq(), false },
        { ActionId::FramePrevious,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Previous Scene"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Show the previous scene"),
            "frame_previous", Seq(Qt::Key_Comma), false },
        { ActionId::FrameNext, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Ne&xt Scene"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Show the next scene"),
            "frame_next", Seq(Qt::Key_Period), false },
        { ActionId::PlayPause, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Pl&ay"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Play the animation in the image editor (press again to stop)"),
            "play", Seq(Qt::Key_P), true },
        { ActionId::OnionSkin, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Onion Skin"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Show the previous and the next scenes faintly over the current one, to guide the "
                "drawing"),
            "onion_skin", Seq(), true },
        { ActionId::LayerNew, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "New La&yer"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Add an empty layer above the current one"),
            "layer_new", Seq(Qt::ALT | Qt::SHIFT | Qt::Key_N), false },
        { ActionId::LayerDuplicate,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Dupl&icate Layer"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Add a copy of the current layer above it"),
            "layer_duplicate", Seq(), false },
        { ActionId::LayerDelete,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Delete Lay&er"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Delete the current layer from all scenes"),
            "layer_delete", Seq(), false },
        { ActionId::LayerMoveUp,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Layer &Up"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the current layer one position up"),
            "layer_move_up", Seq(), false },
        { ActionId::LayerMoveDown,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Move Layer Do&wn"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Move the current layer one position down"),
            "layer_move_down", Seq(), false },
        { ActionId::LayerMergeDown,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Merge Layer Down"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Merge the current layer into the one below it, in all scenes"),
            "layer_merge", Seq(), false },
        { ActionId::LayerAbove,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Select Layer Above"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Make the layer above the current one the active layer"),
            nullptr, Seq(Qt::ALT | Qt::Key_Up), false },
        { ActionId::LayerBelow,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Select Layer Below"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Make the layer below the current one the active layer"),
            nullptr, Seq(Qt::ALT | Qt::Key_Down), false },
        { ActionId::FrameFirst,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&First Scene"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Show the first scene"),
            "frame_first", Seq(Qt::Key_Home) },
        { ActionId::FrameLast, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "La&st Scene"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Show the last scene"),
            "frame_last", Seq(Qt::Key_End) },
        { ActionId::PlaybackPingPong,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Pin&g-Pong"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Play forward and then backward (otherwise, start again from the first scene)"),
            "loop", Seq(), true },
        { ActionId::LayerLock, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Loc&k Layer"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Protect the current layer: the tools do not change it"),
            "layer_lock", Seq(), true },
        // --- Exibir ---
        { ActionId::ViewGrids, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Grids..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Change the spacing of the main grid and of the sub grid (character size)"),
            "view_grids", Seq() },
        { ActionId::ViewMainGrid,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Main Grid"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Show or hide the main grid"),
            "view_main_grid", Seq(Qt::CTRL | Qt::Key_G), true },
        { ActionId::ViewSubGrid, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Sub Grid"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show or hide the sub grid (character cells)"),
            "view_sub_grid", Seq(Qt::SHIFT | Qt::Key_G), true },
        // Zoom do editor de imagem ativo (a roda do mouse também amplia).
        { ActionId::ViewZoomIn, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom &In"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Magnify the image"), nullptr,
            Seq(Seq::ZoomIn) },
        { ActionId::ViewZoomOut, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom &Out"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Reduce the image"), nullptr,
            Seq(Seq::ZoomOut) },
        { ActionId::ViewZoomActual,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Actual Si&ze (100%)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the image without magnification"),
            nullptr, Seq(Qt::CTRL | Qt::Key_1) },
        { ActionId::ViewZoomFit,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Fit to Window"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Choose the largest zoom that shows the whole image"),
            nullptr, Seq(Qt::CTRL | Qt::Key_0) },
        { ActionId::ViewTimeline, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Tim&eline"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Show or hide the timeline with the scenes and the layers"),
            "view_animation", Seq(Qt::Key_F2), true },
        // --- Canvas ---
        { ActionId::CanvasResize,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Set Canvas &Size..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Change the size of the canvas"),
            "canvas_resize", Seq(Qt::CTRL | Qt::Key_E) },
        { ActionId::CanvasClearAll,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Clear All"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Erase the whole canvas"), nullptr,
            Seq(Qt::CTRL | Qt::SHIFT | Qt::Key_N) },
        // --- Paleta ---
        { ActionId::PaletteTransparent,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Transparent Color Effect"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Do not paste pixels of the background color (or fully transparent ones)"),
            "palette_transparent", Seq(Qt::Key_F8), true },
        { ActionId::PaletteLoad,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Load Palette..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Read palette data from a file"),
            "palette_load", Seq(Qt::SHIFT | Qt::Key_L) },
        { ActionId::PaletteSave,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Save Palette..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Write the palette to a file"),
            "palette_save", Seq(Qt::SHIFT | Qt::Key_S) },
        { ActionId::PaletteEdit,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Edit Palette..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Open the palette editor"),
            "palette_edit", Seq(Qt::SHIFT | Qt::Key_P) }, // Shift+E: espelhar a seleção
        { ActionId::PaletteClearMask,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Clear &Mask"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Remove the mask from every palette color"),
            "mask_clear", Seq(Qt::Key_F6) },
        { ActionId::PaletteInvertMask,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Inve&rt Mask"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Invert the mask state of every palette color"),
            "mask_invert", Seq(Qt::Key_F7) },
        { ActionId::PaletteAutoMask,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Automask All but Background"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Mask every color except the background color (press again to release)"),
            "mask_auto", Seq(Qt::Key_F9), true },
        { ActionId::PaletteMaskMode,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Mask &Mode"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Click palette colors to protect them from being drawn over"),
            "palette_mask_mode", Seq(Qt::Key_F5), true },
        { ActionId::PaletteGradation,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Gradation"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Interpolate the palette between the drawing and the background colors"),
            "palette_gradation", Seq(Qt::CTRL | Qt::Key_D) },
        // --- Janela ---
        { ActionId::WindowReverseCanvas,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Reverse Canvas"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Swap the sides of the tool panel and the canvas area"),
            nullptr, Seq(), true },
        // --- Ajuda ---
        { ActionId::HelpContents,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Help &Contents"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Open the user manual"),
            "help_contents", Seq(Qt::Key_F1) },
        { ActionId::HelpAbout, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&About..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Show version information"),
            nullptr, Seq() },
    };
    return specs;
}

} // namespace

ActionCatalog::ActionCatalog(QObject* parent)
    : QObject(parent)
{
    for (const ActionSpec& spec : actionSpecs()) {
        auto* created = new QAction(this);
        if (spec.icon != nullptr) {
            created->setIcon(IconProvider::icon(QString::fromLatin1(spec.icon)));
        }
        created->setShortcut(spec.shortcut);
        created->setCheckable(spec.checkable);
        m_actions.at(static_cast<std::size_t>(spec.id)) = created;
    }

    // Estados iniciais.
    action(ActionId::ViewMainGrid)->setChecked(true);

    // Setas: só com o editor de imagem em foco (ver editorActions).
    for (QAction* editorAction : editorActions()) {
        editorAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    // Garante, em builds de depuração, que a tabela cobre todas as ações.
    for ([[maybe_unused]] QAction* created : m_actions) {
        assert(created != nullptr && "ActionCatalog: ação ausente da tabela");
    }
    retranslate();
}

void ActionCatalog::retranslate()
{
    for (const ActionSpec& spec : actionSpecs()) {
        QAction* target = action(spec.id);
        const QString tip = tr(spec.statusTip);
        target->setText(tr(spec.text));
        target->setStatusTip(tip);
        target->setToolTip(tip);
    }
}

QString ActionCatalog::settingsKey(ActionId id)
{
    // Nomes estáveis (não mudam com o idioma nem com o texto do menu), na
    // ordem de ActionId.
    static constexpr const char* kKeys[] = { "FileNew", "FileOpen", "FileRevert", "FileSave",
        "FileSaveAs", "FileClose", "FileSetBackupFolder", "FileExit", "EditUndo", "EditRedo",
        "EditCut", "EditCopy", "EditPaste", "EditClear", "EditSelectAll", "EditDrawText",
        "EditShadow", "EditShortcuts", "SelectNone", "SelectInvert", "SelectRotateClockwise",
        "SelectRotateCounterClockwise", "SelectFlipHorizontal", "SelectFlipVertical",
        "SelectApplyTransform", "SelectMoveLeft", "SelectMoveRight", "SelectMoveUp",
        "SelectMoveDown", "SelectMoveLeftFast", "SelectMoveRightFast", "SelectMoveUpFast",
        "SelectMoveDownFast", "SelectOutlineLeft", "SelectOutlineRight", "SelectOutlineUp",
        "SelectOutlineDown", "PenSizeDecrease", "PenSizeIncrease", "FrameNew", "FrameDuplicate",
        "FrameDelete", "FrameMoveLeft", "FrameMoveRight", "FramePrevious", "FrameNext", "PlayPause",
        "OnionSkin", "LayerNew", "LayerDuplicate", "LayerDelete", "LayerMoveUp", "LayerMoveDown",
        "LayerMergeDown", "LayerAbove", "LayerBelow", "FrameFirst", "FrameLast", "PlaybackPingPong",
        "LayerLock", "ViewGrids", "ViewMainGrid", "ViewSubGrid", "ViewZoomIn", "ViewZoomOut",
        "ViewZoomActual", "ViewZoomFit", "ViewTimeline", "CanvasResize", "CanvasClearAll",
        "PaletteTransparent", "PaletteLoad", "PaletteSave", "PaletteEdit", "PaletteClearMask",
        "PaletteInvertMask", "PaletteAutoMask", "PaletteMaskMode", "PaletteGradation",
        "WindowReverseCanvas", "HelpContents", "HelpAbout" };
    static_assert(std::size(kKeys) == kActionCount, "ActionCatalog: falta a chave de uma ação");
    return QString::fromLatin1(kKeys[static_cast<std::size_t>(id)]);
}

QKeySequence ActionCatalog::defaultShortcut(ActionId id)
{
    for (const ActionSpec& spec : actionSpecs()) {
        if (spec.id == id) {
            return spec.shortcut;
        }
    }
    return { };
}

QList<QAction*> ActionCatalog::editorActions() const
{
    QList<QAction*> result;
    for (std::size_t index = static_cast<std::size_t>(ActionId::SelectMoveLeft);
        index <= static_cast<std::size_t>(ActionId::SelectOutlineDown); ++index) {
        result.append(m_actions.at(index));
    }
    return result;
}

QAction* ActionCatalog::action(ActionId id) const
{
    return m_actions.at(static_cast<std::size_t>(id));
}

} // namespace spritenator::gui
