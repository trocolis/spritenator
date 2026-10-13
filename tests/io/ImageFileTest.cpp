// =============================================================================
// ImageFileTest.cpp — testes de ida e volta (gravar -> ler) de imagens.
// =============================================================================
#include "io/ImageFile.h"
#include "io/IoError.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

using namespace spritenator;
using spritenator::io::ImageData;
using spritenator::io::ImageFile;

namespace {

/// Imagem de teste com um padrão que usa vários índices.
ImageData makeSample(core::ColorDepth depth)
{
    const core::Palette palette = core::Palette::makeDefault(depth);
    core::IndexedImage image({ 7, 5 }); // largura ímpar: testa alinhamento de linhas
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel({ x, y }, static_cast<core::ColorIndex>((x + y * 3) % palette.size()));
        }
    }
    return { image, palette };
}

/// Lê o campo biBitCount de um BMP gravado.
int bmpBitCount(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return -1;
    }
    const QByteArray header = file.read(30);
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(header.constData()) + 28);
}

} // namespace

class ImageFileTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTrip_data()
    {
        QTest::addColumn<QString>("extension");
        QTest::addColumn<int>("depth");
        QTest::newRow("bmp-8") << "bmp" << 8;
        QTest::newRow("bmp-4") << "bmp" << 4;
        QTest::newRow("png-8") << "png" << 8;
        QTest::newRow("png-4") << "png" << 4;
    }

    /// Gravar e ler de volta deve preservar pixels e paleta exatamente.
    void roundTrip()
    {
        QFETCH(QString, extension);
        QFETCH(int, depth);
        const auto colorDepth = depth == 4 ? core::ColorDepth::Bits4 : core::ColorDepth::Bits8;

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("sample.") + extension);

        const ImageData original = makeSample(colorDepth);
        ImageFile::save(path, original);
        const ImageData loaded = ImageFile::load(path);

        QCOMPARE(loaded.image, original.image);
        for (int index = 0; index < original.palette.size(); ++index) {
            QCOMPARE(loaded.palette.color(index), original.palette.color(index));
        }
        // A profundidade (16/256 cores) é preservada em ambos os formatos.
        QCOMPARE(loaded.palette.depth(), colorDepth);
        if (extension == QLatin1String("bmp")) {
            QCOMPARE(bmpBitCount(path), depth); // BMP de 16 cores gravado em 4 bits
        }
    }

    /// Imagem em cores reais com poucas cores vira paleta exata.
    void trueColorWithFewColorsIsExact()
    {
        QImage rgb(4, 2, QImage::Format_RGB32);
        rgb.fill(qRgb(10, 20, 30));
        rgb.setPixel(3, 1, qRgb(200, 100, 0));
        const ImageData data = ImageFile::fromQImage(rgb);
        QCOMPARE(data.palette.depth(), core::ColorDepth::Bits8);
        QCOMPARE(data.palette.color(data.image.pixel(0, 0)), (core::Rgb { 10, 20, 30 }));
        QCOMPARE(data.palette.color(data.image.pixel(3, 1)), (core::Rgb { 200, 100, 0 }));
    }

    /// Mais de 256 cores: a imagem é reduzida, mas mantém o tamanho.
    void trueColorWithManyColorsIsReduced()
    {
        QImage rgb(32, 32, QImage::Format_RGB32);
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                rgb.setPixel(x, y, qRgb(x * 8, y * 8, (x + y) * 4));
            }
        }
        const ImageData data = ImageFile::fromQImage(rgb);
        QCOMPARE(data.image.size(), (core::Size { 32, 32 }));
        QCOMPARE(data.palette.size(), 256);
    }

    /// Imagens maiores que o limite do editor geram IoError (e não outra
    /// exceção que derrubaria a aplicação).
    void oversizedImageThrowsIoError()
    {
        const QImage huge(core::IndexedImage::kMaxDimension + 1, 1, QImage::Format_Indexed8);
        QVERIFY_THROWS_EXCEPTION(io::IoError, std::ignore = ImageFile::fromQImage(huge));
    }

    void missingFileThrows()
    {
        QVERIFY_THROWS_EXCEPTION(
            io::IoError, std::ignore = ImageFile::load(QStringLiteral("/nonexistent/file.png")));
    }

    void garbageFileThrows()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("garbage.png"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("definitely not an image");
        file.close();
        QVERIFY_THROWS_EXCEPTION(io::IoError, std::ignore = ImageFile::load(path));
    }

    void unsupportedExtensionThrows()
    {
        QVERIFY_THROWS_EXCEPTION(
            io::IoError, std::ignore = ImageFile::formatFromPath(QStringLiteral("a.gif")));
        QCOMPARE(ImageFile::formatFromPath(QStringLiteral("A.PNG")), io::ImageFormat::Png);
    }
};

QTEST_GUILESS_MAIN(ImageFileTest)
#include "ImageFileTest.moc"
