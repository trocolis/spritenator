// =============================================================================
// MainWindow.cpp
// =============================================================================
#include "MainWindow.h"

#include "ActionCatalog.h"
#include "CanvasView.h"
#include "CanvasWindow.h"
#include "Document.h"
#include "IconProvider.h"
#include "dialogs/NewImageDialog.h"
#include "io/ImageFile.h"
#include "io/IoError.h"

#include <spritenator/Version.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
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
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>

#include <algorithm>

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
    setCentralWidget(m_mdiArea);

    createMenus();
    createToolBar();
    createStatusBar();
    connectActions();
    updateRecentFilesMenu();

    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::updateActionStates);
    updateActionStates();

    resize(1024, 720);
    restoreGeometry(m_settings.mainWindowGeometry());
    restoreState(m_settings.mainWindowState());
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
    m_fileMenu = menuBar()->addMenu(tr("&File"));
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

    // --- Janela ---
    connect(action(ActionId::WindowCascade), &QAction::triggered, m_mdiArea,
        &QMdiArea::cascadeSubWindows);
    connect(action(ActionId::WindowTileHorizontal), &QAction::triggered, this,
        [this] { tileWindows(Qt::Vertical); }); // janelas empilhadas (uma sobre a outra)
    connect(action(ActionId::WindowTileVertical), &QAction::triggered, this,
        [this] { tileWindows(Qt::Horizontal); }); // janelas lado a lado
    connect(action(ActionId::WindowFit), &QAction::triggered, this, &MainWindow::fitActiveWindow);

    // --- Ajuda ---
    connect(action(ActionId::HelpAbout), &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::updateActionStates()
{
    // Ações sempre disponíveis.
    const ActionId alwaysEnabled[] = {
        ActionId::FileNew,
        ActionId::FileOpen,
        ActionId::FileExit,
        ActionId::HelpAbout,
    };
    // Ações que exigem um documento ativo.
    const ActionId needDocument[] = {
        ActionId::FileSave,
        ActionId::FileSaveAs,
        ActionId::FileClose,
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
    // Reverter só faz sentido para documentos que já existem em disco.
    action(ActionId::FileRevert)->setEnabled(document != nullptr && document->hasFilePath());
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

    const core::ColorDepth depth = dialog.colorDepth();
    core::Palette palette = core::Palette::makeDefault(depth);
    if (dialog.copyCurrentPalette() && current != nullptr) {
        palette = current->palette();
        palette.setDepth(depth); // ajusta ao número de cores escolhido
    }
    // Canvas preenchido com o índice 0 (cor de fundo/transparente no RPG Maker).
    auto* document = new Document(core::IndexedImage(dialog.imageSize(), 0), palette);
    addDocumentWindow(document);
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
        io::ImageFile::saveFilters().join(QStringLiteral(";;")), &selectedFilter);
    if (path.isEmpty()) {
        return false; // usuário cancelou
    }
    path = ensureExtension(path, selectedFilter);

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

    // Barra de status: posição do pixel sob o mouse.
    connect(window->view(), &CanvasView::pixelHovered, this, [this](const core::Point& pixel) {
        m_positionLabel->setText(QStringLiteral("X: %1  Y: %2").arg(pixel.x).arg(pixel.y));
    });
    connect(document, &Document::filePathChanged, this, &MainWindow::updateActionStates);

    window->show();
    subWindow->adjustSize(); // janela do tamanho do canvas (limitada pela área MDI)
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

void MainWindow::showError(const QString& message)
{
    QMessageBox::critical(this, toQString(version::kApplicationName), message);
}

} // namespace spritenator::gui
