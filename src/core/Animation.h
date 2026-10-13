// =============================================================================
// Animation.h
//
// Modelo de uma animação de prévia: uma sequência de quadros (retângulos do
// canvas) exibidos em laço com um intervalo fixo. Corresponde à janela
// "Animate Cartoon" (TFormAnimation) do original.
// =============================================================================
#pragma once

#include "Geometry.h"

#include <cstddef>
#include <vector>

namespace spritenator::core {

/// Sequência de quadros de animação.
class Animation {
public:
    /// Limites do intervalo entre quadros (os mesmos do original).
    static constexpr int kMinWaitMs = 50;
    static constexpr int kMaxWaitMs = 1000;
    static constexpr int kDefaultWaitMs = 200;

    [[nodiscard]] const std::vector<Rect>& frames() const { return m_frames; }
    [[nodiscard]] bool isEmpty() const { return m_frames.empty(); }
    [[nodiscard]] std::size_t size() const { return m_frames.size(); }

    void addFrame(const Rect& frame) { m_frames.push_back(frame); }
    void addFrames(const std::vector<Rect>& frames);
    /// Remove o quadro na posição (ignorado se inválida).
    void removeFrame(std::size_t position);
    void clear() { m_frames.clear(); }

    /// Intervalo entre quadros, sempre limitado a [kMinWaitMs, kMaxWaitMs].
    [[nodiscard]] int waitMs() const { return m_waitMs; }
    void setWaitMs(int milliseconds);

    /// Posição do quadro seguinte (volta ao início após o último).
    [[nodiscard]] std::size_t nextPosition(std::size_t current) const;

private:
    std::vector<Rect> m_frames;
    int m_waitMs = kDefaultWaitMs;
};

/// Quadros do ciclo de caminhada do RPG Maker: cada direção ocupa
/// "framesPerDirection" (3) células lado a lado e a caminhada é 1-2-3-2
/// (passo, parado, outro passo, parado). Recebe qualquer célula do bloco.
[[nodiscard]] std::vector<Rect> walkCycleFrames(const Rect& cell, int framesPerDirection = 3);

} // namespace spritenator::core
