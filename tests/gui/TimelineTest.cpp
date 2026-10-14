// =============================================================================
// TimelineTest.cpp — cenas e camadas: operações do documento (com desfazer),
// composição (índice 0 transparente, opacidade), edição da célula ativa,
// cópia ao escrever, gravação nos formatos de animação e o painel inferior.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"
#include "gui/TimelinePanel.h"
#include "io/ImageFile.h"

#include <QAction>
#include <QFileInfo>
#include <QImageReader>
#include <QListWidget>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace spritenator;
using gui::ActionId;
using gui::Document;
using gui::EditorSession;

namespace {

std::unique_ptr<Document> makeIndexed(core::Size size = { 4, 3 }, int color = 1)
{
    return std::make_unique<Document>(
        core::IndexedImage(size, static_cast<core::ColorIndex>(color)),
        core::Palette::makeDefault(core::ColorDepth::Bits8));
}

/// Pinta um pixel da célula ativa como um passo de desfazer.
void paint(Document& document, core::Point point, int color)
{
    core::IndexedImage pixel({ 1, 1 }, static_cast<core::ColorIndex>(color));
    document.beginEdit();
    document.editLive([&](core::AnyImage& image) {
        core::pasteImage(image, pixel, point);
        return core::Rect { point.x, point.y, 1, 1 };
    });
    document.commitEdit(gui::UndoLabel { gui::UndoOperation::Pen });
}

int indexAt(const Document& document, int frame, int layer, int x, int y)
{
    return std::get<core::IndexedImage>(document.cel(frame, layer)).pixel(x, y);
}

} // namespace

class TimelineTest final : public QObject {
    Q_OBJECT

private slots:
    /// Cenas: nova (vazia), duplicar (cópia independente), mover, apagar;
    /// cada operação desfaz.
    void frameOperations()
    {
        auto document = makeIndexed();
        paint(*document, { 0, 0 }, 5);
        document->duplicateFrame();
        QCOMPARE(document->frameCount(), 2);
        QCOMPARE(document->currentFrame(), 1);
        QCOMPARE(indexAt(*document, 1, 0, 0, 0), 5);
        // Cópia ao escrever: editar a cópia não muda a original.
        paint(*document, { 0, 0 }, 6);
        QCOMPARE(indexAt(*document, 0, 0, 0, 0), 5);
        QCOMPARE(indexAt(*document, 1, 0, 0, 0), 6);

        document->addFrame(); // vazia: índice 0
        QCOMPARE(document->frameCount(), 3);
        QCOMPARE(indexAt(*document, 2, 0, 0, 0), 0);
        document->moveFrame(-1);
        QCOMPARE(document->currentFrame(), 1);
        QCOMPARE(indexAt(*document, 2, 0, 0, 0), 6);
        document->removeFrame();
        QCOMPARE(document->frameCount(), 2);

        // Desfazer tudo volta a uma cena com o pixel 5.
        while (document->undoStack()->canUndo()) {
            document->undoStack()->undo();
        }
        QCOMPARE(document->frameCount(), 1);
        QCOMPARE(indexAt(*document, 0, 0, 0, 0), 1);
    }

    /// Desfazer um traço de outra cena leva àquela cena.
    void undoGoesToTheEditedCel()
    {
        auto document = makeIndexed();
        document->addFrame();
        paint(*document, { 1, 1 }, 9); // na cena 2
        document->setCurrentFrame(0);
        document->undoStack()->undo();
        QCOMPARE(document->currentFrame(), 1);
        QCOMPARE(indexAt(*document, 1, 0, 1, 1), 0);
    }

