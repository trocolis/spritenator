// =============================================================================
// EditOperationsTest.cpp — área de transferência, transformações e texto.
// =============================================================================
#include "gui/EditOperations.h"
#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"

#include <QTest>
#include <QUndoStack>

using namespace spritenator;
using spritenator::gui::EditOperations;
using spritenator::gui::EditorSession;
using spritenator::gui::TransformKind;

class EditOperationsTest final : public QObject {
    Q_OBJECT

private slots:
    /// Copiar e colar preserva os índices exatos (formato privado).
    void copyPastePreservesIndices()
    {
        EditorSession session;
        EditOperations operations(&session);
        gui::DrawBuffer* buffer = session.buffer();
        core::IndexedImage frame = buffer->image();
        frame.setPixel({ 1, 1 }, 200);
        buffer->replace(frame, QStringLiteral("setup"));

        buffer->setSelection(core::Rect { 1, 1, 2, 2 });
        operations.copy();
        QVERIFY(EditOperations::canPaste());

        buffer->setSelection(core::Rect { 10, 10, 1, 1 });
        operations.paste();
        QCOMPARE(int(buffer->image().pixel(10, 10)), 200);
        QCOMPARE(buffer->selection().value(), (core::Rect { 10, 10, 2, 2 }));
    }

    /// Recortar = copiar + limpar com a cor de fundo.
    void cutClearsWithBackground()
    {
        EditorSession session;
        EditOperations operations(&session);
        session.setBackgroundColor(3);
        operations.selectAll();
        operations.cut();
        QCOMPARE(int(session.buffer()->image().pixel(0, 0)), 3);
        QCOMPARE(session.buffer()->undoStack()->undoText(), QStringLiteral("Clear"));
    }

    /// Transformações atuam só na seleção e geram um passo de desfazer cada.
    void transformsActOnSelection()
    {
        EditorSession session;
        EditOperations operations(&session);
        gui::DrawBuffer* buffer = session.buffer();
        core::IndexedImage frame = buffer->image();
        frame.setPixel({ 0, 0 }, 5);
        frame.setPixel({ 10, 0 }, 6);
        buffer->replace(frame, QStringLiteral("setup"));
        buffer->setSelection(core::Rect { 0, 0, 4, 4 });

        const int before = buffer->undoStack()->count();
        operations.transform(TransformKind::FlipHorizontal);
        QCOMPARE(int(buffer->image().pixel(3, 0)), 5);
        QCOMPARE(int(buffer->image().pixel(10, 0)), 6); // fora da seleção: intacto
        QCOMPARE(buffer->undoStack()->count(), before + 1);

        buffer->undoStack()->undo();
        QCOMPARE(int(buffer->image().pixel(0, 0)), 5);
    }

    /// Esticar amplia o conteúdo e seleciona a nova área.
    void stretchSelectsNewArea()
    {
        EditorSession session;
        EditOperations operations(&session);
        gui::DrawBuffer* buffer = session.buffer();
        core::IndexedImage frame = buffer->image();
        frame.setPixel({ 0, 0 }, 4);
        buffer->replace(frame, QStringLiteral("setup"));
        buffer->setSelection(core::Rect { 0, 0, 2, 2 });
        operations.stretch({ 4, 4 });
        QCOMPARE(int(buffer->image().pixel(1, 1)), 4);
        QCOMPARE(buffer->selection().value(), (core::Rect { 0, 0, 4, 4 }));
    }

    /// Sombra: tudo que não é fundo vira a cor de desenho.
    void shadowUsesDrawColor()
    {
        EditorSession session;
        EditOperations operations(&session);
        core::IndexedImage frame = session.buffer()->image();
        frame.setPixel({ 2, 2 }, 8);
        session.buffer()->replace(frame, QStringLiteral("setup"));
        session.setBackgroundColor(0);
        session.setDrawColor(1);
        operations.prepareShadow();
        QCOMPARE(int(session.buffer()->image().pixel(2, 2)), 1);
        QCOMPARE(int(session.buffer()->image().pixel(3, 3)), 0);
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

        EditorSession session;
        EditOperations operations(&session);
        session.setDrawColor(9);
        operations.drawText(QStringLiteral("H"), font);
        QVERIFY(session.buffer()->image().usedColors().test(9));
        QVERIFY(EditOperations::renderText(QString(), font).isEmpty());
    }
};

QTEST_MAIN(EditOperationsTest)
#include "EditOperationsTest.moc"
