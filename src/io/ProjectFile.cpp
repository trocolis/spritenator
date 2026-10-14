// =============================================================================
// ProjectFile.cpp
// =============================================================================
#include "ProjectFile.h"

#include "IoError.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QSaveFile>

#include <algorithm>
#include <cstring>
#include <variant>

namespace spritenator::io {

namespace {

constexpr char kMagic[] = { 'S', 'P', 'R', 'N' };
constexpr quint8 kIndexedTag = 0;
constexpr quint8 kRgbaTag = 1;
// Limites de sanidade (arquivos corrompidos não devem pedir memória absurda).
constexpr quint32 kMaxLayers = 1000;
constexpr quint32 kMaxFrames = 10000;

QString tr(const char* text)
{
    return QCoreApplication::translate("spritenator::io::ProjectFile", text);
}

[[noreturn]] void invalid(const QString& path)
{
    throw IoError(QCoreApplication::translate(
        "spritenator::io::ProjectFile", "\"%1\" is not a valid Spritenator project or is damaged.")
            .arg(path));
}

/// Bytes crus dos pixels de uma imagem.
QByteArray rawBytes(const core::AnyImage& image)
{
    return std::visit(
        [](const auto& concrete) {
            const auto pixels = concrete.data();
            return QByteArray(reinterpret_cast<const char*>(pixels.data()),
                static_cast<qsizetype>(pixels.size_bytes()));
        },
        image);
}

} // namespace

void ProjectFile::save(const QString& path, const ProjectData& data)
{
    if (data.cels.empty() || data.cels.front().empty()) {
        throw IoError(tr("There is nothing to save."));
    }
    const core::AnyImage& sample = data.cels.front().front();
    const core::Size size = core::sizeOf(sample);

    QByteArray bytes;
    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream.writeRawData(kMagic, sizeof(kMagic));
        stream << quint32(kVersion);
        stream << (core::modeOf(sample) == core::ColorMode::Rgba ? kRgbaTag : kIndexedTag);
        stream << qint32(size.width) << qint32(size.height);
        stream << quint8(core::bitsPerPixelOf(data.palette.depth()));
        for (int index = 0; index < data.palette.size(); ++index) {
            const core::Rgb& color = data.palette.color(index);
            stream << quint8(color.red) << quint8(color.green) << quint8(color.blue);
        }
        stream << quint32(data.layers.size());
        for (const LayerData& layer : data.layers) {
            stream << layer.name << layer.visible << quint8(layer.opacity);
        }
        stream << quint32(data.durations.size());
        for (const int duration : data.durations) {
            stream << qint32(duration);
        }
        for (const auto& row : data.cels) {
            for (const core::AnyImage& cel : row) {
                stream << qCompress(rawBytes(cel), 9);
            }
        }
    }

    QSaveFile file(path); // grava num temporário e troca no fim: sem arquivo pela metade
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        throw IoError(tr("Cannot write \"%1\": %2").arg(path, file.errorString()));
    }
}

ProjectData ProjectFile::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw IoError(tr("Cannot open \"%1\": %2").arg(path, file.errorString()));
    }
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);

    char magic[sizeof(kMagic)] = { };
    quint32 version = 0;
    quint8 mode = 0;
    qint32 width = 0;
    qint32 height = 0;
    quint8 bits = 0;
    if (stream.readRawData(magic, sizeof(magic)) != sizeof(magic)
        || !std::equal(std::begin(magic), std::end(magic), std::begin(kMagic))) {
        invalid(path);
    }
    stream >> version >> mode >> width >> height >> bits;
    if (stream.status() != QDataStream::Ok) {
        invalid(path);
    }
    if (version > kVersion) {
        throw IoError(tr("\"%1\" was saved by a newer version of Spritenator.").arg(path));
    }
    constexpr int kLimit = core::IndexedImage::kMaxDimension;
    if ((mode != kIndexedTag && mode != kRgbaTag) || width <= 0 || height <= 0 || width > kLimit
        || height > kLimit || (bits != 1 && bits != 4 && bits != 8)) {
        invalid(path);
    }

    ProjectData data;
    const core::ColorDepth depth = bits == 1 ? core::ColorDepth::Bits1
        : bits == 4                          ? core::ColorDepth::Bits4
                                             : core::ColorDepth::Bits8;
    data.palette = core::Palette::makeDefault(depth);
    for (int index = 0; index < data.palette.size(); ++index) {
        quint8 red = 0;
        quint8 green = 0;
        quint8 blue = 0;
        stream >> red >> green >> blue;
        data.palette.setColor(index, { red, green, blue });
    }

    quint32 layerCount = 0;
    stream >> layerCount;
    if (layerCount == 0 || layerCount > kMaxLayers) {
        invalid(path);
    }
    for (quint32 index = 0; index < layerCount; ++index) {
        LayerData layer;
        quint8 opacity = 255;
        stream >> layer.name >> layer.visible >> opacity;
        layer.opacity = opacity;
        data.layers.push_back(layer);
    }
    quint32 frameCount = 0;
    stream >> frameCount;
    if (frameCount == 0 || frameCount > kMaxFrames || stream.status() != QDataStream::Ok) {
        invalid(path);
    }
    for (quint32 index = 0; index < frameCount; ++index) {
        qint32 duration = 0;
        stream >> duration;
        data.durations.push_back(duration);
    }

    const auto colorMode = mode == kRgbaTag ? core::ColorMode::Rgba : core::ColorMode::Indexed;
    for (quint32 frame = 0; frame < frameCount; ++frame) {
        std::vector<core::AnyImage> row;
        for (quint32 layer = 0; layer < layerCount; ++layer) {
            QByteArray compressed;
            stream >> compressed;
            const QByteArray raw = qUncompress(compressed);
            core::AnyImage image = core::blankImage(colorMode, { width, height });
            const bool complete = std::visit(
                [&](auto& concrete) {
                    auto pixels = concrete.data();
                    if (raw.size() != static_cast<qsizetype>(pixels.size_bytes())) {
                        return false;
                    }
                    std::memcpy(pixels.data(), raw.constData(), pixels.size_bytes());
                    return true;
                },
                image);
            if (!complete || stream.status() != QDataStream::Ok) {
                invalid(path);
            }
            // Índices fora da paleta indicam arquivo danificado.
            if (const auto* indexed = std::get_if<core::IndexedImage>(&image)) {
                const auto pixels = indexed->data();
                if (std::any_of(pixels.begin(), pixels.end(),
                        [&](core::ColorIndex value) { return value >= data.palette.size(); })) {
                    invalid(path);
                }
            }
            row.push_back(std::move(image));
        }
        data.cels.push_back(std::move(row));
    }
    return data;
}

} // namespace spritenator::io