    /// Camadas: o índice 0 das de cima é transparente; visibilidade, mover,
    /// mesclar e apagar.
    void layerOperationsAndComposite()
    {
        auto document = makeIndexed({ 3, 1 }, 1); // fundo cor 1
        document->addLayer();
        QCOMPARE(document->layerCount(), 2);
        QCOMPARE(document->currentLayer(), 1);
        paint(*document, { 1, 0 }, 4);
        const auto rgbOf = [&](int index) {
            const core::Rgb color = document->palette().color(index);
            return qRgb(color.red, color.green, color.blue);
        };
        QCOMPARE(document->renderedImage().pixel(0, 0), rgbOf(1)); // 0 de cima: transparente
        QCOMPARE(document->renderedImage().pixel(1, 0), rgbOf(4));
        QCOMPARE(std::get<core::IndexedImage>(document->flattened(0)).pixel(1, 0),
            core::ColorIndex { 4 });

        document->setLayerVisible(1, false);
        QCOMPARE(document->renderedImage().pixel(1, 0), rgbOf(1));
        document->setLayerVisible(1, true);
        document->renameLayer(1, QStringLiteral("Olhos"));
        QCOMPARE(document->layers()[1].name, QStringLiteral("Olhos"));

        document->duplicateLayer();
        QCOMPARE(document->layerCount(), 3);
        document->moveLayer(-1);
        QCOMPARE(document->currentLayer(), 1);
        document->mergeLayerDown(); // junta com o fundo
        QCOMPARE(document->layerCount(), 2);
        QCOMPARE(indexAt(*document, 0, 0, 1, 0), 4);
        QCOMPARE(indexAt(*document, 0, 0, 0, 0), 1); // o 0 de cima não cobriu o fundo
        document->removeLayer();
        QCOMPARE(document->layerCount(), 1);
        document->removeLayer(); // a última camada fica
        QCOMPARE(document->layerCount(), 1);
    }

    /// RGBA: a opacidade da camada entra na composição; ajustes seguidos de
    /// opacidade viram um passo de desfazer.
    void rgbaOpacity()
    {
        Document document(core::RgbaImage({ 1, 1 }, core::Rgba { 0, 0, 0, 255 }),
            core::Palette::makeDefault(core::ColorDepth::Bits8));
        document.addLayer();
        core::RgbaImage white({ 1, 1 }, core::Rgba { 255, 255, 255, 255 });
        document.beginEdit();
        document.editLive([&](core::AnyImage& image) {
            core::pasteImage(image, white, { 0, 0 });
            return core::Rect { 0, 0, 1, 1 };
        });
        document.commitEdit(gui::UndoLabel { gui::UndoOperation::Pen });
        const int before = document.undoStack()->count();
        document.setLayerOpacity(1, 200);
        document.setLayerOpacity(1, 128);
        QCOMPARE(document.undoStack()->count(), before + 1);
        QVERIFY(std::abs(qRed(document.renderedImage().pixel(0, 0)) - 128) <= 1);
        QCOMPARE(qAlpha(document.renderedImage().pixel(0, 0)), 255);
    }

    /// Redimensionar o canvas muda todas as células, em todas as cenas.
    void resizeAllCels()
    {
        auto document = makeIndexed({ 2, 2 }, 3);
        document->addFrame();
        document->addLayer();
        document->transformCels(
            [](const core::AnyImage& cel, int /*layer*/) -> core::AnyImage {
                core::AnyImage bigger = core::blankImage(core::modeOf(cel), { 4, 4 });
                core::pasteImage(bigger, cel, { 0, 0 });
                return bigger;
            },
            document->palette(), gui::UndoLabel { gui::UndoOperation::CanvasSize });
        for (int frame = 0; frame < 2; ++frame) {
            for (int layer = 0; layer < 2; ++layer) {
                QCOMPARE(core::sizeOf(document->cel(frame, layer)), (core::Size { 4, 4 }));
            }
        }
        QCOMPARE(document->renderedImage().size(), QSize(4, 4));
    }

