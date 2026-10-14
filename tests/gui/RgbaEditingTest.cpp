// =============================================================================
// RgbaEditingTest.cpp — edição de documentos RGBA: cores primária/
// secundária, ferramentas, transparência, conversões entre modos, operações
// de edição e estado da janela principal.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/Document.h"
#include "gui/EditOperations.h"
#include "gui/EditorSession.h"
#include "gui/MainWindow.h"
#include "gui/widgets/PaletteView.h"
#include "io/ImageFile.h"

#include <QAction>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace spritenator;
using gui::ColorRole;
using gui::Document;
using gui::EditorSession;
using gui::ToolKind;

namespace {

constexpr core::Rgba kRed { 255, 0, 0, 255 };
constexpr core::Rgba kTranslucentBlue { 0, 0, 255, 100 };
constexpr core::Rgba kClear { };

/// Documento RGBA 48x32 (duas células 24x32), todo vermelho.
std::unique_ptr<Document> makeRgbaDocument()
{
    return std::make_unique<Document>(
        core::RgbaImage({ 48, 32 }, kRed), core::Palette::makeDefault(core::ColorDepth::Bits8));
}

/// Pixel RGBA do canvas de um documento.
core::Rgba canvasPixel(const Document& document, int x, int y)
{
    return std::get<core::RgbaImage>(document.canvas()).pixel(x, y);
}

} // namespace

class RgbaEditingTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("RgbaEditingTest"));
    }

    /// O modo de cor da sessão acompanha o documento ativo.
    void sessionFollowsActiveDocumentMode()
    {
        EditorSession session;
        QCOMPARE(session.colorMode(), core::ColorMode::Indexed);
        QSignalSpy modeChanges(&session, &EditorSession::colorModeChanged);
        auto document = makeRgbaDocument();
        session.setActiveDocument(document.get());
        QCOMPARE(session.colorMode(), core::ColorMode::Rgba);
        QCOMPARE(modeChanges.count(), 1);
        session.setActiveDocument(nullptr);
        QCOMPARE(session.colorMode(), core::ColorMode::Indexed);
        QCOMPARE(modeChanges.count(), 2);
    }

    /// Lápis: esquerdo pinta a primária (com alfa exato); direito, a
    /// secundária, que por padrão é transparente (apaga).
    void penUsesRgbaColors()
    {
        EditorSession session;
        auto document = makeRgbaDocument();
        session.setActiveDocument(document.get());
        session.setPrimaryRgba(kTranslucentBlue);
        session.setToolKind(ToolKind::Pen);

        session.tool().press(session, { 2, 2 }, Qt::LeftButton);
        session.tool().release(session, { 2, 2 });
        QCOMPARE(canvasPixel(*document, 2, 2), kTranslucentBlue); // substitui, não mistura

        session.tool().press(session, { 5, 5 }, Qt::RightButton);
        session.tool().release(session, { 5, 5 });
        QCOMPARE(canvasPixel(*document, 5, 5), kClear);
        QCOMPARE(document->undoStack()->count(), 2);
        QCOMPARE(session.rgbaFor(ColorRole::Secondary), kClear);
    }

    /// Balde em RGBA: preenche a região contígua da mesma cor.
    void fillWorksWithRgba()
    {
        EditorSession session;
        auto document = makeRgbaDocument();
        session.setActiveDocument(document.get());
        session.setPrimaryRgba(kTranslucentBlue);
        session.setToolKind(ToolKind::Fill);
        session.tool().press(session, { 0, 0 }, Qt::LeftButton);
        session.tool().release(session, { 0, 0 });
        QCOMPARE(canvasPixel(*document, 47, 31), kTranslucentBlue);
    }

    /// Colar com o efeito transparente: pixels de alfa 0 não são colados.
    void pasteSkipsTransparentPixelsWithEffect()
    {
        EditorSession session;
        gui::EditOperations operations(&session);
        auto document = makeRgbaDocument();
        session.setActiveDocument(document.get());
        // Origem: [transparente, azul translúcido].
        core::RgbaImage canvas = std::get<core::RgbaImage>(document->canvas());
        canvas.setPixel({ 0, 0 }, kClear);
        canvas.setPixel({ 1, 0 }, kTranslucentBlue);
        document->applyCanvas(canvas);
        document->setSelection(core::Rect { 0, 0, 2, 1 });
        operations.copy();

        session.setTransparentEffect(true);
        document->setSelection(core::Rect { 10, 10, 1, 1 });
        operations.paste();
        QCOMPARE(canvasPixel(*document, 10, 10), kRed); // preservado
        QCOMPARE(canvasPixel(*document, 11, 10), kTranslucentBlue); // colado

        session.setTransparentEffect(false);
        document->setSelection(core::Rect { 20, 20, 1, 1 });
        operations.paste();
        QCOMPARE(canvasPixel(*document, 20, 20), kClear); // sem o efeito: substitui
    }

    /// Colar pixels RGBA num documento indexado converte para a paleta.
    void pasteIntoIndexedDocumentConverts()
    {
        EditorSession session;
        gui::EditOperations operations(&session);
        auto rgba = makeRgbaDocument();
        session.setActiveDocument(rgba.get());
        rgba->setSelection(core::Rect { 0, 0, 4, 4 });
        operations.copy(); // vermelho

        Document indexed(
            core::IndexedImage({ 24, 32 }, 0), core::Palette::makeDefault(core::ColorDepth::Bits4));
        session.setActiveDocument(&indexed);
        operations.paste();
        QCOMPARE(int(indexed.indexedCanvas().pixel(3, 3)), 9); // vermelho vivo da paleta
        session.setActiveDocument(nullptr);
    }

    /// Operações de edição em RGBA: limpar, sombra, rotação, copiar/colar.
    void editOperationsWorkWithRgba()
    {
        EditorSession session;
        gui::EditOperations operations(&session);
        auto document = makeRgbaDocument();
        session.setActiveDocument(document.get());
        core::RgbaImage frame({ 48, 32 });
        frame.setPixel({ 0, 0 }, kTranslucentBlue);
        frame.setPixel({ 1, 0 }, kRed);
        document->applyCanvas(frame);

        // Sombra: cor primária, alfa original.
        session.setPrimaryRgba({ 10, 20, 30, 255 });
        operations.prepareShadow();
        QCOMPARE(canvasPixel(*document, 0, 0), (core::Rgba { 10, 20, 30, 100 }));
        QCOMPARE(canvasPixel(*document, 5, 5), kClear);

        // Copiar e colar preserva o alfa exato.
        document->setSelection(core::Rect { 0, 0, 1, 1 });
        operations.copy();
        document->setSelection(core::Rect { 10, 10, 1, 1 });
        operations.paste();
        QCOMPARE(canvasPixel(*document, 10, 10), (core::Rgba { 10, 20, 30, 100 }));

        // Rotação: lacunas ficam transparentes.
        document->setSelection(core::Rect { 0, 0, 4, 2 });
        operations.transform(gui::TransformKind::RotateClockwise);
        // Limpar: tudo transparente.
        operations.selectAll();
        operations.clear();
        QCOMPARE(canvasPixel(*document, 1, 0), kClear);
    }

    /// Janela principal com documento RGBA: menu Paleta desabilitado, grade
    /// da paleta oculta; voltam ao abrir um documento indexado.
    void mainWindowAdaptsToColorMode()
    {
        QTemporaryDir directory;
        const QString rgbaPath = directory.filePath(QStringLiteral("rgba.png"));
        const QString indexedPath = directory.filePath(QStringLiteral("indexed.png"));
        io::ImageFile::save(rgbaPath,
            { core::RgbaImage({ 8, 8 }, kRed),
                core::Palette::makeDefault(core::ColorDepth::Bits8) });
        io::ImageFile::save(indexedPath,
            { core::IndexedImage({ 8, 8 }), core::Palette::makeDefault(core::ColorDepth::Bits8) });

        gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* paletteView = window.findChild<gui::PaletteView*>();
        QVERIFY(paletteView != nullptr);

        QVERIFY(window.openFile(rgbaPath));
        QCOMPARE(window.session()->colorMode(), core::ColorMode::Rgba);
        QVERIFY(!window.actions()->action(gui::ActionId::PaletteEdit)->isEnabled());
        QVERIFY(!window.actions()->action(gui::ActionId::PaletteMaskMode)->isEnabled());
        QVERIFY(window.actions()->action(gui::ActionId::PaletteTransparent)->isEnabled());
        QVERIFY(!paletteView->isVisible());

        QVERIFY(window.openFile(indexedPath));
        QCOMPARE(window.session()->colorMode(), core::ColorMode::Indexed);
        QVERIFY(window.actions()->action(gui::ActionId::PaletteEdit)->isEnabled());
        QVERIFY(paletteView->isVisible());
    }
};

QTEST_MAIN(RgbaEditingTest)
#include "RgbaEditingTest.moc"
