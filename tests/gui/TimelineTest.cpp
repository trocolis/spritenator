// =============================================================================
// TimelineTest.cpp — cenas e camadas: operações do documento (com desfazer),
// composição (índice 0 transparente, opacidade), edição da célula ativa,
// cópia ao escrever, gravação nos formatos de animação e o painel inferior.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"
#include "gui/TimelineGrid.h"
#include "gui/TimelinePanel.h"
#include "io/ImageFile.h"

#include <QAction>
#include <QFileInfo>
#include <QImageReader>
#include <QSignalSpy>
#include <QSpinBox>
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
        document->setLayerLocked(0, true);

        const QString project = directory.filePath(QStringLiteral("anim.sprn"));
        document->saveAs(project, { gui::SaveFormat::Project, 0 });
        QVERIFY(!document->isModified());
        std::unique_ptr<Document> reopened(Document::load(project));
        QCOMPARE(reopened->frameCount(), 2);
        QCOMPARE(reopened->layerCount(), 2);
        QCOMPARE(reopened->frames()[1].durationMs, 250);
        QVERIFY(reopened->layers()[0].locked);
        QVERIFY(!reopened->layers()[1].locked);
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

    /// Cadeado: a camada bloqueada não aceita pintura (as outras sim) e
    /// bloquear se desfaz; "todas as durações" vale para todas as cenas.
    void lockedLayerAndDurations()
    {
        EditorSession session;
        auto document = makeIndexed();
        session.setActiveDocument(document.get());
        document->addLayer(); // a de cima fica ativa
        QVERIFY(session.canPlot(document->canvas(), { 0, 0 }));
        document->setLayerLocked(1, true);
        QVERIFY(!document->canEditActiveCel());
        QVERIFY(!session.canPlot(document->canvas(), { 0, 0 }));
        document->setCurrentLayer(0);
        QVERIFY(session.canPlot(document->canvas(), { 0, 0 }));
        document->undoStack()->undo();
        QVERIFY(!document->layers()[1].locked);

        document->addFrame();
        document->setAllFrameDurations(40);
        document->setAllFrameDurations(50); // ajustes seguidos: um passo só
        QCOMPARE(document->frames()[0].durationMs, 50);
        QCOMPARE(document->frames()[1].durationMs, 50);
        document->undoStack()->undo();
        QCOMPARE(document->frames()[0].durationMs, 100);
    }

    /// Painel: a grade acompanha o documento; clicar escolhe cena e camada;
    /// o olho e o cadeado mudam a camada; o FPS vale para todas as cenas;
    /// reproduzir avança as cenas em laço ou em vai e volta.
    void timelinePanel()
    {
        QObject owner;
        gui::ActionCatalog actions(&owner);
        EditorSession session;
        gui::TimelinePanel panel(&session, &actions);
        panel.resize(600, 200);
        panel.show();
        auto document = makeIndexed();
        session.setActiveDocument(document.get());
        gui::TimelineGrid* grid = panel.grid();
        QWidget* viewport = grid->viewport();
        QVERIFY(!actions.action(ActionId::FrameDelete)->isEnabled());
        QVERIFY(!actions.action(ActionId::FrameFirst)->isEnabled());

        actions.action(ActionId::FrameNew)->trigger();
        actions.action(ActionId::LayerNew)->trigger();
        QCOMPARE(document->frameCount(), 2);
        QCOMPARE(document->layerCount(), 2);
        // A camada de cima fica na primeira linha.
        QVERIFY(grid->cellRect(0, 1).y() < grid->cellRect(0, 0).y());
        QVERIFY(grid->cellFilled(0, 0)); // fundo na cor 1
        QVERIFY(!grid->cellFilled(1, 1)); // camada nova: vazia

        QTest::mouseClick(viewport, Qt::LeftButton, { }, grid->cellRect(0, 0).center());
        QCOMPARE(document->currentFrame(), 0);
        QCOMPARE(document->currentLayer(), 0);
        QTest::mouseClick(viewport, Qt::LeftButton, { }, grid->frameHeaderRect(1).center());
        QCOMPARE(document->currentFrame(), 1);
        QTest::mouseClick(viewport, Qt::LeftButton, { }, grid->nameRect(1).center());
        QCOMPARE(document->currentLayer(), 1);
        QTest::mouseClick(viewport, Qt::LeftButton, { }, grid->eyeRect(1).center());
        QVERIFY(!document->layers()[1].visible);
        QTest::mouseClick(viewport, Qt::LeftButton, { }, grid->lockRect(1).center());
        QVERIFY(document->layers()[1].locked);
        QVERIFY(actions.action(ActionId::LayerLock)->isChecked());
        actions.action(ActionId::LayerLock)->trigger();
        QVERIFY(!document->layers()[1].locked);
        QVERIFY(!panel.opacityBox()->isEnabled()); // indexada: sem opacidade

        actions.action(ActionId::FrameFirst)->trigger();
        QCOMPARE(document->currentFrame(), 0);
        actions.action(ActionId::FrameLast)->trigger();
        QCOMPARE(document->currentFrame(), 1);

        actions.action(ActionId::OnionSkin)->trigger();
        QVERIFY(session.onionSkin());

        panel.fpsBox()->setValue(50); // 20 ms por cena
        QCOMPARE(document->frames()[0].durationMs, 20);
        QCOMPARE(document->frames()[1].durationMs, 20);

        // Registra cada troca de cena (conferir currentFrame() de tempos em
        // tempos poderia sempre cair na mesma cena). Espera com
        // QSignalSpy::wait (os macros QTRY_* geram avisos de conversão no
        // Qt 6.9 com -Werror).
        const auto play = [&](int changes) {
            QSignalSpy spy(document.get(), &Document::currentCelChanged);
            actions.action(ActionId::PlayPause)->trigger();
            for (int attempt = 0; attempt < 40 && spy.count() < changes; ++attempt) {
                spy.wait(100);
            }
            actions.action(ActionId::PlayPause)->trigger();
            QList<int> frames;
            for (const QList<QVariant>& arguments : spy) {
                frames.append(arguments.at(0).toInt());
            }
            return frames.mid(0, changes);
        };
        actions.action(ActionId::FrameNew)->trigger(); // três cenas
        actions.action(ActionId::FrameFirst)->trigger();
        QCOMPARE(play(3), (QList<int> { 1, 2, 0 })); // em laço
        QVERIFY(!panel.isPlaying());
        actions.action(ActionId::FrameFirst)->trigger();
        actions.action(ActionId::PlaybackPingPong)->trigger();
        QCOMPARE(play(4), (QList<int> { 1, 2, 1, 0 })); // vai e volta

        session.setActiveDocument(nullptr);
        QVERIFY(!actions.action(ActionId::FrameNew)->isEnabled());
        QVERIFY(!panel.fpsBox()->isEnabled());
    }
};

QTEST_MAIN(TimelineTest)
#include "TimelineTest.moc"
