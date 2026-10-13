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
#include <QtEndian>

#include <array>
#include <utility>
#include <variant>
#include <vector>

namespace spritenator::io {

namespace {

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
/// A profundidade é deduzida pela tabela de cores: até 2 entradas => 2
/// cores; até 16 entradas => 16 cores; senão, 256. Não usamos a profundidade declarada no arquivo
/// porque o Qt grava PNG indexado sempre com 8 bits; e como PNG/BMP só podem referenciar índices
/// existentes na tabela, a dedução é equivalente e sem perdas.
ImageData fromIndexed(const QImage& indexed)
{
    const QList<QRgb> table = indexed.colorTable();
    core::IndexedImage image({ indexed.width(), indexed.height() });
    const int highestIndex = copyIndices(indexed, image);

    // Verificamos também o maior índice, por segurança contra arquivos malformados.
    core::ColorDepth depth = core::ColorDepth::Bits8;
    if (table.size() <= 2 && highestIndex < 2) {
        depth = core::ColorDepth::Bits1;
    } else if (table.size() <= 16 && highestIndex < 16) {
        depth = core::ColorDepth::Bits4;
    }
    core::Palette palette(depth);
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

/// Tabela de cores declarada num arquivo BMP indexado (1, 4 ou 8 bits), na
/// ordem do arquivo. Vazia se não for um BMP indexado reconhecível.
QList<core::Rgb> bmpColorTable(const QByteArray& contents)
{
    QList<core::Rgb> table;
    if (!contents.startsWith("BM") || contents.size() < 54) {
        return table;
    }
    const auto* bytes = reinterpret_cast<const uchar*>(contents.constData());
    const auto headerSize = qFromLittleEndian<quint32>(bytes + 14);
    const auto bitCount = qFromLittleEndian<quint16>(bytes + 28);
    const auto colorsUsed = qFromLittleEndian<quint32>(bytes + 46);
    if (bitCount > 8 || headerSize < 40) {
        return table;
    }
    const quint32 count = colorsUsed != 0 ? colorsUsed : (1U << bitCount);
    const qsizetype offset = 14 + static_cast<qsizetype>(headerSize);
    if (count > 256 || contents.size() < offset + static_cast<qsizetype>(count) * 4) {
        return table;
    }
    for (quint32 index = 0; index < count; ++index) {
        const uchar* entry = bytes + offset + static_cast<qsizetype>(index) * 4; // B, G, R, 0
        table.append({ entry[2], entry[1], entry[0] });
    }
    return table;
}

/// O leitor de BMP do Qt reordena a paleta de imagens de 1 bit (e inverte os
/// índices): a aparência é a mesma, mas o NÚMERO de cada cor muda — o que
/// importa num editor indexado. Se a paleta lida for uma permutação da
/// tabela do arquivo, restauramos a ordem original e remapeamos os pixels.
void restoreFilePaletteOrder(ImageData& data, const QList<core::Rgb>& fileTable)
{
    const auto count = static_cast<int>(fileTable.size());
    if (count == 0 || count > data.palette.size()) {
        return;
    }
    // Com cores repetidas no arquivo a correspondência seria ambígua.
    for (int a = 0; a < count; ++a) {
        for (int b = a + 1; b < count; ++b) {
            if (fileTable.at(a) == fileTable.at(b)) {
                return;
            }
        }
    }
    // Para cada índice lido, o índice da mesma cor na tabela do arquivo.
    std::array<core::ColorIndex, 256> remap { };
    bool identity = true;
    for (int index = 0; index < count; ++index) {
        const auto found = fileTable.indexOf(data.palette.color(index));
        if (found < 0) {
            return; // não é uma permutação: mantém o que o Qt leu
        }
        remap.at(static_cast<std::size_t>(index)) = static_cast<core::ColorIndex>(found);
        identity = identity && found == index;
    }
    if (identity) {
        return;
    }
    std::get<core::IndexedImage>(data.image).remap(remap);
    for (int index = 0; index < count; ++index) {
        data.palette.setColor(index, fileTable.at(index));
    }
}

// -----------------------------------------------------------------------------
// Conversões QImage <-> ImageData
// -----------------------------------------------------------------------------
/// Verifica se o QImage pode ser carregado (não vazio, dentro do limite).
void validateSource(const QImage& source)
{
    if (source.isNull()) {
        throw IoError(
            QCoreApplication::translate("spritenator::io::ImageFile", "The image is empty."));
    }
    // O editor limita as dimensões (ver IndexedImage::kMaxDimension); sem esta
    // verificação o construtor da imagem lançaria std::invalid_argument.
    constexpr int kLimit = core::IndexedImage::kMaxDimension;
    if (source.width() > kLimit || source.height() > kLimit) {
        throw IoError(QCoreApplication::translate("spritenator::io::ImageFile",
            "The image is too large (%1 x %2). The maximum is %3 x %3 pixels.")
                .arg(source.width())
                .arg(source.height())
                .arg(kLimit));
    }
}

/// Formatos do Qt que têm paleta (ou equivalem a uma, como tons de cinza).
bool isIndexedFormat(QImage::Format format)
{
    return format == QImage::Format_Indexed8 || format == QImage::Format_Mono
        || format == QImage::Format_MonoLSB || format == QImage::Format_Grayscale8;
}

/// Imagem indexada/cinza do Qt -> dados indexados.
ImageData fromIndexedFormat(const QImage& source)
{
    switch (source.format()) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
        // 1 bit: expande para 8 bits preservando a tabela de cores.
        return fromIndexed(source.convertToFormat(QImage::Format_Indexed8));
    case QImage::Format_Grayscale8: return fromGrayscale(source);
    default: return fromIndexed(source);
    }
}

/// Imagem em cores reais -> RGBA, pixel a pixel e sem perdas.
ImageData fromRgbaFormat(const QImage& source)
{
    // ARGB32 (não pré-multiplicado): 0xAARRGGBB por pixel, como core::Rgba.
    const QImage argb = source.convertToFormat(QImage::Format_ARGB32);
    core::RgbaImage image({ argb.width(), argb.height() });
    auto pixels = image.data();
    const auto width = static_cast<std::size_t>(argb.width());
    for (int y = 0; y < argb.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (std::size_t x = 0; x < width; ++x) {
            pixels[static_cast<std::size_t>(y) * width + x] = core::Rgba::fromArgb32(line[x]);
        }
    }
    return { std::move(image), core::Palette::makeDefault(core::ColorDepth::Bits8) };
}

} // namespace

ImageData ImageFile::fromQImage(const QImage& source)
{
    validateSource(source);
    return isIndexedFormat(source.format()) ? fromIndexedFormat(source) : fromRgbaFormat(source);
}

ImageData ImageFile::fromQImageIndexed(const QImage& source)
{
    validateSource(source);
    return isIndexedFormat(source.format()) ? fromIndexedFormat(source) : fromTrueColor(source);
}

QImage ImageFile::toQImage(const ImageData& data)
{
    // RGBA: Format_ARGB32 (0xAARRGGBB por pixel, igual a core::Rgba).
    if (const auto* rgba = std::get_if<core::RgbaImage>(&data.image)) {
        QImage result(rgba->width(), rgba->height(), QImage::Format_ARGB32);
        for (int y = 0; y < rgba->height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < rgba->width(); ++x) {
                line[x] = rgba->pixel(x, y).toArgb32();
            }
        }
        return result;
    }

