// =============================================================================
// PaletteFile.cpp
//
// Layout dos formatos binários:
//
// RIFF PAL:
//   "RIFF" <tamanho-1 (u32)> "PAL " "data" <tamanho-2 (u32)>
//   <versão 0x0300 (u16)> <quantidade (u16)> N x { R, G, B, flags }
//
// ACT:
//   256 x { R, G, B } (768 bytes), opcionalmente seguido de
//   <quantidade (u16 big-endian)> <índice transparente (u16 big-endian)>
// =============================================================================
#include "PaletteFile.h"

#include "ImageFile.h"
#include "IoError.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QtEndian>

#include <algorithm>
#include <cstdint>

namespace spritenator::io {

namespace {

constexpr int kActBaseSize = 768; ///< 256 cores x 3 bytes
constexpr int kActExtendedSize = 772; ///< + quantidade + índice transparente
constexpr quint16 kRiffPaletteVersion = 0x0300;
const QByteArray kJascSignature = QByteArrayLiteral("JASC-PAL");

/// Lê 1 byte sem sinal de uma posição (já validada).
std::uint8_t byteAt(const QByteArray& data, qsizetype offset)
{
    return static_cast<std::uint8_t>(data.at(offset));
}

/// Interpreta um arquivo RIFF PAL.
std::vector<core::Rgb> parseRiff(const QByteArray& contents)
{
    // Procura o chunk "data" (normalmente logo após "RIFF....PAL ").
    const qsizetype dataChunk = contents.indexOf("data", 12);
    if (dataChunk < 0 || contents.size() < dataChunk + 12) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::PaletteFile", "Invalid RIFF palette: missing data chunk."));
    }
    const auto* bytes = reinterpret_cast<const uchar*>(contents.constData());
    const int count = qFromLittleEndian<quint16>(bytes + dataChunk + 10);
    const qsizetype entries = dataChunk + 12;
    if (count <= 0 || count > core::Palette::kMaxColors
        || contents.size() < entries + static_cast<qsizetype>(count) * 4) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::PaletteFile", "Invalid RIFF palette: bad color count."));
    }

    std::vector<core::Rgb> colors;
    colors.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const qsizetype offset = entries + static_cast<qsizetype>(index) * 4;
        colors.push_back({ byteAt(contents, offset), byteAt(contents, offset + 1),
            byteAt(contents, offset + 2) });
    }
    return colors;
}

/// Interpreta um arquivo JASC-PAL (texto).
std::vector<core::Rgb> parseJasc(const QByteArray& contents)
{
    QTextStream stream(contents);
    const QString signature = stream.readLine().trimmed();
    const QString version = stream.readLine().trimmed();
    bool countOk = false;
    const int count = stream.readLine().trimmed().toInt(&countOk);
    if (signature != QLatin1String(kJascSignature) || version.isEmpty() || !countOk || count <= 0
        || count > core::Palette::kMaxColors) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::PaletteFile", "Invalid JASC palette header."));
    }

    std::vector<core::Rgb> colors;
    colors.reserve(static_cast<std::size_t>(count));
    while (static_cast<int>(colors.size()) < count && !stream.atEnd()) {
        const QStringList parts = stream.readLine().simplified().split(QLatin1Char(' '));
        if (parts.size() < 3) {
            continue; // ignora linhas vazias
        }
        int channels[3] = { };
        for (int channel = 0; channel < 3; ++channel) {
            bool ok = false;
            channels[channel] = parts.at(channel).toInt(&ok);
            if (!ok || channels[channel] < 0 || channels[channel] > 255) {
                throw IoError(QCoreApplication::translate(
                    "spritenator::io::PaletteFile", "Invalid color value in JASC palette."));
            }
        }
        colors.push_back({ static_cast<std::uint8_t>(channels[0]),
            static_cast<std::uint8_t>(channels[1]), static_cast<std::uint8_t>(channels[2]) });
    }
    if (static_cast<int>(colors.size()) != count) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::PaletteFile", "JASC palette ended before all colors were read."));
    }
    return colors;
}

/// Interpreta um arquivo ACT.
std::vector<core::Rgb> parseAct(const QByteArray& contents)
{
    int count = core::Palette::kMaxColors;
    if (contents.size() == kActExtendedSize) {
        const auto* bytes = reinterpret_cast<const uchar*>(contents.constData());
        count = std::clamp<int>(qFromBigEndian<quint16>(bytes + kActBaseSize), 1, 256);
    }
    std::vector<core::Rgb> colors;
    colors.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const qsizetype offset = static_cast<qsizetype>(index) * 3;
        colors.push_back({ byteAt(contents, offset), byteAt(contents, offset + 1),
            byteAt(contents, offset + 2) });
    }
    return colors;
}

/// Acrescenta inteiros little-endian ao buffer.
void appendLe16(QByteArray& out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFFU));
    out.append(static_cast<char>((value >> 8U) & 0xFFU));
}

