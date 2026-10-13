// =============================================================================
// RgbaImageTest.cpp — imagens RGBA: Image<Rgba>, regras de escrita,
// algoritmos genéricos e conversões indexada <-> RGBA.
// =============================================================================
#include "core/ColorConversion.h"
#include "core/Image.h"
#include "core/Raster.h"
#include "core/Transform.h"

#include <QTest>

using namespace spritenator::core;

namespace {

constexpr Rgba kRed { 255, 0, 0, 255 };
constexpr Rgba kGreen { 0, 255, 0, 255 };
constexpr Rgba kHalfBlue { 0, 0, 255, 128 };
constexpr Rgba kClear { }; // transparente

} // namespace

class RgbaImageTest final : public QObject {
    Q_OBJECT

private slots:
    void rgbaPacking()
    {
        QCOMPARE(kHalfBlue.toArgb32(), 0x800000FFU);
        QCOMPARE(Rgba::fromArgb32(0x800000FFU), kHalfBlue);
        QCOMPARE(Rgba::opaque({ 1, 2, 3 }), (Rgba { 1, 2, 3, 255 }));
        QCOMPARE(Rgba { }.alpha, std::uint8_t { 0 }); // padrão = transparente
    }

    /// Uma imagem RGBA nova é totalmente transparente.
    void newImageIsTransparent()
    {
        const RgbaImage image({ 3, 2 });
        QCOMPARE(image.pixel(2, 1), kClear);
        QCOMPARE(image.data().size(), std::size_t { 6 });
    }

    /// "Efeito de cor transparente" em RGBA: alfa 0 na origem não é copiado.
    void pasteSkipsTransparentPixels()
    {
        RgbaImage target({ 2, 1 }, kRed);
        RgbaImage source({ 2, 1 }, kClear);
        source.setPixel({ 1, 0 }, kHalfBlue);

        RgbaImage plain = target;
        plain.paste(source, { 0, 0 });
        QCOMPARE(plain.pixel(0, 0), kClear); // sem a regra: substitui

        RgbaWriteRules rules;
        rules.skipTransparent = true;
        target.paste(source, { 0, 0 }, rules);
        QCOMPARE(target.pixel(0, 0), kRed); // preservado
        QCOMPARE(target.pixel(1, 0), kHalfBlue); // semitransparente é copiado
    }

    /// O balde funciona com igualdade exata de RGBA (inclusive o alfa).
    void floodRegionComparesAlpha()
    {
        RgbaImage image({ 3, 1 }, kRed);
        image.setPixel({ 1, 0 }, Rgba { 255, 0, 0, 200 }); // mesma cor, outro alfa
        QCOMPARE(floodRegion(image, { 0, 0 }).size(), std::size_t { 1 });
    }

    /// As transformações genéricas funcionam com RGBA.
    void transformsWorkWithRgba()
    {
        RgbaImage image({ 2, 1 });
        image.setPixel({ 0, 0 }, kRed);
        QCOMPARE(flippedHorizontally(image).pixel(1, 0), kRed);
        QCOMPARE(shifted(image, 1, 0).pixel(1, 0), kRed);
        QCOMPARE(scaled(image, { 4, 2 }).pixel(1, 1), kRed);
        QCOMPARE(rotated90(RgbaImage({ 3, 3 }, kGreen), true, kClear).pixel(1, 1), kGreen);
    }

    /// Sombra RGBA: cor substituída, alfa preservado, transparentes intactos.
    void shadowKeepsAlpha()
    {
        RgbaImage image({ 3, 1 });
        image.setPixel({ 0, 0 }, kRed);
        image.setPixel({ 1, 0 }, kHalfBlue);
        const RgbaImage shadow = shadowed(image, Rgba { 10, 20, 30, 255 });
        QCOMPARE(shadow.pixel(0, 0), (Rgba { 10, 20, 30, 255 }));
        QCOMPARE(shadow.pixel(1, 0), (Rgba { 10, 20, 30, 128 }));
        QCOMPARE(shadow.pixel(2, 0), kClear);
    }

    /// Indexada -> RGBA é exata; com índice transparente, alfa 0.
    void indexedToRgba()
    {
        const Palette palette = Palette::makeDefault(ColorDepth::Bits4);
        IndexedImage image({ 2, 1 }, 0);
        image.setPixel({ 1, 0 }, 9); // vermelho vivo
        const RgbaImage opaque = toRgba(image, palette);
        QCOMPARE(opaque.pixel(0, 0), (Rgba { 0, 0, 0, 255 }));
        QCOMPARE(opaque.pixel(1, 0), kRed);
        QCOMPARE(toRgba(image, palette, 0).pixel(0, 0).alpha, std::uint8_t { 0 });
    }

    /// RGBA -> indexada: transparentes viram o índice 0; demais, a mais próxima.
    void rgbaToIndexed()
    {
        const Palette palette = Palette::makeDefault(ColorDepth::Bits4);
        RgbaImage image({ 3, 1 });
        image.setPixel({ 0, 0 }, Rgba { 250, 5, 5, 255 });
        image.setPixel({ 1, 0 }, Rgba { 250, 5, 5, 40 }); // quase transparente
        const IndexedImage indexed = toIndexed(image, palette);
        QCOMPARE(int(indexed.pixel(0, 0)), 9);
        QCOMPARE(int(indexed.pixel(1, 0)), 0);
        QCOMPARE(int(indexed.pixel(2, 0)), 0);

        // Ida e volta indexada -> RGBA -> indexada preserva os índices
        // (paleta sem cores repetidas).
        IndexedImage original({ 4, 4 });
        for (int index = 0; index < 16; ++index) {
            original.setPixel({ index % 4, index / 4 }, static_cast<ColorIndex>(index));
        }
        QCOMPARE(toIndexed(toRgba(original, palette), palette), original);
    }

    /// Com pontilhamento, um cinza médio vira mistura de preto e branco.
    void rgbaToIndexedWithDither()
    {
        const Palette palette = Palette::makeDefault(ColorDepth::Bits1);
        const RgbaImage gray({ 16, 16 }, Rgba { 128, 128, 128, 255 });
        IndexingOptions options;
        options.dither = true;
        const IndexedImage result = toIndexed(gray, palette, options);
        int white = 0;
        for (const ColorIndex value : result.data()) {
            white += value == 1 ? 1 : 0;
        }
        QVERIFY(white > 100 && white < 156);
    }
};

QTEST_APPLESS_MAIN(RgbaImageTest)
#include "RgbaImageTest.moc"
