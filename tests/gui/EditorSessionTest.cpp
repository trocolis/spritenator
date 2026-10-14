// =============================================================================
// EditorSessionTest.cpp — testes do estado central do editor: documento
// ativo, ferramentas editando a imagem diretamente, regras de escrita,
// grades, pegadas (destaque sob o mouse) e undo.
// =============================================================================
#include "gui/EditorSession.h"
#include "gui/Document.h"

#include <QTest>
#include <QUndoGroup>
#include <QUndoStack>

#include <algorithm>

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

/// Documento indexado vazio (cor 0).
std::unique_ptr<Document> makeBlankDocument(core::Size size = { 24, 32 })
{
    return std::make_unique<Document>(
        core::IndexedImage(size, 0), core::Palette::makeDefault(core::ColorDepth::Bits8));
}

/// Pixel do canvas indexado do documento.
int pixelAt(const Document& document, int x, int y)
{
    return document.indexedCanvas().pixel(x, y);
}

/// Pegada ordenada (a ordem devolvida pelas ferramentas não importa).
std::vector<core::Point> sortedFootprint(const EditorSession& session, core::Point point)
{
    std::vector<core::Point> points = session.tool().footprint(session, point);
    std::sort(points.begin(), points.end(),
        [](core::Point a, core::Point b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return points;
}

} // namespace

class EditorSessionTest final : public QObject {
    Q_OBJECT

private slots:
    /// A pilha de desfazer ativa é a do documento ativo.
    void activeDocumentDrivesUndoStack()
    {
        EditorSession session;
        QVERIFY(session.activeDocument() == nullptr);
        auto first = makeBlankDocument();
        auto second = makeBlankDocument();
        session.setActiveDocument(first.get());
        QCOMPARE(session.undoGroup()->activeStack(), first->undoStack());
        session.setActiveDocument(second.get());
        QCOMPARE(session.undoGroup()->activeStack(), second->undoStack());
        session.setActiveDocument(nullptr);
        QVERIFY(session.undoGroup()->activeStack() == nullptr);
    }

    /// Sem documento ativo, as ferramentas não fazem nada (nem quebram).
    void toolsWithoutDocumentAreHarmless()
    {
        EditorSession session;
        for (const ToolKind kind : { ToolKind::Pen, ToolKind::Fill, ToolKind::Rectangle,
                 ToolKind::Select, ToolKind::MagicWand }) {
            session.setToolKind(kind);
            session.tool().press(session, { 1, 1 }, Qt::LeftButton);
            session.tool().move(session, { 2, 2 });
            session.tool().release(session, { 2, 2 });
            QVERIFY(session.tool().footprint(session, { 1, 1 }).size() <= 5);
        }
    }

    /// O lápis desenha um traço contínuo no documento e gera UM passo de desfazer.
    void penStrokeIsSingleUndoStep()
    {
        EditorSession session;
        auto document = makeBlankDocument();
        session.setActiveDocument(document.get());
        session.setToolKind(ToolKind::Pen);
        session.setDrawColor(7);
        gui::Tool& pen = session.tool();
        pen.press(session, { 0, 0 }, Qt::LeftButton);
        pen.move(session, { 5, 0 }); // salto de vários pixels: deve ser ligado
        pen.release(session, { 5, 0 });

        for (int x = 0; x <= 5; ++x) {
            QCOMPARE(pixelAt(*document, x, 0), 7);
        }
        QCOMPARE(document->undoStack()->count(), 1);
        QVERIFY(document->isModified());
        session.undoGroup()->undo();
        QCOMPARE(pixelAt(*document, 3, 0), 0);
    }

    /// Botão direito desenha com a cor de fundo; Esc (cancel) descarta o traço.
    void penRightButtonAndCancel()
    {
        EditorSession session;
        auto document = makeBlankDocument();
        session.setActiveDocument(document.get());
        session.setBackgroundColor(2);
        gui::Tool& pen = session.tool();
        pen.press(session, { 3, 3 }, Qt::RightButton);
        QCOMPARE(pixelAt(*document, 3, 3), 2);
        session.cancelTool();
        QCOMPARE(pixelAt(*document, 3, 3), 0);
        QCOMPARE(document->undoStack()->count(), 0);
    }

    /// Trocar de documento no meio de um traço não deixa o traço pela metade.
    void switchingDocumentCancelsStroke()
    {
        EditorSession session;
        auto first = makeBlankDocument();
        auto second = makeBlankDocument();
        session.setActiveDocument(first.get());
        session.setDrawColor(5);
        session.tool().press(session, { 1, 1 }, Qt::LeftButton);
        QCOMPARE(pixelAt(*first, 1, 1), 5);
        session.setActiveDocument(second.get());
        QCOMPARE(pixelAt(*first, 1, 1), 0);
        QVERIFY(!first->isEditing());
        session.tool().release(session, { 1, 1 });
        QCOMPARE(first->undoStack()->count(), 0);
    }

    /// Figuras: o efeito elástico deixa apenas a figura final.
    void shapeToolKeepsOnlyFinalShape()
    {
        EditorSession session;
        auto document = makeBlankDocument();
        session.setActiveDocument(document.get());
        session.setToolKind(ToolKind::Rectangle);
        session.setDrawColor(5);
        gui::Tool& tool = session.tool();
        tool.press(session, { 1, 1 }, Qt::LeftButton);
        tool.move(session, { 10, 10 }); // retângulo grande (temporário)
        tool.move(session, { 4, 4 }); // retângulo final, menor
        tool.release(session, { 4, 4 });
        QCOMPARE(pixelAt(*document, 10, 10), 0); // apagado
        QCOMPARE(pixelAt(*document, 4, 4), 5);
        QCOMPARE(pixelAt(*document, 2, 2), 0); // contorno: interior vazio
        QCOMPARE(document->undoStack()->count(), 1);
    }

    /// Balde com tom: só os pixels do padrão são pintados.
    void fillHonoursTone()
    {
        EditorSession session;
        auto document = makeBlankDocument();
        session.setActiveDocument(document.get());
        session.setToolKind(ToolKind::Fill);
        session.setTone(core::Tone::E); // xadrez
        session.setDrawColor(9);
        session.tool().press(session, { 0, 0 }, Qt::LeftButton);
        session.tool().release(session, { 0, 0 });
        QCOMPARE(pixelAt(*document, 0, 0), 9);
        QCOMPARE(pixelAt(*document, 1, 0), 0);
        QCOMPARE(pixelAt(*document, 1, 1), 9);
    }

    /// Seleção: arrastar seleciona no documento; clique simples limpa.
    void selectToolCreatesAndClearsSelection()
    {
        EditorSession session;
        auto document = makeBlankDocument();
        session.setActiveDocument(document.get());
        session.setToolKind(ToolKind::Select);
        gui::Tool& tool = session.tool();
        tool.press(session, { 5, 6 }, Qt::LeftButton);
        tool.move(session, { 2, 3 });
        tool.release(session, { 2, 3 });
        QCOMPARE(document->selection().bounds(), (core::Rect { 2, 3, 4, 4 }));
        QCOMPARE(document->workArea(), (core::Rect { 2, 3, 4, 4 }));
        tool.press(session, { 1, 1 }, Qt::LeftButton);
        tool.release(session, { 1, 1 });
        QVERIFY(!document->hasSelection());
        QCOMPARE(document->workArea(), (core::Rect { 0, 0, 24, 32 }));
    }

    /// Pegadas usadas no destaque sob o mouse.
    void toolFootprints()
    {
        EditorSession session;
        auto document = makeTwoCellDocument();
        session.setActiveDocument(document.get());

        session.setToolKind(ToolKind::Pen);
        QCOMPARE(sortedFootprint(session, { 5, 5 }), (std::vector<core::Point> { { 5, 5 } }));

        // Ponta redonda de 3 px (o antigo lápis grosso): o "+" de 5 pixels.
        session.setBrushShape(core::BrushShape::Round);
        session.setBrushSize(3);
        QCOMPARE(sortedFootprint(session, { 5, 5 }),
            (std::vector<core::Point> { { 5, 4 }, { 4, 5 }, { 5, 5 }, { 6, 5 }, { 5, 6 } }));
        // Ponta quadrada de 2 px: o ponteiro é o pixel de cima e da esquerda.
        session.setBrushShape(core::BrushShape::Square);
        session.setBrushSize(2);
        QCOMPARE(sortedFootprint(session, { 5, 5 }),
            (std::vector<core::Point> { { 5, 5 }, { 6, 5 }, { 5, 6 }, { 6, 6 } }));

        // Figuras: a ponta no ponto inicial (aqui, quadrada de 2 px).
        session.setToolKind(ToolKind::FilledEllipse);
        QCOMPARE(sortedFootprint(session, { 2, 3 }),
            (std::vector<core::Point> { { 2, 3 }, { 3, 3 }, { 2, 4 }, { 3, 4 } }));
        session.setBrushSize(1);
        // Seleção: o pixel inicial.
        session.setToolKind(ToolKind::Select);
        QCOMPARE(sortedFootprint(session, { 2, 3 }), (std::vector<core::Point> { { 2, 3 } }));

        // Balde: a região contígua inteira (a metade direita, cor 4).
        session.setToolKind(ToolKind::Fill);
        const auto region = session.tool().footprint(session, { 30, 10 });
        QCOMPARE(region.size(), std::size_t { 24 * 32 });
        QVERIFY(std::all_of(
            region.begin(), region.end(), [](core::Point point) { return point.x >= 24; }));
    }

    /// canPlot reflete os limites da imagem, o tom e a máscara.
    void canPlotHonoursToneAndMask()
    {
        EditorSession session;
        auto document = makeTwoCellDocument();
        session.setActiveDocument(document.get());
        const core::AnyImage& canvas = document->canvas();
        QVERIFY(session.canPlot(canvas, { 0, 0 }));
        QVERIFY(!session.canPlot(canvas, { -1, 0 }));
        QVERIFY(!session.canPlot(canvas, { 48, 0 }));

        session.setTone(core::Tone::E); // xadrez: (1, 0) fica de fora
        QVERIFY(session.canPlot(canvas, { 0, 0 }));
        QVERIFY(!session.canPlot(canvas, { 1, 0 }));
        session.setTone(core::Tone::A);

        session.toggleMasked(4); // protege a metade direita
        QVERIFY(session.canPlot(canvas, { 0, 0 }));
        QVERIFY(!session.canPlot(canvas, { 30, 0 }));
    }

    /// Regras de colar: o efeito transparente ignora a cor de fundo (indexado)
    /// ou os pixels de alfa 0 (RGBA).
    void pasteRulesFollowTransparentEffect()
    {
        EditorSession session;
        session.setBackgroundColor(6);
        QVERIFY(!session.pasteRules().transparentIndex.has_value());
        QVERIFY(!session.rgbaPasteRules().skipTransparent);
        session.setTransparentEffect(true);
        QCOMPARE(int(session.pasteRules().transparentIndex.value()), 6);
        QVERIFY(session.rgbaPasteRules().skipTransparent);
    }

    /// Grades: valores vazios são rejeitados.
    void gridLayoutAndLastCell()
    {
        EditorSession session;
        QCOMPARE(session.gridLayout(), core::GridLayout { });
        core::GridLayout invalid;
        invalid.mainGrid = { 0, 1 };
        session.setGridLayout(invalid);
        QCOMPARE(session.gridLayout(), core::GridLayout { });

        core::GridLayout layout;
        layout.subGrid = { 16, 16 };
        session.setGridLayout(layout);
        QCOMPARE(session.gridLayout().subGrid, (core::Size { 16, 16 }));
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
};

QTEST_MAIN(EditorSessionTest)
#include "EditorSessionTest.moc"
