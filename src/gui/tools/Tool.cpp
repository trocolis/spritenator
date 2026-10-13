// =============================================================================
// Tool.cpp — implementações padrão da interface Tool.
// =============================================================================
#include "Tool.h"

#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"

namespace spritenator::gui {

void Tool::cancel(EditorSession& session)
{
    // Comportamento padrão: descarta a edição ao vivo em andamento, se houver.
    if (session.buffer()->isEditing()) {
        session.buffer()->cancelEdit();
    }
}

void Tool::paintOverlay(QPainter& /*painter*/, int /*scale*/) const
{
    // A maioria das ferramentas não desenha nada além do próprio buffer.
}

} // namespace spritenator::gui
