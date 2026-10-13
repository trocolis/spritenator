// =============================================================================
// DrawBuffer.cpp
// =============================================================================
#include "DrawBuffer.h"

#include "ImageRendering.h"
#include "commands/ImageCommands.h"

#include <QUndoStack>

#include <cassert>
#include <variant>

namespace spritenator::gui {

DrawBuffer::DrawBuffer(core::Size size, QObject* parent)
    : QObject(parent)
    , m_image(core::IndexedImage(size, 0))
    , m_palette(core::Palette::makeDefault(core::ColorDepth::Bits8))
    , m_undoStack(new QUndoStack(this))
{
    m_rendered = renderImage(m_image, m_palette);
}

const core::IndexedImage& DrawBuffer::indexedImage() const
{
    assert(colorMode() == core::ColorMode::Indexed);
    return std::get<core::IndexedImage>(m_image);
}

void DrawBuffer::convertTo(core::ColorMode mode, const core::Palette& palette)
{
    if (mode == colorMode()) {
        return;
    }
    if (m_editing) {
        cancelEdit();
    }
    m_undoStack->clear(); // passos antigos são do outro modo de cor
    m_palette = palette;
    m_image = core::convertImage(m_image, mode, palette);
    m_rendered = renderImage(m_image, m_palette);
    emit colorModeChanged(mode);
    emit changed(core::boundsOf(m_image));
}

DrawBuffer::~DrawBuffer() = default;

void DrawBuffer::setPalette(const core::Palette& palette)
{
    m_palette = palette;
    if (colorMode() == core::ColorMode::Indexed) { // RGBA não usa tabela de cores
        m_rendered.setColorTable(colorTableOf(m_palette));
    }
    emit changed(core::boundsOf(m_image));
}

void DrawBuffer::replace(const core::AnyImage& image, const UndoLabel& undoLabel)
{
    if (m_editing) {
        cancelEdit();
    }
    m_undoStack->push(new ReplaceImageCommand(this, m_image, image, undoLabel));
}

// -----------------------------------------------------------------------------
// Edição ao vivo
// -----------------------------------------------------------------------------
void DrawBuffer::beginEdit()
{
    m_snapshot = m_image;
    m_dirty = { };
    m_editing = true;
}

void DrawBuffer::editLive(const LiveEdit& edit)
{
    if (!m_editing) {
        beginEdit(); // tolerância: permite editar sem chamar beginEdit
    }
    const core::Rect area = edit(m_image).intersected(core::boundsOf(m_image));
    if (area.isEmpty()) {
        return;
    }
    m_dirty = m_dirty.united(area);
    notifyChanged(area);
}

void DrawBuffer::restoreSnapshot()
{
    if (!m_editing || m_dirty.isEmpty()) {
        return;
    }
    // Copia de volta apenas a área tocada até agora.
    core::pasteImage(m_image, core::copyRegion(m_snapshot, m_dirty), m_dirty.topLeft());
    notifyChanged(m_dirty);
}

void DrawBuffer::commitEdit(const UndoLabel& undoLabel)
{
    if (!m_editing) {
        return;
    }
    m_editing = false;
    if (m_dirty.isEmpty()) {
        return;
    }
    core::AnyImage before = core::copyRegion(m_snapshot, m_dirty);
    core::AnyImage after = core::copyRegion(m_image, m_dirty);
    m_snapshot = core::emptyImage(colorMode()); // libera memória
    if (before == after) {
        return; // ex.: traço desenhado com a mesma cor que já estava lá
    }
    // A edição já está aplicada: o comando não deve reaplicá-la no push.
    m_undoStack->push(new RegionCommand(
        this, m_dirty.topLeft(), std::move(before), std::move(after), undoLabel, true));
}

void DrawBuffer::cancelEdit()
{
    restoreSnapshot();
    m_editing = false;
    m_snapshot = core::emptyImage(colorMode());
    m_dirty = { };
}

// -----------------------------------------------------------------------------
// Seleção
// -----------------------------------------------------------------------------
void DrawBuffer::setSelection(const std::optional<core::Rect>& selection)
{
    std::optional<core::Rect> clipped;
    if (selection) {
        const core::Rect area = selection->intersected(core::boundsOf(m_image));
        if (!area.isEmpty()) {
            clipped = area;
        }
    }
    if (clipped != m_selection) {
        m_selection = clipped;
        emit selectionChanged();
    }
}

core::Rect DrawBuffer::workArea() const
{
    return m_selection.value_or(core::boundsOf(m_image));
}

// -----------------------------------------------------------------------------
// ImageTarget
// -----------------------------------------------------------------------------
void DrawBuffer::applyRegion(core::Point origin, const core::AnyImage& pixels)
{
    core::pasteImage(m_image, pixels, origin);
    notifyChanged(core::Rect::fromPointAndSize(origin, core::sizeOf(pixels))
            .intersected(core::boundsOf(m_image)));
}

void DrawBuffer::applyImage(const core::AnyImage& image)
{
    const bool resizedNow = core::sizeOf(image) != size();
    m_image = image;
    m_rendered = renderImage(m_image, m_palette);
    if (resizedNow) {
        setSelection(std::nullopt); // a seleção antiga pode não caber mais
        emit resized(size());
    }
    emit changed(core::boundsOf(m_image));
}

void DrawBuffer::notifyChanged(const core::Rect& area)
{
    updateRenderedRegion(m_rendered, m_image, area);
    emit changed(area);
}

} // namespace spritenator::gui
