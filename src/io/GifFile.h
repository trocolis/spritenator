// =============================================================================
// GifFile.h
//
// GIF animado: gravação (codificador próprio — o Qt só lê GIF) e leitura das
// cenas como um documento.
//
// Gravação (GIF89a): uma paleta global; laço infinito (extensão NETSCAPE2.0);
// a duração de cada cena em centésimos de segundo. Imagens indexadas usam a
// própria paleta, sem perdas. Imagens RGBA são reduzidas a 255 cores (corte
// mediano) e a transparência vira 1 bit: alfa abaixo de 128 = transparente.
// =============================================================================
#pragma once

#include "ProjectFile.h"

#include "core/AnyImage.h"
#include "core/Palette.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace spritenator::io {

/// Leitura e gravação de GIF animado.
class GifFile final {
public:
    GifFile() = delete;

    /// Grava as cenas (todas do mesmo tamanho e modo). "durations" em ms.
    /// Lança IoError em caso de falha.
    static void save(const QString& path, const std::vector<core::AnyImage>& frames,
        const std::vector<int>& durations, const core::Palette& palette);

    /// Codifica as cenas em bytes de um GIF (exposto para testes).
    [[nodiscard]] static QByteArray encode(const std::vector<core::AnyImage>& frames,
        const std::vector<int>& durations, const core::Palette& palette);

    /// Lê as cenas de um GIF (RGBA, uma camada). Lança IoError se falhar.
    [[nodiscard]] static ProjectData load(const QString& path);
};

} // namespace spritenator::io
