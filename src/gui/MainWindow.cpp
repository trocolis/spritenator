// =============================================================================
// MainWindow.cpp
// =============================================================================
#include "MainWindow.h"

#include "ActionCatalog.h"
#include "AnimationWindow.h"
#include "CanvasView.h"
#include "CanvasWindow.h"
#include "Document.h"
#include "DrawBuffer.h"
#include "EditOperations.h"
#include "EditorSession.h"
#include "HelpWindow.h"
#include "IconProvider.h"
#include "IntegratorPanel.h"
#include "Translations.h"
#include "commands/ImageCommands.h"
#include "core/PaletteOps.h"
#include "core/Transform.h"
#include "dialogs/BackupFolderDialog.h"
#include "dialogs/CanvasSizeDialog.h"
#include "dialogs/CharacterSizeDialog.h"
#include "dialogs/LoadPaletteDialog.h"
#include "dialogs/NewImageDialog.h"
#include "dialogs/PaletteEditDialog.h"
#include "dialogs/TextInputDialog.h"
#include "io/Backup.h"
#include "io/ImageFile.h"
#include "io/IoError.h"
#include "io/PaletteFile.h"
#include "widgets/DrawAreaView.h"

#include <spritenator/Version.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoGroup>
#include <QUndoStack>

#include <algorithm>
#include <exception>
#include <variant>

namespace spritenator::gui {

namespace {

/// Converte std::string_view (UTF-8) em QString.
QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

/// Garante que o caminho termine com a extensão do filtro escolhido no
/// diálogo de salvar (ex.: usuário digitou "heroi" e escolheu PNG).
QString ensureExtension(const QString& path, const QString& selectedFilter)
{
    if (!QFileInfo(path).suffix().isEmpty()) {
        return path;
    }
    const bool isBmp = selectedFilter.contains(QLatin1String("*.bmp"));
    return path + (isBmp ? QStringLiteral(".bmp") : QStringLiteral(".png"));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_session(new EditorSession(this))
    , m_operations(new EditOperations(m_session, this))
    , m_actions(new ActionCatalog(this))
    , m_mdiArea(new QMdiArea(this))
{
    setWindowTitle(toQString(version::kApplicationName));
    setWindowIcon(IconProvider::icon(QStringLiteral("app_icon")));
    setAcceptDrops(true); // abrir imagens arrastando-as para a janela

    // A área MDI ocupa o centro da janela, como no original.
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setActivationOrder(QMdiArea::ActivationHistoryOrder);

    // Layout do original: Integrator de um lado, canvas (MDI) do outro.
    m_integrator = new IntegratorPanel(m_session, m_actions, m_operations, this);
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_integrator);
    m_mainSplitter->addWidget(m_mdiArea);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 2);
    m_mainSplitter->setChildrenCollapsible(false);
    setCentralWidget(m_mainSplitter);

    // Preferências persistentes da grade de personagens.
    m_session->setCharacterLayout(m_settings.characterLayout());

    createMenus();
    createToolBar();
    createStatusBar();
    connectActions();
    retranslateUi(); // textos iniciais (e a cada troca de idioma)
    updateRecentFilesMenu();

    // Modo de cor mudou (ex.: "Reverter" trocou um PNG indexado por RGBA).
    connect(m_session, &EditorSession::colorModeChanged, this, &MainWindow::updateActionStates);

    // Janela ativa -> documento ativo da sessão.
    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, [this] {
        m_session->setActiveDocument(activeDocument());
        updateActionStates();
    });
    updateActionStates();

    resize(1024, 720);
    restoreGeometry(m_settings.mainWindowGeometry());
    restoreState(m_settings.mainWindowState());
    restoreViewSettings();
}

MainWindow::~MainWindow()
{
    // Durante ~QWidget os filhos são escondidos e o QMdiArea emite
    // subWindowActivated(nullptr); a sessão então troca a pilha ativa do
    // QUndoGroup, cujos sinais chamariam slots desta classe — que, nesse
    // ponto, já teve o destrutor executado (comportamento indefinido).
    // Desconectar aqui, enquanto o objeto ainda está íntegro, evita isso.
    disconnect(m_mdiArea, nullptr, this, nullptr);
    disconnect(m_session, nullptr, this, nullptr);
    disconnect(m_session->undoGroup(), nullptr, this, nullptr);
    disconnect(m_mdiArea, nullptr, m_session, nullptr);
}

QAction* MainWindow::action(ActionId id) const
{
    return m_actions->action(id);
}

