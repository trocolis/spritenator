// =============================================================================
// PaletteTest.cpp — testes de Rgb e Palette.
// =============================================================================
#include "core/Palette.h"

#include <QTest>

#include <stdexcept>

using namespace spritenator::core;

class PaletteTest final : public QObject {
    Q_OBJECT

private slots:
    void packedRoundTrip()
    {
        const Rgb color { 0x12, 0x34, 0x56 };
        QCOMPARE(color.toPacked(), 0x123456U);
        QCOMPARE(Rgb::fromPacked(0xFF123456U), color);
    }

    /// 15 bits: extremos preservados, valores intermediários quantizados.
    void fifteenBitQuantization()
    {
        QCOMPARE((Rgb { 255, 0, 128 }.to15Bit()), (Rgb { 255, 0, 132 }));
        QCOMPARE((Rgb { 7, 8, 9 }.to15Bit()), (Rgb { 0, 8, 8 }));
    }

    void defaultPalettes()
    {
        const Palette small = Palette::makeDefault(ColorDepth::Bits4);
        QCOMPARE(small.size(), 16);
        QCOMPARE(small.color(15), (Rgb { 255, 255, 255 }));

        const Palette large = Palette::makeDefault(ColorDepth::Bits8);
        QCOMPARE(large.size(), 256);
        QCOMPARE(large.color(16), (Rgb { 0, 0, 0 })); // início do cubo
        QCOMPARE(large.color(231), (Rgb { 255, 255, 255 })); // fim do cubo
        QCOMPARE(large.color(255), (Rgb { 238, 238, 238 })); // fim da rampa
    }

    void invalidIndexThrows()
    {
        Palette palette(ColorDepth::Bits4);
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, std::ignore = palette.color(16));
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, palette.setColor(-1, { }));
    }

    void nearestIndexRespectsRange()
    {
        const Palette palette = Palette::makeDefault(ColorDepth::Bits4);
        QCOMPARE(int(palette.nearestIndex({ 250, 5, 5 })), 9); // vermelho vivo
        QCOMPARE(int(palette.nearestIndex({ 250, 5, 5 }, 0, 8)), 1); // vermelho escuro
        QCOMPARE(int(palette.nearestIndex({ 0, 0, 0 }, 10, 3)), 4); // intervalo invertido
    }

    /// Reduzir a profundidade descarta as cores excedentes.
    void depthChangeDropsEntries()
    {
        Palette palette = Palette::makeDefault(ColorDepth::Bits8);
        palette.setDepth(ColorDepth::Bits4);
        QCOMPARE(palette.size(), 16);
        palette.setDepth(ColorDepth::Bits8);
        QCOMPARE(palette.color(200), (Rgb { }));
    }

    /// Gradação: entradas intermediárias interpoladas linearmente.
    void interpolateFillsGradient()
    {
        Palette palette;
        palette.setColor(10, { 0, 0, 0 });
        palette.setColor(14, { 200, 100, 40 });
        palette.interpolate(14, 10); // ordem invertida é aceita
        QCOMPARE(palette.color(11), (Rgb { 50, 25, 10 }));
        QCOMPARE(palette.color(12), (Rgb { 100, 50, 20 }));
        QCOMPARE(palette.color(13), (Rgb { 150, 75, 30 }));
        QCOMPARE(palette.color(14), (Rgb { 200, 100, 40 }));
    }
};

QTEST_APPLESS_MAIN(PaletteTest)
#include "PaletteTest.moc"