    /// Gravar: projeto (reabre igual), GIF (as cenas), sprite sheet (cenas
    /// lado a lado) e imagens separadas (nome_001.png...).
    void savingFormats()
    {
        QTemporaryDir directory;
        auto document = makeIndexed({ 4, 3 }, 1);
        document->addLayer();
        paint(*document, { 2, 1 }, 7);
        document->duplicateFrame();
        document->setFrameDuration(1, 250);

        const QString project = directory.filePath(QStringLiteral("anim.sprn"));
        document->saveAs(project, { gui::SaveFormat::Project, 0 });
        QVERIFY(!document->isModified());
        std::unique_ptr<Document> reopened(Document::load(project));
        QCOMPARE(reopened->frameCount(), 2);
        QCOMPARE(reopened->layerCount(), 2);
        QCOMPARE(reopened->frames()[1].durationMs, 250);
        QCOMPARE(indexAt(*reopened, 1, 1, 2, 1), 7);
        QCOMPARE(reopened->saveOptions().format, gui::SaveFormat::Project);

        const QString gif = directory.filePath(QStringLiteral("anim.gif"));
        document->saveAs(gif, { gui::SaveFormat::Gif, 0 });
        std::unique_ptr<Document> fromGif(Document::load(gif));
        QCOMPARE(fromGif->frameCount(), 2);
        QCOMPARE(fromGif->frames()[1].durationMs, 250);

        const QString sheet = directory.filePath(QStringLiteral("sheet.png"));
        document->saveAs(sheet, { gui::SaveFormat::SpriteSheet, 1 }); // uma por linha
        QCOMPARE(QImageReader(sheet).size(), QSize(4, 6));
        QVERIFY(!document->canRevert());

        const QString frames = directory.filePath(QStringLiteral("walk.png"));
        document->saveAs(frames, { gui::SaveFormat::Frames, 0 });
        QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("walk_001.png"))));
        QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("walk_002.png"))));
        const io::ImageData second
            = io::ImageFile::load(directory.filePath(QStringLiteral("walk_002.png")));
        QCOMPARE(std::get<core::IndexedImage>(second.image).pixel(2, 1), core::ColorIndex { 7 });
    }

    /// Painel: listas acompanham o documento; clicar escolhe cena e camada;
    /// a caixa da camada controla a visibilidade; reproduzir avança as cenas.
    void timelinePanel()
    {
        QObject owner;
        gui::ActionCatalog actions(&owner);
        EditorSession session;
        gui::TimelinePanel panel(&session, &actions);
        auto document = makeIndexed();
        session.setActiveDocument(document.get());
        QCOMPARE(panel.frameList()->count(), 1);
        QCOMPARE(panel.layerList()->count(), 1);
        QVERIFY(!actions.action(ActionId::FrameDelete)->isEnabled());

        actions.action(ActionId::FrameNew)->trigger();
        actions.action(ActionId::LayerNew)->trigger();
        QCOMPARE(panel.frameList()->count(), 2);
        QCOMPARE(panel.layerList()->count(), 2);
        QCOMPARE(
            panel.layerList()->item(0)->text(), document->layers()[1].name); // a de cima primeiro

        panel.frameList()->setCurrentRow(0);
        QCOMPARE(document->currentFrame(), 0);
        panel.layerList()->setCurrentRow(1); // a de baixo
        QCOMPARE(document->currentLayer(), 0);
        panel.layerList()->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!document->layers()[1].visible);
        QVERIFY(!panel.opacitySlider()->isEnabled()); // indexada: sem opacidade

        actions.action(ActionId::OnionSkin)->trigger();
        QVERIFY(session.onionSkin());

        document->setFrameDuration(0, 20);
        document->setFrameDuration(1, 20);
        // Registra cada troca de cena (conferir currentFrame() de tempos em
        // tempos poderia sempre cair na mesma cena).
        QSignalSpy changes(document.get(), &Document::currentCelChanged);
        actions.action(ActionId::PlayPause)->trigger();
        QVERIFY(panel.isPlaying());
        // Espera com QSignalSpy::wait (os macros QTRY_* geram avisos de
        // conversão no Qt 6.9 com -Werror).
        for (int attempt = 0; attempt < 20 && changes.count() < 2; ++attempt) {
            changes.wait(100);
        }
        QVERIFY(changes.count() >= 2);
        QCOMPARE(changes.at(0).at(0).toInt(), 1);
        QCOMPARE(changes.at(1).at(0).toInt(), 0); // em laço
        actions.action(ActionId::PlayPause)->trigger();
        QVERIFY(!panel.isPlaying());
        session.setActiveDocument(nullptr);
        QCOMPARE(panel.frameList()->count(), 0);
    }
};

QTEST_MAIN(TimelineTest)
#include "TimelineTest.moc"
