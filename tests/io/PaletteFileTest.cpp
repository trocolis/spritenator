// =============================================================================
// PaletteFileTest.cpp — testes dos formatos de paleta.
// =============================================================================
#include "io/PaletteFile.h"
#include "io/ImageFile.h"
#include "io/IoError.h"

#include <QTemporaryDir>
#include <QTest>

using namespace spritenator;
using spritenator::io::PaletteFile;
using spritenator::io::PaletteFormat;

class PaletteFileTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTrip_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("depth");
        QTest::newRow("riff-256") << int(PaletteFormat::RiffPal) << 8;
        QTest::newRow("riff-16") << int(PaletteFormat::RiffPal) << 4;
        QTest::newRow("riff-2") << int(PaletteFormat::RiffPal) << 1;
        QTest::newRow("jasc-2") << int(PaletteFormat::JascPal) << 1;
        QTest::newRow("jasc-256") << int(PaletteFormat::JascPal) << 8;
        QTest::newRow("jasc-16") << int(PaletteFormat::JascPal) << 4;
        QTest::newRow("act-256") << int(PaletteFormat::Act) << 8;
        QTest::newRow("act-16") << int(PaletteFormat::Act) << 4;
    }

    /// Codificar e decodificar devolve exatamente as mesmas cores.
    void roundTrip()
    {
        QFETCH(int, format);
        QFETCH(int, depth);
        const core::Palette palette
            = core::Palette::makeDefault(depth == 1 ? core::ColorDepth::Bits1
                    : depth == 4                    ? core::ColorDepth::Bits4
                                                    : core::ColorDepth::Bits8);

        const auto colors
            = PaletteFile::parse(PaletteFile::encode(palette, static_cast<PaletteFormat>(format)));
        QCOMPARE(static_cast<int>(colors.size()), palette.size());
        for (int index = 0; index < palette.size(); ++index) {
            QCOMPARE(colors.at(static_cast<std::size_t>(index)), palette.color(index));
        }
    }

    /// A paleta de uma imagem pode ser carregada como arquivo de paleta.
    void loadFromImage()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("image.bmp"));
        const core::Palette palette = core::Palette::makeDefault(core::ColorDepth::Bits4);
        io::ImageFile::save(path, { core::IndexedImage({ 2, 2 }), palette });

        const auto colors = PaletteFile::load(path);
        QCOMPARE(static_cast<int>(colors.size()), 16);
        QCOMPARE(colors.at(9), palette.color(9));
    }

    void saveAndLoadFromDisk()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("colors.pal"));
        core::Palette palette(core::ColorDepth::Bits4);
        palette.setColor(3, { 1, 2, 3 });
        PaletteFile::save(path, palette, PaletteFormat::JascPal);
        QCOMPARE(PaletteFile::load(path).at(3), (core::Rgb { 1, 2, 3 }));
    }

    void invalidContentsThrow()
    {
        QVERIFY_THROWS_EXCEPTION(io::IoError, std::ignore = PaletteFile::parse("hello"));
        QVERIFY_THROWS_EXCEPTION(
            io::IoError, std::ignore = PaletteFile::parse("JASC-PAL\r\n0100\r\n2\r\n1 2 3\r\n"));
        QVERIFY_THROWS_EXCEPTION(
            io::IoError, std::ignore = PaletteFile::parse("JASC-PAL\r\n0100\r\n1\r\n1 2 300\r\n"));
    }

    void filterIndexMapsToFormat()
    {
        QCOMPARE(PaletteFile::saveFilters().size(), 3);
        QCOMPARE(PaletteFile::formatFromFilterIndex(1), PaletteFormat::JascPal);
        QCOMPARE(PaletteFile::formatFromFilterIndex(2), PaletteFormat::Act);
        QCOMPARE(PaletteFile::formatFromFilterIndex(99), PaletteFormat::RiffPal);
    }
};

QTEST_GUILESS_MAIN(PaletteFileTest)
#include "PaletteFileTest.moc"
