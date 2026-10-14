// =============================================================================
// MainWindow.cpp
// =============================================================================
#include "MainWindow.h"

#include "ActionCatalog.h"
#include "AnimationWindow.h"
#include "CanvasEditor.h"
#include "Document.h"
#include "EditOperations.h"
#include "EditorSession.h"
#include "HelpWindow.h"
#include "IconProvider.h"
#include "IntegratorPanel.h"
#include "SelectionTransform.h"
#include "Translations.h"
#include "commands/ImageCommands.h"
#include "core/PaletteOps.h"
#include "core/Transform.h"
#include "dialogs/BackupFolderDialog.h"
#include "dialogs/CanvasSizeDialog.h"
#include "dialogs/GridDialog.h"
#include "dialogs/LoadPaletteDialog.h"
#include "dialogs/NewImageDialog.h"
#include "dialogs/PaletteEditDialog.h"
#include "dialogs/ShortcutsDialog.h"
#include "dialogs/TextInputDialog.h"
#include "io/Backup.h"
#include "io/ImageFile.h"
#include "io/IoError.h"
#include "io/PaletteFile.h"

#include <spritenator/Version.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
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
#include <cmath>
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

/// Formatação dos números do indicador de zoom: no idioma da interface, sem
/// separador de milhar ("3200%", "12,5%").
QLocale zoomLocale()
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    return locale;
}