// -----------------------------------------------------------------------------
// Menus: mesma estrutura do menu principal do Character Maker 1999.
// -----------------------------------------------------------------------------
void MainWindow::createMenus()
{
    m_fileMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&File"));
    m_fileMenu->addAction(action(ActionId::FileNew));
    m_fileMenu->addAction(action(ActionId::FileOpen));
    m_fileMenu->addAction(action(ActionId::FileRevert));
    m_fileMenu->addAction(action(ActionId::FileSave));
    m_fileMenu->addAction(action(ActionId::FileSaveAs));
    m_fileMenu->addAction(action(ActionId::FileClose));

    // Arquivos recentes: ações reutilizadas, preenchidas em updateRecentFilesMenu().
    m_recentSeparator = m_fileMenu->addSeparator();
    for (int index = 0; index < AppSettings::kMaxRecentFiles; ++index) {
        auto* recent = new QAction(this);
        recent->setVisible(false);
        connect(recent, &QAction::triggered, this,
            [this, recent] { openFile(recent->data().toString()); });
        m_fileMenu->addAction(recent);
        m_recentFileActions.append(recent);
    }

    m_fileMenu->addSeparator();
    m_fileMenu->addAction(action(ActionId::FileSetBackupFolder));
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(action(ActionId::FileExit));

    QMenu* editMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Edit"));
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

    QMenu* viewMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&View"));
    viewMenu->addAction(action(ActionId::ViewCharaSize));
    viewMenu->addAction(action(ActionId::ViewMainGrid));
    viewMenu->addAction(action(ActionId::ViewSubGrid));
    viewMenu->addSeparator();
    viewMenu->addActions(m_actions->zoomGroup()->actions());
    viewMenu->addSeparator();
    viewMenu->addAction(action(ActionId::ViewAnimation));
    viewMenu->addSeparator();
    createLanguageMenu(viewMenu);

    QMenu* canvasMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Canvas"));
    canvasMenu->addAction(action(ActionId::CanvasResize));
    canvasMenu->addSeparator();
    canvasMenu->addAction(action(ActionId::CanvasClearAll));
    canvasMenu->addAction(action(ActionId::CanvasGetAll));

    QMenu* paletteMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Palette"));
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

    QMenu* windowMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Window"));
    windowMenu->addAction(action(ActionId::WindowCascade));
    windowMenu->addAction(action(ActionId::WindowTileHorizontal));
    windowMenu->addAction(action(ActionId::WindowTileVertical));
    windowMenu->addSeparator();
    windowMenu->addAction(action(ActionId::WindowFit));
    windowMenu->addSeparator();
    windowMenu->addAction(action(ActionId::WindowReverseCanvas));

    QMenu* helpMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Help"));
    helpMenu->addAction(action(ActionId::HelpContents));
    helpMenu->addSeparator();
    helpMenu->addAction(action(ActionId::HelpAbout));
}

// -----------------------------------------------------------------------------
// Barra de ferramentas: mesma ordem dos botões do original.
// -----------------------------------------------------------------------------
void MainWindow::createToolBar()
{
    m_toolBar = addToolBar(QString()); // título definido em retranslateUi()
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
    m_positionLabel = new QLabel(this);
    m_positionLabel->setMinimumWidth(120);
    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->showMessage(tr("Ready"), 2000);
}

