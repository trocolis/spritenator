// =============================================================================
// ImageFile.cpp
// =============================================================================
#include "ImageFile.h"

#include "BmpWriter.h"
#include "IoError.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageWriter>
#include <QSaveFile>

#include <vector>

namespace spritenator::io {

namespace {

/// Tradução de mensagens fora de classes QObject.
QString translate(const char* text)
{
    return QCoreApplication::translate("spritenator::io::ImageFile", text);
}

/// Converte QRgb (ARGB do Qt) em core::Rgb, descartando o alfa.
core::Rgb toCoreRgb(QRgb color)
{
    return { static_cast<std::uint8_t>(qRed(color)), static_cast<std::uint8_t>(qGreen(color)),
        static_cast<std::uint8_t>(qBlue(color)) };
}

/// Copia os índices de um QImage Format_Indexed8 / Grayscale8 (1 byte por
/// pixel) para uma IndexedImage. Devolve o maior índice encontrado.
int copyIndices(const QImage& source, core::IndexedImage& target)
{
    int highest = 0;
    auto pixels = target.data();
    const auto width = static_cast<std::size_t>(source.width());
    for (int y = 0; y < source.height(); ++y) {
        const uchar* line = source.constScanLine(y);
        for (std::size_t x = 0; x < width; ++x) {
            pixels[static_cast<std::size_t>(y) * width + x] = line[x];
            highest = std::max(highest, static_cast<int>(line[x]));
        }
    }
    return highest;
}

/// Converte uma imagem já indexada (com tabela de cores) em ImageData.
///
/// A profundidade é deduzida pela tabela de cores: até 16 entradas => 16
/// cores. Não usamos a profundidade declarada no arquivo porque o Qt grava
/// PNG indexado sempre com 8 bits; e como PNG/BMP só podem referenciar
/// índices existentes na tabela, a dedução é equivalente e sem perdas.
ImageData fromIndexed(const QImage& indexed)
{
    const QList<QRgb> table = indexed.colorTable();
    core::IndexedImage image({ indexed.width(), indexed.height() });
    const int highestIndex = copyIndices(indexed, image);

    // Verificamos também o maior índice, por segurança contra arquivos malformados.
    const bool fitsIn4Bits = table.size() <= 16 && highestIndex < 16;
    core::Palette palette(fitsIn4Bits ? core::ColorDepth::Bits4 : core::ColorDepth::Bits8);
    for (int index = 0; index < std::min<int>(static_cast<int>(table.size()), palette.size());
        ++index) {
        palette.setColor(index, toCoreRgb(table.at(index)));
    }
    return { std::move(image), palette };
}

/// Imagem em tons de cinza de 8 bits: o "índice" é o próprio nível de cinza.
ImageData fromGrayscale(const QImage& gray)
{
    core::IndexedImage image({ gray.width(), gray.height() });
    copyIndices(gray, image);
    core::Palette palette(core::ColorDepth::Bits8);
    for (int level = 0; level < palette.size(); ++level) {
        const auto channel = static_cast<std::uint8_t>(level);
        palette.setColor(level, { channel, channel, channel });
    }
    return { std::move(image), palette };
}

/// Imagem em cores reais: tenta montar uma paleta exata; se houver mais de
/// 256 cores, reduz com pontilhamento por difusão de erro (feito pelo Qt).
ImageData fromTrueColor(const QImage& source)
{
    const QImage rgb = source.convertToFormat(QImage::Format_RGB32);

    // 1ª tentativa: paleta exata, na ordem em que as cores aparecem.
    QHash<QRgb, int> indexOf;
    std::vector<QRgb> colors;
    bool exact = true;
    for (int y = 0; y < rgb.height() && exact; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); ++x) {
            const QRgb color = line[x] | 0xFF000000U; // ignora o alfa
            if (!indexOf.contains(color)) {
                if (colors.size() == 256) {
                    exact = false;
                    break;
                }
                indexOf.insert(color, static_cast<int>(colors.size()));
                colors.push_back(color);
            }
        }
    }

