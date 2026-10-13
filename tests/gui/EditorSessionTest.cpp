// =============================================================================
// EditorSessionTest.cpp — testes do estado central do editor: transferência
// canvas <-> área de desenho, regras de escrita, ferramentas e undo.
// =============================================================================
#include "gui/EditorSession.h"
#include "gui/Document.h"
#include "gui/DrawBuffer.h"

#include <QTest>
#include <QUndoGroup>
#include <QUndoStack>

using namespace spritenator;
using spritenator::gui::Document;
using spritenator::gui::EditorSession;
using spritenator::gui::ToolKind;

namespace {

/// Documento 48x32 (duas células 24x32): esquerda cor 3, direita cor 4.
std::unique_ptr<Document> makeTwoCellDocument()
{
    core::IndexedImage canvas({ 48, 32 }, 3);
    canvas.fillRect({ 24, 0, 24, 32 }, 4);
    return std::make_unique<Document>(canvas, core::Palette::makeDefault(core::ColorDepth::Bits8));
}

} // namespace

class EditorSessionTest final : public QObject {
    Q_OBJECT

private slots:
    /// O buffer começa com o tamanho da célula padrão (24x32).
    void bufferStartsWithCellSize()
    {
        EditorSession session;
        QCOMPARE(session.buffer()->image().size(), (core::Size { 24, 32 }));
    }

    /// Botão direito: a célula do canvas vai para o buffer.
    void getCopiesCellIntoBuffer()
    {
        EditorSession session;
        auto document = makeTwoCellDocument();
        session.setActiveDocument(document.get());
        session.getFromCanvas(document.get(), { 24, 0, 24, 32 });
        QCOMPARE(int(session.buffer()->image().pixel(0, 0)), 4);
        // Desfazer devolve o buffer anterior.
        session.undoGroup()->undo();
        QCOMPARE(int(session.buffer()->image().pixel(0, 0)), 0);
    }

    /// Botão esquerdo: o buffer é gravado no canvas, com desfazer no documento.
    void putWritesBufferIntoCanvas()
    {
        EditorSession session;
        auto document = makeTwoCellDocument();
        session.setActiveDocument(document.get());
        session.getFromCanvas(document.get(), { 0, 0, 24, 32 }); // buffer = cor 3
        session.putToCanvas(document.get(), { 24, 0, 24, 32 });
        QCOMPARE(int(document->canvas().pixel(30, 5)), 3);
        QVERIFY(document->isModified());

        // A pilha ativa passou a ser a do documento: desfazer reverte o "put".
        QCOMPARE(session.undoGroup()->activeStack(), document->undoStack());
        session.undoGroup()->undo();
        QCOMPARE(int(document->canvas().pixel(30, 5)), 4);
    }

    /// Efeito de cor transparente: a cor de fundo do buffer não é gravada.
    void transparentEffectSkipsBackground()
    {
        EditorSession session;
        auto document = makeTwoCellDocument();
        session.setActiveDocument(document.get());
        session.setBackgroundColor(0);
        session.setTransparentEffect(true);
        // Buffer: tudo cor 0, exceto um pixel cor 9.
        core::IndexedImage frame({ 24, 32 }, 0);
        frame.setPixel({ 1, 1 }, 9);
        session.buffer()->replace(frame, QStringLiteral("test"));

        session.putToCanvas(document.get(), { 0, 0, 24, 32 });
        QCOMPARE(int(document->canvas().pixel(0, 0)), 3); // preservado
        QCOMPARE(int(document->canvas().pixel(1, 1)), 9); // gravado
    }

    /// O lápis desenha um traço contínuo e gera UM passo de desfazer.
    void penStrokeIsSingleUndoStep()
    {
        EditorSession session;
        session.setToolKind(ToolKind::PenThin);
        session.setDrawColor(7);
        gui::Tool& pen = session.tool();
        pen.press(session, { 0, 0 }, Qt::LeftButton);
        pen.move(session, { 5, 0 }); // salto de vários pixels: deve ser ligado
        pen.release(session, { 5, 0 });

        for (int x = 0; x <= 5; ++x) {
            QCOMPARE(int(session.buffer()->image().pixel(x, 0)), 7);
        }
        QCOMPARE(session.buffer()->undoStack()->count(), 1);
        session.undoGroup()->undo();
        QCOMPARE(int(session.buffer()->image().pixel(3, 0)), 0);
    }

    /// Botão direito desenha com a cor de fundo; Esc (cancel) descarta o traço.
    void penRightButtonAndCancel()
    {
        EditorSession session;
        session.setBackgroundColor(2);
        gui::Tool& pen = session.tool();
        pen.press(session, { 3, 3 }, Qt::RightButton);
        QCOMPARE(int(session.buffer()->image().pixel(3, 3)), 2);
        session.cancelTool();
        QCOMPARE(int(session.buffer()->image().pixel(3, 3)), 0);
        QCOMPARE(session.buffer()->undoStack()->count(), 0);
    }

    /// Mudar o tamanho da célula redimensiona o buffer.
    void layoutChangeResizesBuffer()
    {
        EditorSession session;
        core::CharacterLayout layout = session.characterLayout();
        layout.cellSize = { 16, 16 };
        session.setCharacterLayout(layout);
        QCOMPARE(session.buffer()->image().size(), (core::Size { 16, 16 }));
    }

    /// Cores são limitadas ao tamanho da paleta do documento ativo.
    void colorsClampToPaletteSize()
    {
        EditorSession session;
        session.setDrawColor(200);
        Document small(
            core::IndexedImage({ 4, 4 }), core::Palette::makeDefault(core::ColorDepth::Bits4));
        session.setActiveDocument(&small);
        QVERIFY(session.drawColor() < 16);
        session.setDrawColor(100); // inválido para 16 cores: ignorado
        QVERIFY(session.drawColor() < 16);
        session.setActiveDocument(nullptr);
    }

    /// Zoom aceita apenas os níveis conhecidos; lupa avança/recua.
    void zoomLevels()
    {
        EditorSession session;
        session.setZoom(5);
        QCOMPARE(session.zoom(), 4); // mais próximo de 5 (empate: o menor)
        session.zoomIn();
        QCOMPARE(session.zoom(), 6);
        session.setZoom(12);
        session.zoomIn();
        QCOMPARE(session.zoom(), 12);
        session.setZoom(1);
        session.zoomOut();
        QCOMPARE(session.zoom(), 1);
    }
};

QTEST_MAIN(EditorSessionTest)
#include "EditorSessionTest.moc"
