// =============================================================================
// ColorPickerTest.cpp — seletor circular (anel + triângulo), mostradores e
// barras R, G, B, A, e a sincronização entre eles e a sessão.
// =============================================================================
#include "gui/Document.h"
#include "gui/EditorSession.h"
#include "gui/IntegratorPanel.h"
#include "gui/MainWindow.h"
#include "gui/widgets/ColorEditor.h"
#include "gui/widgets/ColorSwatch.h"
#include "gui/widgets/ColorWheel.h"
#include "io/ImageFile.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace spritenator;
using gui::ColorRole;
using gui::ColorWheel;

namespace {

/// Cria uma janela principal com um documento RGBA aberto.
struct RgbaWindow {
    QTemporaryDir directory;
    gui::MainWindow window;

    RgbaWindow()
    {
        const QString path = directory.filePath(QStringLiteral("rgba.png"));
        io::ImageFile::save(path,
            { core::RgbaImage({ 24, 32 }), core::Palette::makeDefault(core::ColorDepth::Bits8) });
        window.resize(1100, 800);
        window.show();
        exposed = QTest::qWaitForWindowExposed(&window);
        opened = window.openFile(path);
    }

    bool exposed = false; ///< verificados pelos testes (QVERIFY não vale em construtor)
    bool opened = false;

    [[nodiscard]] gui::EditorSession* session() const { return window.session(); }
    [[nodiscard]] ColorWheel* wheel() const { return window.findChild<ColorWheel*>(); }
    [[nodiscard]] gui::ColorEditor* editor() const { return window.findChild<gui::ColorEditor*>(); }
};

} // namespace

class ColorPickerTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("ColorPickerTest"));
    }

    /// Geometria do triângulo: vértices e ida e volta saturação/valor.
    void triangleMath()
    {
        using namespace gui::wheel;
        // Vértices: cor pura (S=1, V=1), branco (S=0, V=1), preto (V=0).
        const SaturationValue pure = toSaturationValue({ 1, 0, 0 });
        QCOMPARE(pure.saturation, 1.0);
        QCOMPARE(pure.value, 1.0);
        QCOMPARE(toSaturationValue({ 0, 1, 0 }).saturation, 0.0);
        QCOMPARE(toSaturationValue({ 0, 0, 1 }).value, 0.0);

        for (const SaturationValue color : { SaturationValue { 0.3, 0.7 },
                 SaturationValue { 1.0, 0.5 }, SaturationValue { 0.0, 0.2 } }) {
            const SaturationValue back = toSaturationValue(toBarycentric(color));
            QVERIFY(std::abs(back.value - color.value) < 1e-9);
            if (color.value > 0) {
                QVERIFY(std::abs(back.saturation - color.saturation) < 1e-9);
            }
        }
        // Pontos fora do triângulo são projetados para dentro.
        const SaturationValue outside = toSaturationValue({ 1.5, -0.2, -0.3 });
        QCOMPARE(outside.value, 1.0);

        // Baricêntricas: o centróide tem pesos iguais.
        const Barycentric centroid
            = barycentricOf(QPointF(1, 1), QPointF(0, 0), QPointF(3, 0), QPointF(0, 3));
        QVERIFY(std::abs(centroid.hueWeight - 1.0 / 3) < 1e-9);
        QVERIFY(std::abs(centroid.whiteWeight - 1.0 / 3) < 1e-9);
    }

    /// Clicar no anel muda o matiz; nos vértices do triângulo, branco/preto.
    void wheelClicksPickColors()
    {
        ColorWheel wheel;
        wheel.resize(200, 200);
        wheel.setColor(Qt::red);
        QSignalSpy picked(&wheel, &ColorWheel::colorPicked);

        // Anel no matiz 240 (azul): a cor pura do triângulo passa a ser azul.
        QTest::mouseClick(&wheel, Qt::LeftButton, { }, wheel.ringPointForHue(240).toPoint());
        QCOMPARE(picked.count(), 1);
        QVERIFY(std::abs(wheel.color().hsvHue() - 240) <= 1);
        QCOMPARE(picked.last().at(1).value<Qt::MouseButton>(), Qt::LeftButton);

        QPointF pure;
        QPointF white;
        QPointF black;
        wheel.triangleVertices(pure, white, black);
        const auto towardsCenter
            = [&](QPointF vertex) { return (vertex * 0.97 + QPointF(100, 100) * 0.03).toPoint(); };
        QTest::mouseClick(&wheel, Qt::RightButton, { }, towardsCenter(white));
        QVERIFY(wheel.color().value() > 240 && wheel.color().hsvSaturation() < 20);
        QCOMPARE(picked.last().at(1).value<Qt::MouseButton>(), Qt::RightButton);

        QTest::mouseClick(&wheel, Qt::LeftButton, { }, towardsCenter(black));
        QVERIFY(wheel.color().value() < 15);

        // setColor com cinza mantém o matiz atual (o triângulo não "pula").
        wheel.setColor(QColor(128, 128, 128));
        QVERIFY(std::abs(wheel.color().hsvHue() - 240) <= 1);
    }

    /// Integração: esquerdo -> primária, direito -> secundária (que fica opaca
    /// se era transparente); o mostrador clicado vira o ativo.
    void wheelUpdatesSessionColors()
    {
        RgbaWindow fixture;
        QVERIFY(fixture.exposed && fixture.opened);
        gui::EditorSession* session = fixture.session();
        ColorWheel* wheel = fixture.wheel();
        QVERIFY(wheel != nullptr);
        QVERIFY(wheel->isVisible()); // modo RGBA: seletor no lugar da paleta
        session->setPrimaryRgba({ 0, 0, 0, 90 });

        QTest::mouseClick(wheel, Qt::LeftButton, { }, wheel->ringPointForHue(0).toPoint());
        // Preto não tem matiz/saturação: o clique no anel define o matiz, e a
        // cor resultante mantém S/V do seletor; o alfa (90) é preservado.
        QCOMPARE(session->primaryRgba().alpha, std::uint8_t { 90 });
        QCOMPARE(session->activeColorRole(), ColorRole::Primary);

        QPointF pure;
        QPointF white;
        QPointF black;
        wheel->triangleVertices(pure, white, black);
        const QPointF center(wheel->width() / 2.0, wheel->height() / 2.0);
        QTest::mouseClick(wheel, Qt::RightButton, { }, (pure * 0.95 + center * 0.05).toPoint());
        QCOMPARE(session->activeColorRole(), ColorRole::Secondary);
        const core::Rgba secondary = session->secondaryRgba();
        QCOMPARE(secondary.alpha, std::uint8_t { 255 }); // era transparente -> opaca
        QVERIFY(secondary.red > 200 && secondary.green < 60 && secondary.blue < 60); // ~vermelho
    }

    /// Barras editam o mostrador ativo (inclusive A) e atualizam o seletor.
    void slidersEditActiveColorAndSyncWheel()
    {
        RgbaWindow fixture;
        QVERIFY(fixture.exposed && fixture.opened);
        gui::EditorSession* session = fixture.session();
        gui::ColorEditor* editor = fixture.editor();
        QVERIFY(editor->slider(gui::ColorEditor::Alpha)->isVisible());

        // Clicar no mostrador secundário o torna ativo.
        QTest::mouseClick(editor->swatch(ColorRole::Secondary), Qt::LeftButton);
        QCOMPARE(session->activeColorRole(), ColorRole::Secondary);
        QVERIFY(editor->swatch(ColorRole::Secondary)->isActive());
        QVERIFY(!editor->swatch(ColorRole::Primary)->isActive());

        editor->slider(gui::ColorEditor::Blue)->setValue(255);
        editor->slider(gui::ColorEditor::Alpha)->setValue(200);
        QCOMPARE(session->secondaryRgba(), (core::Rgba { 0, 0, 255, 200 }));
        QCOMPARE(session->primaryRgba(), (core::Rgba { 0, 0, 0, 255 })); // intacta

        // O seletor acompanha a cor ativa (azul puro: matiz 240).
        QVERIFY(std::abs(fixture.wheel()->color().hsvHue() - 240) <= 1);
        // O mostrador exibe a cor com o alfa.
        QCOMPARE(editor->swatch(ColorRole::Secondary)->color().alpha(), 200);
    }

    /// Modo indexado: sem barra A, sem seletor; barras editam a paleta da cor
    /// ativa (também a secundária).
    void indexedModeKeepsPalette()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("indexed.png"));
        io::ImageFile::save(path,
            { core::IndexedImage({ 8, 8 }), core::Palette::makeDefault(core::ColorDepth::Bits8) });
        gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.openFile(path));

        auto* editor = window.findChild<gui::ColorEditor*>();
        QVERIFY(!window.findChild<ColorWheel*>()->isVisible());
        QVERIFY(!editor->slider(gui::ColorEditor::Alpha)->isVisible());

        window.session()->setBackgroundColor(3);
        QTest::mouseClick(editor->swatch(ColorRole::Secondary), Qt::LeftButton);
        editor->slider(gui::ColorEditor::Red)->setValue(17);
        QCOMPARE(window.activeDocument()->palette().color(3).red, std::uint8_t { 17 });
        QCOMPARE(editor->swatch(ColorRole::Secondary)->text(), QStringLiteral("3"));
    }
};

QTEST_MAIN(ColorPickerTest)
#include "ColorPickerTest.moc"
