// =============================================================================
// DocumentTest.cpp — testes do ciclo de vida de um documento.
// =============================================================================
#include "gui/Document.h"
#include "io/IoError.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoCommand>
#include <QUndoStack>

using namespace spritenator;
using spritenator::gui::Document;

namespace {

/// Comando mínimo que pinta um pixel (simula uma edição do usuário).
class PaintPixelCommand final : public QUndoCommand {
public:
    PaintPixelCommand(Document* document, core::Point point, core::ColorIndex color)
        : m_document(document)
        , m_point(point)
        , m_before(document->indexedCanvas().copy({ point.x, point.y, 1, 1 }))
        , m_after(core::Size { 1, 1 }, color)
    {
    }
    void redo() override { m_document->applyCanvasRegion(m_point, m_after); }
    void undo() override { m_document->applyCanvasRegion(m_point, m_before); }

private:
    Document* m_document;
    core::Point m_point;
    core::IndexedImage m_before;
    core::IndexedImage m_after;
};

Document* makeDocument()
{
    return new Document(
        core::IndexedImage({ 8, 4 }, 0), core::Palette::makeDefault(core::ColorDepth::Bits8));
}

} // namespace

class DocumentTest final : public QObject {
    Q_OBJECT

private slots:
    void newDocumentIsUntitledAndClean()
    {
        std::unique_ptr<Document> document(makeDocument());
        QVERIFY(!document->hasFilePath());
        QVERIFY(document->displayName().startsWith(QStringLiteral("Untitled")));
        QVERIFY(!document->isModified());
        QCOMPARE(document->renderedImage().size(), QSize(8, 4));
    }

    /// Editar marca como modificado; desfazer volta ao estado limpo.
    void editingTracksModification()
    {
        std::unique_ptr<Document> document(makeDocument());
        QSignalSpy modified(document.get(), &Document::modifiedChanged);
        QSignalSpy changed(document.get(), &Document::canvasChanged);

        document->undoStack()->push(new PaintPixelCommand(document.get(), { 2, 1 }, 9));
        QVERIFY(document->isModified());
        QCOMPARE(int(document->indexedCanvas().pixel(2, 1)), 9);
        QCOMPARE(document->renderedImage().pixelIndex(2, 1), 9); // cache atualizado
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().value<core::Rect>(), (core::Rect { 2, 1, 1, 1 }));

        document->undoStack()->undo();
        QVERIFY(!document->isModified());
        QCOMPARE(int(document->indexedCanvas().pixel(2, 1)), 0);
        QCOMPARE(modified.count(), 2);
    }

    /// Salvar limpa o estado; reverter descarta edições posteriores.
    void saveAndRevert()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("doc.png"));

        std::unique_ptr<Document> document(makeDocument());
        document->undoStack()->push(new PaintPixelCommand(document.get(), { 0, 0 }, 5));
        document->saveAs(path);
        QVERIFY(!document->isModified());
        QCOMPARE(document->displayName(), QStringLiteral("doc.png"));

        document->undoStack()->push(new PaintPixelCommand(document.get(), { 0, 0 }, 7));
        QVERIFY(document->isModified());
        document->revert();
        QVERIFY(!document->isModified());
        QCOMPARE(int(document->indexedCanvas().pixel(0, 0)), 5);
        QCOMPARE(document->undoStack()->count(), 0);

        std::unique_ptr<Document> reopened(Document::load(path));
        QCOMPARE(reopened->canvas(), document->canvas());
    }

    void loadingMissingFileThrows()
    {
        QVERIFY_THROWS_EXCEPTION(
            io::IoError, std::ignore = Document::load(QStringLiteral("/nonexistent.png")));
    }

    void paletteChangeUpdatesColorTable()
    {
        std::unique_ptr<Document> document(makeDocument());
        core::Palette palette = document->palette();
        palette.setColor(0, { 1, 2, 3 });
        document->applyPalette(palette);
        QCOMPARE(document->renderedImage().color(0), qRgb(1, 2, 3));
    }

    void resizingEmitsSignal()
    {
        std::unique_ptr<Document> document(makeDocument());
        QSignalSpy resized(document.get(), &Document::canvasResized);
        document->applyCanvas(core::IndexedImage({ 3, 3 }));
        QCOMPARE(resized.count(), 1);
        QCOMPARE(document->renderedImage().size(), QSize(3, 3));
    }
};

QTEST_MAIN(DocumentTest)
#include "DocumentTest.moc"
