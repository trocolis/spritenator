// =============================================================================
// EditorSession.cpp
// =============================================================================
#include "EditorSession.h"

#include "Document.h"
#include "DrawBuffer.h"
#include "commands/ImageCommands.h"
#include "tools/PenTool.h"
#include "tools/ShapeTool.h"
#include "tools/SimpleTools.h"

#include <QUndoGroup>
#include <QUndoStack>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>

namespace spritenator::gui {

EditorSession::EditorSession(QObject* parent)
    : QObject(parent)
    , m_undoGroup(new QUndoGroup(this))
    , m_defaultPalette(core::Palette::makeDefault(core::ColorDepth::Bits8))
{
    m_buffer = new DrawBuffer(m_layout.cellSize, this);
    trackUndoStack(m_buffer->undoStack());
    m_undoGroup->setActiveStack(m_buffer->undoStack());

    // Registro das ferramentas disponíveis (uma instância de cada).
    m_tools.emplace(ToolKind::Select, std::make_unique<SelectTool>());
    m_tools.emplace(ToolKind::Zoom, std::make_unique<ZoomTool>());
    m_tools.emplace(ToolKind::Fill, std::make_unique<FillTool>());
    m_tools.emplace(ToolKind::PenThin, std::make_unique<PenTool>(false));
    m_tools.emplace(ToolKind::PenThick, std::make_unique<PenTool>(true));
    m_tools.emplace(ToolKind::Line, std::make_unique<ShapeTool>(Shape::Line, false));
    m_tools.emplace(ToolKind::Rectangle, std::make_unique<ShapeTool>(Shape::Rectangle, false));
    m_tools.emplace(
        ToolKind::RoundRectangle, std::make_unique<ShapeTool>(Shape::RoundRectangle, false));
    m_tools.emplace(ToolKind::Ellipse, std::make_unique<ShapeTool>(Shape::Ellipse, false));
    m_tools.emplace(ToolKind::FilledRectangle, std::make_unique<ShapeTool>(Shape::Rectangle, true));
    m_tools.emplace(
        ToolKind::FilledRoundRectangle, std::make_unique<ShapeTool>(Shape::RoundRectangle, true));
    m_tools.emplace(ToolKind::FilledEllipse, std::make_unique<ShapeTool>(Shape::Ellipse, true));
}

EditorSession::~EditorSession() = default;

// -----------------------------------------------------------------------------
// Documento ativo e paleta
// -----------------------------------------------------------------------------
Document* EditorSession::activeDocument() const
{
    return m_activeDocument.data(); // nulo se o documento já foi fechado
}

void EditorSession::setActiveDocument(Document* document)
{
    if (document == m_activeDocument) {
        return;
    }
    if (m_activeDocument) {
        disconnect(m_activeDocument, nullptr, this, nullptr);
    }
    // Se o usuário estava desfazendo edições do documento anterior, passa a
    // desfazer as do novo; se estava no buffer, permanece no buffer.
    const bool documentStackWasActive = m_undoGroup->activeStack() != m_buffer->undoStack();

    m_activeDocument = document;
    if (document != nullptr) {
        trackUndoStack(document->undoStack());
        connect(document, &Document::paletteChanged, this, &EditorSession::syncPalette);
        if (documentStackWasActive) {
            m_undoGroup->setActiveStack(document->undoStack());
        }
    } else if (documentStackWasActive) {
        m_undoGroup->setActiveStack(m_buffer->undoStack());
    }

    syncPalette();
    emit activeDocumentChanged(document);
}

const core::Palette& EditorSession::palette() const
{
    return m_activeDocument ? m_activeDocument->palette() : m_defaultPalette;
}

void EditorSession::syncPalette()
{
    const int last = palette().size() - 1;
    const bool clamped = m_drawColor > last || m_backgroundColor > last;
    m_drawColor = static_cast<core::ColorIndex>(std::min<int>(m_drawColor, last));
    m_backgroundColor = static_cast<core::ColorIndex>(std::min<int>(m_backgroundColor, last));

    m_buffer->setPalette(palette());
    emit paletteChanged();
    if (clamped) {
        emit colorsChanged();
    }
}

void EditorSession::trackUndoStack(QUndoStack* stack)
{
    if (m_undoGroup->stacks().contains(stack)) {
        return;
    }
    m_undoGroup->addStack(stack);
    // Qualquer alteração na pilha (push/undo/redo) a torna a pilha ativa. Assim
    // Ctrl+Z desfaz sempre o alvo editado por último (buffer ou canvas).
    connect(stack, &QUndoStack::indexChanged, this,
        [this, stack] { m_undoGroup->setActiveStack(stack); });
}

// -----------------------------------------------------------------------------
// Cores
// -----------------------------------------------------------------------------
void EditorSession::setDrawColor(int index)
{
    if (!palette().isValidIndex(index) || index == m_drawColor) {
        return;
    }
    m_drawColor = static_cast<core::ColorIndex>(index);
    emit colorsChanged();
}

void EditorSession::setBackgroundColor(int index)
{
    if (!palette().isValidIndex(index) || index == m_backgroundColor) {
        return;
    }
    m_backgroundColor = static_cast<core::ColorIndex>(index);
    emit colorsChanged();
}

// -----------------------------------------------------------------------------
// Visualização
// -----------------------------------------------------------------------------
void EditorSession::setZoom(int factor)
{
    // Aceita apenas os níveis conhecidos (escolhe o mais próximo).
    const auto nearest = std::min_element(std::begin(kZoomLevels), std::end(kZoomLevels),
        [factor](int a, int b) { return std::abs(a - factor) < std::abs(b - factor); });
    if (*nearest != m_zoom) {
        m_zoom = *nearest;
        emit zoomChanged(m_zoom);
    }
}

void EditorSession::zoomIn()
{
    const auto next = std::upper_bound(std::begin(kZoomLevels), std::end(kZoomLevels), m_zoom);
    if (next != std::end(kZoomLevels)) {
        setZoom(*next);
    }
}

void EditorSession::zoomOut()
{
    const auto current = std::lower_bound(std::begin(kZoomLevels), std::end(kZoomLevels), m_zoom);
    if (current != std::begin(kZoomLevels)) {
        setZoom(*std::prev(current));
    }
}

void EditorSession::setMainGridVisible(bool visible)
{
    if (visible != m_mainGridVisible) {
        m_mainGridVisible = visible;
        emit gridsChanged();
    }
}

void EditorSession::setSubGridVisible(bool visible)
{
    if (visible != m_subGridVisible) {
        m_subGridVisible = visible;
        emit gridsChanged();
    }
}

void EditorSession::setCharacterLayout(const core::CharacterLayout& layout)
{
    if (layout == m_layout) {
        return;
    }
    const bool cellSizeChanged = layout.cellSize != m_layout.cellSize;
    m_layout = layout;
    if (cellSizeChanged && m_buffer->image().size() != layout.cellSize) {
        // O buffer acompanha o tamanho da célula (conteúdo preservado no canto).
        m_buffer->replace(m_buffer->image().resizedCanvas(layout.cellSize, m_backgroundColor),
            tr("Character Size"));
    }
    emit characterLayoutChanged(m_layout);
}

// -----------------------------------------------------------------------------
// Regras de escrita
// -----------------------------------------------------------------------------
void EditorSession::setTransparentEffect(bool enabled)
{
    if (enabled != m_transparentEffect) {
        m_transparentEffect = enabled;
        emit transparentEffectChanged(enabled);
    }
}

core::WriteRules EditorSession::drawingRules() const
{
    return { };
}

core::WriteRules EditorSession::transferRules() const
{
    core::WriteRules rules = drawingRules();
    if (m_transparentEffect) {
        // "Transparent Colour Effect": a cor de fundo não é copiada.
        rules.transparentIndex = m_backgroundColor;
    }
    return rules;
}

bool EditorSession::plot(core::IndexedImage& image, core::Point point, core::ColorIndex color) const
{
    // Pixels fora do padrão de tom ficam intactos.
    if (!core::toneCovers(m_tone, point, m_toneShift)) {
        return false;
    }
    return image.setPixel(point, color, drawingRules());
}

// -----------------------------------------------------------------------------
// Tom
// -----------------------------------------------------------------------------
void EditorSession::setTone(core::Tone tone)
{
    if (tone != m_tone) {
        m_tone = tone;
        emit toneChanged();
    }
}

void EditorSession::cycleToneShift()
{
    m_toneShift = (m_toneShift + 1) % core::kToneShiftCount;
    emit toneChanged();
}

// -----------------------------------------------------------------------------
// Ferramentas
// -----------------------------------------------------------------------------
void EditorSession::setToolKind(ToolKind kind)
{
    if (kind == m_toolKind || !m_tools.contains(kind)) {
        return;
    }
    cancelTool();
    m_toolKind = kind;
    emit toolChanged(kind);
}

Tool& EditorSession::tool() const
{
    const auto found = m_tools.find(m_toolKind);
    assert(found != m_tools.end());
    return *found->second;
}

void EditorSession::cancelTool()
{
    tool().cancel(*this);
}

// -----------------------------------------------------------------------------
// Transferência canvas <-> área de desenho
// -----------------------------------------------------------------------------
void EditorSession::getFromCanvas(Document* document, const core::Rect& cell)
{
    if (document == nullptr || cell.isEmpty()) {
        return;
    }
    cancelTool();
    // Áreas da célula fora do canvas viram cor de fundo.
    m_buffer->replace(document->canvas().copy(cell, m_backgroundColor), tr("Get Cell"));
    m_buffer->setSelection(std::nullopt);
}

void EditorSession::putToCanvas(Document* document, const core::Rect& cell)
{
    if (document == nullptr) {
        return;
    }
    cancelTool();
    const core::Rect target = core::Rect::fromPointAndSize(cell.topLeft(), m_buffer->image().size())
                                  .intersected(document->canvas().bounds());
    if (target.isEmpty()) {
        return;
    }
    // "Depois" = canvas atual com o buffer colado segundo as regras.
    const core::IndexedImage before = document->canvas().copy(target);
    core::IndexedImage after = before;
    after.paste(m_buffer->image(), cell.topLeft() - target.topLeft(), transferRules());
    if (after == before) {
        return;
    }
    document->undoStack()->push(
        new RegionCommand(document, target.topLeft(), before, std::move(after), tr("Put Cell")));
}

void EditorSession::transferAll()
{
    if (m_activeDocument) {
        cancelTool();
        m_buffer->replace(m_activeDocument->canvas(), tr("Transfer All"));
    }
}

} // namespace spritenator::gui
