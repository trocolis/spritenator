// =============================================================================
// ImageCommands.cpp
// =============================================================================
#include "ImageCommands.h"

#include "gui/Document.h"
#include "gui/ImageTarget.h"

#include <cassert>

namespace spritenator::gui {

// -----------------------------------------------------------------------------
// LabeledCommand
// -----------------------------------------------------------------------------
LabeledCommand::LabeledCommand(const UndoLabel& label)
    : m_label(label)
{
    setText(m_label.text());
}

void LabeledCommand::retranslate()
{
    setText(m_label.text());
}

// -----------------------------------------------------------------------------
// RegionCommand
// -----------------------------------------------------------------------------
RegionCommand::RegionCommand(ImageTarget* target, core::Point origin, core::AnyImage before,
    core::AnyImage after, const UndoLabel& label, bool alreadyApplied)
    : LabeledCommand(label)
    , m_target(target)
    , m_origin(origin)
    , m_before(std::move(before))
    , m_after(std::move(after))
    , m_skipNextRedo(alreadyApplied)
{
    assert(core::sizeOf(m_before) == core::sizeOf(m_after));
    assert(m_before.index() == m_after.index()); // mesmo modo de cor
}

void RegionCommand::undo()
{
    m_target->applyRegion(m_origin, m_before);
}

void RegionCommand::redo()
{
    if (m_skipNextRedo) {
        m_skipNextRedo = false; // a edição já estava aplicada
        return;
    }
    m_target->applyRegion(m_origin, m_after);
}

// -----------------------------------------------------------------------------
// ReplaceImageCommand
// -----------------------------------------------------------------------------
ReplaceImageCommand::ReplaceImageCommand(
    ImageTarget* target, core::AnyImage before, core::AnyImage after, const UndoLabel& label)
    : LabeledCommand(label)
    , m_target(target)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void ReplaceImageCommand::undo()
{
    m_target->applyImage(m_before);
}

void ReplaceImageCommand::redo()
{
    m_target->applyImage(m_after);
}

// -----------------------------------------------------------------------------
// PaletteCommand
// -----------------------------------------------------------------------------
PaletteCommand::PaletteCommand(Document* document, core::Palette before, core::Palette after,
    const UndoLabel& label, int mergeKey)
    : LabeledCommand(label)
    , m_document(document)
    , m_before(std::move(before))
    , m_after(std::move(after))
    , m_mergeKey(mergeKey)
{
}

void PaletteCommand::undo()
{
    m_document->applyPalette(m_before);
}

void PaletteCommand::redo()
{
    m_document->applyPalette(m_after);
}

int PaletteCommand::id() const
{
    // -1 desativa a fusão para comandos sem chave.
    return m_mergeKey >= 0 ? kCommandId : -1;
}

bool PaletteCommand::mergeWith(const QUndoCommand* other)
{
    const auto* next = dynamic_cast<const PaletteCommand*>(other);
    if (next == nullptr || next->m_mergeKey != m_mergeKey || next->m_document != m_document) {
        return false;
    }
    // Mantém o "antes" original e adota o "depois" mais recente.
    m_after = next->m_after;
    return true;
}

// -----------------------------------------------------------------------------
// DocumentStateCommand
// -----------------------------------------------------------------------------
DocumentStateCommand::DocumentStateCommand(Document* document, core::AnyImage beforeImage,
    core::Palette beforePalette, core::AnyImage afterImage, core::Palette afterPalette,
    const UndoLabel& label)
    : LabeledCommand(label)
    , m_document(document)
    , m_beforeImage(std::move(beforeImage))
    , m_beforePalette(std::move(beforePalette))
    , m_afterImage(std::move(afterImage))
    , m_afterPalette(std::move(afterPalette))
{
}

void DocumentStateCommand::undo()
{
    m_document->applyPalette(m_beforePalette);
    m_document->applyCanvas(m_beforeImage);
}

void DocumentStateCommand::redo()
{
    m_document->applyPalette(m_afterPalette);
    m_document->applyCanvas(m_afterImage);
}

} // namespace spritenator::gui