// -----------------------------------------------------------------------------
// Ligação das ações aos tratadores. As demais ações são conectadas nas etapas
// seguintes, à medida que as funcionalidades forem implementadas.
// -----------------------------------------------------------------------------
void MainWindow::connectActions()
{
    // --- Arquivo ---
    connect(action(ActionId::FileNew), &QAction::triggered, this, &MainWindow::newImage);
    connect(action(ActionId::FileOpen), &QAction::triggered, this, &MainWindow::openImage);
    connect(action(ActionId::FileSave), &QAction::triggered, this,
        [this] { saveDocument(activeDocument()); });
    connect(action(ActionId::FileSaveAs), &QAction::triggered, this,
        [this] { saveDocumentAs(activeDocument()); });
    connect(action(ActionId::FileRevert), &QAction::triggered, this, &MainWindow::revertDocument);
    connect(action(ActionId::FileClose), &QAction::triggered, m_mdiArea,
        &QMdiArea::closeActiveSubWindow);
    connect(action(ActionId::FileExit), &QAction::triggered, this, &QWidget::close);
    connect(action(ActionId::FileSetBackupFolder), &QAction::triggered, this,
        &MainWindow::setBackupFolder);

    // --- Editar: desfazer/refazer roteados pelo grupo de pilhas ---
    QUndoGroup* undoGroup = m_session->undoGroup();
    connect(action(ActionId::EditUndo), &QAction::triggered, this, [this, undoGroup] {
        m_session->cancelTool(); // não desfazer no meio de um traço
        undoGroup->undo();
    });
    connect(action(ActionId::EditRedo), &QAction::triggered, this, [this, undoGroup] {
        m_session->cancelTool();
        undoGroup->redo();
    });
    connect(undoGroup, &QUndoGroup::canUndoChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::canRedoChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::undoTextChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::redoTextChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::activeStackChanged, this, &MainWindow::updateUndoActions);

    // --- Editar: operações sobre a área de desenho ---
    connect(action(ActionId::EditCut), &QAction::triggered, m_operations, &EditOperations::cut);
    connect(action(ActionId::EditCopy), &QAction::triggered, m_operations, &EditOperations::copy);
    connect(action(ActionId::EditPaste), &QAction::triggered, m_operations, &EditOperations::paste);
    connect(action(ActionId::EditClear), &QAction::triggered, m_operations, &EditOperations::clear);
    connect(action(ActionId::EditSelectAll), &QAction::triggered, m_operations,
        &EditOperations::selectAll);
    connect(action(ActionId::EditShadow), &QAction::triggered, m_operations,
        &EditOperations::prepareShadow);
    connect(action(ActionId::EditDrawText), &QAction::triggered, this, &MainWindow::drawText);
    // "Colar" só fica ativo quando há algo colável na área de transferência.
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this,
        [this] { action(ActionId::EditPaste)->setEnabled(EditOperations::canPaste()); });

    // --- Exibir ---
    connect(
        action(ActionId::ViewCharaSize), &QAction::triggered, this, &MainWindow::editCharacterSize);
    connect(action(ActionId::ViewMainGrid), &QAction::toggled, m_session,
        &EditorSession::setMainGridVisible);
    connect(action(ActionId::ViewSubGrid), &QAction::toggled, m_session,
        &EditorSession::setSubGridVisible);
    m_session->setMainGridVisible(action(ActionId::ViewMainGrid)->isChecked());
    m_session->setSubGridVisible(action(ActionId::ViewSubGrid)->isChecked());

    // O fator de zoom fica no data() de cada ação (ver ActionCatalog).
    connect(m_actions->zoomGroup(), &QActionGroup::triggered, this,
        [this](const QAction* zoomAction) { m_session->setZoom(zoomAction->data().toInt()); });
    // Zoom alterado por outra via (ex.: ferramenta lupa) -> marca a ação certa.
    connect(m_session, &EditorSession::zoomChanged, this, [this](int factor) {
        for (QAction* zoomAction : m_actions->zoomGroup()->actions()) {
            zoomAction->setChecked(zoomAction->data().toInt() == factor);
        }
    });
    m_session->setZoom(8);

    // --- Canvas ---
    connect(action(ActionId::CanvasGetAll), &QAction::triggered, m_session,
        &EditorSession::transferAll);
    connect(action(ActionId::CanvasResize), &QAction::triggered, this, &MainWindow::resizeCanvas);
    connect(action(ActionId::CanvasClearAll), &QAction::triggered, this, &MainWindow::clearCanvas);
    connect(action(ActionId::ViewAnimation), &QAction::triggered, this,
        &MainWindow::showAnimationWindow);

    // --- Paleta ---
    connect(action(ActionId::PaletteTransparent), &QAction::toggled, m_session,
        &EditorSession::setTransparentEffect);
    connect(action(ActionId::PaletteLoad), &QAction::triggered, this, &MainWindow::loadPalette);
    connect(action(ActionId::PaletteSave), &QAction::triggered, this, &MainWindow::savePalette);
    connect(action(ActionId::PaletteEdit), &QAction::triggered, this, &MainWindow::editPalette);
    connect(
        action(ActionId::PaletteGradation), &QAction::triggered, this, &MainWindow::applyGradation);

    // Máscara: as ações espelham o estado da sessão nos dois sentidos.
    connect(action(ActionId::PaletteMaskMode), &QAction::toggled, m_session,
        &EditorSession::setMaskMode);
    connect(m_session, &EditorSession::maskModeChanged, action(ActionId::PaletteMaskMode),
        &QAction::setChecked);
    connect(action(ActionId::PaletteAutoMask), &QAction::toggled, m_session,
        &EditorSession::setAutoMask);
    connect(m_session, &EditorSession::autoMaskChanged, action(ActionId::PaletteAutoMask),
        &QAction::setChecked);
    connect(action(ActionId::PaletteClearMask), &QAction::triggered, m_session,
        &EditorSession::clearMask);
    connect(action(ActionId::PaletteInvertMask), &QAction::triggered, m_session,
        &EditorSession::invertMask);

    // --- Barra de status: posição na área de desenho ---
    connect(m_integrator->drawArea(), &DrawAreaView::pixelHovered, this,
        [this](const core::Point& pixel) { showPosition(tr("Draw"), pixel); });

    // --- Janela ---
    connect(action(ActionId::WindowReverseCanvas), &QAction::toggled, this,
        &MainWindow::setCanvasReversed);
    connect(action(ActionId::WindowCascade), &QAction::triggered, m_mdiArea,
        &QMdiArea::cascadeSubWindows);
    connect(action(ActionId::WindowTileHorizontal), &QAction::triggered, this,
        [this] { tileWindows(Qt::Vertical); }); // janelas empilhadas (uma sobre a outra)
    connect(action(ActionId::WindowTileVertical), &QAction::triggered, this,
        [this] { tileWindows(Qt::Horizontal); }); // janelas lado a lado
    connect(action(ActionId::WindowFit), &QAction::triggered, this, &MainWindow::fitActiveWindow);

    // --- Ajuda ---
    connect(action(ActionId::HelpContents), &QAction::triggered, this, &MainWindow::showHelp);
    connect(action(ActionId::HelpAbout), &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::updateActionStates()
{
    // Ações sempre disponíveis (não dependem de documento).
    const ActionId alwaysEnabled[] = {
        ActionId::FileNew,
        ActionId::FileOpen,
        ActionId::FileExit,
        ActionId::FileSetBackupFolder,
        ActionId::HelpContents,
        ActionId::EditCut,
        ActionId::EditCopy,
        ActionId::EditClear,
        ActionId::EditSelectAll,
        ActionId::EditDrawText,
        ActionId::EditShadow,
        ActionId::ViewCharaSize,
        ActionId::ViewMainGrid,
        ActionId::ViewSubGrid,
        ActionId::ViewZoom1,
        ActionId::ViewZoom2,
        ActionId::ViewZoom4,
        ActionId::ViewZoom6,
        ActionId::ViewZoom8,
        ActionId::ViewZoom12,
        ActionId::ViewAnimation,
        ActionId::PaletteTransparent,
        ActionId::WindowReverseCanvas,
        ActionId::HelpAbout,
    };
    // Ações de paleta: só existem em imagens indexadas (sem documento, a
    // máscara continua disponível, como antes).
    const ActionId needPalette[] = {
        ActionId::PaletteMaskMode,
        ActionId::PaletteClearMask,
        ActionId::PaletteInvertMask,
        ActionId::PaletteAutoMask,
    };
    const ActionId needIndexedDocument[] = {
        ActionId::PaletteLoad,
        ActionId::PaletteSave,
        ActionId::PaletteEdit,
        ActionId::PaletteGradation,
    };
    // Ações que exigem um documento ativo.
    const ActionId needDocument[] = {
        ActionId::FileSave,
        ActionId::FileSaveAs,
        ActionId::FileClose,
        ActionId::CanvasGetAll,
        ActionId::CanvasResize,
        ActionId::CanvasClearAll,
        ActionId::WindowCascade,
        ActionId::WindowTileHorizontal,
        ActionId::WindowTileVertical,
        ActionId::WindowFit,
    };

    for (std::size_t index = 0; index < kActionCount; ++index) {
        action(static_cast<ActionId>(index))->setEnabled(false);
    }
    for (const ActionId id : alwaysEnabled) {
        action(id)->setEnabled(true);
    }

    const Document* document = activeDocument();
    for (const ActionId id : needDocument) {
        action(id)->setEnabled(document != nullptr);
    }
    const bool indexed = m_session->colorMode() == core::ColorMode::Indexed;
    for (const ActionId id : needPalette) {
        action(id)->setEnabled(indexed);
    }
    for (const ActionId id : needIndexedDocument) {
        action(id)->setEnabled(document != nullptr && indexed);
    }
    // Reverter só faz sentido para documentos que já existem em disco.
    action(ActionId::FileRevert)->setEnabled(document != nullptr && document->hasFilePath());
    action(ActionId::EditPaste)->setEnabled(EditOperations::canPaste());
    updateUndoActions();
}

void MainWindow::updateUndoActions()
{
    const QUndoGroup* group = m_session->undoGroup();
    QAction* undo = action(ActionId::EditUndo);
    QAction* redo = action(ActionId::EditRedo);
    undo->setEnabled(group->canUndo());
    redo->setEnabled(group->canRedo());
    // Mostra o que será desfeito/refeito, ex.: "Undo Pen".
    undo->setText(group->canUndo() ? tr("&Undo %1").arg(group->undoText()) : tr("&Undo"));
    redo->setText(group->canRedo() ? tr("&Redo %1").arg(group->redoText()) : tr("&Redo"));
}

// -----------------------------------------------------------------------------
// Arquivo > Novo
// -----------------------------------------------------------------------------
void MainWindow::newImage()
{
    const Document* current = activeDocument();
    NewImageDialog dialog(current != nullptr, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (dialog.colorMode() == core::ColorMode::Rgba) {
        // RGBA: canvas totalmente transparente; a paleta padrão serve apenas
        // para converter conteúdo vindo de documentos indexados.
        addDocumentWindow(new Document(core::RgbaImage(dialog.imageSize()),
            core::Palette::makeDefault(core::ColorDepth::Bits8)));
        return;
    }
    const core::ColorDepth depth = dialog.colorDepth();
    core::Palette palette = core::Palette::makeDefault(depth);
    if (dialog.copyCurrentPalette() && current != nullptr) {
        palette = current->palette();
        palette.setDepth(depth); // ajusta ao número de cores escolhido
    }
    // Canvas preenchido com o índice 0 (cor de fundo/transparente no RPG Maker).
    addDocumentWindow(new Document(core::IndexedImage(dialog.imageSize(), 0), palette));
}

// -----------------------------------------------------------------------------
// Arquivo > Abrir
// -----------------------------------------------------------------------------
void MainWindow::openImage()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Open Image"),
        m_settings.lastDirectory(), io::ImageFile::openFilters().join(QStringLiteral(";;")));
    for (const QString& path : paths) {
        openFile(path);
    }
}

