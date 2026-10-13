// =============================================================================
// PaletteOpsTest.cpp — testes de remapeamento, compactação e pontilhamento.
// =============================================================================
#include "core/PaletteOps.h"

#include <QTest>

using namespace spritenator::core;

namespace {

/// Paleta de 16 cores com tons de cinza crescentes (0, 17, 34, ..., 255).
Palette grayRamp()
{
    Palette palette(ColorDepth::Bits4);
    for (int index = 0; index < 16; ++index) {
        const auto level = static_cast<std::uint8_t>(index * 17);
        palette.setColor(index, { level, level, level });
    }
    return palette;
}

} // namespace

class PaletteOpsTest final : public QObject {
    Q_OBJECT

private slots:
    void identityAndSwap()
    {
        const RemapTable identity = identityRemap();
        QCOMPARE(int(identity[200]), 200);
        const RemapTable swap = swapRemap(3, 7);
        QCOMPARE(int(swap[3]), 7);
        QCOMPARE(int(swap[7]), 3);
        QCOMPARE(int(swap[4]), 4);
    }

    void systemColorsAreTwenty()
    {
        const ColorSet system = systemColors();
        QCOMPARE(system.count(), std::size_t { 20 });
        QVERIFY(system.test(0) && system.test(9) && system.test(246) && system.test(255));
        QVERIFY(!system.test(10) && !system.test(245));
    }

    /// Remapeamento para a cor mais próxima respeita os destinos permitidos.
    void nearestRemapHonoursTargets()
    {
        const Palette from = grayRamp();
        Palette to(ColorDepth::Bits4);
        to.setColor(1, { 0, 0, 0 });
        to.setColor(2, { 255, 255, 255 });
        ColorSet targets;
        targets.set(1);
        targets.set(2);
        const RemapTable table = nearestRemap(from, to, targets);
        QCOMPARE(int(table[0]), 1); // preto -> preto
        QCOMPARE(int(table[15]), 2); // branco -> branco
        QCOMPARE(int(table[3]), 1); // cinza escuro -> preto
    }

    /// Compactar: cores usadas vão para o início na ordem original.
    void removeUnusedCompacts()
    {
        const Palette palette = grayRamp();
        ColorSet used;
        used.set(4);
        used.set(9);
        const CompactResult result = removeUnusedColors(palette, used);
        QCOMPARE(result.removedCount, 14);
        QCOMPARE(result.palette.color(0), palette.color(4));
        QCOMPARE(result.palette.color(1), palette.color(9));
        QCOMPARE(int(result.remap[4]), 0);
        QCOMPARE(int(result.remap[9]), 1);
    }

    /// Mesclar cores lidas numa posição específica.
    void mergeAtPosition()
    {
        const Palette base = grayRamp();
        const std::vector<Rgb> loaded = { { 1, 0, 0 }, { 2, 0, 0 }, { 3, 0, 0 } };
        const Palette merged = mergeColors(base, loaded, 1, 5, 14); // só 2 cabem/existem
        QCOMPARE(merged.color(14), (Rgb { 2, 0, 0 }));
        QCOMPARE(merged.color(15), (Rgb { 3, 0, 0 }));
        QCOMPARE(merged.color(13), base.color(13));
    }

    /// Pontilhamento: uma área de cinza médio vira mistura de preto e branco
    /// com proporção aproximadamente igual.
    void ditherApproximatesAverage()
    {
        Palette from(ColorDepth::Bits4);
        from.setColor(0, { 128, 128, 128 });
        Palette to(ColorDepth::Bits4);
        to.setColor(0, { 0, 0, 0 });
        to.setColor(1, { 255, 255, 255 });
        ColorSet targets;
        targets.set(0);
        targets.set(1);

        const IndexedImage image({ 16, 16 }, 0);
        const IndexedImage dithered = ditherToPalette(image, from, to, targets);
        int white = 0;
        for (const ColorIndex value : dithered.data()) {
            white += value == 1 ? 1 : 0;
        }
        QVERIFY(white > 100 && white < 156); // ~50% de 256 pixels
    }

    /// Aplicar paleta com "índice 0 transparente": o 0 é preservado.
    void applyPaletteKeepsTransparentZero()
    {
        const Palette from = grayRamp();
        Palette to = grayRamp();
        to.setColor(0, { 255, 0, 255 }); // magenta "transparente"
        IndexedImage image({ 2, 1 }, 0);
        image.setPixel({ 1, 0 }, 15);

        PaletteApplyOptions options;
        options.method = RemapMethod::NearestColor;
        options.keepTransparentZero = true;
        const IndexedImage result = applyPalette(image, from, to, options);
        QCOMPARE(int(result.pixel(0, 0)), 0);
        QCOMPARE(int(result.pixel(1, 0)), 15);

        options.method = RemapMethod::KeepIndices;
        QCOMPARE(applyPalette(image, from, to, options), image);
    }
};

QTEST_APPLESS_MAIN(PaletteOpsTest)
#include "PaletteOpsTest.moc"
