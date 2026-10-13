// =============================================================================
// ImageFile.h
//
// Leitura e gravação de imagens indexadas em disco. Formatos suportados:
//   - BMP (1, 4, 8, 24 e 32 bits na leitura; 1, 4 e 8 bits na gravação)
//   - PNG (qualquer variante na leitura; indexada na gravação)
//
// Imagens em cores reais (sem paleta) são convertidas na leitura: se tiverem
// até 256 cores distintas, a paleta é montada sem perdas; caso contrário a
// imagem é reduzida para 256 cores com pontilhamento por difusão de erro.
// =============================================================================
#pragma once

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

/// Resultado da leitura de uma imagem: pixels indexados + paleta.
struct ImageData {
    core::IndexedImage image;
    core::Palette palette;
};

/// Funções de leitura/gravação de imagens (classe puramente estática).
class ImageFile final {
public:
    ImageFile() = delete;

    /// Lê a imagem do caminho informado. Lança IoError em caso de falha.
    [[nodiscard]] static ImageData load(const QString& path);

    /// Grava a imagem no formato deduzido pela extensão do caminho
    /// (.bmp ou .png). Lança IoError em caso de falha.
    static void save(const QString& path, const ImageData& data);

    /// Grava a imagem no formato informado. Lança IoError em caso de falha.
    static void save(const QString& path, const ImageData& data, ImageFormat format);

    /// Deduz o formato pela extensão. Lança IoError se não for suportada.
    [[nodiscard]] static ImageFormat formatFromPath(const QString& path);

    /// Filtros para diálogos de arquivo (ex.: "PNG image (*.png)").
    [[nodiscard]] static QStringList openFilters();
    [[nodiscard]] static QStringList saveFilters();

    /// Converte um QImage qualquer em dados indexados (exposto para que o
    /// colar da área de transferência reutilize a mesma lógica).
    [[nodiscard]] static ImageData fromQImage(const QImage& source);

    /// Converte dados indexados em QImage (Format_Indexed8).
    [[nodiscard]] static QImage toQImage(const ImageData& data);
};

} // namespace spritenator::io
