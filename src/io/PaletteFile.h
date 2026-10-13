// =============================================================================
// PaletteFile.h
//
// Leitura e gravação de arquivos de paleta. Formatos suportados:
//   - Microsoft RIFF Palette (.pal): binário, usado pelo Windows e pelo
//     próprio Character Maker 1999;
//   - JASC-PAL (.pal): texto, usado pelo Paint Shop Pro e outros editores;
//   - Adobe Color Table (.act): binário, 256 entradas RGB;
//   - Imagens BMP/PNG indexadas: somente leitura (a paleta da imagem).
// =============================================================================
#pragma once

#include "core/Color.h"
#include "core/Palette.h"

#include <QString>
#include <QStringList>

#include <vector>

class QByteArray;

namespace spritenator::io {

/// Formatos de arquivo de paleta para gravação.
enum class PaletteFormat {
    RiffPal, ///< Microsoft RIFF "PAL " (.pal)
    JascPal, ///< JASC-PAL em texto (.pal)
    Act, ///< Adobe Color Table (.act)
};

/// Funções de leitura/gravação de paletas (classe puramente estática).
class PaletteFile final {
public:
    PaletteFile() = delete;

    /// Lê as cores do arquivo, detectando o formato pelo conteúdo. A lista
    /// pode ter menos de 256 cores. Lança IoError em caso de falha.
    [[nodiscard]] static std::vector<core::Rgb> load(const QString& path);

    /// Interpreta o conteúdo bruto de um arquivo de paleta (exposto para testes).
    [[nodiscard]] static std::vector<core::Rgb> parse(const QByteArray& contents);

    /// Grava as cores válidas da paleta no formato informado.
    static void save(const QString& path, const core::Palette& palette, PaletteFormat format);

    /// Codifica a paleta em memória no formato informado (exposto para testes).
    [[nodiscard]] static QByteArray encode(const core::Palette& palette, PaletteFormat format);

    /// Filtros para diálogos de arquivo. A ordem de saveFilters() corresponde
    /// à ordem dos valores de PaletteFormat.
    [[nodiscard]] static QStringList openFilters();
    [[nodiscard]] static QStringList saveFilters();

    /// Formato correspondente ao índice do filtro escolhido em saveFilters().
    [[nodiscard]] static PaletteFormat formatFromFilterIndex(int index);
};

} // namespace spritenator::io
