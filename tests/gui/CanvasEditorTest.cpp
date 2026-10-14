// =============================================================================
// CanvasEditorTest.cpp — o editor de imagem: zoom (níveis, âncora, roda do
// mouse), rolagem com Ctrl, edição direta com o mouse, destaque dos
// pixels sob o mouse e exibição das grades.
// =============================================================================
#include "gui/CanvasEditor.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"

#include <QApplication>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>
#include <QWheelEvent>

#include <algorithm>

using namespace spritenator;
using gui::CanvasEditor;
using gui::Document;
using gui::EditorSession;
using gui::ToolKind;

namespace {

/// Sessão + editor exibido de um documento indexado (metade direita cor 4).
struct Fixture {
    EditorSession session;
    Document* document;
    CanvasEditor editor;

    explicit Fixture(core::Size size = { 64, 64 })
        : document(
              new Document(makeCanvas(size), core::Palette::makeDefault(core::ColorDepth::Bits8)))
        , editor(document, &session)
    {
        session.setActiveDocument(document);
        editor.resize(240, 240);
        editor.show();
    }

    static core::IndexedImage makeCanvas(core::Size size)
    {
        core::IndexedImage canvas(size, 0);
        canvas.fillRect({ size.width / 2, 0, size.width - size.width / 2, size.height }, 4);
        return canvas;
    }

    /// Centro do pixel da imagem, em coordenadas do viewport.
    [[nodiscard]] QPoint centerOf(int x, int y) const
    {
        return editor.viewportPointOf(QPointF(x + 0.5, y + 0.5)).toPoint();
    }

    [[nodiscard]] int pixel(int x, int y) const { return document->indexedCanvas().pixel(x, y); }

    /// Envia um evento de roda (um "clique" = 120) ao viewport.
    void wheel(QPoint position, int delta, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QWheelEvent event(QPointF(position), QPointF(editor.viewport()->mapToGlobal(position)),
            QPoint(), QPoint(0, delta), Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(editor.viewport(), &event);
    }
};

} // namespace

class CanvasEditorTest final : public QObject {
    Q_OBJECT

private slots:
    /// Zoom livre, limitado a [12,5%, 3200%]; "aumentar/diminuir" seguem os níveis.
    void zoomLevelsAndLimits()
    {
        Fixture fixture;
        CanvasEditor& editor = fixture.editor;
        QSignalSpy changes(&editor, &CanvasEditor::zoomChanged);
        editor.setZoom(5);
        QCOMPARE(editor.zoom(), 5.0); // valores intermediários são aceitos
        QCOMPARE(changes.count(), 1);
        editor.zoomIn();
        QCOMPARE(editor.zoom(), 6.0); // próximo nível acima de 5
        editor.zoomOut();
        QCOMPARE(editor.zoom(), 4.0);
        editor.setZoom(1000);
        QCOMPARE(editor.zoom(), CanvasEditor::kMaxZoom);
        editor.zoomIn();
        QCOMPARE(editor.zoom(), CanvasEditor::kMaxZoom);
        editor.setZoom(0.001);
        QCOMPARE(editor.zoom(), CanvasEditor::kMinZoom);
        editor.zoomOut();
        QCOMPARE(editor.zoom(), CanvasEditor::kMinZoom);
    }

    /// Antes de qualquer zoom do usuário, a imagem se ajusta à janela (com
    /// fator inteiro ao ampliar).
    void startsFittedToWindow()
    {
        Fixture fixture({ 24, 32 });
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        const double zoom = fixture.editor.zoom();
        QCOMPARE(zoom, std::floor(zoom));
        QVERIFY(32 * zoom <= fixture.editor.viewport()->height());
        QVERIFY(32 * (zoom + 1) > fixture.editor.viewport()->height());
    }

