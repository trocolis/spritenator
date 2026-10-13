// =============================================================================
// DrawBuffer.cpp
// =============================================================================
#include "DrawBuffer.h"

#include "ImageRendering.h"
#include "commands/ImageCommands.h"

#include <QUndoStack>

namespace spritenator::gui {

DrawBuffer::DrawBuffer(core::Size size, QObject* parent)
    : QObject(parent)
    , m_image(size, 0)
    , m_palette(core::Palette::makeDefault(core::ColorDepth::Bits8))
    , m_undoStack(new QUndoStack(this))
{
    m_rendered = renderIndexed(m_image, m_palette);
}

DrawBuffer::~DrawBuffer() = default;

void DrawBuffer::setPalette(const core::Palette& palette)
{
    m_palette = palette;
    m_rendered.setColorTable(colorTableOf(m_palette));
    emit changed(m_image.bounds());
}

void DrawBuffer::replace(const core::IndexedImage& image, const UndoLabel& undoLabel)
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
    const core::Rect area = edit(m_image).intersected(m_image.bounds());
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
    m_image.paste(m_snapshot.copy(m_dirty), m_dirty.topLeft());
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
    core::IndexedImage before = m_snapshot.copy(m_dirty);
    core::IndexedImage after = m_image.copy(m_dirty);
    m_snapshot = { }; // libera memória
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
    m_snapshot = { };
    m_dirty = { };
}

// -----------------------------------------------------------------------------
// Seleção
// -----------------------------------------------------------------------------
void DrawBuffer::setSelection(const std::optional<core::Rect>& selection)
{
    std::optional<core::Rect> clipped;
    if (selection) {
        const core::Rect area = selection->intersected(m_image.bounds());
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
    return m_selection.value_or(m_image.bounds());
}

// -----------------------------------------------------------------------------
// ImageTarget
// -----------------------------------------------------------------------------
void DrawBuffer::applyRegion(core::Point origin, const core::IndexedImage& pixels)
{
    m_image.paste(pixels, origin);
    notifyChanged(
        core::Rect::fromPointAndSize(origin, pixels.size()).intersected(m_image.bounds()));
}

void DrawBuffer::applyImage(const core::IndexedImage& image)
{
    const bool resizedNow = image.size() != m_image.size();
    m_image = image;
    m_rendered = renderIndexed(m_image, m_palette);
    if (resizedNow) {
        setSelection(std::nullopt); // a seleção antiga pode não caber mais
        emit resized(m_image.size());
    }
    emit changed(m_image.bounds());
}

void DrawBuffer::notifyChanged(const core::Rect& area)
{
    updateRenderedRegion(m_rendered, m_image, area);
    emit changed(area);
}

} // namespace spritenator::gui
