// =============================================================================
// MainWindow.cpp
// =============================================================================
#include "MainWindow.h"

#include "ActionCatalog.h"
#include "IconProvider.h"

#include <spritenator/Version.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMdiArea>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

namespace spritenator::gui {

namespace {

/// Converte std::string_view (UTF-8) em QString.
QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_actions(new ActionCatalog(this))
    , m_mdiArea(new QMdiArea(this))
{
    setWindowTitle(toQString(version::kApplicationName));
    setWindowIcon(IconProvider::icon(QStringLiteral("app_icon")));

    // A área MDI ocupa o centro da janela, como no original.
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setCentralWidget(m_mdiArea);

    createMenus();
    createToolBar();
    createStatusBar();
    connectActions();
    updateActionStates();

    resize(1024, 720);
}

MainWindow::~MainWindow() = default;

QAction* MainWindow::action(ActionId id) const
{
    return m_actions->action(id);
}

// -----------------------------------------------------------------------------
// Menus: mesma estrutura do menu principal do Character Maker 1999.
// -----------------------------------------------------------------------------
void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(action(ActionId::FileNew));
    fileMenu->addAction(action(ActionId::FileOpen));
    fileMenu->addAction(action(ActionId::FileRevert));
    fileMenu->addAction(action(ActionId::FileSave));
    fileMenu->addAction(action(ActionId::FileSaveAs));
    fileMenu->addAction(action(ActionId::FileClose));
    fileMenu->addSeparator();
    fileMenu->addAction(action(ActionId::FileSetBackupFolder));
    fileMenu->addSeparator();
    fileMenu->addAction(action(ActionId::FileExit));

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(action(ActionId::EditUndo));
    editMenu->addAction(action(ActionId::EditRedo));
    editMenu->addSeparator();
    editMenu->addAction(action(ActionId::EditCut));
    editMenu->addAction(action(ActionId::EditCopy));
    editMenu->addAction(action(ActionId::EditPaste));
    editMenu->addAction(action(ActionId::EditClear));
    editMenu->addAction(action(ActionId::EditSelectAll));
    editMenu->addSeparator();
    editMenu->addAction(action(ActionId::EditDrawText));
    editMenu->addSeparator();
    editMenu->addAction(action(ActionId::EditShadow));

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(action(ActionId::ViewCharaSize));
    viewMenu->addAction(action(ActionId::ViewMainGrid));
    viewMenu->addAction(action(ActionId::ViewSubGrid));
    viewMenu->addSeparator();
    viewMenu->addActions(m_actions->zoomGroup()->actions());
    viewMenu->addSeparator();
    viewMenu->addAction(action(ActionId::ViewAnimation));

    QMenu* canvasMenu = menuBar()->addMenu(tr("&Canvas"));
    canvasMenu->addAction(action(ActionId::CanvasResize));
    canvasMenu->addSeparator();
    canvasMenu->addAction(action(ActionId::CanvasClearAll));
    canvasMenu->addAction(action(ActionId::CanvasGetAll));

    QMenu* paletteMenu = menuBar()->addMenu(tr("&Palette"));
    paletteMenu->addAction(action(ActionId::PaletteTransparent));
    paletteMenu->addSeparator();
    paletteMenu->addAction(action(ActionId::PaletteLoad));
    paletteMenu->addAction(action(ActionId::PaletteSave));
    paletteMenu->addAction(action(ActionId::PaletteEdit));
    paletteMenu->addSeparator();
    paletteMenu->addAction(action(ActionId::PaletteMaskMode));
    paletteMenu->addAction(action(ActionId::PaletteClearMask));
    paletteMenu->addAction(action(ActionId::PaletteInvertMask));
    paletteMenu->addAction(action(ActionId::PaletteAutoMask));
    paletteMenu->addSeparator();
    paletteMenu->addAction(action(ActionId::PaletteGradation));

