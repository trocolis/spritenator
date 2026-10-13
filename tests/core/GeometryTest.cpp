// =============================================================================
// GeometryTest.cpp — testes de Point/Size/Rect e GridLayout.
// =============================================================================
#include "core/Geometry.h"
#include "core/GridLayout.h"

#include <QTest>

using namespace spritenator::core;

class GeometryTest final : public QObject {
    Q_OBJECT

private slots:
    /// fromCorners aceita os cantos em qualquer ordem e é inclusivo.
    void rectFromCornersIsInclusive()
    {
        const Rect forward = Rect::fromCorners({ 2, 3 }, { 5, 7 });
        const Rect backward = Rect::fromCorners({ 5, 7 }, { 2, 3 });
        QCOMPARE(forward, backward);
        QCOMPARE(forward.width, 4);
        QCOMPARE(forward.height, 5);
        QVERIFY(forward.contains(Point { 5, 7 }));
        QVERIFY(!forward.contains(Point { 6, 7 }));
    }

    void rectIntersectionAndUnion()
    {
        const Rect a { 0, 0, 10, 10 };
        const Rect b { 5, 5, 10, 10 };
        QCOMPARE(a.intersected(b), (Rect { 5, 5, 5, 5 }));
        QCOMPARE(a.united(b), (Rect { 0, 0, 15, 15 }));
        QVERIFY(a.intersected(Rect { 20, 20, 2, 2 }).isEmpty());
        QCOMPARE(Rect { }.united(b), b);
        QVERIFY(a.contains(Rect { 1, 1, 9, 9 }));
        QVERIFY(!a.contains(b));
    }

    /// Grades: padrão 1x1 (pixels) e sub-grade 24x32; células alinhadas.
    void gridLayoutCells()
    {
        const GridLayout grid;
        QCOMPARE(grid.mainGrid, (Size { 1, 1 }));
        QCOMPARE(grid.subGridCellAt({ 30, 40 }), (Rect { 24, 32, 24, 32 }));
        QCOMPARE(GridLayout::cellAt({ -1, 5 }, { 2, 2 }), (Rect { -2, 4, 2, 2 }));
        // Células da sub-grade (tamanho do personagem), inclusive negativas.
        QCOMPARE(grid.subGridCellAt({ 0, 0 }), (Rect { 0, 0, 24, 32 }));
        QCOMPARE(grid.subGridCellAt({ 47, 63 }), (Rect { 24, 32, 24, 32 }));
        QCOMPARE(grid.subGridCellAt({ -1, -1 }), (Rect { -24, -32, 24, 32 }));
    }
};

QTEST_APPLESS_MAIN(GeometryTest)
#include "GeometryTest.moc"
