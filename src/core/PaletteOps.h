// =============================================================================
// PaletteOps.h
//
// Operações que envolvem paleta E pixels ao mesmo tempo. Numa imagem
// indexada, mexer na paleta sem cuidado muda a aparência da imagem; estas
// funções produzem "tabelas de remapeamento" (índice antigo -> índice novo)
// para que a imagem continue com a mesma aparência (ou a mais próxima).
// =============================================================================
#pragma once

#include "Image.h"
#include "Palette.h"

#include <array>
#include <vector>

namespace spritenator::core {

/// Tabela de remapeamento: posição = índice antigo, valor = índice novo.
using RemapTable = std::array<ColorIndex, 256>;

/// Tabela identidade (nenhuma mudança).
[[nodiscard]] RemapTable identityRemap();

/// Conjunto com todos os índices válidos da paleta.
[[nodiscard]] ColorSet allColorsOf(const Palette& palette);

/// Índices reservados pelo Windows na paleta de 256 cores (0-9 e 246-255),
/// que o original permitia não usar ao remapear ("20 cores do sistema").
[[nodiscard]] ColorSet systemColors();

/// Para cada cor da paleta "from", o índice da cor mais parecida em "to",
/// considerando apenas os índices em "targets" (vazio = todos).
[[nodiscard]] RemapTable nearestRemap(
    const Palette& from, const Palette& to, const ColorSet& targets);

/// Converte a imagem (cores de "from") para a paleta "to" com pontilhamento
/// por difusão de erro de Floyd–Steinberg. Pixels com índice em "keep"
/// (ex.: cor transparente) são mantidos sem alteração e não espalham erro.
[[nodiscard]] IndexedImage ditherToPalette(const IndexedImage& image, const Palette& from,
    const Palette& to, const ColorSet& targets, const ColorSet& keep = { });

/// Resultado de removeUnusedColors.
struct CompactResult {
    Palette palette; ///< paleta com as cores usadas no início
    RemapTable remap; ///< como atualizar os pixels
    int removedCount = 0;
};

/// Junta as cores efetivamente usadas no início da paleta (mantendo a ordem)
/// e zera o restante; "used" normalmente vem de IndexedImage::usedColors().
[[nodiscard]] CompactResult removeUnusedColors(const Palette& palette, const ColorSet& used);

/// Tabela que troca os índices "a" e "b" (usada ao trocar duas cores de lugar
/// na paleta sem alterar a aparência da imagem).
[[nodiscard]] RemapTable swapRemap(int a, int b);

/// Copia "count" cores de "colors" (a partir de "sourceStart") para a paleta,
/// começando no índice "destination". Entradas que ultrapassem a paleta ou a
/// lista são ignoradas. Equivale a "posição de leitura" do diálogo original.
[[nodiscard]] Palette mergeColors(const Palette& palette, const std::vector<Rgb>& colors,
    int sourceStart, int count, int destination);

/// Como os pixels são tratados ao trocar a paleta de uma imagem.
enum class RemapMethod {
    KeepIndices, ///< nada muda nos pixels (prioriza o número da cor)
    NearestColor, ///< cada cor antiga vira a mais próxima da paleta nova
    Dither, ///< reproduz as cores antigas com difusão de erro
};

/// Opções do diálogo "Carregar paleta".
struct PaletteApplyOptions {
    RemapMethod method = RemapMethod::NearestColor;
    /// Não usar as 20 cores do sistema como destino (só em 256 cores).
    bool avoidSystemColors = true;
    /// O índice 0 é a cor transparente: mantido e nunca usado como destino.
    bool keepTransparentZero = true;
};

/// Converte os pixels de "image" (cores de "oldPalette") para "newPalette".
[[nodiscard]] IndexedImage applyPalette(const IndexedImage& image, const Palette& oldPalette,
    const Palette& newPalette, const PaletteApplyOptions& options);

} // namespace spritenator::core
