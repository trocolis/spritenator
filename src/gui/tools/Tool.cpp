// =============================================================================
// Tool.cpp — implementações padrão da interface Tool.
// =============================================================================
#include "Tool.h"

#include "gui/Document.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

void Tool::cancel(EditorSession& session)
{
    // Comportamento padrão: descarta a edição ao vivo em andamento, se houver.
    Document* document = session.activeDocument();
    if (document != nullptr && document->isEditing()) {
        document->cancelEdit();
    }
}

std::vector<core::Point> Tool::footprint(const EditorSession& /*session*/, core::Point point) const
{
    return { point };
}

} // namespace spritenator::gui
