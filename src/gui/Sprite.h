// =============================================================================
// Sprite.h
//
// Estrutura de um documento animado (como no Aseprite):
//   - CAMADAS, comuns a todas as cenas (índice 0 = a de baixo);
//   - CENAS (quadros da linha do tempo), cada uma com sua duração;
//   - uma imagem (CÉLULA) para cada par cena x camada.
//
// As células são compartilhadas (std::shared_ptr): um "instantâneo" do
// documento (SpriteState) copia só ponteiros — barato o bastante para cada
// operação de linha do tempo guardar o estado inteiro no histórico. Antes de
// ALTERAR uma célula, o Document a separa (cópia ao escrever) se ela ainda é
// compartilhada com algum instantâneo.
//
// Transparência entre camadas: em RGBA, o alfa (e a opacidade da camada); em
// imagens indexadas, o índice 0 das camadas de cima deixa ver as de baixo (a
// camada de baixo mostra o índice 0 normalmente).
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/Palette.h"

#include <QString>

#include <memory>
#include <vector>

namespace spritenator::gui {

/// Camada: comum a todas as cenas.
struct Layer {
    int id = 0; ///< identificador estável (não muda ao reordenar)
    QString name;
    bool visible = true;
    int opacity = 255; ///< 0..255 (só tem efeito em documentos RGBA)

    friend bool operator==(const Layer&, const Layer&) = default;
};

/// Cena (quadro da linha do tempo).
struct Frame {
    int id = 0; ///< identificador estável
    int durationMs = 100;

    friend bool operator==(const Frame&, const Frame&) = default;
};

/// Duração mínima e máxima de uma cena (ms).
inline constexpr int kMinFrameDuration = 10;
inline constexpr int kMaxFrameDuration = 60000;

/// Célula compartilhada entre instantâneos.
using CelPtr = std::shared_ptr<core::AnyImage>;

/// Identifica uma célula pelos identificadores estáveis da cena e da camada.
struct CelKey {
    int frameId = 0;
    int layerId = 0;

    friend bool operator==(const CelKey&, const CelKey&) = default;
};

/// Instantâneo completo do conteúdo de um documento.
struct SpriteState {
    std::vector<Layer> layers; ///< de baixo para cima
    std::vector<Frame> frames; ///< em ordem
    std::vector<std::vector<CelPtr>> cels; ///< [cena][camada]
    core::Palette palette;
    int nextId = 1; ///< próximo identificador livre (cenas e camadas)
};

} // namespace spritenator::gui
