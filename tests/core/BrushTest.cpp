// =============================================================================
// BrushTest.cpp — pontas do lápis (core::Brush): formatos e tamanhos.
// =============================================================================
#include "core/Brush.h"

#include <QTest>

#include <algorithm>

using namespace spritenator::core;

namespace {

bool has(const std::vector<Point>& points, Point point)
{
    return std::find(points.begin(), points.end(), point) != points.end();
}

} // namespace

class BrushTest final : public QObject {
    Q_OBJECT

private slots:
    /// Ponta quadrada: tamanho x tamanho pixels; tamanhos fora da faixa são
    /// limitados a 1..64.
    void squareSizes()
    {
        QCOMPARE(brushOffsets(BrushShape::Square, 1), (std::vector<Point> { { 0, 0 } }));
        QCOMPARE(brushOffsets(BrushShape::Square, 4).size(), std::size_t { 16 });
        QCOMPARE(brushOffsets(BrushShape::Square, 0).size(), std::size_t { 1 });
        QCOMPARE(brushOffsets(BrushShape::Square, 999).size(), std::size_t { 64 * 64 });
    }

    /// Tamanhos pares: o ponteiro fica no pixel de cima e da esquerda do centro.
    void evenSizesAreAnchoredTopLeft()
    {
        QCOMPARE(brushExtent(1), (Rect { 0, 0, 1, 1 }));
        QCOMPARE(brushExtent(2), (Rect { 0, 0, 2, 2 }));
        QCOMPARE(brushExtent(3), (Rect { -1, -1, 3, 3 }));
        QCOMPARE(brushExtent(4), (Rect { -1, -1, 4, 4 }));
    }

    /// Ponta redonda: 3 px é o "+"; maiores perdem os cantos; é simétrica.
    void roundShapes()
    {
        const auto plus = brushOffsets(BrushShape::Round, 3);
        QCOMPARE(plus.size(), std::size_t { 5 });
        QVERIFY(!has(plus, { 1, 1 }));
        QCOMPARE(brushOffsets(BrushShape::Round, 2).size(), std::size_t { 4 });

        const auto four = brushOffsets(BrushShape::Round, 4);
        QCOMPARE(four.size(), std::size_t { 12 }); // 4x4 sem os 4 cantos
        QVERIFY(!has(four, { -1, -1 }));
        QVERIFY(!has(four, { 2, 2 }));

        const auto big = brushOffsets(BrushShape::Round, 9);
        for (const Point point : big) { // simétrica nos dois eixos
            QVERIFY(has(big, { -point.x, point.y }));
            QVERIFY(has(big, { point.x, -point.y }));
        }
        QVERIFY(has(big, { 4, 0 }));
        QVERIFY(!has(big, { 4, 4 }));
    }
};

QTEST_APPLESS_MAIN(BrushTest)
#include "BrushTest.moc"