    if (exact) {
        core::IndexedImage image({ rgb.width(), rgb.height() });
        auto pixels = image.data();
        const auto width = static_cast<std::size_t>(rgb.width());
        for (int y = 0; y < rgb.height(); ++y) {
            const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
            for (std::size_t x = 0; x < width; ++x) {
                pixels[static_cast<std::size_t>(y) * width + x]
                    = static_cast<core::ColorIndex>(indexOf.value(line[x] | 0xFF000000U));
            }
        }
        core::Palette palette(core::ColorDepth::Bits8);
        for (std::size_t index = 0; index < colors.size(); ++index) {
            palette.setColor(static_cast<int>(index), toCoreRgb(colors[index]));
        }
        return { std::move(image), palette };
    }

    // 2ª tentativa: redução de cores com pontilhamento.
    const QImage reduced = rgb.convertToFormat(
        QImage::Format_Indexed8, Qt::DiffuseDither | Qt::PreferDither | Qt::ColorOnly);
    return fromIndexed(reduced);
}

} // namespace

// -----------------------------------------------------------------------------
// Conversões QImage <-> ImageData
// -----------------------------------------------------------------------------
ImageData ImageFile::fromQImage(const QImage& source)
{
    if (source.isNull()) {
        throw IoError(translate("The image is empty."));
    }
    switch (source.format()) {
    case QImage::Format_Indexed8: return fromIndexed(source);
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
        // 1 bit: expande para 8 bits preservando a tabela de cores.
        return fromIndexed(source.convertToFormat(QImage::Format_Indexed8));
    case QImage::Format_Grayscale8: return fromGrayscale(source);
    default: return fromTrueColor(source);
    }
}

QImage ImageFile::toQImage(const ImageData& data)
{
    const core::IndexedImage& image = data.image;
    QImage result(image.width(), image.height(), QImage::Format_Indexed8);

    QList<QRgb> table;
    table.reserve(data.palette.size());
    for (int index = 0; index < data.palette.size(); ++index) {
        const core::Rgb& color = data.palette.color(index);
        table.append(qRgb(color.red, color.green, color.blue));
    }
    result.setColorTable(table);

    // As linhas do QImage são alinhadas a 4 bytes; copiamos linha a linha.
    const auto pixels = image.data();
    const auto width = static_cast<std::size_t>(image.width());
    for (int y = 0; y < image.height(); ++y) {
        std::copy_n(
            pixels.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * width),
            width, result.scanLine(y));
    }
    return result;
}

// -----------------------------------------------------------------------------
// Leitura
// -----------------------------------------------------------------------------
ImageData ImageFile::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw IoError(translate("Cannot open \"%1\": %2").arg(path, file.errorString()));
    }
    const QByteArray contents = file.readAll();

    QImage decoded;
    if (!decoded.loadFromData(contents)) {
        throw IoError(translate("\"%1\" is not a supported image file.").arg(path));
    }
    return fromQImage(decoded);
}

// -----------------------------------------------------------------------------
// Gravação
// -----------------------------------------------------------------------------
ImageFormat ImageFile::formatFromPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("bmp")) {
        return ImageFormat::Bmp;
    }
    if (suffix == QLatin1String("png")) {
        return ImageFormat::Png;
    }
    throw IoError(translate("Unsupported file extension: \"%1\".").arg(suffix));
}

void ImageFile::save(const QString& path, const ImageData& data)
{
    save(path, data, formatFromPath(path));
}

void ImageFile::save(const QString& path, const ImageData& data, ImageFormat format)
{
    // Codifica primeiro em memória: se algo falhar, o arquivo antigo fica intacto.
    QByteArray encoded;
    if (format == ImageFormat::Bmp) {
        encoded = detail::encodeBmp(data.image, data.palette);
    } else {
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, "png");
        if (!writer.write(toQImage(data))) {
            throw IoError(translate("Cannot encode PNG: %1").arg(writer.errorString()));
        }
    }

    // QSaveFile grava num arquivo temporário e só o renomeia no commit(),
    // evitando arquivos corrompidos se o programa for interrompido.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size()
        || !file.commit()) {
        throw IoError(translate("Cannot write \"%1\": %2").arg(path, file.errorString()));
    }
}

// -----------------------------------------------------------------------------
// Filtros de diálogo
// -----------------------------------------------------------------------------
QStringList ImageFile::openFilters()
{
    return { translate("Supported images (*.png *.bmp)"), translate("PNG image (*.png)"),
        translate("Bitmap image (*.bmp)"), translate("All files (*)") };
}

QStringList ImageFile::saveFilters()
{
    return { translate("PNG image (*.png)"), translate("Bitmap image (*.bmp)") };
}

} // namespace spritenator::io
