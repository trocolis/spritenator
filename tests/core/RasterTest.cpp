// =============================================================================
// RasterTest.cpp — testes dos algoritmos de rasterização.
// =============================================================================
#include "core/Raster.h"
#include "core/IndexedImage.h"
#include "core/Tone.h"

#include <QTest>

#include <set>
#include <utility>
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

/// Conjunto de pixels gerados por uma função de rasterização.
template <typename Function> std::set<std::pair<int, int>> collect(Function&& rasterize)
{
    std::set<std::pair<int, int>> points;
    rasterize([&](Point point) { points.insert({ point.x, point.y }); });
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

    /// Contorno de retângulo: só o perímetro (2w + 2h - 4 pixels).
    void rectangleOutlineAndFill()
    {
        const Rect bounds { 2, 3, 5, 4 };
        QCOMPARE(collect([&](auto plot) { plotRectangle(bounds, false, plot); }).size(),
            std::size_t { 2 * 5 + 2 * 4 - 4 });
        QCOMPARE(collect([&](auto plot) { plotRectangle(bounds, true, plot); }).size(),
            std::size_t { 20 });
    }

    /// Elipse: dentro do retângulo, tocando as quatro bordas, simétrica.
    void ellipseTouchesBoundsAndIsSymmetric()
    {
        for (const Rect bounds :
            { Rect { 0, 0, 7, 5 }, Rect { 0, 0, 8, 8 }, Rect { 3, 2, 10, 3 } }) {
            const auto points = collect([&](auto plot) { plotEllipse(bounds, false, plot); });
            int minX = 1000, maxX = -1000, minY = 1000, maxY = -1000;
            for (const auto& [x, y] : points) {
                QVERIFY(bounds.contains(Point { x, y }));
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
                // Simetria horizontal e vertical em relação ao centro do retângulo.
                const int mirroredX = bounds.left() + bounds.right() - 1 - x;
                const int mirroredY = bounds.top() + bounds.bottom() - 1 - y;
                QVERIFY(points.contains({ mirroredX, y }));
                QVERIFY(points.contains({ x, mirroredY }));
            }
            QCOMPARE(minX, bounds.left());
            QCOMPARE(maxX, bounds.right() - 1);
            QCOMPARE(minY, bounds.top());
            QCOMPARE(maxY, bounds.bottom() - 1);
        }
    }

    /// Elipse preenchida inclui o centro; o contorno não.
    void filledEllipseIncludesCenter()
    {
        const Rect bounds { 0, 0, 9, 7 };
        QVERIFY(collect([&](auto plot) { plotEllipse(bounds, true, plot); }).contains({ 4, 3 }));
        QVERIFY(!collect([&](auto plot) { plotEllipse(bounds, false, plot); }).contains({ 4, 3 }));
    }

    /// Retângulo arredondado: cantos vazios, lados retos presentes.
    void roundRectangleHasRoundCorners()
    {
        const Rect bounds { 0, 0, 12, 9 };
        const auto points = collect(
            [&](auto plot) { plotRoundRectangle(bounds, defaultCornerSize(bounds), false, plot); });
        QVERIFY(!points.contains({ 0, 0 }));
        QVERIFY(!points.contains({ 11, 8 }));
        QVERIFY(points.contains({ 6, 0 })); // meio do lado de cima
        QVERIFY(points.contains({ 0, 4 })); // meio do lado esquerdo
        QVERIFY(points.contains({ 11, 4 })); // meio do lado direito
    }

    /// Balde: apenas a região contígua da mesma cor (vizinhança 4).
    void floodRegionStopsAtBorders()
    {
        IndexedImage image({ 5, 5 }, 0);
        // Parede vertical na coluna 2 separa as metades.
        image.fillRect({ 2, 0, 1, 5 }, 1);
        QCOMPARE(floodRegion(image, { 0, 0 }).size(), std::size_t { 10 });
        QCOMPARE(floodRegion(image, { 2, 2 }).size(), std::size_t { 5 });
        QVERIFY(floodRegion(image, { 9, 9 }).empty());
        // Diagonais não conectam regiões (vizinhança 4).
        IndexedImage diagonal({ 2, 2 }, 0);
        diagonal.setPixel({ 1, 0 }, 1);
        diagonal.setPixel({ 0, 1 }, 1);
        QCOMPARE(floodRegion(diagonal, { 0, 0 }).size(), std::size_t { 1 });
    }

    /// Cobertura de cada tom num bloco 4x4: 16, 14, 12, ..., 2 pixels.
    void toneCoverage()
    {
        for (int tone = 0; tone < kToneCount; ++tone) {
            int covered = 0;
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    covered += toneCovers(static_cast<Tone>(tone), { x, y }) ? 1 : 0;
                }
            }
            QCOMPARE(covered, 16 - 2 * tone);
        }
    }

    /// O deslocamento muda a fase do padrão (xadrez E alterna).
    void toneShiftChangesPhase()
    {
        QVERIFY(toneCovers(Tone::E, { 0, 0 }, 0));
        QVERIFY(!toneCovers(Tone::E, { 0, 0 }, 1));
        QVERIFY(toneCovers(Tone::E, { -4, -8 }, 0)); // periodicidade com negativos
    }
};

QTEST_APPLESS_MAIN(RasterTest)
#include "RasterTest.moc"
