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
#include <QActionGroup>

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
    int zoomFactor = 0; ///< > 0 apenas nas ações de zoom
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
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Move the selection of the drawing area to the clipboard"),
            "edit_cut", Seq(Qt::CTRL | Qt::Key_X) },
        { ActionId::EditCopy, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Copy"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Copy the selection of the drawing area to the clipboard"),
            "edit_copy", Seq(Qt::CTRL | Qt::Key_C) },
        { ActionId::EditPaste, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Paste"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Paste the clipboard contents into the drawing area"),
            "edit_paste", Seq(Qt::CTRL | Qt::Key_V) },
        { ActionId::EditClear, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "C&lear"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Erase the selection of the drawing area without copying it"),
            nullptr, Seq(Qt::Key_Delete) },
        { ActionId::EditSelectAll,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Select &All"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Select the whole drawing area"),
            nullptr, Seq(Qt::CTRL | Qt::Key_A) },
        { ActionId::EditDrawText,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Text &Input..."),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Render text into the drawing area"),
            "edit_draw_text", Seq(Qt::CTRL | Qt::Key_T) },
        { ActionId::EditShadow,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Prepare &Shadow"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Replace every non-background color with the drawing color"),
            nullptr, Seq(Qt::CTRL | Qt::Key_Q) },
        // --- Exibir ---
        { ActionId::ViewCharaSize,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Character Size..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Change the character cell size and the sub grid"),
            "view_chara_size", Seq() },
        { ActionId::ViewMainGrid,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Main Grid"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Show or hide the main grid of the drawing area"),
            "view_main_grid", Seq(Qt::CTRL | Qt::Key_G), true },
        { ActionId::ViewSubGrid, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Sub Grid"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show or hide the sub grid of the drawing area"),
            "view_sub_grid", Seq(Qt::SHIFT | Qt::Key_G), true },
        // Zoom: "%1" = fator e "%2" = posição no menu (acelerador 1..6).
        { ActionId::ViewZoom1,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom %1x (&%2)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the drawing area magnified %1 times"),
            "zoom_1", Seq(), true, 1 },
        { ActionId::ViewZoom2,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom %1x (&%2)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the drawing area magnified %1 times"),
            "zoom_2", Seq(), true, 2 },
        { ActionId::ViewZoom4,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom %1x (&%2)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the drawing area magnified %1 times"),
            "zoom_4", Seq(), true, 4 },
        { ActionId::ViewZoom6,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom %1x (&%2)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the drawing area magnified %1 times"),
            "zoom_6", Seq(), true, 6 },
        { ActionId::ViewZoom8,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom %1x (&%2)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the drawing area magnified %1 times"),
            "zoom_8", Seq(), true, 8 },
        { ActionId::ViewZoom12,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Zoom %1x (&%2)"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Show the drawing area magnified %1 times"),
            "zoom_12", Seq(), true, 12 },
        { ActionId::ViewAnimation,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Animation..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Open the window used to preview the character animation"),
            "view_animation", Seq(Qt::Key_F2) },
        // --- Canvas ---
        { ActionId::CanvasResize,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Set Canvas &Size..."),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Change the size of the canvas"),
            "canvas_resize", Seq(Qt::CTRL | Qt::Key_E) },
        { ActionId::CanvasClearAll,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Clear All"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Erase the whole canvas"), nullptr,
            Seq(Qt::CTRL | Qt::SHIFT | Qt::Key_N) },
        { ActionId::CanvasGetAll,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Transfer All"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Transfer the whole canvas to the drawing area"),
            nullptr, Seq(Qt::Key_F3) },
        // --- Paleta ---
        { ActionId::PaletteTransparent,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Transparent Color Effect"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Treat the background color as transparent when transferring images"),
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
            "palette_edit", Seq(Qt::SHIFT | Qt::Key_E) },
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
        { ActionId::WindowCascade, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Cascade"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Arrange the canvas windows in cascade"),
            nullptr, Seq() },
        { ActionId::WindowTileHorizontal,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Tile &Horizontally"),
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog",
                "Arrange the canvas windows one above the other"),
            nullptr, Seq() },
        { ActionId::WindowTileVertical,
            QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "Tile &Vertically"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Arrange the canvas windows side by side"),
            nullptr, Seq() },
        { ActionId::WindowFit, QT_TRANSLATE_NOOP("spritenator::gui::ActionCatalog", "&Fit"),
            QT_TRANSLATE_NOOP(
                "spritenator::gui::ActionCatalog", "Resize the canvas window to fit the canvas"),
            nullptr, Seq(Qt::CTRL | Qt::Key_W) },
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
    , m_zoomGroup(new QActionGroup(this))
{
    m_zoomGroup->setExclusive(true); // apenas um zoom marcado, como "radio buttons"

    for (const ActionSpec& spec : actionSpecs()) {
        auto* created = new QAction(this);
        if (spec.icon != nullptr) {
            created->setIcon(IconProvider::icon(QString::fromLatin1(spec.icon)));
        }
        created->setShortcut(spec.shortcut);
        created->setCheckable(spec.checkable);
        if (spec.zoomFactor > 0) {
            created->setData(spec.zoomFactor); // consultado pela MainWindow ao trocar o zoom
            m_zoomGroup->addAction(created);
        }
        m_actions.at(static_cast<std::size_t>(spec.id)) = created;
    }

    // Estados iniciais.
    action(ActionId::ViewMainGrid)->setChecked(true);
    action(ActionId::ViewZoom8)->setChecked(true);

    // Garante, em builds de depuração, que a tabela cobre todas as ações.
    for ([[maybe_unused]] QAction* created : m_actions) {
        assert(created != nullptr && "ActionCatalog: ação ausente da tabela");
    }
    retranslate();
}

void ActionCatalog::retranslate()
{
    int zoomPosition = 1; // posição no menu, usada como acelerador (&1..&6)
    for (const ActionSpec& spec : actionSpecs()) {
        QAction* target = action(spec.id);
        QString text = tr(spec.text);
        QString tip = tr(spec.statusTip);
        if (spec.zoomFactor > 0) {
            text = text.arg(spec.zoomFactor).arg(zoomPosition++);
            tip = tip.arg(spec.zoomFactor);
        }
        target->setText(text);
        target->setStatusTip(tip);
        target->setToolTip(tip);
    }
}

QAction* ActionCatalog::action(ActionId id) const
{
    return m_actions.at(static_cast<std::size_t>(id));
}

int ActionCatalog::zoomFactorOf(ActionId id)
{
    for (const ActionSpec& spec : actionSpecs()) {
        if (spec.id == id) {
            return spec.zoomFactor;
        }
    }
    return 0;
}

} // namespace spritenator::gui
