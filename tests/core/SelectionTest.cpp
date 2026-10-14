// =============================================================================
// SelectionTest.cpp — seleções de forma livre (core::Selection), varinha
// mágica e transformação livre (core::FreeTransform).
// =============================================================================
#include "core/Selection.h"
#include "core/FreeTransform.h"

#include <QTest>

using namespace spritenator::core;

namespace {

/// Desenho de uma seleção como texto ("#" selecionado), dentro de "area".
QString picture(const Selection& selection, const Rect& area)
{
    QString text;
    for (int y = area.top(); y < area.bottom(); ++y) {
        for (int x = area.left(); x < area.right(); ++x) {
            text += selection.contains({ x, y }) ? QLatin1Char('#') : QLatin1Char('.');
        }
        text += QLatin1Char('\n');
    }
    return text;
}

} // namespace

class SelectionTest final : public QObject {
    Q_OBJECT

private slots:
    /// Retângulo, vazio e normalização (o envolvente é sempre o mínimo).
    void rectangleAndNormalization()
    {
        QVERIFY(Selection().isEmpty());
        QVERIFY(Selection::rectangle({ 0, 0, 0, 5 }).isEmpty());
        const Selection rect = Selection::rectangle({ 2, 3, 4, 2 });
        QCOMPARE(rect.bounds(), (Rect { 2, 3, 4, 2 }));
        QVERIFY(rect.isRectangular());
        QCOMPARE(rect.pixelCount(), 8LL);
        QVERIFY(rect.contains({ 5, 4 }));
        QVERIFY(!rect.contains({ 6, 4 }));
        // Subtrair a metade esquerda encolhe o envolvente.
        const Selection half = rect.subtracted(Selection::rectangle({ 2, 3, 2, 2 }));
        QCOMPARE(half, Selection::rectangle({ 4, 3, 2, 2 }));
    }

    /// Oval: os mesmos pixels da elipse preenchida, simétricos.
    void ellipseShape()
    {
        const Selection oval = Selection::ellipse({ 0, 0, 7, 5 });
        QCOMPARE(oval.bounds(), (Rect { 0, 0, 7, 5 }));
        QVERIFY(!oval.isRectangular());
        QVERIFY(!oval.contains({ 0, 0 })); // cantos fora
        QVERIFY(oval.contains({ 3, 2 })); // centro dentro
        QCOMPARE(picture(oval, oval.bounds()),
            picture(Selection::fromPoints(oval.points()), oval.bounds()));
    }

    /// Polígono: interior + contorno; um triângulo e uma linha fina.
    void polygonShape()
    {
        const Selection triangle = Selection::polygon({ { 0, 0 }, { 6, 0 }, { 0, 6 } });
        QCOMPARE(triangle.bounds(), (Rect { 0, 0, 7, 7 }));
        QVERIFY(triangle.contains({ 1, 1 }));
        QVERIFY(triangle.contains({ 6, 0 }));
        QVERIFY(!triangle.contains({ 5, 5 }));
        // Dois pontos: a própria linha (nada some).
        const Selection line = Selection::polygon({ { 0, 0 }, { 4, 0 } });
        QCOMPARE(line, Selection::rectangle({ 0, 0, 5, 1 }));
    }

    /// Somar, subtrair, inverter, recortar e deslocar.
    void combinations()
    {
        const Selection a = Selection::rectangle({ 0, 0, 2, 2 });
        const Selection b = Selection::rectangle({ 2, 0, 2, 2 });
        QCOMPARE(a.united(b), Selection::rectangle({ 0, 0, 4, 2 }));
        QCOMPARE(a.united(Selection()), a);
        const Rect canvas { 0, 0, 4, 2 };
        QCOMPARE(a.inverted(canvas), b);
        QVERIFY(Selection().inverted(canvas).isRectangular());
        QCOMPARE(a.united(b).intersected({ 1, 0, 2, 2 }), Selection::rectangle({ 1, 0, 2, 2 }));
        QCOMPARE(a.translated({ 5, 1 }), Selection::rectangle({ 5, 1, 2, 2 }));
        QCOMPARE(a.subtracted(a), Selection());
    }