    /// O zoom ancorado mantém o mesmo pixel sob o ponto de âncora.
    void anchoredZoomKeepsPixelUnderCursor()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        CanvasEditor& editor = fixture.editor;
        editor.setZoom(8); // 512 px: maior que a área, há rolagem
        const QPointF anchor(60, 150);
        const core::Point before = editor.imagePointAt(anchor);
        editor.zoomIn(anchor);
        QCOMPARE(editor.imagePointAt(anchor), before);
        editor.zoomOut(anchor);
        editor.zoomOut(anchor);
        QCOMPARE(editor.imagePointAt(anchor), before);
    }

    /// Roda: zoom no ponteiro; Ctrl+roda rola na vertical; Ctrl+Shift, na
    /// horizontal.
    void wheelZoomsAndCtrlWheelScrolls()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        CanvasEditor& editor = fixture.editor;
        editor.setZoom(4);
        const QPoint position(100, 100);
        fixture.wheel(position, 120);
        QCOMPARE(editor.zoom(), 6.0);
        fixture.wheel(position, -120);
        QCOMPARE(editor.zoom(), 4.0);
        // Meio "clique" (touchpad) acumula: dois deles = um nível.
        fixture.wheel(position, 60);
        QCOMPARE(editor.zoom(), 4.0);
        fixture.wheel(position, 60);
        QCOMPARE(editor.zoom(), 6.0);

        editor.setZoom(8);
        QScrollBar* vertical = editor.verticalScrollBar();
        QScrollBar* horizontal = editor.horizontalScrollBar();
        vertical->setValue(vertical->maximum() / 2);
        horizontal->setValue(horizontal->maximum() / 2);
        const int top = vertical->value();
        const int left = horizontal->value();
        fixture.wheel(position, -120, Qt::ControlModifier); // rola para baixo
        QVERIFY(vertical->value() > top);
        QCOMPARE(horizontal->value(), left);
        QCOMPARE(editor.zoom(), 8.0);
        fixture.wheel(position, -120, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(horizontal->value() > left);
        QCOMPARE(editor.zoom(), 8.0);
    }

    /// O lápis edita a imagem diretamente; o clique registra a célula da
    /// sub-grade; Esc cancela o traço em andamento.
    void mouseEditsImageDirectly()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.editor.setZoom(2);
        fixture.session.setToolKind(ToolKind::Pen);
        fixture.session.setDrawColor(9);
        QTest::mouseClick(
            fixture.editor.viewport(), Qt::LeftButton, Qt::NoModifier, fixture.centerOf(30, 40));
        QCOMPARE(fixture.pixel(30, 40), 9);
        QCOMPARE(fixture.document->undoStack()->count(), 1);
        QCOMPARE(fixture.session.lastCell().value(), (core::Rect { 24, 32, 24, 32 }));