    QMenu* windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(action(ActionId::WindowCascade));
    windowMenu->addAction(action(ActionId::WindowTileHorizontal));
    windowMenu->addAction(action(ActionId::WindowTileVertical));
    windowMenu->addSeparator();
    windowMenu->addAction(action(ActionId::WindowFit));
    windowMenu->addSeparator();
    windowMenu->addAction(action(ActionId::WindowReverseCanvas));

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(action(ActionId::HelpContents));
    helpMenu->addSeparator();
    helpMenu->addAction(action(ActionId::HelpAbout));
}

// -----------------------------------------------------------------------------
// Barra de ferramentas: mesma ordem dos botões do original.
// -----------------------------------------------------------------------------
void MainWindow::createToolBar()
{
    m_toolBar = addToolBar(tr("Main"));
    m_toolBar->setObjectName(QStringLiteral("MainToolBar")); // necessário p/ salvar estado
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setMovable(false);

    m_toolBar->addAction(action(ActionId::FileNew));
    m_toolBar->addAction(action(ActionId::FileOpen));
    m_toolBar->addAction(action(ActionId::FileSave));
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::EditUndo));
    m_toolBar->addAction(action(ActionId::EditRedo));
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::EditCut));
    m_toolBar->addAction(action(ActionId::EditCopy));
    m_toolBar->addAction(action(ActionId::EditPaste));
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::EditDrawText));
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::ViewCharaSize));
    m_toolBar->addAction(action(ActionId::ViewMainGrid));
    m_toolBar->addAction(action(ActionId::ViewSubGrid));
    m_toolBar->addSeparator();
    m_toolBar->addActions(m_actions->zoomGroup()->actions());
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::ViewAnimation));
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::CanvasResize));
    m_toolBar->addSeparator();
    m_toolBar->addAction(action(ActionId::HelpContents));
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

// -----------------------------------------------------------------------------
// Ligação das ações aos tratadores. As demais ações são conectadas nas etapas
// seguintes, à medida que as funcionalidades forem implementadas.
// -----------------------------------------------------------------------------
void MainWindow::connectActions()
{
    connect(action(ActionId::FileExit), &QAction::triggered, this, &QWidget::close);
    connect(action(ActionId::HelpAbout), &QAction::triggered, this, &MainWindow::showAbout);

    connect(action(ActionId::WindowCascade), &QAction::triggered, m_mdiArea,
        &QMdiArea::cascadeSubWindows);
    connect(action(ActionId::WindowTileHorizontal), &QAction::triggered, m_mdiArea,
        &QMdiArea::tileSubWindows);
    connect(action(ActionId::WindowTileVertical), &QAction::triggered, m_mdiArea,
        &QMdiArea::tileSubWindows);
}

void MainWindow::updateActionStates()
{
    // Nesta versão inicial apenas as ações já implementadas ficam ativas.
    const ActionId implemented[] = {
        ActionId::FileExit,
        ActionId::HelpAbout,
        ActionId::WindowCascade,
        ActionId::WindowTileHorizontal,
        ActionId::WindowTileVertical,
    };
    for (std::size_t index = 0; index < kActionCount; ++index) {
        action(static_cast<ActionId>(index))->setEnabled(false);
    }
    for (const ActionId id : implemented) {
        action(id)->setEnabled(true);
    }
}

// -----------------------------------------------------------------------------
// Ajuda > Sobre
// -----------------------------------------------------------------------------
void MainWindow::showAbout()
{
    const QString text = tr("<h3>%1 %2</h3>"
                            "<p>%3</p>"
                            "<p>A cross-platform recreation of <i>Character Maker 1999</i> "
                            "(Copyright &copy; 1998-2000 Yoji Ojima).</p>"
                            "<p>Built with Qt %4.</p>")
                             .arg(toQString(version::kApplicationName), toQString(version::kString),
                                 toQString(version::kDescription), QString::fromLatin1(qVersion()));
    QMessageBox::about(this, tr("About %1").arg(toQString(version::kApplicationName)), text);
}

} // namespace spritenator::gui
