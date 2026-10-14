// =============================================================================
// GifFile.cpp
// =============================================================================
#include "GifFile.h"

#include "IoError.h"
#include "core/Quantize.h"

#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <variant>

namespace spritenator::io {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("spritenator::io::GifFile", text);
}

/// Alfa a partir do qual um pixel RGBA conta como opaco no GIF.
constexpr int kOpaqueAlpha = 128;

/// Grava bits de tamanho variável (LSB primeiro) em sub-blocos de até 255
/// bytes, como o GIF exige.
class BitWriter {
public:
    void write(int code, int size)
    {
        m_buffer |= static_cast<quint32>(code) << m_bits;
        m_bits += size;
        while (m_bits >= 8) {
            m_bytes.append(static_cast<char>(m_buffer & 0xFFU));
            m_buffer >>= 8;
            m_bits -= 8;
        }
    }

    /// Bytes finais, já divididos em sub-blocos e com o terminador.
    [[nodiscard]] QByteArray finish()
    {
        if (m_bits > 0) {
            m_bytes.append(static_cast<char>(m_buffer & 0xFFU));
        }
        QByteArray blocks;
        for (qsizetype offset = 0; offset < m_bytes.size(); offset += 255) {
            const qsizetype size = std::min<qsizetype>(255, m_bytes.size() - offset);
            blocks.append(static_cast<char>(size));
            blocks.append(m_bytes.mid(offset, size));
        }
        blocks.append('\0'); // fim dos dados da imagem
        return blocks;
    }

private:
    QByteArray m_bytes;
    quint32 m_buffer = 0;
    int m_bits = 0;
};

/// Compressão LZW do GIF (códigos de 2 a 12 bits, com "clear" ao encher).
QByteArray lzwEncode(const std::vector<quint8>& indices, int minCodeSize)
{
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    constexpr int kMaxCode = 4095;

    BitWriter writer;
    QHash<quint32, int> dictionary; // (prefixo << 8 | byte) -> código
    int codeSize = minCodeSize + 1;
    int nextCode = endCode + 1;
    const auto reset = [&] {
        dictionary.clear();
        codeSize = minCodeSize + 1;
        nextCode = endCode + 1;
    };

    writer.write(clearCode, codeSize);
    if (indices.empty()) {
        writer.write(endCode, codeSize);
        return writer.finish();
    }
    int prefix = indices.front();
    for (std::size_t index = 1; index < indices.size(); ++index) {
        const quint8 value = indices[index];
        const quint32 key = (static_cast<quint32>(prefix) << 8) | value;
        if (const auto found = dictionary.constFind(key); found != dictionary.constEnd()) {
            prefix = found.value();
            continue;
        }
        writer.write(prefix, codeSize);
        if (nextCode <= kMaxCode) {
            dictionary.insert(key, nextCode);
            if (nextCode == (1 << codeSize) && codeSize < 12) {
                ++codeSize;
            }
            ++nextCode;
        } else {
            writer.write(clearCode, codeSize); // dicionário cheio: recomeça
            reset();
        }
        prefix = value;
    }
    writer.write(prefix, codeSize);
    writer.write(endCode, codeSize);
    return writer.finish();
}

void appendWord(QByteArray& bytes, int value)
{
    bytes.append(static_cast<char>(value & 0xFF));
    bytes.append(static_cast<char>((value >> 8) & 0xFF));
}

} // namespace

