// =============================================================================
// ImageCommands.cpp
// =============================================================================
#include "ImageCommands.h"

#include "gui/Document.h"

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
RegionCommand::RegionCommand(Document* document, const CelKey& cel, core::Point origin,
    core::AnyImage before, core::AnyImage after, const UndoLabel& label, bool alreadyApplied)
    : LabeledCommand(label)
    , m_document(document)
    , m_cel(cel)
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
    m_document->applyCelRegion(m_cel, m_origin, m_before);
}

void RegionCommand::redo()
{
    if (m_skipNextRedo) {
        m_skipNextRedo = false; // a edição já estava aplicada
        return;
    }
    m_document->applyCelRegion(m_cel, m_origin, m_after);
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
// SpriteStateCommand
// -----------------------------------------------------------------------------
SpriteStateCommand::SpriteStateCommand(Document* document, SpriteState before, SpriteState after,
    const UndoLabel& label, bool mergeable)
    : LabeledCommand(label)
    , m_document(document)
    , m_mergeable(mergeable)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void SpriteStateCommand::undo()
{
    m_document->applyState(m_before);
}

void SpriteStateCommand::redo()
{
    m_document->applyState(m_after);
}

int SpriteStateCommand::id() const
{
    return m_mergeable ? kCommandId : -1;
}

bool SpriteStateCommand::mergeWith(const QUndoCommand* other)
{
    const auto* next = dynamic_cast<const SpriteStateCommand*>(other);
    if (next == nullptr || !next->m_mergeable || next->m_document != m_document
        || next->label().operation != label().operation) {
        return false;
    }
    m_after = next->m_after; // mantém o "antes" original
    return true;
}

} // namespace spritenator::gui
