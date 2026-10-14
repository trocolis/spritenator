// =============================================================================
// SelectionToolsTest.cpp — ferramentas de seleção (retângulo, oval, ponto a
// ponto, laço, varinha), combinações com Shift/Ctrl, Esc, pintura restrita à
// seleção, operações com seleção de forma livre, transformação flutuante
// (setas, R/E, alças do mouse), menu Selecionar e atalhos configuráveis.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/AppSettings.h"
#include "gui/CanvasEditor.h"
#include "gui/Document.h"
#include "gui/EditOperations.h"
#include "gui/EditorSession.h"
#include "gui/MainWindow.h"
#include "gui/SelectionTransform.h"
#include "gui/dialogs/ShortcutsDialog.h"
#include "io/ImageFile.h"

#include <QAction>
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QUndoStack>

#include <memory>

using namespace spritenator;
using gui::ActionId;
using gui::CanvasEditor;
using gui::Document;
using gui::EditorSession;
using gui::ToolKind;

namespace {

/// Sessão + editor exibido de um documento indexado 32x32 (cor 0), com a
/// metade direita na cor 4.
struct Fixture {
    EditorSession session;
    Document* document;
    CanvasEditor editor;
    gui::EditOperations operations { &session };

    Fixture()
        : document(new Document(makeCanvas(), core::Palette::makeDefault(core::ColorDepth::Bits8)))
        , editor(document, &session)
    {
        session.setActiveDocument(document);
        editor.resize(400, 400);
        editor.show();
        editor.setZoom(8);
    }

    static core::IndexedImage makeCanvas()
    {
        core::IndexedImage canvas({ 32, 32 }, 0);
        canvas.fillRect({ 16, 0, 16, 32 }, 4);
        return canvas;
    }

    [[nodiscard]] QPoint at(double x, double y) const
    {
        return editor.viewportPointOf(QPointF(x, y)).toPoint();
    }
    [[nodiscard]] QPoint center(int x, int y) const { return at(x + 0.5, y + 0.5); }
    [[nodiscard]] int pixel(int x, int y) const { return document->indexedCanvas().pixel(x, y); }

    /// Arrasta com o botão esquerdo entre dois pixels.
    void drag(QPoint from, QPoint to, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QTest::mousePress(editor.viewport(), Qt::LeftButton, modifiers, from);
        QMouseEvent move(QEvent::MouseMove, QPointF(to),
            editor.viewport()->mapToGlobal(QPointF(to)), Qt::NoButton, Qt::LeftButton, modifiers);
        QCoreApplication::sendEvent(editor.viewport(), &move);
        QTest::mouseRelease(editor.viewport(), Qt::LeftButton, modifiers, to);
    }
};

} // namespace

class SelectionToolsTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("SelectionToolsTest"));
        QSettings().clear();
    }

    /// Retângulo e oval arrastando; Shift soma e Ctrl subtrai.
    void dragShapesAndCombine()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.session.setToolKind(ToolKind::Select);
        fixture.drag(fixture.center(1, 1), fixture.center(4, 4));
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 1, 1, 4, 4 }));

        // Shift: soma um retângulo separado (fora da seleção atual).
        fixture.drag(fixture.center(10, 10), fixture.center(11, 11), Qt::ShiftModifier);
        QCOMPARE(fixture.document->selection().pixelCount(), 16LL + 4LL);
        // Ctrl: subtrai.
        fixture.drag(fixture.center(10, 10), fixture.center(11, 11), Qt::ControlModifier);
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 1, 1, 4, 4 }));

        fixture.session.setToolKind(ToolKind::SelectEllipse);
        fixture.drag(fixture.center(20, 20), fixture.center(26, 24));
        QCOMPARE(fixture.document->selection(), core::Selection::ellipse({ 20, 20, 7, 5 }));
    }

    /// Ponto a ponto: cliques, Backspace e fechamento com clique duplo.
    void polygonByClicks()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.session.setToolKind(ToolKind::SelectPolygon);
        QWidget* viewport = fixture.editor.viewport();
        QTest::mouseClick(viewport, Qt::LeftButton, { }, fixture.center(2, 2));
        QTest::mouseClick(viewport, Qt::LeftButton, { }, fixture.center(12, 2));
        QTest::mouseClick(viewport, Qt::LeftButton, { }, fixture.center(30, 30)); // engano
        QVERIFY(fixture.session.tool().isBusy());
        QTest::keyClick(&fixture.editor, Qt::Key_Backspace);
        QTest::mouseClick(viewport, Qt::LeftButton, { }, fixture.center(12, 12));
        QVERIFY(!fixture.document->hasSelection()); // ainda aberto
        QTest::mouseClick(viewport, Qt::LeftButton, { }, fixture.center(12, 12)); // "duplo"
        QVERIFY(!fixture.session.tool().isBusy());
        QCOMPARE(fixture.document->selection(),
            core::Selection::polygon({ { 2, 2 }, { 12, 2 }, { 12, 12 } }));

        // Esc com um polígono aberto cancela só o polígono.
        QTest::mouseClick(viewport, Qt::LeftButton, { }, fixture.center(20, 20));
        QTest::keyClick(&fixture.editor, Qt::Key_Escape);
        QVERIFY(!fixture.session.tool().isBusy());
        QVERIFY(fixture.document->hasSelection());
        // Esc sem nada em andamento desfaz a seleção.
        QTest::keyClick(&fixture.editor, Qt::Key_Escape);
        QVERIFY(!fixture.document->hasSelection());
    }

    /// Laço e varinha mágica.
    void lassoAndWand()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.session.setToolKind(ToolKind::SelectLasso);
        fixture.drag(fixture.center(3, 3), fixture.center(8, 3));
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 3, 3, 6, 1 }));

        fixture.session.setToolKind(ToolKind::MagicWand);
        QTest::mouseClick(fixture.editor.viewport(), Qt::LeftButton, { }, fixture.center(20, 5));
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 16, 0, 16, 32 }));
    }

    /// Pintar com seleção: só dentro dela.
    void paintingStaysInsideSelection()
    {
        Fixture fixture;
        fixture.document->setSelection(core::Selection::ellipse({ 0, 0, 9, 9 }));
        fixture.session.setToolKind(ToolKind::Fill);
        fixture.session.setDrawColor(9);
        fixture.session.tool().press(fixture.session, { 4, 4 }, Qt::LeftButton);
        fixture.session.tool().release(fixture.session, { 4, 4 });
        QCOMPARE(fixture.pixel(4, 4), 9);
        QCOMPARE(fixture.pixel(0, 0), 0); // canto: fora da oval
        QCOMPARE(fixture.pixel(12, 12), 0); // fora da seleção
    }

    /// Operações respeitam a forma: limpar, copiar e espelhar.
    void operationsRespectShape()
    {
        Fixture fixture;
        fixture.session.setBackgroundColor(2);
        const core::Selection oval = core::Selection::ellipse({ 14, 0, 6, 6 });
        fixture.document->setSelection(oval);
        fixture.operations.clear();
        QCOMPARE(fixture.pixel(16, 2), 2);
        QCOMPARE(fixture.pixel(14, 0), 0); // canto da oval: intacto

        // Espelhar com seleção: a forma vai junto; o resto não muda.
        fixture.document->setSelection(core::Rect { 14, 10, 4, 1 }); // 0 0 4 4
        fixture.operations.transform(gui::TransformKind::FlipHorizontal);
        QCOMPARE(fixture.pixel(14, 10), 4);
        QCOMPARE(fixture.pixel(17, 10), 0);
        QCOMPARE(fixture.pixel(13, 10), 0);
        QCOMPARE(fixture.document->undoStack()->undoText(), QStringLiteral("Flip Horizontally"));
    }

    /// Setas: os pixels flutuam até Enter; tudo vira UM passo de desfazer.
    void arrowsMoveFloatingSelection()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.session.setBackgroundColor(0);
        fixture.document->setSelection(core::Rect { 16, 0, 2, 2 }); // cor 4
        fixture.operations.moveSelection(-1, 0);
        fixture.operations.moveSelection(-8, 0);
        gui::SelectionTransform* transform = fixture.session.selectionTransform();
        QVERIFY(transform->isActive());
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 7, 0, 2, 2 }));
        QCOMPARE(fixture.pixel(7, 0), 4);
        QCOMPARE(fixture.pixel(16, 0), 0); // o lugar de origem recebe o fundo
        QCOMPARE(fixture.document->undoStack()->count(), 0);

        QTest::keyClick(&fixture.editor, Qt::Key_Return); // aplica
        QVERIFY(!transform->isActive());
        QCOMPARE(fixture.document->undoStack()->count(), 1);
        fixture.document->undoStack()->undo();
        QCOMPARE(fixture.pixel(16, 0), 4);
        QCOMPARE(fixture.pixel(7, 0), 0);

        // Esc cancela uma transformação em andamento (e mantém a seleção).
        fixture.document->setSelection(core::Rect { 16, 0, 2, 2 });
        fixture.operations.moveSelection(0, 5);
        QTest::keyClick(&fixture.editor, Qt::Key_Escape);
        QVERIFY(!transform->isActive());
        QCOMPARE(fixture.pixel(16, 0), 4);
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 16, 0, 2, 2 }));

        // Ctrl+setas: só o contorno.
        fixture.operations.moveSelectionOutline(-3, 0);
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 13, 0, 2, 2 }));
        QCOMPARE(fixture.pixel(16, 0), 4);
    }

    /// Alças: arrastar uma alça escala; perto do canto, por fora, gira.
    void mouseHandlesScaleAndRotate()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.session.setToolKind(ToolKind::Select);
        fixture.document->setSelection(core::Rect { 16, 4, 4, 2 });
        QCOMPARE(fixture.editor.transformHandles().size(), 8);
        // Entre as alças (numa seleção pequena elas ocupam quase tudo).
        QCOMPARE(
            fixture.editor.transformHitAt(fixture.at(17, 5)), CanvasEditor::TransformHit::Move);

        // Alça do meio da direita: de x = 20 até x = 24 dobra a largura.
        const QPoint right = fixture.editor.transformHandles().at(5).toPoint();
        QCOMPARE(fixture.editor.transformHitAt(right), CanvasEditor::TransformHit::Scale);
        fixture.drag(right, fixture.at(24, 5));
        QCOMPARE(fixture.document->selection(), core::Selection::rectangle({ 16, 4, 8, 2 }));

        // Girar 90° (Shift: múltiplos de 15°) pela zona de fora do canto.
        const QPointF corner = fixture.editor.transformBox().at(1);
        const QPoint outside = (corner + QPointF(6, -6)).toPoint();
        QCOMPARE(fixture.editor.transformHitAt(outside), CanvasEditor::TransformHit::Rotate);
        const QPointF boxCenter
            = (fixture.editor.transformBox().at(0) + fixture.editor.transformBox().at(2)) / 2;
        const QPointF arm = QPointF(outside) - boxCenter;
        const QPoint turned = (boxCenter + QPointF(-arm.y(), arm.x())).toPoint(); // +90°
        fixture.drag(outside, turned, Qt::ShiftModifier);
        QCOMPARE(fixture.session.selectionTransform()->params().angle, 90.0);
        QCOMPARE(fixture.document->selection().bounds().size(), (core::Size { 2, 8 }));

        // Clicar fora aplica (um passo) e começa outra seleção.
        QTest::mouseClick(fixture.editor.viewport(), Qt::LeftButton, { }, fixture.center(1, 30));
        QVERIFY(!fixture.session.selectionTransform()->isActive());
        QCOMPARE(fixture.document->undoStack()->count(), 1);
    }

    /// Menu Selecionar: R, E, inverter e desfazer seleção, e o desfazer de
    /// uma transformação em andamento.
    void selectMenuActions()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("select.png"));
        core::IndexedImage canvas({ 8, 8 }, 0);
        canvas.setPixel({ 0, 0 }, 5);
        io::ImageFile::save(path, { canvas, core::Palette::makeDefault(core::ColorDepth::Bits8) });
        gui::MainWindow window;
        QVERIFY(window.openFile(path));
        Document* document = window.activeDocument();
        const auto trigger = [&](ActionId id) { window.actions()->action(id)->trigger(); };
        QCOMPARE(window.actions()->action(ActionId::SelectRotateClockwise)->shortcut(),
            QKeySequence(Qt::Key_R));
        QCOMPARE(window.actions()->action(ActionId::SelectFlipVertical)->shortcut(),
            QKeySequence(Qt::SHIFT | Qt::Key_E));

        document->setSelection(core::Rect { 0, 0, 2, 1 });
        trigger(ActionId::SelectFlipHorizontal); // E
        QCOMPARE(int(document->indexedCanvas().pixel(1, 0)), 5);
        trigger(ActionId::SelectRotateClockwise); // R: 2x1 vira 1x2
        QCOMPARE(document->selection().bounds().size(), (core::Size { 1, 2 }));
        QCOMPARE(document->undoStack()->count(), 2);

        trigger(ActionId::SelectInvert);
        QCOMPARE(document->selection().pixelCount(), 64LL - 2LL);
        trigger(ActionId::SelectNone);
        QVERIFY(!document->hasSelection());

        // Desfazer com a seleção flutuando: cancela a transformação.
        document->setSelection(core::Rect { 4, 4, 2, 2 });
        window.session()->setToolKind(ToolKind::Select);
        gui::EditOperations operations(window.session());
        operations.moveSelection(1, 0);
        trigger(ActionId::EditUndo);
        QVERIFY(!window.session()->selectionTransform()->isActive());
        QCOMPARE(document->undoStack()->count(), 2); // os dois passos anteriores ficam
    }

    /// Atalhos configuráveis: o diálogo troca, avisa conflito e o resultado
    /// é gravado e reaplicado ao abrir.
    void shortcutsAreConfigurable()
    {
        QObject owner;
        const gui::ActionCatalog catalog(&owner);
        gui::ShortcutsDialog dialog(&catalog);
        const int rotateRow = static_cast<int>(ActionId::SelectRotateClockwise);
        QCOMPARE(dialog.list()->topLevelItemCount(), static_cast<int>(gui::kActionCount));
        QVERIFY(dialog.assign(rotateRow, QKeySequence(Qt::Key_T)));
        QCOMPARE(dialog.shortcuts().at(rotateRow), QKeySequence(Qt::Key_T));

        // Gravado nas preferências e reaplicado por uma janela nova.
        gui::AppSettings settings;
        settings.setCustomShortcuts(
            { { gui::ActionCatalog::settingsKey(ActionId::SelectRotateClockwise),
                QKeySequence(Qt::Key_T) } });
        {
            gui::MainWindow window;
            QCOMPARE(window.actions()->action(ActionId::SelectRotateClockwise)->shortcut(),
                QKeySequence(Qt::Key_T));
            QCOMPARE(window.actions()->action(ActionId::FileNew)->shortcut(),
                gui::ActionCatalog::defaultShortcut(ActionId::FileNew));
        }
        settings.setCustomShortcuts({ });
    }
};

QTEST_MAIN(SelectionToolsTest)
#include "SelectionToolsTest.moc"