    const auto& image = std::get<core::IndexedImage>(data.image);
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
namespace {

/// Lê o arquivo e o decodifica com o Qt (conteúdo bruto + imagem).
std::pair<QByteArray, QImage> readAndDecode(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw IoError(
            QCoreApplication::translate("spritenator::io::ImageFile", "Cannot open \"%1\": %2")
                .arg(path, file.errorString()));
    }
    QByteArray contents = file.readAll();
    QImage decoded;
    if (!decoded.loadFromData(contents)) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::ImageFile", "\"%1\" is not a supported image file.")
                .arg(path));
    }
    return { std::move(contents), std::move(decoded) };
}

/// Ajustes finais de imagens indexadas lidas de BMP (ordem da paleta).
void fixIndexedBmp(ImageData& data, const QByteArray& contents)
{
    if (data.colorMode() == core::ColorMode::Indexed) {
        restoreFilePaletteOrder(data, bmpColorTable(contents));
    }
}

} // namespace

ImageData ImageFile::load(const QString& path)
{
    const auto [contents, decoded] = readAndDecode(path);
    ImageData data = fromQImage(decoded);
    fixIndexedBmp(data, contents);
    return data;
}

ImageData ImageFile::loadIndexed(const QString& path)
{
    const auto [contents, decoded] = readAndDecode(path);
    ImageData data = fromQImageIndexed(decoded);
    fixIndexedBmp(data, contents);
    return data;
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
    throw IoError(QCoreApplication::translate(
        "spritenator::io::ImageFile", "Unsupported file extension: \"%1\".")
            .arg(suffix));
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
        const auto* indexed = std::get_if<core::IndexedImage>(&data.image);
        if (indexed == nullptr) {
            throw IoError(QCoreApplication::translate("spritenator::io::ImageFile",
                "BMP files cannot store transparency. Save RGBA images as PNG."));
        }
        encoded = detail::encodeBmp(*indexed, data.palette);
    } else {
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, "png");
        if (!writer.write(toQImage(data))) {
            throw IoError(
                QCoreApplication::translate("spritenator::io::ImageFile", "Cannot encode PNG: %1")
                    .arg(writer.errorString()));
        }
    }

    // QSaveFile grava num arquivo temporário e só o renomeia no commit(),
    // evitando arquivos corrompidos se o programa for interrompido.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size()
        || !file.commit()) {
        throw IoError(
            QCoreApplication::translate("spritenator::io::ImageFile", "Cannot write \"%1\": %2")
                .arg(path, file.errorString()));
    }
}

// -----------------------------------------------------------------------------
// Filtros de diálogo
// -----------------------------------------------------------------------------
QStringList ImageFile::openFilters()
{
    return { QCoreApplication::translate(
                 "spritenator::io::ImageFile", "Supported images (*.png *.bmp)"),
        QCoreApplication::translate("spritenator::io::ImageFile", "PNG image (*.png)"),
        QCoreApplication::translate("spritenator::io::ImageFile", "Bitmap image (*.bmp)"),
        QCoreApplication::translate("spritenator::io::ImageFile", "All files (*)") };
}

QStringList ImageFile::saveFilters(core::ColorMode mode)
{
    QStringList filters {
        QCoreApplication::translate("spritenator::io::ImageFile", "PNG image (*.png)"),
    };
    if (mode == core::ColorMode::Indexed) { // BMP não guarda transparência
        filters.append(
            QCoreApplication::translate("spritenator::io::ImageFile", "Bitmap image (*.bmp)"));
    }
    return filters;
}

} // namespace spritenator::io
