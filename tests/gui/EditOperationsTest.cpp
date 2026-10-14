// =============================================================================
// EditOperationsTest.cpp — área de transferência, transformações e texto.
// =============================================================================
#include "gui/EditOperations.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"

#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace spritenator;
using spritenator::gui::Document;
using spritenator::gui::EditOperations;
using spritenator::gui::EditorSession;
using spritenator::gui::TransformKind;

namespace {

/// Sessão com um documento indexado 24x32 (cor 0) ativo.
struct Fixture {
    EditorSession session;
    std::unique_ptr<Document> document = std::make_unique<Document>(
        core::IndexedImage({ 24, 32 }, 0), core::Palette::makeDefault(core::ColorDepth::Bits8));
    EditOperations operations { &session };

    Fixture() { session.setActiveDocument(document.get()); }

    /// Pinta um pixel diretamente (sem passar por ferramentas).
    void setPixel(core::Point point, int color)
    {
        // Região de 1x1 pixel: não copia a imagem inteira.
        document->applyCanvasRegion(
            point, core::IndexedImage({ 1, 1 }, static_cast<core::ColorIndex>(color)));
    }

    [[nodiscard]] int pixel(int x, int y) const { return document->indexedCanvas().pixel(x, y); }
};

} // namespace

class EditOperationsTest final : public QObject {
    Q_OBJECT

private slots:
    /// Copiar e colar preserva os índices exatos (formato privado).
    void copyPastePreservesIndices()
    {
        Fixture fixture;
        fixture.setPixel({ 1, 1 }, 200);

        fixture.document->setSelection(core::Rect { 1, 1, 2, 2 });
        fixture.operations.copy();
        QVERIFY(EditOperations::canPaste());

        fixture.document->setSelection(core::Rect { 10, 10, 1, 1 });
        fixture.operations.paste();
        QCOMPARE(fixture.pixel(10, 10), 200);
        QCOMPARE(fixture.document->selection().bounds(), (core::Rect { 10, 10, 2, 2 }));
        QCOMPARE(fixture.document->undoStack()->count(), 1);
    }

    /// F8 (efeito de cor transparente): ao colar, a cor de fundo não cobre
    /// o que já existe na imagem.
    void pasteHonoursTransparentEffect()
    {
        Fixture fixture;
        fixture.session.setBackgroundColor(0);
        fixture.setPixel({ 1, 0 }, 7); // origem: [0, 7]
        fixture.setPixel({ 10, 10 }, 5); // destino: [5, ?]
        fixture.document->setSelection(core::Rect { 0, 0, 2, 1 });
        fixture.operations.copy();

        fixture.session.setTransparentEffect(true);
        fixture.document->setSelection(core::Rect { 10, 10, 1, 1 });
        fixture.operations.paste();
        QCOMPARE(fixture.pixel(10, 10), 5); // fundo da origem: ignorado
        QCOMPARE(fixture.pixel(11, 10), 7);
    }

    /// Recortar = copiar + limpar com a cor de fundo.
    void cutClearsWithBackground()
    {
        Fixture fixture;
        fixture.session.setBackgroundColor(3);
        fixture.operations.selectAll();
        QCOMPARE(fixture.document->selection().bounds(), (core::Rect { 0, 0, 24, 32 }));
        fixture.operations.cut();
        QCOMPARE(fixture.pixel(0, 0), 3);
        QCOMPARE(fixture.document->undoStack()->undoText(), QStringLiteral("Clear"));
    }

    /// Transformações atuam só na seleção e geram um passo de desfazer cada.
    void transformsActOnSelection()
    {
        Fixture fixture;
        fixture.setPixel({ 0, 0 }, 5);
        fixture.setPixel({ 10, 0 }, 6);
        fixture.document->setSelection(core::Rect { 0, 0, 4, 4 });

        QUndoStack* undoStack = fixture.document->undoStack();
        const int before = undoStack->count();
        fixture.operations.transform(TransformKind::FlipHorizontal);
        QCOMPARE(fixture.pixel(3, 0), 5);
        QCOMPARE(fixture.pixel(10, 0), 6); // fora da seleção: intacto
        QCOMPARE(undoStack->count(), before + 1);

        undoStack->undo();
        QCOMPARE(fixture.pixel(0, 0), 5);
    }

    /// Esticar amplia o conteúdo e seleciona a nova área.
    void stretchSelectsNewArea()
    {
        Fixture fixture;
        fixture.setPixel({ 0, 0 }, 4);
        fixture.document->setSelection(core::Rect { 0, 0, 2, 2 });
        fixture.operations.stretch({ 4, 4 });
        QCOMPARE(fixture.pixel(1, 1), 4);
        QCOMPARE(fixture.document->selection().bounds(), (core::Rect { 0, 0, 4, 4 }));
    }

    /// Sombra: tudo que não é fundo vira a cor de desenho.
    void shadowUsesDrawColor()
    {
        Fixture fixture;
        fixture.setPixel({ 2, 2 }, 8);
        fixture.session.setBackgroundColor(0);
        fixture.session.setDrawColor(1);
        fixture.operations.prepareShadow();
        QCOMPARE(fixture.pixel(2, 2), 1);
        QCOMPARE(fixture.pixel(3, 3), 0);
    }

    /// Sem documento ativo, nenhuma operação faz nada (nem quebra).
    void operationsWithoutDocumentAreHarmless()
    {
        EditorSession session;
        EditOperations operations(&session);
        operations.selectAll();
        operations.copy();
        operations.cut();
        operations.paste();
        operations.clear();
        operations.transform(TransformKind::RotateClockwise);
        operations.rotate(45);
        operations.stretch({ 2, 2 });
        operations.prepareShadow();
        QVERIFY(session.activeDocument() == nullptr);
    }

    /// Texto: rasterizado sem suavização e desenhado com a cor de desenho.
    void textIsRenderedCrisply()
    {
        QFont font;
        font.setPixelSize(12);
        const core::IndexedImage mask = EditOperations::renderText(QStringLiteral("H"), font);
        QVERIFY(!mask.isEmpty());
        const core::ColorSet used = mask.usedColors();
        QVERIFY(used.test(1));
        QCOMPARE(used.count(), std::size_t { 2 }); // só 0 e 1: nada de meios-tons

        Fixture fixture;
        fixture.session.setDrawColor(9);
        fixture.operations.drawText(QStringLiteral("H"), font);
        QVERIFY(fixture.document->indexedCanvas().usedColors().test(9));
        QVERIFY(EditOperations::renderText(QString(), font).isEmpty());
    }
};

QTEST_MAIN(EditOperationsTest)
#include "EditOperationsTest.moc"
