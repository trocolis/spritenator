// =============================================================================
// IndexedImageTest.cpp — testes de IndexedImage e WriteRules.
// =============================================================================
#include "core/IndexedImage.h"

#include <QTest>

#include <stdexcept>

using namespace spritenator::core;

class IndexedImageTest final : public QObject {
    Q_OBJECT

private slots:
    void constructionFillsPixels()
    {
        const IndexedImage image({ 3, 2 }, 7);
        QCOMPARE(image.width(), 3);
        QCOMPARE(image.height(), 2);
        QCOMPARE(int(image.pixel(2, 1)), 7);
        QCOMPARE(image.data().size(), std::size_t { 6 });
    }

    void invalidSizeThrows()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, IndexedImage({ 0, 5 }));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument, IndexedImage({ IndexedImage::kMaxDimension + 1, 1 }));
    }

    /// Escrita fora dos limites é ignorada (recorte), sem exceção.
    void setPixelClipsAndReportsChange()
    {
        IndexedImage image({ 4, 4 });
        QVERIFY(image.setPixel({ 1, 1 }, 5));
        QVERIFY(!image.setPixel({ 1, 1 }, 5)); // já tinha essa cor
        QVERIFY(!image.setPixel({ -1, 0 }, 5));
        QVERIFY(!image.setPixel({ 4, 0 }, 5));
        QCOMPARE(int(image.pixelOr({ 9, 9 }, 42)), 42);
    }

    /// Cópia de região parcialmente fora preenche o resto com "outside".
    void copyRegionOutsideIsFilled()
    {
        IndexedImage image({ 4, 4 }, 1);
        image.setPixel({ 3, 3 }, 9);
        const IndexedImage region = image.copy({ 2, 2, 4, 4 }, 0);
        QCOMPARE(region.size(), (Size { 4, 4 }));
        QCOMPARE(int(region.pixel(0, 0)), 1);
        QCOMPARE(int(region.pixel(1, 1)), 9);
        QCOMPARE(int(region.pixel(2, 2)), 0);
    }

    /// Cor transparente de origem não é copiada.
    void pasteHonoursTransparency()
    {
        IndexedImage target({ 3, 1 }, 4);
        IndexedImage source({ 3, 1 }, 0);
        source.setPixel({ 1, 0 }, 6);
        WriteRules rules;
        rules.transparentIndex = 0;
        target.paste(source, { 0, 0 }, rules);
        QCOMPARE(int(target.pixel(0, 0)), 4);
        QCOMPARE(int(target.pixel(1, 0)), 6);
    }

    /// Cores protegidas (máscara) no destino não são sobrescritas.
    void pasteHonoursMask()
    {
        IndexedImage target({ 2, 1 }, 3);
        target.setPixel({ 1, 0 }, 8);
        ColorSet mask;
        mask.set(8);
        WriteRules rules;
        rules.protectedColors = &mask;
        target.fillRect(target.bounds(), 1, rules);
        QCOMPARE(int(target.pixel(0, 0)), 1);
        QCOMPARE(int(target.pixel(1, 0)), 8);
    }

    void pasteClipsAtEdges()
    {
        IndexedImage target({ 2, 2 }, 0);
        const IndexedImage source({ 2, 2 }, 5);
        target.paste(source, { 1, 1 });
        QCOMPARE(int(target.pixel(0, 0)), 0);
        QCOMPARE(int(target.pixel(1, 1)), 5);
        target.paste(source, { -1, -1 });
        QCOMPARE(int(target.pixel(0, 0)), 5);
    }

    void resizeKeepsTopLeft()
    {
        IndexedImage image({ 2, 2 }, 3);
        const IndexedImage bigger = image.resizedCanvas({ 3, 3 }, 7);
        QCOMPARE(int(bigger.pixel(1, 1)), 3);
        QCOMPARE(int(bigger.pixel(2, 2)), 7);
        const IndexedImage smaller = image.resizedCanvas({ 1, 1 });
        QCOMPARE(int(smaller.pixel(0, 0)), 3);
    }

    void usedColorsAndRemap()
    {
        IndexedImage image({ 2, 1 }, 2);
        image.setPixel({ 1, 0 }, 200);
        const ColorSet used = image.usedColors();
        QCOMPARE(used.count(), std::size_t { 2 });
        QVERIFY(used.test(2) && used.test(200));

        std::array<ColorIndex, 256> table { };
        table[2] = 0;
        table[200] = 1;
        image.remap(table);
        QCOMPARE(int(image.pixel(0, 0)), 0);
        QCOMPARE(int(image.pixel(1, 0)), 1);
    }
};

QTEST_APPLESS_MAIN(IndexedImageTest)
#include "IndexedImageTest.moc"
