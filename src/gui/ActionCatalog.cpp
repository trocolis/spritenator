// =============================================================================
// ActionCatalog.cpp
//
// Os atalhos de teclado reproduzem os do Character Maker 1999 (extraídos do
// TActionList do formulário principal). Os textos seguem a tradução inglesa
// do original, revisada, e passam por tr() para permitir traduções futuras.
// =============================================================================
#include "ActionCatalog.h"

#include "IconProvider.h"

#include <QAction>
#include <QActionGroup>

#include <cassert>

namespace spritenator::gui {

ActionCatalog::ActionCatalog(QObject* parent)
    : QObject(parent)
{
    createFileActions();
    createEditActions();
    createViewActions();
    createCanvasActions();
    createPaletteActions();
    createWindowActions();
    createHelpActions();

    // Garante, em builds de depuração, que nenhuma ação foi esquecida.
    for ([[maybe_unused]] QAction* created : m_actions) {
        assert(created != nullptr && "ActionCatalog: ação não criada");
    }
}

QAction* ActionCatalog::action(ActionId id) const
{
    return m_actions.at(static_cast<std::size_t>(id));
}

int ActionCatalog::zoomFactorOf(ActionId id)
{
    switch (id) {
    case ActionId::ViewZoom1: return 1;
    case ActionId::ViewZoom2: return 2;
    case ActionId::ViewZoom4: return 4;
    case ActionId::ViewZoom6: return 6;
    case ActionId::ViewZoom8: return 8;
    case ActionId::ViewZoom12: return 12;
    default: return 0;
    }
}

QAction* ActionCatalog::create(
    ActionId id, const QString& text, const QString& statusTip, const QString& iconName)
{
    auto* newAction = new QAction(text, this);
    newAction->setStatusTip(statusTip);
    newAction->setToolTip(statusTip);
    if (!iconName.isEmpty()) {
        newAction->setIcon(IconProvider::icon(iconName));
    }
    m_actions.at(static_cast<std::size_t>(id)) = newAction;
    return newAction;
}

// -----------------------------------------------------------------------------
// Menu Arquivo
// -----------------------------------------------------------------------------
void ActionCatalog::createFileActions()
{
    create(ActionId::FileNew, tr("&New..."), tr("Create a new image"), QStringLiteral("file_new"))
        ->setShortcut(QKeySequence::New);
    create(ActionId::FileOpen, tr("&Open..."), tr("Open an existing image"),
        QStringLiteral("file_open"))
        ->setShortcut(QKeySequence::Open);
    create(ActionId::FileRevert, tr("&Revert"),
        tr("Discard all changes and reload the image from disk"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    create(
        ActionId::FileSave, tr("&Save"), tr("Save the current image"), QStringLiteral("file_save"))
        ->setShortcut(QKeySequence::Save);
    create(ActionId::FileSaveAs, tr("Save &As..."),
        tr("Save the current image with another name or format"))
        ->setShortcut(QKeySequence(Qt::Key_F12));
    create(ActionId::FileClose, tr("&Close"), tr("Close the current image"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F4));
    create(ActionId::FileSetBackupFolder, tr("Set &Backup Folder..."),
        tr("Choose the folder where backups are written automatically on save"));
    create(ActionId::FileExit, tr("E&xit"), tr("Quit the application"))
        ->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
}

// -----------------------------------------------------------------------------
// Menu Editar
// -----------------------------------------------------------------------------
void ActionCatalog::createEditActions()
{
    create(
        ActionId::EditUndo, tr("&Undo"), tr("Undo the last operation"), QStringLiteral("edit_undo"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    create(ActionId::EditRedo, tr("&Redo"), tr("Redo the last undone operation"),
        QStringLiteral("edit_redo"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    create(ActionId::EditCut, tr("Cu&t"),
        tr("Move the selection of the drawing area to the clipboard"), QStringLiteral("edit_cut"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
    create(ActionId::EditCopy, tr("&Copy"),
        tr("Copy the selection of the drawing area to the clipboard"), QStringLiteral("edit_copy"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    create(ActionId::EditPaste, tr("&Paste"),
        tr("Paste the clipboard contents into the drawing area"), QStringLiteral("edit_paste"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
    create(ActionId::EditClear, tr("C&lear"),
        tr("Erase the selection of the drawing area without copying it"))
        ->setShortcut(QKeySequence(Qt::Key_Delete));
    create(ActionId::EditSelectAll, tr("Select &All"), tr("Select the whole drawing area"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    create(ActionId::EditDrawText, tr("Text &Input..."), tr("Render text into the drawing area"),
        QStringLiteral("edit_draw_text"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    create(ActionId::EditShadow, tr("Prepare &Shadow"),
        tr("Replace every non-background color with the drawing color"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
}

// -----------------------------------------------------------------------------
// Menu Exibir
// -----------------------------------------------------------------------------
void ActionCatalog::createViewActions()
{
    create(ActionId::ViewCharaSize, tr("&Character Size..."),
        tr("Change the character cell size and the sub grid"), QStringLiteral("view_chara_size"));

    QAction* mainGrid = create(ActionId::ViewMainGrid, tr("&Main Grid"),
        tr("Show or hide the main grid of the drawing area"), QStringLiteral("view_main_grid"));
    mainGrid->setCheckable(true);
    mainGrid->setChecked(true);
    mainGrid->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));

    QAction* subGrid = create(ActionId::ViewSubGrid, tr("&Sub Grid"),
        tr("Show or hide the sub grid of the drawing area"), QStringLiteral("view_sub_grid"));
    subGrid->setCheckable(true);
    subGrid->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_G));

    // Ações de zoom: grupo exclusivo, como "radio buttons".
    m_zoomGroup = new QActionGroup(this);
    m_zoomGroup->setExclusive(true);
    struct ZoomSpec {
        ActionId id;
        int factor;
        const char* icon;
    };
    constexpr ZoomSpec zoomSpecs[] = {
        { ActionId::ViewZoom1, 1, "zoom_1" },
        { ActionId::ViewZoom2, 2, "zoom_2" },
        { ActionId::ViewZoom4, 4, "zoom_4" },
        { ActionId::ViewZoom6, 6, "zoom_6" },
        { ActionId::ViewZoom8, 8, "zoom_8" },
        { ActionId::ViewZoom12, 12, "zoom_12" },
    };
    int position = 1;
    for (const ZoomSpec& spec : zoomSpecs) {
        QAction* zoom = create(spec.id, tr("Zoom %1x (&%2)").arg(spec.factor).arg(position),
            tr("Show the drawing area magnified %1 times").arg(spec.factor),
            QString::fromLatin1(spec.icon));
        zoom->setCheckable(true);
        m_zoomGroup->addAction(zoom);
        ++position;
    }
    action(ActionId::ViewZoom8)->setChecked(true);

    create(ActionId::ViewAnimation, tr("&Animation..."),
        tr("Open the window used to preview the character animation"),
        QStringLiteral("view_animation"))
        ->setShortcut(QKeySequence(Qt::Key_F2));
}

// -----------------------------------------------------------------------------
// Menu Canvas
// -----------------------------------------------------------------------------
void ActionCatalog::createCanvasActions()
{
    create(ActionId::CanvasResize, tr("Set Canvas &Size..."), tr("Change the size of the canvas"),
        QStringLiteral("canvas_resize"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    create(ActionId::CanvasClearAll, tr("&Clear All"), tr("Erase the whole canvas"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    create(ActionId::CanvasGetAll, tr("&Transfer All"),
        tr("Transfer the whole canvas to the drawing area"))
        ->setShortcut(QKeySequence(Qt::Key_F3));
}

// -----------------------------------------------------------------------------
// Menu Paleta
// -----------------------------------------------------------------------------
void ActionCatalog::createPaletteActions()
{
    QAction* transparent = create(ActionId::PaletteTransparent, tr("&Transparent Color Effect"),
        tr("Treat the background color as transparent when transferring images"),
        QStringLiteral("palette_transparent"));
    transparent->setCheckable(true);
    transparent->setShortcut(QKeySequence(Qt::Key_F8));

    create(ActionId::PaletteLoad, tr("&Load Palette..."), tr("Read palette data from a file"),
        QStringLiteral("palette_load"))
        ->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_L));
    create(ActionId::PaletteSave, tr("&Save Palette..."), tr("Write the palette to a file"),
        QStringLiteral("palette_save"))
        ->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_S));
    create(ActionId::PaletteEdit, tr("&Edit Palette..."), tr("Open the palette editor"),
        QStringLiteral("palette_edit"))
        ->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_E));
    create(ActionId::PaletteClearMask, tr("Clear &Mask"),
        tr("Remove the mask from every palette color"), QStringLiteral("mask_clear"))
        ->setShortcut(QKeySequence(Qt::Key_F6));
    create(ActionId::PaletteInvertMask, tr("Inve&rt Mask"),
        tr("Invert the mask state of every palette color"), QStringLiteral("mask_invert"))
        ->setShortcut(QKeySequence(Qt::Key_F7));

    QAction* autoMask = create(ActionId::PaletteAutoMask, tr("&Automask All but Background"),
        tr("Mask every color except the background color (press again to release)"),
        QStringLiteral("mask_auto"));
    autoMask->setCheckable(true);
    autoMask->setShortcut(QKeySequence(Qt::Key_F9));

    QAction* maskMode = create(ActionId::PaletteMaskMode, tr("Mask &Mode"),
        tr("Click palette colors to protect them from being drawn over"),
        QStringLiteral("palette_mask_mode"));
    maskMode->setCheckable(true);
    maskMode->setShortcut(QKeySequence(Qt::Key_F5));

    create(ActionId::PaletteGradation, tr("&Gradation"),
        tr("Interpolate the palette between the drawing and the background colors"),
        QStringLiteral("palette_gradation"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
}

// -----------------------------------------------------------------------------
// Menu Janela
// -----------------------------------------------------------------------------
void ActionCatalog::createWindowActions()
{
    create(ActionId::WindowCascade, tr("&Cascade"), tr("Arrange the canvas windows in cascade"));
    create(ActionId::WindowTileHorizontal, tr("Tile &Horizontally"),
        tr("Arrange the canvas windows one above the other"));
    create(ActionId::WindowTileVertical, tr("Tile &Vertically"),
        tr("Arrange the canvas windows side by side"));
    create(ActionId::WindowFit, tr("&Fit"), tr("Resize the canvas window to fit the canvas"))
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));

    QAction* reverse = create(ActionId::WindowReverseCanvas, tr("&Reverse Canvas"),
        tr("Swap the sides of the tool panel and the canvas area"));
    reverse->setCheckable(true);
}

// -----------------------------------------------------------------------------
// Menu Ajuda
// -----------------------------------------------------------------------------
void ActionCatalog::createHelpActions()
{
    create(ActionId::HelpContents, tr("Help &Contents"), tr("Open the user manual"),
        QStringLiteral("help_contents"))
        ->setShortcut(QKeySequence(Qt::Key_F1));
    create(ActionId::HelpAbout, tr("&About..."), tr("Show version information"));
}

} // namespace spritenator::gui
