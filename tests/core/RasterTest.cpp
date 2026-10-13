// =============================================================================
// RasterTest.cpp — testes dos algoritmos de rasterização.
// =============================================================================
#include "core/Raster.h"

#include <QTest>

#include <vector>

using namespace spritenator::core;

namespace {

/// Coleta os pixels gerados por uma reta.
std::vector<Point> collectLine(Point from, Point to)
{
    std::vector<Point> points;
    plotLine(from, to, [&](Point point) { points.push_back(point); });
    return points;
}

} // namespace

class RasterTest final : public QObject {
    Q_OBJECT

private slots:
    void singlePointLine()
    {
        const auto points = collectLine({ 3, 4 }, { 3, 4 });
        QCOMPARE(points.size(), std::size_t { 1 });
        QCOMPARE(points.front(), (Point { 3, 4 }));
    }

    /// Retas horizontais, verticais e diagonais têm exatamente N+1 pixels.
    void straightLines()
    {
        QCOMPARE(collectLine({ 0, 0 }, { 5, 0 }).size(), std::size_t { 6 });
        QCOMPARE(collectLine({ 0, 5 }, { 0, 0 }).size(), std::size_t { 6 });
        const auto diagonal = collectLine({ 0, 0 }, { 4, 4 });
        QCOMPARE(diagonal.size(), std::size_t { 5 });
        QCOMPARE(diagonal.at(2), (Point { 2, 2 }));
    }

    /// Reta inclinada: sem lacunas (pixels vizinhos sempre se tocam).
    void steepLineIsConnected()
    {
        const auto points = collectLine({ 0, 0 }, { 3, 10 });
        QCOMPARE(points.front(), (Point { 0, 0 }));
        QCOMPARE(points.back(), (Point { 3, 10 }));
        for (std::size_t index = 1; index < points.size(); ++index) {
            QVERIFY(std::abs(points[index].x - points[index - 1].x) <= 1);
            QVERIFY(std::abs(points[index].y - points[index - 1].y) <= 1);
        }
    }
};

QTEST_APPLESS_MAIN(RasterTest)
#include "RasterTest.moc"
