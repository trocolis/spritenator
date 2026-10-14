// =============================================================================
// AnimationFileTest.cpp — projeto .sprn (ida e volta sem perdas, arquivos
// danificados), GIF animado (codificador próprio lido de volta pelo Qt) e a
// redução de cores por corte mediano.
// =============================================================================
#include "core/Quantize.h"
#include "io/GifFile.h"
#include "io/IoError.h"
#include "io/ProjectFile.h"

#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>

using namespace spritenator;

class AnimationFileTest final : public QObject {
    Q_OBJECT

private slots:
    /// Corte mediano: poucas cores voltam exatas; muitas ficam no limite.
    void medianCut()
    {
        const std::vector<core::Rgb> few = { { 1, 2, 3 }, { 1, 2, 3 }, { 200, 0, 0 } };
        QCOMPARE(core::medianCut(few, 4).size(), std::size_t { 2 });
        std::vector<core::Rgb> many;
        for (int value = 0; value < 256; ++value) {
            many.push_back(
                { static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(255 - value), 7 });
        }
        const auto palette = core::medianCut(many, 16);
        QCOMPARE(palette.size(), std::size_t { 16 });
        QCOMPARE(
            core::nearestColor(palette, { 0, 255, 7 }), core::nearestColor(palette, { 2, 253, 7 }));
    }

    /// Projeto: camadas, cenas, durações, paleta e pixels voltam iguais.
    void projectRoundTrip()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("hero.sprn"));
        io::ProjectData data;
        data.palette = core::Palette::makeDefault(core::ColorDepth::Bits4);
        data.palette.setColor(3, { 10, 20, 30 });
        data.layers = { { QStringLiteral("Fundo"), true, 255 },
            { QStringLiteral("Contorno ✓"), false, 128 } };
        data.durations = { 120, 80 };
        for (int frame = 0; frame < 2; ++frame) {
            core::IndexedImage bottom({ 5, 3 }, 1);
            core::IndexedImage top({ 5, 3 }, 0);
            top.setPixel({ frame, 1 }, 3);
            data.cels.push_back({ bottom, top });
        }
        io::ProjectFile::save(path, data);
        const io::ProjectData loaded = io::ProjectFile::load(path);
        QCOMPARE(loaded.palette, data.palette);
        QCOMPARE(loaded.durations, data.durations);
        QCOMPARE(loaded.layers.size(), std::size_t { 2 });
        QCOMPARE(loaded.layers[1].name, QStringLiteral("Contorno ✓"));
        QVERIFY(!loaded.layers[1].visible);
        QCOMPARE(loaded.layers[1].opacity, 128);
        QCOMPARE(loaded.cels, data.cels);

        // RGBA também.
        data.palette = core::Palette::makeDefault(core::ColorDepth::Bits8);
        data.cels = { { core::RgbaImage({ 2, 2 }, core::Rgba { 1, 2, 3, 4 }),
            core::RgbaImage({ 2, 2 }) } };
        data.durations = { 100 };
        io::ProjectFile::save(path, data);
        QCOMPARE(io::ProjectFile::load(path).cels, data.cels);
    }

    /// Arquivo danificado ou de outro tipo: erro claro, sem quebrar.
    void damagedProjectIsRejected()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("bad.sprn"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("SPRN\\x00\\x00\\x00\\x01garbage");
        file.close();
        QVERIFY_THROWS_EXCEPTION(io::IoError, std::ignore = io::ProjectFile::load(path));
        QVERIFY_THROWS_EXCEPTION(io::IoError,
            std::ignore
            = io::ProjectFile::load(directory.filePath(QStringLiteral("missing.sprn"))));
    }

    /// GIF de imagens indexadas: cores exatas da paleta, cenas e durações.
    void gifIndexedRoundTrip()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("walk.gif"));
        core::Palette palette = core::Palette::makeDefault(core::ColorDepth::Bits4);
        std::vector<core::AnyImage> frames;
        for (int frame = 0; frame < 3; ++frame) {
            core::IndexedImage image({ 6, 4 }, 0);
            image.setPixel({ frame, frame }, 12);
            frames.push_back(image);
        }
        io::GifFile::save(path, frames, { 100, 200, 300 }, palette);

        QImageReader reader(path);
        QCOMPARE(reader.imageCount(), 3);
        const io::ProjectData loaded = io::GifFile::load(path);
        QCOMPARE(loaded.cels.size(), std::size_t { 3 });
        QCOMPARE(loaded.durations, (std::vector<int> { 100, 200, 300 }));
        const auto& second = std::get<core::RgbaImage>(loaded.cels[1][0]);
        QCOMPARE(second.pixel(1, 1), core::Rgba::opaque(palette.color(12)));
        QCOMPARE(second.pixel(0, 0), core::Rgba::opaque(palette.color(0)));
    }

    /// GIF de imagens RGBA: transparência de 1 bit e cores aproximadas.
    void gifRgbaTransparency()
    {
        core::RgbaImage image({ 4, 1 });
        image.setPixel({ 0, 0 }, core::Rgba { 250, 10, 10, 255 });
        image.setPixel({ 1, 0 }, core::Rgba { 0, 0, 0, 0 });
        image.setPixel({ 2, 0 }, core::Rgba { 10, 250, 10, 200 }); // >= 128: opaco
        image.setPixel({ 3, 0 }, core::Rgba { 10, 10, 250, 60 }); // < 128: transparente
        QImage decoded;
        QVERIFY(decoded.loadFromData(
            io::GifFile::encode({ image }, { 100 }, core::Palette { }), "GIF"));
        decoded = decoded.convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(QColor::fromRgba(decoded.pixel(0, 0)), QColor(250, 10, 10));
        QCOMPARE(qAlpha(decoded.pixel(1, 0)), 0);
        QCOMPARE(QColor::fromRgba(decoded.pixel(2, 0)), QColor(10, 250, 10));
        QCOMPARE(qAlpha(decoded.pixel(3, 0)), 0);
    }

    /// Imagem grande e variada: exercita o LZW (códigos até 12 bits e
    /// "clear" ao encher o dicionário); o Qt deve ler exatamente os índices.
    void gifLzwStress()
    {
        core::Palette palette = core::Palette::makeDefault(core::ColorDepth::Bits8);
        core::IndexedImage image({ 300, 200 }, 0);
        QRandomGenerator random(42);
        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 300; ++x) {
                // Mistura de trechos repetidos e ruído.
                const int value = (x / 7 + y / 5) % 3 == 0 ? random.bounded(256) : (x * y) % 17;
                image.setPixel({ x, y }, static_cast<core::ColorIndex>(value));
            }
        }
        QImage decoded;
        QVERIFY(decoded.loadFromData(io::GifFile::encode({ image }, { 100 }, palette), "GIF"));
        decoded = decoded.convertToFormat(QImage::Format_RGB32);
        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 300; ++x) {
                const core::Rgb expected = palette.color(image.pixel(x, y));
                QCOMPARE(decoded.pixel(x, y), qRgb(expected.red, expected.green, expected.blue));
            }
        }
    }
};

QTEST_MAIN(AnimationFileTest)
#include "AnimationFileTest.moc"
