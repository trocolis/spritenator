// =============================================================================
// SelectionTransform.cpp
// =============================================================================
#include "SelectionTransform.h"

#include "Document.h"
#include "EditorSession.h"

#include <variant>

namespace spritenator::gui {

SelectionTransform::SelectionTransform(EditorSession* session)
    : QObject(session)
    , m_session(session)
{
}

bool SelectionTransform::isActive() const
{
    return m_document != nullptr;
}

Document* SelectionTransform::document() const
{
    return m_document.data();
}

bool SelectionTransform::begin()
{
    Document* document = m_session->activeDocument();
    if (isActive()) {
        if (m_document == document) {
            return true; // continua a transformação em andamento
        }
        commit(); // outra imagem: aplica a anterior antes
    }
    // Camada bloqueada: os pixels não podem ser movidos.
    if (document == nullptr || !document->hasSelection() || !document->canEditActiveCel()) {
        return false;
    }
    m_session->cancelTool();
    m_document = document;
    m_original = document->selection();
    m_content = core::copyRegion(document->canvas(), m_original.bounds());
    m_params = { };
    document->beginEdit();
    emit changed();
    return true;
}

core::Affine SelectionTransform::affine() const
{
    return core::affineFor(m_params, m_original.bounds());
}

void SelectionTransform::setParams(const core::TransformParams& params)
{
    if (!isActive() || params == m_params) {
        return;
    }
    m_params = params;
    render();
    emit changed();
}

void SelectionTransform::render()
{
    Document* document = m_document.data();
    if (document == nullptr) {
        return;
    }
    const core::Affine transform = affine();
    const core::Rect canvas = core::boundsOf(document->canvas());
    core::Selection moved;

    document->restoreSnapshot(); // volta ao estado do início
    document->editLive([&](core::AnyImage& image) {
        return std::visit(
            [&](auto& target) -> core::Rect {
                using ImageType = std::decay_t<decltype(target)>;
                using Pixel = typename ImageType::PixelType;
                // 1. Ergue os pixels: o lugar deles recebe o pixel de fundo.
                Pixel hole { };
                if constexpr (std::is_same_v<Pixel, core::ColorIndex>) {
                    hole = m_session->backgroundColor();
                }
                for (const core::Point point : m_original.points()) {
                    target.setPixel(point, hole);
                }
                // 2. Escreve os pixels transformados.
                const auto floating = core::transformFloating(
                    std::get<ImageType>(m_content), m_original, transform, canvas);
                moved = floating.mask;
                for (const core::Point point : moved.points()) {
                    const core::Point local = point - moved.bounds().topLeft();
                    target.setPixel(point, floating.pixels.pixel(local.x, local.y));
                }
                return m_original.bounds().united(moved.bounds());
            },
            image);
    });
    document->setSelection(moved);
}

void SelectionTransform::translate(double dx, double dy)
{
    core::TransformParams params = m_params;
    params.dx += dx;
    params.dy += dy;
    setParams(params);
}

void SelectionTransform::rotate90(bool clockwise)
{
    rotateBy(clockwise ? 90 : -90);
}

void SelectionTransform::rotateBy(double degrees)
{
    core::TransformParams params = m_params;
    params.angle = std::fmod(params.angle + degrees, 360.0);
    setParams(params);
}

void SelectionTransform::flip(bool horizontal)
{
    // Espelhar na tela depois de girar: F·R(a) = R(-a)·F. Assim o espelho é
    // sempre "esquerda <-> direita" (ou "cima <-> baixo") como se vê.
    core::TransformParams params = m_params;
    params.angle = -params.angle;
    if (horizontal) {
        params.sx = -params.sx;
    } else {
        params.sy = -params.sy;
    }
    setParams(params);
}

void SelectionTransform::scaleTo(core::Size size)
{
    const core::Rect bounds = m_original.bounds();
    if (size.isEmpty() || bounds.isEmpty()) {
        return;
    }
    core::TransformParams params = m_params;
    params.sx = (params.sx < 0 ? -1.0 : 1.0) * size.width / bounds.width;
    params.sy = (params.sy < 0 ? -1.0 : 1.0) * size.height / bounds.height;
    setParams(params);
}

void SelectionTransform::commit(UndoOperation operation)
{
    Document* document = m_document.data();
    if (document == nullptr) {
        return;
    }
    m_document = nullptr;
    document->commitEdit(UndoLabel { operation });
    m_content = core::AnyImage { };
    emit changed();
}

void SelectionTransform::cancel()
{
    Document* document = m_document.data();
    if (document == nullptr) {
        return;
    }
    m_document = nullptr;
    document->cancelEdit();
    document->setSelection(m_original);
    m_content = core::AnyImage { };
    emit changed();
}

} // namespace spritenator::gui
