// =============================================================================
// BmpWriter.cpp
//
// Estrutura de um BMP "Windows 3.x" indexado:
//
//   BITMAPFILEHEADER (14 bytes)
//     "BM", tamanho total, 2 campos reservados, deslocamento dos pixels
//   BITMAPINFOHEADER (40 bytes)
//     tamanho do cabeçalho, largura, altura (positiva = linhas de baixo para
//     cima), planos (1), bits por pixel, compressão (0), tamanho dos pixels,
//     resolução X/Y, cores usadas, cores importantes
//   Tabela de cores: N entradas no formato B, G, R, 0
//   Pixels: linhas de baixo para cima, cada uma alinhada a 4 bytes
// =============================================================================
#include "BmpWriter.h"

#include <cstdint>

namespace spritenator::io::detail {

namespace {

constexpr int kFileHeaderSize = 14;
constexpr int kInfoHeaderSize = 40;

/// Resolução gravada no cabeçalho: 2835 pixels/metro ≈ 72 DPI.
constexpr std::uint32_t kPixelsPerMeter = 2835;

/// Acrescenta um inteiro de 16 bits little-endian.
void appendUInt16(QByteArray& out, std::uint16_t value)
{
    out.append(static_cast<char>(value & 0xFFU));
    out.append(static_cast<char>((value >> 8U) & 0xFFU));
}

/// Acrescenta um inteiro de 32 bits little-endian.
void appendUInt32(QByteArray& out, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.append(static_cast<char>((value >> shift) & 0xFFU));
    }
}

/// Bytes ocupados por uma linha de pixels, já com o alinhamento a 4 bytes.
std::uint32_t strideOf(int width, int bitsPerPixel)
{
    const auto bits = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(bitsPerPixel);
    return ((bits + 31U) / 32U) * 4U;
}

} // namespace

QByteArray encodeBmp(const core::IndexedImage& image, const core::Palette& palette)
{
    const int bitsPerPixel = palette.depth() == core::ColorDepth::Bits4 ? 4 : 8;
    const auto colorCount = static_cast<std::uint32_t>(palette.size());
    const std::uint32_t stride = strideOf(image.width(), bitsPerPixel);
    const std::uint32_t pixelBytes = stride * static_cast<std::uint32_t>(image.height());
    const std::uint32_t pixelOffset = kFileHeaderSize + kInfoHeaderSize + colorCount * 4U;
    const std::uint32_t fileSize = pixelOffset + pixelBytes;

    QByteArray out;
    out.reserve(static_cast<qsizetype>(fileSize));

    // --- BITMAPFILEHEADER ---
    out.append("BM", 2);
    appendUInt32(out, fileSize);
    appendUInt16(out, 0); // reservado
    appendUInt16(out, 0); // reservado
    appendUInt32(out, pixelOffset);

    // --- BITMAPINFOHEADER ---
    appendUInt32(out, kInfoHeaderSize);
    appendUInt32(out, static_cast<std::uint32_t>(image.width()));
    appendUInt32(out, static_cast<std::uint32_t>(image.height())); // positivo: de baixo p/ cima
    appendUInt16(out, 1); // planos
    appendUInt16(out, static_cast<std::uint16_t>(bitsPerPixel));
    appendUInt32(out, 0); // BI_RGB: sem compressão
    appendUInt32(out, pixelBytes);
    appendUInt32(out, kPixelsPerMeter);
    appendUInt32(out, kPixelsPerMeter);
    appendUInt32(out, colorCount); // cores usadas
    appendUInt32(out, 0); // todas as cores são "importantes"

    // --- Tabela de cores (B, G, R, reservado) ---
    for (int index = 0; index < palette.size(); ++index) {
        const core::Rgb& color = palette.color(index);
        out.append(static_cast<char>(color.blue));
        out.append(static_cast<char>(color.green));
        out.append(static_cast<char>(color.red));
        out.append('\0');
    }

    // --- Pixels: de baixo para cima ---
    QByteArray row(static_cast<qsizetype>(stride), '\0');
    for (int y = image.height() - 1; y >= 0; --y) {
        row.fill('\0');
        for (int x = 0; x < image.width(); ++x) {
            const core::ColorIndex value = image.pixel(x, y);
            if (bitsPerPixel == 8) {
                row[x] = static_cast<char>(value);
            } else {
                // 4 bits: dois pixels por byte, o da esquerda no nibble alto.
                const auto byteIndex = static_cast<qsizetype>(x / 2);
                const unsigned nibble = static_cast<unsigned>(value) & 0x0FU;
                const unsigned current = static_cast<unsigned char>(row[byteIndex]);
                const unsigned merged
                    = (x % 2 == 0) ? (current | (nibble << 4U)) : (current | nibble);
                row[byteIndex] = static_cast<char>(merged);
            }
        }
        out.append(row);
    }
    return out;
}

} // namespace spritenator::io::detail
