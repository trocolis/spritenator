// =============================================================================
// Animation.cpp
// =============================================================================
#include "Animation.h"

#include <algorithm>

namespace spritenator::core {

void Animation::addFrames(const std::vector<Rect>& frames)
{
    m_frames.insert(m_frames.end(), frames.begin(), frames.end());
}

void Animation::removeFrame(std::size_t position)
{
    if (position < m_frames.size()) {
        m_frames.erase(m_frames.begin() + static_cast<std::ptrdiff_t>(position));
    }
}

void Animation::setWaitMs(int milliseconds)
{
    m_waitMs = std::clamp(milliseconds, kMinWaitMs, kMaxWaitMs);
}

std::size_t Animation::nextPosition(std::size_t current) const
{
    if (m_frames.empty()) {
        return 0;
    }
    return (current + 1) % m_frames.size();
}

std::vector<Rect> walkCycleFrames(const Rect& cell, int framesPerDirection)
{
    if (cell.isEmpty() || framesPerDirection < 1) {
        return { };
    }
    // Início do bloco da direção: arredonda a coluna para baixo ao múltiplo
    // de "framesPerDirection" células (ex.: colunas 0-2, 3-5, ...).
    const int column = cell.x / cell.width;
    const int blockStart = (column / framesPerDirection) * framesPerDirection * cell.width;
    const auto frameAt = [&](int offset) {
        return Rect { blockStart + offset * cell.width, cell.y, cell.width, cell.height };
    };
    if (framesPerDirection < 3) {
        std::vector<Rect> frames;
        for (int offset = 0; offset < framesPerDirection; ++offset) {
            frames.push_back(frameAt(offset));
        }
        return frames;
    }
    // Sequência clássica do RPG Maker 2000: esquerda, meio, direita, meio.
    return { frameAt(0), frameAt(1), frameAt(2), frameAt(1) };
}

} // namespace spritenator::core
