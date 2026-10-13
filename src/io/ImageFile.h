// =============================================================================
// ImageFile.h
//
// Leitura e gravação de imagens em disco. Formatos suportados:
//   - BMP (1, 4, 8, 24 e 32 bits na leitura; 1, 4 e 8 bits na gravação)
//   - PNG (qualquer variante na leitura; indexada ou RGBA na gravação)
//
// Na leitura, arquivos indexados (com paleta) viram imagens indexadas e
// arquivos em cores reais viram imagens RGBA, sem perdas. Imagens RGBA só
// podem ser gravadas em PNG: o BMP gravado pelo Qt descarta o alfa.
//
// loadIndexed() força o resultado indexado (usado, por exemplo, para ler a
// paleta de uma imagem): cores reais são reduzidas a uma paleta exata, se
// houver até 256 cores, ou a 256 cores com pontilhamento.
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/Image.h"
#include "core/Palette.h"

#include <QString>
#include <QStringList>

class QImage;

namespace spritenator::io {

/// Formatos de arquivo de imagem suportados.
enum class ImageFormat {
    Bmp,
    Png,
};

/// Resultado da leitura de uma imagem: pixels (indexados ou RGBA) + paleta.
/// Em imagens RGBA a paleta não descreve os pixels (fica a paleta padrão).
struct ImageData {
    core::AnyImage image;
    core::Palette palette;

    [[nodiscard]] core::ColorMode colorMode() const { return core::modeOf(image); }
};

/// Funções de leitura/gravação de imagens (classe puramente estática).
class ImageFile final {
public:
    ImageFile() = delete;

    /// Lê a imagem do caminho informado. Lança IoError em caso de falha.
    [[nodiscard]] static ImageData load(const QString& path);

    /// Lê a imagem sempre como indexada (cores reais são reduzidas).
    [[nodiscard]] static ImageData loadIndexed(const QString& path);

    /// Grava a imagem no formato deduzido pela extensão do caminho
    /// (.bmp ou .png). Lança IoError em caso de falha.
    static void save(const QString& path, const ImageData& data);

    /// Grava a imagem no formato informado. Lança IoError em caso de falha.
    static void save(const QString& path, const ImageData& data, ImageFormat format);

    /// Deduz o formato pela extensão. Lança IoError se não for suportada.
    [[nodiscard]] static ImageFormat formatFromPath(const QString& path);

    /// Filtros para diálogos de arquivo (ex.: "PNG image (*.png)"). Para
    /// imagens RGBA só o PNG é oferecido na gravação.
    [[nodiscard]] static QStringList openFilters();
    [[nodiscard]] static QStringList saveFilters(core::ColorMode mode = core::ColorMode::Indexed);

    /// Converte um QImage qualquer: indexado -> indexada; demais -> RGBA.
    [[nodiscard]] static ImageData fromQImage(const QImage& source);

    /// Converte um QImage qualquer sempre em dados indexados.
    [[nodiscard]] static ImageData fromQImageIndexed(const QImage& source);

    /// Converte em QImage: Format_Indexed8 (indexada) ou Format_ARGB32 (RGBA).
    [[nodiscard]] static QImage toQImage(const ImageData& data);
};

} // namespace spritenator::io