QByteArray GifFile::encode(const std::vector<core::AnyImage>& frames,
    const std::vector<int>& durations, const core::Palette& palette)
{
    if (frames.empty()) {
        throw IoError(tr("There is nothing to save."));
    }
    const core::Size size = core::sizeOf(frames.front());
    const bool rgba = core::modeOf(frames.front()) == core::ColorMode::Rgba;

    // --- Paleta global e índices de cada cena ---
    std::vector<core::Rgb> colors;
    int transparentIndex = -1;
    std::vector<std::vector<quint8>> indices;
    if (!rgba) {
        for (int index = 0; index < palette.size(); ++index) {
            colors.push_back(palette.color(index));
        }
        for (const core::AnyImage& frame : frames) {
            const auto pixels = std::get<core::IndexedImage>(frame).data();
            indices.emplace_back(pixels.begin(), pixels.end());
        }
    } else {
        // Corte mediano sobre os pixels opacos de todas as cenas (255 cores
        // + 1 índice reservado para a transparência).
        std::vector<core::Rgb> opaque;
        for (const core::AnyImage& frame : frames) {
            for (const core::Rgba& pixel : std::get<core::RgbaImage>(frame).data()) {
                if (pixel.alpha >= kOpaqueAlpha) {
                    opaque.push_back(pixel.rgb());
                }
            }
        }
        colors = core::medianCut(opaque, 255);
        transparentIndex = static_cast<int>(colors.size());
        colors.push_back({ 0, 0, 0 });
        QHash<quint32, quint8> cache; // cor -> índice (evita buscas repetidas)
        for (const core::AnyImage& frame : frames) {
            std::vector<quint8> frameIndices;
            for (const core::Rgba& pixel : std::get<core::RgbaImage>(frame).data()) {
                if (pixel.alpha < kOpaqueAlpha) {
                    frameIndices.push_back(static_cast<quint8>(transparentIndex));
                    continue;
                }
                const quint32 key = pixel.rgb().toPacked();
                auto found = cache.constFind(key);
                if (found == cache.constEnd()) {
                    found = cache.insert(key,
                        static_cast<quint8>(core::nearestColor(
                            std::vector<core::Rgb>(colors.begin(), colors.end() - 1),
                            pixel.rgb())));
                }
                frameIndices.push_back(found.value());
            }
            indices.push_back(std::move(frameIndices));
        }
    }

    // A tabela do GIF tem 2^n cores (n de 1 a 8).
    int tableBits = 1;
    while ((1 << tableBits) < static_cast<int>(colors.size())) {
        ++tableBits;
    }
    const int minCodeSize = std::max(2, tableBits);

    // --- Cabeçalho, tela e paleta global ---
    QByteArray bytes("GIF89a");
    appendWord(bytes, size.width);
    appendWord(bytes, size.height);
    bytes.append(static_cast<char>(0x80 | ((tableBits - 1) << 4) | (tableBits - 1)));
    bytes.append('\0'); // cor de fundo
    bytes.append('\0'); // proporção dos pixels
    for (int index = 0; index < (1 << tableBits); ++index) {
        const core::Rgb color = index < static_cast<int>(colors.size())
            ? colors[static_cast<std::size_t>(index)]
            : core::Rgb { };
        bytes.append(static_cast<char>(color.red));
        bytes.append(static_cast<char>(color.green));
        bytes.append(static_cast<char>(color.blue));
    }
    // Laço infinito (NETSCAPE2.0), só se houver animação.
    if (frames.size() > 1) {
        bytes.append("\x21\xFF\x0BNETSCAPE2.0\x03\x01", 16);
        appendWord(bytes, 0);
        bytes.append('\0');
    }

    // --- Cenas ---
    for (std::size_t frame = 0; frame < frames.size(); ++frame) {
        // Controle gráfico: duração (centésimos) e transparência; cada cena
        // substitui a anterior inteira (descarte = restaurar o fundo).
        const int duration = frame < durations.size() ? durations[frame] : 100;
        bytes.append("\x21\xF9\x04", 3);
        bytes.append(static_cast<char>((2 << 2) | (transparentIndex >= 0 ? 1 : 0)));
        appendWord(bytes, std::max(1, static_cast<int>(std::lround(duration / 10.0))));
        bytes.append(static_cast<char>(std::max(0, transparentIndex)));
        bytes.append('\0');
        // Descritor da imagem (sem paleta local, sem entrelaçamento).
        bytes.append('\x2C');
        appendWord(bytes, 0);
        appendWord(bytes, 0);
        appendWord(bytes, size.width);
        appendWord(bytes, size.height);
        bytes.append('\0');
        bytes.append(static_cast<char>(minCodeSize));
        bytes.append(lzwEncode(indices[frame], minCodeSize));
    }
    bytes.append('\x3B'); // fim do arquivo
    return bytes;
}

void GifFile::save(const QString& path, const std::vector<core::AnyImage>& frames,
    const std::vector<int>& durations, const core::Palette& palette)
{
    const QByteArray bytes = encode(frames, durations, palette);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        throw IoError(tr("Cannot write \"%1\": %2").arg(path, file.errorString()));
    }
}

ProjectData GifFile::load(const QString& path)
{
    QImageReader reader(path, "gif");
    ProjectData data;
    data.palette = core::Palette::makeDefault(core::ColorDepth::Bits8);
    data.layers.push_back(
        { QCoreApplication::translate("spritenator::gui::Document", "Layer %1").arg(1), true,
            255 });
    core::Size size;
    while (reader.canRead()) {
        const QImage frame = reader.read().convertToFormat(QImage::Format_ARGB32);
        if (frame.isNull()) {
            break;
        }
        if (data.cels.empty()) {
            size = { frame.width(), frame.height() };
        }
        if (frame.width() != size.width || frame.height() != size.height
            || size.width > core::IndexedImage::kMaxDimension
            || size.height > core::IndexedImage::kMaxDimension) {
            throw IoError(tr("\"%1\" has scenes of different sizes.").arg(path));
        }
        core::RgbaImage image(size);
        for (int y = 0; y < frame.height(); ++y) {
            const auto* line = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
            for (int x = 0; x < frame.width(); ++x) {
                image.setPixel({ x, y }, core::Rgba::fromArgb32(line[x]));
            }
        }
        data.cels.push_back({ core::AnyImage(std::move(image)) });
        const int delay = reader.nextImageDelay();
        data.durations.push_back(delay > 0 ? delay : 100);
    }
    if (data.cels.empty()) {
        throw IoError(tr("Cannot open \"%1\": %2").arg(path, reader.errorString()));
    }
    return data;
}

} // namespace spritenator::io
