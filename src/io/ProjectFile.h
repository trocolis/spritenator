// =============================================================================
// ProjectFile.h
//
// Projeto Spritenator (.sprn): o formato próprio, que guarda TUDO — modo de
// cor, paleta, camadas (nome, visibilidade, opacidade), cenas (duração) e a
// imagem de cada célula. É o único formato que reabre sem perdas.
//
// Estrutura (QDataStream, big-endian):
//   "SPRN" | versão | modo | largura | altura | paleta (bits + cores RGB)
//   | camadas (nome, visível, opacidade, bloqueada*) | durações das cenas
//   (* desde a versão 2)
//   | células [cena][camada]: bytes dos pixels compactados (qCompress)
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/Palette.h"

#include <QString>

#include <vector>

namespace spritenator::io {

/// Uma camada, como gravada no arquivo.
struct LayerData {
    QString name;
    bool visible = true;
    int opacity = 255;
    bool locked = false; ///< bloqueada para edição (versão 2 do formato)
};

/// Conteúdo de um projeto (ou de um GIF importado): todas as células têm o
/// mesmo tamanho e o mesmo modo de cor.
struct ProjectData {
    core::Palette palette;
    std::vector<LayerData> layers; ///< de baixo para cima
    std::vector<int> durations; ///< duração de cada cena, em ms
    std::vector<std::vector<core::AnyImage>> cels; ///< [cena][camada]
};

/// Leitura e gravação de projetos .sprn.
class ProjectFile final {
public:
    ProjectFile() = delete;

    /// Versão atual do formato (2: camadas com cadeado).
    static constexpr int kVersion = 2;

    /// Lê um projeto. Lança IoError se o arquivo for inválido.
    [[nodiscard]] static ProjectData load(const QString& path);
    /// Grava um projeto. Lança IoError em caso de falha.
    static void save(const QString& path, const ProjectData& data);
};

} // namespace spritenator::io