void appendLe32(QByteArray& out, quint32 value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.append(static_cast<char>((value >> shift) & 0xFFU));
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Leitura
// -----------------------------------------------------------------------------
std::vector<core::Rgb> PaletteFile::parse(const QByteArray& contents)
{
    if (contents.startsWith("RIFF") && contents.mid(8, 4) == "PAL ") {
        return parseRiff(contents);
    }
    if (contents.startsWith(kJascSignature)) {
        return parseJasc(contents);
    }
    if (contents.size() == kActBaseSize || contents.size() == kActExtendedSize) {
        return parseAct(contents);
    }
    throw IoError(QCoreApplication::translate(
        "spritenator::io::PaletteFile", "Unrecognized palette format."));
}

std::vector<core::Rgb> PaletteFile::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw IoError(
            QCoreApplication::translate("spritenator::io::PaletteFile", "Cannot open \"%1\": %2")
                .arg(path, file.errorString()));
    }
    const QByteArray contents = file.readAll();
    file.close();

    // Imagens (BMP/PNG) também servem como fonte de paleta.
    if (contents.startsWith("BM") || contents.startsWith("\x89PNG")) {
        const ImageData data = ImageFile::loadIndexed(path); // cores reais -> paleta
        std::vector<core::Rgb> colors;
        for (int index = 0; index < data.palette.size(); ++index) {
            colors.push_back(data.palette.color(index));
        }
        return colors;
    }
    return parse(contents);
}

// -----------------------------------------------------------------------------
// Gravação
// -----------------------------------------------------------------------------
QByteArray PaletteFile::encode(const core::Palette& palette, PaletteFormat format)
{
    const int count = palette.size();
    QByteArray out;

    switch (format) {
    case PaletteFormat::RiffPal: {
        const auto dataSize = static_cast<quint32>(4 + count * 4); // versão+qtd+entradas
        out.append("RIFF", 4);
        appendLe32(out, dataSize + 12U); // "PAL " + "data" + tamanho do chunk
        out.append("PAL data", 8);
        appendLe32(out, dataSize);
        appendLe16(out, kRiffPaletteVersion);
        appendLe16(out, static_cast<quint16>(count));
        for (int index = 0; index < count; ++index) {
            const core::Rgb& color = palette.color(index);
            out.append(static_cast<char>(color.red));
            out.append(static_cast<char>(color.green));
            out.append(static_cast<char>(color.blue));
            out.append('\0'); // flags
        }
        break;
    }
    case PaletteFormat::JascPal: {
        // Quebras de linha CRLF, como no formato original do Paint Shop Pro.
        out.append(kJascSignature + "\r\n0100\r\n" + QByteArray::number(count) + "\r\n");
        for (int index = 0; index < count; ++index) {
            const core::Rgb& color = palette.color(index);
            out.append(QByteArray::number(color.red) + ' ' + QByteArray::number(color.green) + ' '
                + QByteArray::number(color.blue) + "\r\n");
        }
        break;
    }
    case PaletteFormat::Act: {
        // Sempre 256 entradas; as que não existem na paleta ficam pretas.
        out.fill('\0', kActBaseSize);
        for (int index = 0; index < count; ++index) {
            const core::Rgb& color = palette.color(index);
            out[index * 3] = static_cast<char>(color.red);
            out[index * 3 + 1] = static_cast<char>(color.green);
            out[index * 3 + 2] = static_cast<char>(color.blue);
        }
        // Extensão: quantidade real de cores + "sem cor transparente" (0xFFFF).
        out.append(static_cast<char>((count >> 8) & 0xFF));
        out.append(static_cast<char>(count & 0xFF));
        out.append("\xFF\xFF", 2);
        break;
    }
    }
    return out;
}

void PaletteFile::save(const QString& path, const core::Palette& palette, PaletteFormat format)
{
    const QByteArray encoded = encode(palette, format);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size()
        || !file.commit()) {
        throw IoError(
            QCoreApplication::translate("spritenator::io::PaletteFile", "Cannot write \"%1\": %2")
                .arg(path, file.errorString()));
    }
}

// -----------------------------------------------------------------------------
// Filtros de diálogo
// -----------------------------------------------------------------------------
QStringList PaletteFile::openFilters()
{
    return { QCoreApplication::translate(
                 "spritenator::io::PaletteFile", "Palettes and images (*.pal *.act *.png *.bmp)"),
        QCoreApplication::translate("spritenator::io::PaletteFile", "All files (*)") };
}

QStringList PaletteFile::saveFilters()
{
    return { QCoreApplication::translate(
                 "spritenator::io::PaletteFile", "Microsoft RIFF palette (*.pal)"),
        QCoreApplication::translate("spritenator::io::PaletteFile", "JASC palette (*.pal)"),
        QCoreApplication::translate("spritenator::io::PaletteFile", "Adobe color table (*.act)") };
}

PaletteFormat PaletteFile::formatFromFilterIndex(int index)
{
    switch (index) {
    case 1: return PaletteFormat::JascPal;
    case 2: return PaletteFormat::Act;
    default: return PaletteFormat::RiffPal;
    }
}

} // namespace spritenator::io