        // Esc durante o traço: nada fica na imagem.
        QTest::mousePress(
            fixture.editor.viewport(), Qt::LeftButton, Qt::NoModifier, fixture.centerOf(5, 5));
        QCOMPARE(fixture.pixel(5, 5), 9);
        QTest::keyClick(&fixture.editor, Qt::Key_Escape);
        QCOMPARE(fixture.pixel(5, 5), 0);
        QTest::mouseRelease(
            fixture.editor.viewport(), Qt::LeftButton, Qt::NoModifier, fixture.centerOf(5, 5));
        QCOMPARE(fixture.document->undoStack()->count(), 1);
    }

    /// Destaque: os pixels que a ferramenta afetaria, filtrados por imagem,
    /// tom e máscara; some quando o mouse sai.
    void highlightShowsAffectedPixels()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        CanvasEditor& editor = fixture.editor;
        editor.setZoom(2);
        const auto highlighted = [&] {
            std::vector<core::Point> points = editor.highlightedPixels();
            std::sort(points.begin(), points.end(),
                [](core::Point a, core::Point b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
            return points;
        };

        fixture.session.setToolKind(ToolKind::Pen);
        QTest::mouseMove(editor.viewport(), fixture.centerOf(3, 4));
        QCOMPARE(highlighted(), (std::vector<core::Point> { { 3, 4 } }));

        // Ponta redonda de 3 px no canto: só os pixels dentro da imagem.
        fixture.session.setBrushShape(core::BrushShape::Round);
        fixture.session.setBrushSize(3);
        QTest::mouseMove(editor.viewport(), fixture.centerOf(0, 0));
        QCOMPARE(highlighted(), (std::vector<core::Point> { { 0, 0 }, { 1, 0 }, { 0, 1 } }));

        // Balde: a região inteira (metade esquerda, cor 0).
        fixture.session.setToolKind(ToolKind::Fill);
        QCOMPARE(editor.highlightedPixels().size(), std::size_t { 32 * 64 });

        // Máscara protegendo a cor 0: nada seria pintado.
        fixture.session.toggleMasked(0);
        QVERIFY(editor.highlightedPixels().empty());
        fixture.session.clearMask();

        // Tom xadrez: metade da região.
        fixture.session.setTone(core::Tone::E);
        QCOMPARE(editor.highlightedPixels().size(), std::size_t { 32 * 64 / 2 });
        fixture.session.setTone(core::Tone::A);

        // Mouse fora do editor: destaque apagado.
        fixture.session.setToolKind(ToolKind::Pen);
        QVERIFY(!editor.highlightedPixels().empty());
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(editor.viewport(), &leave);
        QVERIFY(editor.highlightedPixels().empty());
    }

    /// O destaque só aparece no editor do documento ativo.
    void highlightOnlyOnActiveDocument()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        fixture.editor.setZoom(2);
        QTest::mouseMove(fixture.editor.viewport(), fixture.centerOf(3, 4));
        QVERIFY(!fixture.editor.highlightedPixels().empty());
        fixture.session.setActiveDocument(nullptr);
        QVERIFY(fixture.editor.highlightedPixels().empty());
    }

    /// Grade principal: visível com espaçamento suficiente na tela; some
    /// quando ficaria densa demais.
    void mainGridHidesWhenTooDense()
    {
        Fixture fixture;
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.editor));
        CanvasEditor& editor = fixture.editor;
        fixture.session.setMainGridVisible(true);
        fixture.session.setSubGridVisible(false);
        // Sem destaque sob o mouse para não interferir.
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(editor.viewport(), &leave);

        // Compara a primeira coluna de um pixel (linha de grade) com o meio
        // do pixel — numa área uniforme (cor 0).
        const auto gridVisible = [&] {
            const QImage image = editor.viewport()->grab().toImage();
            const QPoint edge = editor.viewportPointOf(QPointF(4, 4.5)).toPoint();
            const QPoint middle = fixture.centerOf(4, 4);
            return image.pixel(edge) != image.pixel(middle);
        };
        editor.setZoom(8);
        editor.horizontalScrollBar()->setValue(0); // pixel (4, 4) à vista
        editor.verticalScrollBar()->setValue(0);
        QVERIFY(gridVisible());
        editor.setZoom(2); // 2 px entre linhas: denso demais
        QVERIFY(!gridVisible());

        // Grade principal 4x4: com zoom 2, as linhas ficam a 8 px e voltam.
        core::GridLayout layout;
        layout.mainGrid = { 4, 4 };
        fixture.session.setGridLayout(layout);
        QVERIFY(gridVisible());
    }

    /// Título: nome, tamanho, "RGBA" e o marcador de modificado.
    void titleShowsSizeAndMode()
    {
        EditorSession session;
        CanvasEditor rgba(new Document(core::RgbaImage({ 5, 7 }),
                              core::Palette::makeDefault(core::ColorDepth::Bits8)),
            &session);
        QVERIFY(rgba.windowTitle().endsWith(QStringLiteral("[*] (5x7, RGBA)")));
        Fixture fixture({ 8, 6 });
        QVERIFY(fixture.editor.windowTitle().endsWith(QStringLiteral("[*] (8x6)")));
        QVERIFY(!fixture.editor.isWindowModified());
        fixture.session.tool().press(fixture.session, { 1, 1 }, Qt::LeftButton);
        fixture.session.tool().release(fixture.session, { 1, 1 });
        QVERIFY(fixture.editor.isWindowModified());
    }
};

QTEST_MAIN(CanvasEditorTest)
#include "CanvasEditorTest.moc"