/// Texto do zoom em %, com no máximo uma casa decimal.
QString zoomText(double zoom)
{
    const double percent = std::round(zoom * 1000) / 10;
    return zoomLocale().toString(percent, 'g', 6) + QLatin1Char('%');
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

    // Documentos em abas (fecháveis e reordenáveis), um editor por imagem.
    m_mdiArea->setViewMode(QMdiArea::TabbedView);
    m_mdiArea->setTabsClosable(true);
    m_mdiArea->setTabsMovable(true);
    m_mdiArea->setDocumentMode(true);
    m_mdiArea->setActivationOrder(QMdiArea::ActivationHistoryOrder);

    // Painel lateral de um lado, abas dos documentos do outro.
    m_integrator = new IntegratorPanel(m_session, m_actions, m_operations, this);
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_integrator);
    m_mainSplitter->addWidget(m_mdiArea);
    m_mainSplitter->setStretchFactor(0, 0); // painel com a largura que precisa
    m_mainSplitter->setStretchFactor(1, 1); // a imagem ocupa o restante
    m_mainSplitter->setChildrenCollapsible(false);
    setCentralWidget(m_mainSplitter);

    // Preferências persistentes das grades.
    m_session->setGridLayout(m_settings.gridLayout());

    applyCustomShortcuts();
    createMenus();
    createToolBar();
    createStatusBar();
    connectActions();
    retranslateUi(); // textos iniciais (e a cada troca de idioma)
    updateRecentFilesMenu();

    // Modo de cor mudou (ex.: "Reverter" trocou um PNG indexado por RGBA).
    connect(m_session, &EditorSession::colorModeChanged, this, &MainWindow::updateActionStates);

    // Aba ativa -> documento ativo da sessão e indicador de zoom.
    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::syncActiveDocument);
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
    disconnect(m_zoomConnection);
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
    editMenu->addSeparator();
    editMenu->addAction(action(ActionId::EditDrawText));
    editMenu->addSeparator();
    editMenu->addAction(action(ActionId::EditShadow));
    editMenu->addSeparator();
    editMenu->addAction(action(ActionId::EditShortcuts));

    // Seleção: as ações de mover (setas) não aparecem no menu, mas estão no
    // diálogo de atalhos.
    QMenu* selectMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Select"));
    selectMenu->addAction(action(ActionId::EditSelectAll));
    selectMenu->addAction(action(ActionId::SelectNone));
    selectMenu->addAction(action(ActionId::SelectInvert));
    selectMenu->addSeparator();
    selectMenu->addAction(action(ActionId::SelectRotateClockwise));
    selectMenu->addAction(action(ActionId::SelectRotateCounterClockwise));
    selectMenu->addAction(action(ActionId::SelectFlipHorizontal));
    selectMenu->addAction(action(ActionId::SelectFlipVertical));
    selectMenu->addSeparator();
    selectMenu->addAction(action(ActionId::SelectApplyTransform));

    QMenu* viewMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&View"));
    viewMenu->addAction(action(ActionId::ViewGrids));
    viewMenu->addAction(action(ActionId::ViewMainGrid));
    viewMenu->addAction(action(ActionId::ViewSubGrid));
    viewMenu->addSeparator();
    viewMenu->addAction(action(ActionId::ViewZoomIn));
    viewMenu->addAction(action(ActionId::ViewZoomOut));
    viewMenu->addAction(action(ActionId::ViewZoomActual));
    viewMenu->addAction(action(ActionId::ViewZoomFit));
    viewMenu->addSeparator();
    viewMenu->addAction(action(ActionId::ViewAnimation));
    viewMenu->addSeparator();
    createLanguageMenu(viewMenu);

    QMenu* canvasMenu = addTranslatedMenu(menuBar(), QT_TR_NOOP("&Canvas"));
    canvasMenu->addAction(action(ActionId::CanvasResize));
    canvasMenu->addSeparator();
    canvasMenu->addAction(action(ActionId::CanvasClearAll));

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
    m_toolBar->addAction(action(ActionId::ViewGrids));
    m_toolBar->addAction(action(ActionId::ViewMainGrid));
    m_toolBar->addAction(action(ActionId::ViewSubGrid));
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

    // Indicador de zoom: mostra o zoom do editor ativo e aceita um valor
    // digitado ou escolhido na lista (em %).
    m_zoomCombo = new QComboBox(this);
    m_zoomCombo->setObjectName(QStringLiteral("zoomCombo"));
    m_zoomCombo->setEditable(true);
    m_zoomCombo->setInsertPolicy(QComboBox::NoInsert);
    m_zoomCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    for (const double level : CanvasEditor::kZoomLevels) {
        m_zoomCombo->addItem(zoomText(level), level);
    }
    m_zoomCombo->setCurrentIndex(-1);
    connect(m_zoomCombo, &QComboBox::activated, this,
        [this](int index) { applyZoomText(m_zoomCombo->itemText(index)); });
    connect(m_zoomCombo->lineEdit(), &QLineEdit::returnPressed, this,
        [this] { applyZoomText(m_zoomCombo->currentText()); });
    statusBar()->addPermanentWidget(m_zoomCombo);
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
        // Transformação da seleção ainda não aplicada: desfazer = cancelá-la.
        if (m_session->selectionTransform()->isActive()) {
            m_session->selectionTransform()->cancel();
            return;
        }
        undoGroup->undo();
    });
    connect(action(ActionId::EditRedo), &QAction::triggered, this, [this, undoGroup] {
        m_session->cancelTool();
        m_session->selectionTransform()->cancel();
        undoGroup->redo();
    });
    connect(undoGroup, &QUndoGroup::canUndoChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::canRedoChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::undoTextChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::redoTextChanged, this, &MainWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::activeStackChanged, this, &MainWindow::updateUndoActions);

    // --- Editar: operações sobre o documento ativo ---
    connect(action(ActionId::EditCut), &QAction::triggered, m_operations, &EditOperations::cut);
    connect(action(ActionId::EditCopy), &QAction::triggered, m_operations, &EditOperations::copy);
    connect(action(ActionId::EditPaste), &QAction::triggered, m_operations, &EditOperations::paste);
    connect(action(ActionId::EditClear), &QAction::triggered, m_operations, &EditOperations::clear);
    connect(action(ActionId::EditSelectAll), &QAction::triggered, m_operations,
        &EditOperations::selectAll);
    connect(action(ActionId::EditShadow), &QAction::triggered, m_operations,
        &EditOperations::prepareShadow);
    connect(action(ActionId::EditDrawText), &QAction::triggered, this, &MainWindow::drawText);
    connect(action(ActionId::EditShortcuts), &QAction::triggered, this, &MainWindow::editShortcuts);

    // --- Seleção ---
    connect(
        action(ActionId::SelectNone), &QAction::triggered, m_operations, &EditOperations::deselect);
    connect(action(ActionId::SelectInvert), &QAction::triggered, m_operations,
        &EditOperations::invertSelection);
    const auto connectTransform = [this](ActionId id, TransformKind kind) {
        connect(action(id), &QAction::triggered, this, [this, kind] {
            // Os atalhos (R, E...) só agem havendo seleção.
            if (const Document* document = activeDocument(); document && document->hasSelection()) {
                m_operations->transform(kind);
            }
        });
    };
    connectTransform(ActionId::SelectRotateClockwise, TransformKind::RotateClockwise);
    connectTransform(ActionId::SelectRotateCounterClockwise, TransformKind::RotateCounterClockwise);
    connectTransform(ActionId::SelectFlipHorizontal, TransformKind::FlipHorizontal);
    connectTransform(ActionId::SelectFlipVertical, TransformKind::FlipVertical);
    connect(action(ActionId::SelectApplyTransform), &QAction::triggered, m_session,
        &EditorSession::commitTransform);
    struct Move {
        ActionId id;
        int dx;
        int dy;
        bool outline;
    };
    constexpr int kFast = 8; // Shift+setas
    const Move moves[] = {
        { ActionId::SelectMoveLeft, -1, 0, false },
        { ActionId::SelectMoveRight, 1, 0, false },
        { ActionId::SelectMoveUp, 0, -1, false },
        { ActionId::SelectMoveDown, 0, 1, false },
        { ActionId::SelectMoveLeftFast, -kFast, 0, false },
        { ActionId::SelectMoveRightFast, kFast, 0, false },
        { ActionId::SelectMoveUpFast, 0, -kFast, false },
        { ActionId::SelectMoveDownFast, 0, kFast, false },
        { ActionId::SelectOutlineLeft, -1, 0, true },
        { ActionId::SelectOutlineRight, 1, 0, true },
        { ActionId::SelectOutlineUp, 0, -1, true },
        { ActionId::SelectOutlineDown, 0, 1, true },
    };
    for (const Move& move : moves) {
        connect(action(move.id), &QAction::triggered, this, [this, move] {
            if (move.outline) {
                m_operations->moveSelectionOutline(move.dx, move.dy);
            } else {
                m_operations->moveSelection(move.dx, move.dy);
            }
        });
    }
    // "Colar" só fica ativo quando há algo colável na área de transferência.
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this,
        [this] { action(ActionId::EditPaste)->setEnabled(EditOperations::canPaste()); });

    // --- Exibir ---
    connect(action(ActionId::ViewGrids), &QAction::triggered, this, &MainWindow::editGrids);
    connect(action(ActionId::ViewMainGrid), &QAction::toggled, m_session,
        &EditorSession::setMainGridVisible);
    connect(action(ActionId::ViewSubGrid), &QAction::toggled, m_session,
        &EditorSession::setSubGridVisible);
    m_session->setMainGridVisible(action(ActionId::ViewMainGrid)->isChecked());
    m_session->setSubGridVisible(action(ActionId::ViewSubGrid)->isChecked());

    // Zoom do editor ativo.
    connect(action(ActionId::ViewZoomIn), &QAction::triggered, this, [this] {
        if (CanvasEditor* editor = activeEditor()) {
            editor->zoomIn();
        }
    });
    connect(action(ActionId::ViewZoomOut), &QAction::triggered, this, [this] {
        if (CanvasEditor* editor = activeEditor()) {
            editor->zoomOut();
        }
    });
    connect(action(ActionId::ViewZoomActual), &QAction::triggered, this, [this] {
        if (CanvasEditor* editor = activeEditor()) {
            editor->setZoom(1);
        }
    });
    connect(action(ActionId::ViewZoomFit), &QAction::triggered, this, [this] {
        if (CanvasEditor* editor = activeEditor()) {
            editor->zoomToFit();
        }
    });

    // --- Canvas ---
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

    // --- Janela ---
    connect(action(ActionId::WindowReverseCanvas), &QAction::toggled, this,
        &MainWindow::setCanvasReversed);

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
        ActionId::EditShortcuts,
        ActionId::ViewGrids,
        ActionId::ViewMainGrid,
        ActionId::ViewSubGrid,
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
        ActionId::CanvasResize,
        ActionId::CanvasClearAll,
        ActionId::ViewZoomIn,
        ActionId::ViewZoomOut,
        ActionId::ViewZoomActual,
        ActionId::ViewZoomFit,
        ActionId::SelectNone,
        ActionId::SelectInvert,
        ActionId::SelectRotateClockwise,
        ActionId::SelectRotateCounterClockwise,
        ActionId::SelectFlipHorizontal,
        ActionId::SelectFlipVertical,
        ActionId::SelectApplyTransform,
        ActionId::SelectMoveLeft,
        ActionId::SelectMoveRight,
        ActionId::SelectMoveUp,
        ActionId::SelectMoveDown,
        ActionId::SelectMoveLeftFast,
        ActionId::SelectMoveRightFast,
        ActionId::SelectMoveUpFast,
        ActionId::SelectMoveDownFast,
        ActionId::SelectOutlineLeft,
        ActionId::SelectOutlineRight,
        ActionId::SelectOutlineUp,
        ActionId::SelectOutlineDown,
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
    m_zoomCombo->setEnabled(document != nullptr);
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
    // Copiar a paleta só faz sentido se a imagem ativa for indexada.
    const bool canCopyPalette
        = current != nullptr && current->colorMode() == core::ColorMode::Indexed;
    NewImageDialog dialog(canCopyPalette, m_settings.newImageHistory(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_settings.addNewImageEntry(dialog.entry()); // histórico do diálogo

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
    m_session->commitTransform(); // a transformação em andamento entra no arquivo
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
    m_session->commitTransform(); // conta como alteração a salvar
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
CanvasEditor* MainWindow::addDocumentWindow(Document* document)
{
    auto* editor = new CanvasEditor(document, m_session);
    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(editor);
    subWindow->setWindowIcon(IconProvider::icon(QStringLiteral("app_icon")));
    // Nas abas o menu de sistema da subjanela (Restaurar, Mover, Tamanho...)
    // é inacessível — e o Qt não o retraduz na troca de idioma.
    subWindow->setSystemMenu(nullptr);

    // Intercepta o fechamento da aba para perguntar se deve salvar.
    subWindow->installEventFilter(this);

    // Barra de status: posição do pixel sob o mouse.
    connect(editor, &CanvasEditor::pixelHovered, this, &MainWindow::showPosition);
    // Setas (mover a seleção): só com o editor em foco.
    editor->addActions(m_actions->editorActions());
    connect(document, &Document::filePathChanged, this, &MainWindow::updateActionStates);

    subWindow->show();
    // Ativa explicitamente: sem isso a ativação depende do foco da janela
    // principal, e o documento novo poderia não virar o documento ativo.
    m_mdiArea->setActiveSubWindow(subWindow);
    // Sem foco (ou com a janela ainda oculta) o QMdiArea não emite
    // subWindowActivated: sincroniza diretamente.
    syncActiveDocument();
    return editor;
}

QMdiSubWindow* MainWindow::findWindowForFile(const QString& path) const
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (QMdiSubWindow* subWindow : m_mdiArea->subWindowList()) {
        const auto* editor = qobject_cast<CanvasEditor*>(subWindow->widget());
        if (editor != nullptr && editor->document()->filePath() == absolute) {
            return subWindow;
        }
    }
    return nullptr;
}

void MainWindow::syncActiveDocument()
{
    m_session->setActiveDocument(activeDocument());
    trackActiveEditorZoom();
    updateActionStates();
}

CanvasEditor* MainWindow::activeEditor() const
{
    // currentSubWindow, e não activeSubWindow: este último fica nulo quando
    // a aplicação perde o foco (ex.: um diálogo de outro programa aberto).
    const QMdiSubWindow* subWindow = m_mdiArea->currentSubWindow();
    return subWindow != nullptr ? qobject_cast<CanvasEditor*>(subWindow->widget()) : nullptr;
}

Document* MainWindow::activeDocument() const
{
    const CanvasEditor* editor = activeEditor();
    return editor != nullptr ? editor->document() : nullptr;
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
// Janela > Inverter lados
// -----------------------------------------------------------------------------
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

    commitDocumentState(document,
        core::applyPalette(document->indexedCanvas(), oldPalette, newPalette, options), newPalette,
        UndoLabel { UndoOperation::LoadPalette });
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
    // O diálogo já devolve a imagem remapeada se as cores foram reorganizadas.
    commitDocumentState(
        document, dialog.canvas(), dialog.palette(), UndoLabel { UndoOperation::EditPalette });
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
    // O separador decimal do zoom ("12,5%" / "12.5%") depende do idioma.
    for (int index = 0; index < m_zoomCombo->count(); ++index) {
        m_zoomCombo->setItemText(index, zoomText(m_zoomCombo->itemData(index).toDouble()));
    }
    if (const CanvasEditor* editor = activeEditor()) {
        showZoom(editor->zoom());
    }
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
// Exibir > Grades
// -----------------------------------------------------------------------------
void MainWindow::editGrids()
{
    GridDialog dialog(m_session->gridLayout(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // Os editores observam a sessão e redesenham as grades sozinhos.
    m_session->setGridLayout(dialog.layout());
    m_settings.setGridLayout(dialog.layout());
}

// -----------------------------------------------------------------------------
// Editar > Atalhos de teclado
// -----------------------------------------------------------------------------
void MainWindow::applyCustomShortcuts()
{
    const QHash<QString, QKeySequence> custom = m_settings.customShortcuts();
    for (std::size_t index = 0; index < kActionCount; ++index) {
        const auto id = static_cast<ActionId>(index);
        action(id)->setShortcut(
            custom.value(ActionCatalog::settingsKey(id), ActionCatalog::defaultShortcut(id)));
    }
}

void MainWindow::editShortcuts()
{
    ShortcutsDialog dialog(m_actions, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // Grava só o que difere do padrão (o resto acompanha futuras versões).
    QHash<QString, QKeySequence> custom;
    for (std::size_t index = 0; index < kActionCount; ++index) {
        const auto id = static_cast<ActionId>(index);
        const QKeySequence& chosen = dialog.shortcuts().at(static_cast<qsizetype>(index));
        if (chosen != ActionCatalog::defaultShortcut(id)) {
            custom.insert(ActionCatalog::settingsKey(id), chosen);
        }
    }
    m_settings.setCustomShortcuts(custom);
    applyCustomShortcuts();
    if (m_helpWindow != nullptr) {
        m_helpWindow->retranslate(); // a tabela de atalhos da ajuda acompanha
    }
}

// -----------------------------------------------------------------------------
// Barra de status: posição e zoom
// -----------------------------------------------------------------------------
void MainWindow::showPosition(const core::Point& pixel)
{
    m_positionLabel->setText(QStringLiteral("X: %1  Y: %2").arg(pixel.x).arg(pixel.y));
}

void MainWindow::trackActiveEditorZoom()
{
    disconnect(m_zoomConnection);
    CanvasEditor* editor = activeEditor();
    if (editor == nullptr) {
        m_zoomCombo->setEditText(QString());
        return;
    }
    m_zoomConnection = connect(editor, &CanvasEditor::zoomChanged, this, &MainWindow::showZoom);
    showZoom(editor->zoom());
}

void MainWindow::showZoom(double zoom)
{
    m_zoomCombo->setEditText(zoomText(zoom));
}

void MainWindow::applyZoomText(const QString& text)
{
    CanvasEditor* editor = activeEditor();
    if (editor == nullptr) {
        return;
    }
    // Aceita "200", "200%", "12,5%" ou "12.5%" (vírgula ou ponto).
    QString number = text.trimmed();
    number.remove(QLatin1Char('%'));
    bool ok = false;
    double percent = zoomLocale().toDouble(number.trimmed(), &ok);
    if (!ok) {
        percent = QLocale::c().toDouble(number.trimmed(), &ok);
    }
    if (ok && percent > 0) {
        editor->setZoom(percent / 100);
    }
    showZoom(editor->zoom()); // mostra o valor efetivo (limitado aos extremos)
    editor->setFocus();
}

// -----------------------------------------------------------------------------
// Eventos
// -----------------------------------------------------------------------------
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Fechamento de uma janela-filha: pergunta se deve salvar.
    if (event->type() == QEvent::Close) {
        if (auto* subWindow = qobject_cast<QMdiSubWindow*>(watched)) {
            const auto* editor = qobject_cast<CanvasEditor*>(subWindow->widget());
            if (editor != nullptr && !maybeSave(editor->document())) {
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
                            "<p>Loosely inspired by <i>Character Maker 1999</i> "
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
    m_session->setWandTolerance(view.wandTolerance);
    m_session->setWandContiguous(view.wandContiguous);
    if (!view.splitterState.isEmpty()) {
        m_mainSplitter->restoreState(view.splitterState);
    }
}

void MainWindow::saveViewSettings()
{
    ViewSettings view;
    view.mainGridVisible = m_session->mainGridVisible();
    view.subGridVisible = m_session->subGridVisible();
    view.transparentEffect = m_session->transparentEffect();
    view.canvasReversed = action(ActionId::WindowReverseCanvas)->isChecked();
    view.wandTolerance = m_session->wandTolerance();
    view.wandContiguous = m_session->wandContiguous();
    view.splitterState = m_mainSplitter->saveState();
    m_settings.setViewSettings(view);
}

void MainWindow::showError(const QString& message)
{
    QMessageBox::critical(this, toQString(version::kApplicationName), message);
}

} // namespace spritenator::gui
