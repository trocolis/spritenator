// =============================================================================
// MainWindowTest.cpp — testes de integração: disparam as AÇÕES reais dos
// menus e verificam o efeito, garantindo que cada comando está ligado.
// =============================================================================
#include "gui/MainWindow.h"
#include "gui/ActionCatalog.h"
#include "gui/Document.h"
#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"
#include "io/ImageFile.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoGroup>

using namespace spritenator;
using spritenator::gui::ActionId;
using spritenator::gui::MainWindow;

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
        QVERIFY(window.actions()->action(ActionId::CanvasGetAll)->isEnabled());

        // F3: transfere o canvas inteiro para a área de desenho.
        window.actions()->action(ActionId::CanvasGetAll)->trigger();
        QCOMPARE(window.session()->buffer()->image().size(), (core::Size { 48, 32 }));
        QCOMPARE(int(window.session()->buffer()->image().pixel(0, 0)), 2);

        // Desfazer pelo menu.
        window.actions()->action(ActionId::EditUndo)->trigger();
        QCOMPARE(window.session()->buffer()->image().size(), (core::Size { 24, 32 }));
    }

    /// Ações do menu Editar estão ligadas às operações do buffer.
    void editActionsAreConnected()
    {
        MainWindow window;
        gui::DrawBuffer* buffer = window.session()->buffer();
        window.actions()->action(ActionId::EditSelectAll)->trigger();
        QCOMPARE(buffer->selection().value(), buffer->image().bounds());

        window.session()->setBackgroundColor(5);
        window.actions()->action(ActionId::EditClear)->trigger();
        QCOMPARE(int(buffer->image().pixel(3, 3)), 5);

        window.session()->setDrawColor(9);
        window.actions()->action(ActionId::EditShadow)->trigger(); // fundo 5: nada muda
        QCOMPARE(int(buffer->image().pixel(3, 3)), 5);

        window.actions()->action(ActionId::EditCopy)->trigger();
        QVERIFY(window.actions()->action(ActionId::EditPaste)->isEnabled());
    }

    /// Ações de exibir e paleta refletem na sessão.
    void viewAndPaletteActionsAreConnected()
    {
        MainWindow window;
        gui::EditorSession* session = window.session();

        window.actions()->action(ActionId::ViewZoom2)->trigger();
        QCOMPARE(session->zoom(), 2);
        session->zoomIn(); // lupa: a ação correspondente fica marcada
        QVERIFY(window.actions()->action(ActionId::ViewZoom4)->isChecked());

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
