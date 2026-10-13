// =============================================================================
// TransformTest.cpp — testes das transformações geométricas.
// =============================================================================
#include "core/Transform.h"

#include <QTest>

using namespace spritenator::core;

namespace {

/// Imagem 3x2 com índices distintos:  0 1 2
///                                    3 4 5
IndexedImage makeSample()
{
    IndexedImage image({ 3, 2 });
    for (int index = 0; index < 6; ++index) {
        image.setPixel({ index % 3, index / 3 }, static_cast<ColorIndex>(index));
    }
    return image;
}

/// Lê a imagem como sequência de inteiros (linha a linha), para comparar.
QList<int> values(const IndexedImage& image)
{
    QList<int> result;
    for (const ColorIndex value : image.data()) {
        result.append(value);
    }
    return result;
}

} // namespace

class TransformTest final : public QObject {
    Q_OBJECT

private slots:
    /// Deslocamento circular: o que sai por um lado entra pelo outro.
    void shiftWrapsAround()
    {
        QCOMPARE(values(shifted(makeSample(), 1, 0)), (QList<int> { 2, 0, 1, 5, 3, 4 }));
        QCOMPARE(values(shifted(makeSample(), 0, -1)), (QList<int> { 3, 4, 5, 0, 1, 2 }));
        QCOMPARE(shifted(makeSample(), 3, 2), makeSample()); // volta completa
    }

    void flips()
    {
        QCOMPARE(values(flippedHorizontally(makeSample())), (QList<int> { 2, 1, 0, 5, 4, 3 }));
        QCOMPARE(values(flippedVertically(makeSample())), (QList<int> { 3, 4, 5, 0, 1, 2 }));
    }

    /// Simetria: a metade esquerda/superior é refletida; o centro é mantido.
    void mirrors()
    {
        QCOMPARE(values(mirroredHorizontally(makeSample())), (QList<int> { 0, 1, 0, 3, 4, 3 }));
        QCOMPARE(values(mirroredVertically(makeSample())), (QList<int> { 0, 1, 2, 0, 1, 2 }));
    }

    /// 90 graus num quadrado: o canto superior esquerdo vai para o superior direito.
    void rotate90OnSquare()
    {
        IndexedImage square({ 3, 3 }, 0);
        square.setPixel({ 0, 0 }, 7);
        const IndexedImage clockwise = rotated90(square, true, 0);
        QCOMPARE(int(clockwise.pixel(2, 0)), 7);
        const IndexedImage counter = rotated90(square, false, 0);
        QCOMPARE(int(counter.pixel(0, 2)), 7);
        // Quatro rotações devolvem a original.
        IndexedImage image = makeSample().resizedCanvas({ 4, 4 });
        IndexedImage turned = image;
        for (int step = 0; step < 4; ++step) {
            turned = rotated90(turned, true, 0);
        }
        QCOMPARE(turned, image);
        // Horária seguida de anti-horária também.
        QCOMPARE(rotated90(rotated90(image, true, 0), false, 0), image);
    }

    /// Rotação livre de 0 e 360 graus não altera; 180 equivale a dois flips.
    void freeRotation()
    {
        const IndexedImage image = makeSample();
        QCOMPARE(rotated(image, 0.0, 9), image);
        QCOMPARE(rotated(image, 360.0, 9), image);
        QCOMPARE(rotated(image, 180.0, 9), flippedVertically(flippedHorizontally(image)));
    }

    /// Escala por vizinho mais próximo: ampliar 2x duplica cada pixel.
    void scaling()
    {
        const IndexedImage doubled = scaled(makeSample(), { 6, 4 });
        QCOMPARE(int(doubled.pixel(0, 0)), 0);
        QCOMPARE(int(doubled.pixel(1, 1)), 0);
        QCOMPARE(int(doubled.pixel(5, 3)), 5);
        QCOMPARE(scaled(doubled, { 3, 2 }), makeSample()); // reduzir de volta é exato
    }

    void shadow()
    {
        QCOMPARE(values(shadowed(makeSample(), 4, 9)), (QList<int> { 9, 9, 9, 9, 4, 9 }));
    }
};

QTEST_APPLESS_MAIN(TransformTest)
#include "TransformTest.moc"