bool MainWindow::openFile(const QString& path)
{
    // Se já estiver aberto, apenas ativa a janela existente.
    if (QMdiSubWindow* existing = findWindowForFile(path)) {
        m_mdiArea->setActiveSubWindow(existing);
        return true;
    }

    try {
        Document* document = Document::load(path);
        addDocumentWindow(document);
    } catch (const io::IoError& error) {
        m_settings.removeRecentFile(QFileInfo(path).absoluteFilePath());
        updateRecentFilesMenu();
        showError(error.message());
        return false;
    } catch (const std::exception& error) {
        // Rede de segurança: nenhum arquivo malformado deve derrubar o programa.
        showError(tr("Cannot open \"%1\": %2").arg(path, QString::fromLocal8Bit(error.what())));
        return false;
    }

    const QFileInfo info(path);
    m_settings.setLastDirectory(info.absolutePath());
    m_settings.addRecentFile(info.absoluteFilePath());
    updateRecentFilesMenu();
    statusBar()->showMessage(tr("Opened %1").arg(info.fileName()), 3000);
    return true;
}

// -----------------------------------------------------------------------------
// Arquivo > Salvar / Salvar como
// -----------------------------------------------------------------------------
bool MainWindow::saveDocument(Document* document)
{
    if (document == nullptr) {
        return false;
    }
    if (!document->hasFilePath()) {
        return saveDocumentAs(document);
    }
    if (!backupBeforeSaving(document->filePath())) {
        return false;
    }
    try {
        document->save();
    } catch (const io::IoError& error) {
        showError(error.message());
        return false;
    }
    statusBar()->showMessage(tr("Saved %1").arg(document->displayName()), 3000);
    return true;
}

