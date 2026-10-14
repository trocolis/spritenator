// =============================================================================
// MainWindowTest.cpp — testes de integração: disparam as AÇÕES reais dos
// menus e verificam o efeito, garantindo que cada comando está ligado.
// =============================================================================
#include "gui/MainWindow.h"
#include "gui/ActionCatalog.h"
#include "gui/CanvasEditor.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"
#include "io/ImageFile.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QLineEdit>
#include <QMdiArea>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoGroup>
#include <QUndoStack>

#include <memory>

using namespace spritenator;
using spritenator::gui::ActionId;
using spritenator::gui::MainWindow;

namespace {

/// Grava uma imagem indexada (paleta padrão de 256 cores) e devolve o caminho.
QString saveIndexed(const QTemporaryDir& directory, const char* name, core::IndexedImage image)
{
    const QString path = directory.filePath(QString::fromLatin1(name));
    io::ImageFile::save(
        path, { std::move(image), core::Palette::makeDefault(core::ColorDepth::Bits8) });
    return path;
}

} // namespace

class MainWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Configurações isoladas: não toca nas preferências reais do usuário.
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("MainWindowTest"));
    }

    /// Abre uma imagem e verifica que ela vira o documento ativo da sessão.
    void openFileActivatesDocument()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("sprite.png"));
        io::ImageFile::save(path,
            { core::IndexedImage({ 48, 32 }, 2),
                core::Palette::makeDefault(core::ColorDepth::Bits8) });

        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.openFile(path));
        QVERIFY(window.activeDocument() != nullptr);
        QCOMPARE(window.session()->activeDocument(), window.activeDocument());
        QVERIFY(window.actions()->action(ActionId::FileSave)->isEnabled());
        QVERIFY(window.actions()->action(ActionId::ViewZoomIn)->isEnabled());

        // Documentos em abas, cada um com o seu editor.
        auto* area = window.findChild<QMdiArea*>();
        QVERIFY(area != nullptr);
        QCOMPARE(area->viewMode(), QMdiArea::TabbedView);
        gui::CanvasEditor* editor = window.activeEditor();
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->document(), window.activeDocument());

        // Edição direta: um clique do lápis pinta o pixel da imagem.
        window.session()->setDrawColor(9);
        const QPoint center = editor->viewportPointOf(QPointF(10.5, 10.5)).toPoint();
        QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(int(window.activeDocument()->indexedCanvas().pixel(10, 10)), 9);
        QVERIFY(window.activeDocument()->isModified());

        // Desfazer pelo menu.
        window.actions()->action(ActionId::EditUndo)->trigger();
        QCOMPARE(int(window.activeDocument()->indexedCanvas().pixel(10, 10)), 2);
    }

    /// Sem documento, as ações de zoom e o indicador ficam desabilitados.
    void zoomControlsNeedDocument()
    {
        MainWindow window;
        QVERIFY(!window.actions()->action(ActionId::ViewZoomIn)->isEnabled());
        QVERIFY(!window.actions()->action(ActionId::ViewZoomFit)->isEnabled());
        auto* combo = window.findChild<QComboBox*>(QStringLiteral("zoomCombo"));
        QVERIFY(combo != nullptr);
        QVERIFY(!combo->isEnabled());
    }

    /// Ações de zoom e indicador em % agem no editor ativo (e o refletem).
    void zoomActionsAndIndicator()
    {
        QTemporaryDir directory;
        const QString path = saveIndexed(directory, "zoom.png", core::IndexedImage({ 16, 16 }));
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.openFile(path));
        gui::CanvasEditor* editor = window.activeEditor();
        auto* combo = window.findChild<QComboBox*>(QStringLiteral("zoomCombo"));
        QVERIFY(combo->isEnabled());

        window.actions()->action(ActionId::ViewZoomActual)->trigger();
        QCOMPARE(editor->zoom(), 1.0);
        QCOMPARE(combo->currentText(), QStringLiteral("100%"));
        window.actions()->action(ActionId::ViewZoomIn)->trigger();
        QCOMPARE(editor->zoom(), 2.0);
        window.actions()->action(ActionId::ViewZoomOut)->trigger();
        window.actions()->action(ActionId::ViewZoomOut)->trigger();
        QCOMPARE(editor->zoom(), 0.5);
        QCOMPARE(combo->currentText(), QStringLiteral("50%"));

        // Valor digitado no indicador (com ou sem "%").
        combo->lineEdit()->setText(QStringLiteral("800"));
        QTest::keyClick(combo->lineEdit(), Qt::Key_Return);
        QCOMPARE(editor->zoom(), 8.0);
        QCOMPARE(combo->currentText(), QStringLiteral("800%"));
        // Fora dos limites: fica no máximo (3200%).
        combo->lineEdit()->setText(QStringLiteral("99999%"));
        QTest::keyClick(combo->lineEdit(), Qt::Key_Return);
        QCOMPARE(editor->zoom(), gui::CanvasEditor::kMaxZoom);

        // Ajustar à janela: a imagem inteira cabe na área visível.
        window.actions()->action(ActionId::ViewZoomFit)->trigger();
        QVERIFY(16 * editor->zoom() <= editor->viewport()->width());
        QVERIFY(16 * editor->zoom() <= editor->viewport()->height());
    }

    /// Regressão: destruir a janela com um documento aberto, depois de uma
    /// edição no CANVAS (pilha ativa = do documento), chamava slots da
    /// MainWindow já destruída durante ~QWidget.
    void destroyingWindowAfterCanvasEditIsSafe()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("put.png"));
        io::ImageFile::save(path,
            { core::IndexedImage({ 48, 32 }, 1),
                core::Palette::makeDefault(core::ColorDepth::Bits8) });

        auto window = std::make_unique<MainWindow>();
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        QVERIFY(window->openFile(path));
        gui::Document* document = window->activeDocument();
        window->session()->tool().press(*window->session(), { 30, 5 }, Qt::LeftButton);
        window->session()->tool().release(*window->session(), { 30, 5 });
        QCOMPARE(window->session()->undoGroup()->activeStack(), document->undoStack());
        document->undoStack()->setClean(); // evita o diálogo "salvar alterações?"
        window.reset(); // não pode abortar nem acessar memória inválida
    }

    /// Ações do menu Editar estão ligadas às operações sobre o documento.
    void editActionsAreConnected()
    {
        QTemporaryDir directory;
        const QString path = saveIndexed(directory, "edit.png", core::IndexedImage({ 24, 32 }));
        MainWindow window;
        QVERIFY(window.openFile(path));
        gui::Document* document = window.activeDocument();
        window.actions()->action(ActionId::EditSelectAll)->trigger();
        QCOMPARE(document->selection().bounds(), document->indexedCanvas().bounds());

        window.session()->setBackgroundColor(5);
        window.actions()->action(ActionId::EditClear)->trigger();
        QCOMPARE(int(document->indexedCanvas().pixel(3, 3)), 5);

        window.session()->setDrawColor(9);
        window.actions()->action(ActionId::EditShadow)->trigger(); // fundo 5: nada muda
        QCOMPARE(int(document->indexedCanvas().pixel(3, 3)), 5);

        window.actions()->action(ActionId::EditCopy)->trigger();
        QVERIFY(window.actions()->action(ActionId::EditPaste)->isEnabled());
    }

    /// Ações de exibir e paleta refletem na sessão.
    void viewAndPaletteActionsAreConnected()
    {
        MainWindow window;
        gui::EditorSession* session = window.session();

        window.actions()->action(ActionId::ViewSubGrid)->trigger();
        QVERIFY(session->subGridVisible());

        window.actions()->action(ActionId::PaletteTransparent)->trigger();
        QVERIFY(session->transparentEffect());

        window.actions()->action(ActionId::PaletteMaskMode)->trigger();
        QVERIFY(session->maskMode());
        window.actions()->action(ActionId::PaletteAutoMask)->trigger();
        QVERIFY(session->mask().any());
        QVERIFY(!session->mask().test(session->backgroundColor()));
        window.actions()->action(ActionId::PaletteClearMask)->trigger();
        QVERIFY(session->mask().none());
        QVERIFY(!window.actions()->action(ActionId::PaletteAutoMask)->isChecked());
        window.actions()->action(ActionId::PaletteInvertMask)->trigger();
        QCOMPARE(session->mask().count(), std::size_t { 256 }); // paleta de 256 cores
    }

    /// "Limpar tudo" preenche o canvas com a cor de fundo (com desfazer).
    void clearAllFillsCanvas()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("clear.png"));
        io::ImageFile::save(path,
            { core::IndexedImage({ 8, 8 }, 7),
                core::Palette::makeDefault(core::ColorDepth::Bits8) });

        MainWindow window;
        QVERIFY(window.openFile(path));
        window.session()->setBackgroundColor(3);
        window.actions()->action(ActionId::CanvasClearAll)->trigger();
        QCOMPARE(int(window.activeDocument()->indexedCanvas().pixel(5, 5)), 3);
        window.actions()->action(ActionId::EditUndo)->trigger();
        QCOMPARE(int(window.activeDocument()->indexedCanvas().pixel(5, 5)), 7);
    }

    /// F2 mostra/esconde a linha do tempo (painel inferior); o menu Animação
    /// cria cenas no documento ativo.
    void timelineDockAndAnimationMenu()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("walk.png"));
        io::ImageFile::save(path,
            { core::IndexedImage({ 24, 32 }),
                core::Palette::makeDefault(core::ColorDepth::Bits8) });

        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.openFile(path));
        auto* dock = window.findChild<QDockWidget*>(QStringLiteral("TimelineDock"));
        QVERIFY(dock != nullptr);
        QVERIFY(dock->isVisible());
        QAction* toggle = window.actions()->action(ActionId::ViewTimeline);
        QVERIFY(toggle->isChecked());
        toggle->trigger();
        QVERIFY(!dock->isVisible());
        toggle->trigger();
        QVERIFY(dock->isVisible());

        QVERIFY(!window.actions()->action(ActionId::FrameDelete)->isEnabled()); // só uma cena
        window.actions()->action(ActionId::FrameNew)->trigger();
        QCOMPARE(window.activeDocument()->frameCount(), 2);
        QCOMPARE(window.activeDocument()->currentFrame(), 1);
        QVERIFY(window.actions()->action(ActionId::FrameDelete)->isEnabled());
        window.actions()->action(ActionId::LayerNew)->trigger();
        QCOMPARE(window.activeDocument()->layerCount(), 2);
        window.actions()->action(ActionId::EditUndo)->trigger();
        QCOMPARE(window.activeDocument()->layerCount(), 1);
    }

    /// Gradação altera a paleta do documento com um passo de desfazer.
    void gradationEditsPalette()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("gradient.bmp"));
        core::Palette palette = core::Palette::makeDefault(core::ColorDepth::Bits8);
        palette.setColor(10, { 0, 0, 0 });
        palette.setColor(12, { 100, 100, 100 });
        io::ImageFile::save(path, { core::IndexedImage({ 4, 4 }), palette });

        MainWindow window;
        QVERIFY(window.openFile(path));
        window.session()->setDrawColor(10);
        window.session()->setBackgroundColor(12);
        window.actions()->action(ActionId::PaletteGradation)->trigger();
        QCOMPARE(window.activeDocument()->palette().color(11), (core::Rgb { 50, 50, 50 }));
        QVERIFY(window.activeDocument()->isModified());
    }
};

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