    /// Varinha: contígua ou não, tolerância e cores da paleta (indexada).
    void magicWandIndexed()
    {
        Palette palette = Palette::makeDefault(ColorDepth::Bits8);
        palette.setColor(1, { 100, 100, 100 });
        palette.setColor(2, { 110, 100, 100 }); // parecida com a 1
        palette.setColor(3, { 250, 0, 0 });
        IndexedImage image({ 6, 1 }, 1);
        image.setPixel({ 2, 0 }, 3); // separa a imagem em duas regiões de cor 1
        image.setPixel({ 1, 0 }, 2);
        const AnyImage any(image);

        // Tolerância 0: só a mesma cor (o índice 2 é outra cor).
        QCOMPARE(magicWand(any, palette, { 0, 0 }, 0, true), Selection::rectangle({ 0, 0, 1, 1 }));
        // Tolerância 5% (12): 110 vs 100 entra.
        QCOMPARE(magicWand(any, palette, { 0, 0 }, 5, true), Selection::rectangle({ 0, 0, 2, 1 }));
        // Não contígua: também a cor 1 depois do vermelho.
        const Selection global = magicWand(any, palette, { 0, 0 }, 5, false);
        QCOMPARE(global.pixelCount(), 5LL);
        QVERIFY(!global.contains({ 2, 0 }));
        QVERIFY(magicWand(any, palette, { 9, 9 }, 50, true).isEmpty());
    }

    /// Varinha em RGBA: transparentes são todos iguais; o alfa conta.
    void magicWandRgba()
    {
        RgbaImage image({ 3, 1 });
        image.setPixel({ 0, 0 }, Rgba { 10, 20, 30, 0 });
        image.setPixel({ 1, 0 }, Rgba { 200, 0, 0, 0 });
        image.setPixel({ 2, 0 }, Rgba { 200, 0, 0, 255 });
        const Palette palette = Palette::makeDefault(ColorDepth::Bits8);
        QCOMPARE(magicWand(AnyImage(image), palette, { 0, 0 }, 0, true),
            Selection::rectangle({ 0, 0, 2, 1 }));
    }

    /// Transformação afim: espelhar e girar 90° são exatos e caem na grade.
    void affineExactness()
    {
        // Seleção 3x2 em (1, 1) com um pixel marcado no canto superior esquerdo.
        IndexedImage content({ 3, 2 }, 0);
        content.setPixel({ 0, 0 }, 7);
        const Selection mask = Selection::rectangle({ 1, 1, 3, 2 });
        const Rect canvas { 0, 0, 10, 10 };

        TransformParams flipped;
        flipped.sx = -1;
        const auto mirror
            = transformFloating(content, mask, affineFor(flipped, mask.bounds()), canvas);
        QCOMPARE(mirror.mask, mask);
        QCOMPARE(int(mirror.pixels.pixel(2, 0)), 7); // foi para a direita

        TransformParams turned;
        turned.angle = 90;
        const auto rotated
            = transformFloating(content, mask, affineFor(turned, mask.bounds()), canvas);
        QCOMPARE(rotated.mask.bounds().size(), (Size { 2, 3 }));
        QVERIFY(rotated.mask.isRectangular());
        // Horário: o canto superior esquerdo vai para o superior direito.
        QCOMPARE(int(rotated.pixels.pixel(1, 0)), 7);

        // Quatro giros de 90° voltam ao início.
        TransformParams full;
        full.angle = 360;
        const auto back = transformFloating(content, mask, affineFor(full, mask.bounds()), canvas);
        QCOMPARE(back.mask, mask);
        QCOMPARE(back.pixels, content);
    }

    /// Escala 2x dobra cada pixel; o recorte ao canvas descarta o resto.
    void affineScaleAndClip()
    {
        IndexedImage content({ 2, 1 }, 0);
        content.setPixel({ 1, 0 }, 5);
        const Selection mask = Selection::rectangle({ 0, 0, 2, 1 });
        TransformParams doubled;
        doubled.sx = 2;
        doubled.sy = 2;
        doubled.dx = 1; // mantém o canto superior esquerdo em (0, 0)
        doubled.dy = 0.5;
        const auto result = transformFloating(
            content, mask, affineFor(doubled, mask.bounds()), Rect { 0, 0, 3, 10 });
        QCOMPARE(result.mask.bounds(), (Rect { 0, 0, 3, 2 })); // recortado em x = 3
        QCOMPARE(int(result.pixels.pixel(2, 1)), 5);
        QCOMPARE(int(result.pixels.pixel(1, 1)), 0);
    }
};

QTEST_APPLESS_MAIN(SelectionTest)
#include "SelectionTest.moc"