bool MainWindow::saveDocumentAs(Document* document)
{
    if (document == nullptr) {
        return false;
    }
    const QString suggested = document->hasFilePath()
        ? document->filePath()
        : QDir(m_settings.lastDirectory())
              .filePath(document->displayName() + QStringLiteral(".png"));

    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Image As"), suggested,
        io::ImageFile::saveFilters(document->colorMode()).join(QStringLiteral(";;")),
        &selectedFilter);
    if (path.isEmpty()) {
        return false; // usuário cancelou
    }
    path = ensureExtension(path, selectedFilter);
    if (!backupBeforeSaving(path)) {
        return false;
    }

    try {
        document->saveAs(path);
    } catch (const io::IoError& error) {
        showError(error.message());
        return false;
    }

    const QFileInfo info(path);
    m_settings.setLastDirectory(info.absolutePath());
    m_settings.addRecentFile(info.absoluteFilePath());
    updateRecentFilesMenu();
    updateActionStates(); // "Reverter" passa a estar disponível
    statusBar()->showMessage(tr("Saved %1").arg(info.fileName()), 3000);
    return true;
}

// -----------------------------------------------------------------------------
// Arquivo > Reverter
// -----------------------------------------------------------------------------
void MainWindow::revertDocument()
{
    Document* document = activeDocument();
    if (document == nullptr || !document->hasFilePath()) {
        return;
    }
    if (document->isModified()) {
        const auto answer = QMessageBox::question(this, tr("Revert"),
            tr("Discard all changes to \"%1\" and reload it from disk?")
                .arg(document->displayName()));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    try {
        document->revert();
    } catch (const io::IoError& error) {
        showError(error.message());
    }
}

// -----------------------------------------------------------------------------
// Arquivo > Pasta de backup
// -----------------------------------------------------------------------------
void MainWindow::setBackupFolder()
{
    BackupFolderDialog dialog(m_settings.backupFolder(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_settings.setBackupFolder(dialog.folder());
    }
}

bool MainWindow::backupBeforeSaving(const QString& path)
{
    const QString folder = m_settings.backupFolder();
    if (folder.isEmpty()) {
        return true; // backup desativado
    }
    try {
        io::createBackup(path, folder);
    } catch (const io::IoError& error) {
        // Falha no backup não deve impedir o usuário de salvar: ele decide.
        const auto answer = QMessageBox::warning(this, toQString(version::kApplicationName),
            tr("%1\n\nSave anyway?").arg(error.message()), QMessageBox::Save | QMessageBox::Cancel);
        return answer == QMessageBox::Save;
    }
    return true;
}

bool MainWindow::maybeSave(Document* document)
{
    if (document == nullptr || !document->isModified()) {
        return true;
    }
    const auto answer = QMessageBox::warning(this, toQString(version::kApplicationName),
        tr("\"%1\" has been modified.\nDo you want to save your changes?")
            .arg(document->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    switch (answer) {
    case QMessageBox::Save: return saveDocument(document);
    case QMessageBox::Discard: return true;
    default: return false;
    }
}

// -----------------------------------------------------------------------------
// Gerenciamento de janelas-filhas
// -----------------------------------------------------------------------------
CanvasWindow* MainWindow::addDocumentWindow(Document* document)
{
    auto* window = new CanvasWindow(document);
    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(window);
    subWindow->setWindowIcon(IconProvider::icon(QStringLiteral("app_icon")));

    // Intercepta o fechamento da janela-filha para perguntar se deve salvar.
    subWindow->installEventFilter(this);

    // Cliques nas células: direito traz para a área de desenho; esquerdo grava.
    CanvasView* view = window->view();
    view->setCharacterLayout(m_session->characterLayout());
    connect(view, &CanvasView::getRequested, this,
        [this, document](const core::Rect& cell) { m_session->getFromCanvas(document, cell); });
    connect(view, &CanvasView::putRequested, this,
        [this, document](const core::Rect& cell) { m_session->putToCanvas(document, cell); });

    // Barra de status: posição do pixel sob o mouse.
    connect(view, &CanvasView::pixelHovered, this,
        [this](const core::Point& pixel) { showPosition(tr("Canvas"), pixel); });
    connect(document, &Document::filePathChanged, this, &MainWindow::updateActionStates);

    window->show();
    subWindow->adjustSize(); // janela do tamanho do canvas (limitada pela área MDI)
    // Ativa explicitamente: sem isso a ativação depende do foco da janela
    // principal, e o documento novo poderia não virar o documento ativo.
    m_mdiArea->setActiveSubWindow(subWindow);
    return window;
}

QMdiSubWindow* MainWindow::findWindowForFile(const QString& path) const
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (QMdiSubWindow* subWindow : m_mdiArea->subWindowList()) {
        const auto* window = qobject_cast<CanvasWindow*>(subWindow->widget());
        if (window != nullptr && window->document()->filePath() == absolute) {
            return subWindow;
        }
    }
    return nullptr;
}

CanvasWindow* MainWindow::activeCanvasWindow() const
{
    const QMdiSubWindow* subWindow = m_mdiArea->activeSubWindow();
    return subWindow != nullptr ? qobject_cast<CanvasWindow*>(subWindow->widget()) : nullptr;
}

Document* MainWindow::activeDocument() const
{
    const CanvasWindow* window = activeCanvasWindow();
    return window != nullptr ? window->document() : nullptr;
}

void MainWindow::updateRecentFilesMenu()
{
    const QStringList files = m_settings.recentFiles();
    for (int index = 0; index < m_recentFileActions.size(); ++index) {
        QAction* recent = m_recentFileActions.at(index);
        if (index < files.size()) {
            const QString& path = files.at(index);
            // "&1 nome.png": o número serve de atalho de teclado no menu.
            recent->setText(
                QStringLiteral("&%1 %2").arg(index + 1).arg(QFileInfo(path).fileName()));
            recent->setStatusTip(path);
            recent->setData(path);
            recent->setVisible(true);
        } else {
            recent->setVisible(false);
        }
    }
    m_recentSeparator->setVisible(!files.isEmpty());
}

// -----------------------------------------------------------------------------
// Janela > Lado a lado / Ajustar
// -----------------------------------------------------------------------------
void MainWindow::tileWindows(Qt::Orientation orientation)
{
    // O QMdiArea só oferece um "tile" em grade; o original permitia escolher
    // empilhar verticalmente ou lado a lado, então dividimos a área à mão.
    QList<QMdiSubWindow*> windows;
    for (QMdiSubWindow* subWindow : m_mdiArea->subWindowList()) {
        if (subWindow->isVisible() && !subWindow->isMinimized()) {
            subWindow->showNormal();
            windows.append(subWindow);
        }
    }
    if (windows.isEmpty()) {
        return;
    }

    const QRect area = m_mdiArea->viewport()->rect();
    const auto count = static_cast<int>(windows.size());
    for (int index = 0; index < count; ++index) {
        QRect slot;
        if (orientation == Qt::Horizontal) { // lado a lado
            const int width = area.width() / count;
            slot = QRect(area.x() + index * width, area.y(), width, area.height());
        } else { // empilhadas
            const int height = area.height() / count;
            slot = QRect(area.x(), area.y() + index * height, area.width(), height);
        }
        windows.at(index)->setGeometry(slot);
    }
}

void MainWindow::fitActiveWindow()
{
    if (QMdiSubWindow* subWindow = m_mdiArea->activeSubWindow()) {
        subWindow->showNormal();
        subWindow->adjustSize();
    }
}

void MainWindow::setCanvasReversed(bool reversed)
{
    // Índice 0 = lado esquerdo do splitter.
    m_mainSplitter->insertWidget(0, reversed ? static_cast<QWidget*>(m_mdiArea) : m_integrator);
}

// -----------------------------------------------------------------------------
// Editar > Entrada de texto
// -----------------------------------------------------------------------------
void MainWindow::drawText()
{
    TextInputDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        m_operations->drawText(dialog.text(), dialog.selectedFont());
    }
}

// -----------------------------------------------------------------------------
// Paleta
// -----------------------------------------------------------------------------
void MainWindow::commitDocumentState(Document* document, const core::AnyImage& canvas,
    const core::Palette& palette, const UndoLabel& undoLabel)
{
    if (canvas == document->canvas() && palette == document->palette()) {
        return; // nada mudou
    }
    document->undoStack()->push(new DocumentStateCommand(
        document, document->canvas(), document->palette(), canvas, palette, undoLabel));
}

void MainWindow::loadPalette()
{
    Document* document = activeDocument();
    if (document == nullptr || document->colorMode() != core::ColorMode::Indexed) {
        return; // paletas só existem em imagens indexadas
    }
    LoadPaletteDialog dialog(document->palette(), m_settings.lastDirectory(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const core::Palette oldPalette = document->palette();
    const core::Palette newPalette = dialog.resultPalette();
    const core::PaletteApplyOptions options = dialog.applyOptions();

    // O buffer da área de desenho usa a mesma paleta: converte-o também.
    DrawBuffer* buffer = m_session->buffer();
    commitDocumentState(document,
        core::applyPalette(document->indexedCanvas(), oldPalette, newPalette, options), newPalette,
        UndoLabel { UndoOperation::LoadPalette });
    if (buffer->colorMode() == core::ColorMode::Indexed) {
        const core::IndexedImage convertedBuffer
            = core::applyPalette(buffer->indexedImage(), oldPalette, newPalette, options);
        if (convertedBuffer != buffer->indexedImage()) {
            buffer->replace(convertedBuffer, UndoLabel { UndoOperation::LoadPalette });
        }
    }
    m_settings.setLastDirectory(QFileInfo(dialog.filePath()).absolutePath());
}

void MainWindow::savePalette()
{
    const Document* document = activeDocument();
    if (document == nullptr) {
        return;
    }
    const QStringList filters = io::PaletteFile::saveFilters();
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Palette"),
        QDir(m_settings.lastDirectory()).filePath(QFileInfo(document->displayName()).baseName()),
        filters.join(QStringLiteral(";;")), &selectedFilter);
    if (path.isEmpty()) {
        return;
    }
    const int filterIndex = static_cast<int>(filters.indexOf(selectedFilter));
    const io::PaletteFormat format = io::PaletteFile::formatFromFilterIndex(filterIndex);
    if (QFileInfo(path).suffix().isEmpty()) {
        path += format == io::PaletteFormat::Act ? QStringLiteral(".act") : QStringLiteral(".pal");
    }
    try {
        io::PaletteFile::save(path, document->palette(), format);
    } catch (const io::IoError& error) {
        showError(error.message());
        return;
    }
    m_settings.setLastDirectory(QFileInfo(path).absolutePath());
    statusBar()->showMessage(tr("Palette saved to %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::editPalette()
{
    Document* document = activeDocument();
    if (document == nullptr || document->colorMode() != core::ColorMode::Indexed) {
        return; // paletas só existem em imagens indexadas
    }
    PaletteEditDialog dialog(document->palette(), document->indexedCanvas(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    commitDocumentState(
        document, dialog.canvas(), dialog.palette(), UndoLabel { UndoOperation::EditPalette });

    // Se as cores foram reorganizadas, o buffer acompanha o remapeamento.
    if (dialog.remap() != core::identityRemap()
        && m_session->buffer()->colorMode() == core::ColorMode::Indexed) {
        core::IndexedImage remapped = m_session->buffer()->indexedImage();
        remapped.remap(dialog.remap());
        m_session->buffer()->replace(remapped, UndoLabel { UndoOperation::EditPalette });
    }
}

void MainWindow::applyGradation()
{
    Document* document = activeDocument();
    if (document == nullptr) {
        return;
    }
    core::Palette palette = document->palette();
    palette.interpolate(m_session->drawColor(), m_session->backgroundColor());
    if (palette != document->palette()) {
        document->undoStack()->push(new PaletteCommand(
            document, document->palette(), palette, UndoLabel { UndoOperation::Gradation }));
    }
}

// -----------------------------------------------------------------------------
// Canvas
// -----------------------------------------------------------------------------
void MainWindow::resizeCanvas()
{
    Document* document = activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::Size oldSize = core::sizeOf(document->canvas());
    CanvasSizeDialog dialog(oldSize, this);
    if (dialog.exec() != QDialog::Accepted || dialog.newSize() == oldSize) {
        return;
    }
    core::AnyImage resized;
    if (dialog.scaleContents()) {
        resized = std::visit(
            [&](const auto& image) -> core::AnyImage {
                return core::scaled(image, dialog.newSize());
            },
            document->canvas());
    } else {
        // Conteúdo ancorado no canto; áreas novas com o pixel de fundo do modo.
        resized = m_session->blankLike(document->canvas(), dialog.newSize());
        core::pasteImage(resized, document->canvas(), { 0, 0 });
    }
    document->undoStack()->push(new ReplaceImageCommand(
        document, document->canvas(), resized, UndoLabel { UndoOperation::CanvasSize }));
}

void MainWindow::clearCanvas()
{
    Document* document = activeDocument();
    if (document == nullptr) {
        return;
    }
    // Indexada: cor de fundo; RGBA: transparente.
    const core::AnyImage cleared
        = m_session->blankLike(document->canvas(), core::sizeOf(document->canvas()));
    if (cleared != document->canvas()) {
        document->undoStack()->push(new ReplaceImageCommand(
            document, document->canvas(), cleared, UndoLabel { UndoOperation::ClearAll }));
    }
}

// -----------------------------------------------------------------------------
// Exibir > Idioma
// -----------------------------------------------------------------------------
void MainWindow::createLanguageMenu(QMenu* viewMenu)
{
    QMenu* languageMenu = addTranslatedMenu(viewMenu, QT_TR_NOOP("&Language"));
    auto* group = new QActionGroup(languageMenu);
    group->setExclusive(true);

    // Código vazio = idioma do sistema; "en" = inglês original; demais =
    // traduções embutidas. Os nomes dos idiomas aparecem na própria língua
    // ("English", "Português (Brasil)") e por isso não são traduzidos; só
    // "Padrão do sistema" é retraduzido em retranslateUi().
    QList<QPair<QString, QString>> entries = {
        { QString(), QString() },
        { QStringLiteral("en"), QStringLiteral("English") },
    };
    for (const QString& code : Translations::availableLanguages()) {
        const QLocale locale(code);
        QString name = locale.nativeLanguageName();
        if (locale.territory() != QLocale::AnyTerritory) {
            name += QStringLiteral(" (%1)").arg(locale.nativeTerritoryName());
        }
        name[0] = name[0].toUpper();
        entries.append({ code, name });
    }

    const QString current = m_settings.language();
    for (const auto& [code, name] : entries) {
        QAction* languageAction = languageMenu->addAction(name);
        languageAction->setCheckable(true);
        languageAction->setChecked(code == current);
        group->addAction(languageAction);
        if (code.isEmpty()) {
            m_systemLanguageAction = languageAction;
        }
        connect(languageAction, &QAction::triggered, this, [this, code] {
            if (code == m_settings.language()) {
                return;
            }
            m_settings.setLanguage(code);
            // Troca imediata: os tradutores são substituídos e o Qt envia
            // QEvent::LanguageChange a todas as janelas (ver changeEvent).
            Translations::apply(Translations::localeFor(code));
        });
    }
}

QMenu* MainWindow::addTranslatedMenu(QWidget* parent, const char* title)
{
    QMenu* menu = nullptr;
    if (auto* bar = qobject_cast<QMenuBar*>(parent)) {
        menu = bar->addMenu(QString());
    } else {
        menu = qobject_cast<QMenu*>(parent)->addMenu(QString());
    }
    m_translatedMenus.append({ menu, title });
    return menu;
}

// -----------------------------------------------------------------------------
// Troca de idioma em tempo de execução
// -----------------------------------------------------------------------------
void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    m_actions->retranslate();
    for (const auto& [menu, title] : std::as_const(m_translatedMenus)) {
        menu->setTitle(tr(title));
    }
    m_toolBar->setWindowTitle(tr("Main"));
    if (m_systemLanguageAction != nullptr) {
        m_systemLanguageAction->setText(tr("System Default"));
    }
    // Nomes das operações já registradas no histórico e textos que dependem
    // do estado ("Desfazer Lápis"); o rótulo de posição é refeito no próximo
    // movimento do mouse.
    retranslateUndoHistory(m_session->undoGroup());
    updateUndoActions();
    m_positionLabel->clear();
}

// -----------------------------------------------------------------------------
// Exibir > Animação
// -----------------------------------------------------------------------------
void MainWindow::showAnimationWindow()
{
    if (m_animationWindow == nullptr) {
        m_animationWindow = new AnimationWindow(m_session, this);
        m_animationWindow->resize(420, 300);
    }
    m_animationWindow->show();
    m_animationWindow->raise();
    m_animationWindow->activateWindow();
}

// -----------------------------------------------------------------------------
// Exibir > Tamanho do personagem
// -----------------------------------------------------------------------------
void MainWindow::editCharacterSize()
{
    CharacterSizeDialog dialog(m_session->characterLayout(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_session->setCharacterLayout(dialog.layout());
    m_settings.setCharacterLayout(dialog.layout());
    applyCharacterLayoutToWindows();
}

void MainWindow::applyCharacterLayoutToWindows()
{
    for (QMdiSubWindow* subWindow : m_mdiArea->subWindowList()) {
        if (auto* window = qobject_cast<CanvasWindow*>(subWindow->widget())) {
            window->view()->setCharacterLayout(m_session->characterLayout());
        }
    }
}

void MainWindow::showPosition(const QString& area, const core::Point& pixel)
{
    m_positionLabel->setText(
        QStringLiteral("%1  X: %2  Y: %3").arg(area).arg(pixel.x).arg(pixel.y));
}

// -----------------------------------------------------------------------------
// Eventos
// -----------------------------------------------------------------------------
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Fechamento de uma janela-filha: pergunta se deve salvar.
    if (event->type() == QEvent::Close) {
        if (auto* subWindow = qobject_cast<QMdiSubWindow*>(watched)) {
            const auto* window = qobject_cast<CanvasWindow*>(subWindow->widget());
            if (window != nullptr && !maybeSave(window->document())) {
                event->ignore();
                return true; // fechamento cancelado
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Fecha todas as janelas; se alguma for cancelada, a aplicação não sai.
    // Janelas fechadas ficam ocultas até serem destruídas (deleteLater), por
    // isso verificamos a visibilidade em vez do tamanho da lista.
    m_mdiArea->closeAllSubWindows();
    const auto windows = m_mdiArea->subWindowList();
    const bool anyOpen = std::any_of(windows.begin(), windows.end(),
        [](const QMdiSubWindow* subWindow) { return subWindow->isVisible(); });
    if (anyOpen) {
        event->ignore();
        return;
    }
    m_settings.setMainWindowGeometry(saveGeometry());
    m_settings.setMainWindowState(saveState());
    saveViewSettings();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            openFile(url.toLocalFile());
        }
    }
    event->acceptProposedAction();
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

void MainWindow::showHelp()
{
    if (m_helpWindow == nullptr) {
        m_helpWindow = new HelpWindow(m_actions, this);
    }
    m_helpWindow->show();
    m_helpWindow->raise();
    m_helpWindow->activateWindow();
}

// -----------------------------------------------------------------------------
// Preferências de exibição (restauradas ao abrir, gravadas ao sair)
// -----------------------------------------------------------------------------
void MainWindow::restoreViewSettings()
{
    const ViewSettings view = m_settings.viewSettings();
    // As ações propagam o estado para a sessão pelos sinais já conectados.
    action(ActionId::ViewMainGrid)->setChecked(view.mainGridVisible);
    action(ActionId::ViewSubGrid)->setChecked(view.subGridVisible);
    action(ActionId::PaletteTransparent)->setChecked(view.transparentEffect);
    action(ActionId::WindowReverseCanvas)->setChecked(view.canvasReversed);
    m_session->setZoom(view.zoom);
    if (!view.splitterState.isEmpty()) {
        m_mainSplitter->restoreState(view.splitterState);
    }
}

void MainWindow::saveViewSettings()
{
    ViewSettings view;
    view.zoom = m_session->zoom();
    view.mainGridVisible = m_session->mainGridVisible();
    view.subGridVisible = m_session->subGridVisible();
    view.transparentEffect = m_session->transparentEffect();
    view.canvasReversed = action(ActionId::WindowReverseCanvas)->isChecked();
    view.splitterState = m_mainSplitter->saveState();
    m_settings.setViewSettings(view);
}

void MainWindow::showError(const QString& message)
{
    QMessageBox::critical(this, toQString(version::kApplicationName), message);
}

} // namespace spritenator::gui
