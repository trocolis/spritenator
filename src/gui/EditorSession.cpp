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
#include <variant>

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
        connect(document, &Document::colorModeChanged, this, &EditorSession::syncColorMode);
        if (documentStackWasActive) {
            m_undoGroup->setActiveStack(document->undoStack());
        }
    } else if (documentStackWasActive) {
        m_undoGroup->setActiveStack(m_buffer->undoStack());
    }

    syncPalette();
    syncColorMode();
    emit activeDocumentChanged(document);
}

core::ColorMode EditorSession::colorMode() const
{
    return m_activeDocument ? m_activeDocument->colorMode() : core::ColorMode::Indexed;
}

void EditorSession::syncColorMode()
{
    // O buffer é compartilhado: passa a ter o modo do documento ativo (a
    // conversão usa a paleta do documento quando um dos lados é indexado).
    m_buffer->convertTo(colorMode(), palette());
    if (colorMode() != m_colorMode) {
        m_colorMode = colorMode();
        emit colorModeChanged(m_colorMode);
    }
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
    if (cellSizeChanged && m_buffer->size() != layout.cellSize) {
        // O buffer acompanha o tamanho da célula (conteúdo preservado no
        // canto; a área nova recebe o pixel de fundo do modo).
        core::AnyImage resized = blankLike(m_buffer->image(), layout.cellSize);
        core::pasteImage(resized, m_buffer->image(), { 0, 0 });
        m_buffer->replace(resized, UndoLabel { UndoOperation::CharacterSize });
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
    core::WriteRules rules;
    if (m_mask.any()) {
        rules.protectedColors = &m_mask; // válido enquanto a sessão existir
    }
    return rules;
}

// -----------------------------------------------------------------------------
// Máscara
// -----------------------------------------------------------------------------
void EditorSession::setMask(const core::ColorSet& mask)
{
    if (mask != m_mask) {
        m_mask = mask;
        emit maskChanged();
    }
}

void EditorSession::toggleMasked(int index)
{
    if (!palette().isValidIndex(index)) {
        return;
    }
    core::ColorSet mask = m_mask;
    mask.flip(static_cast<std::size_t>(index));
    setMask(mask);
}

void EditorSession::clearMask()
{
    setAutoMask(false);
    setMask({ });
}

void EditorSession::invertMask()
{
    // Inverte apenas as cores existentes na paleta atual.
    core::ColorSet valid;
    for (int index = 0; index < palette().size(); ++index) {
        valid.set(static_cast<std::size_t>(index));
    }
    setMask(~m_mask & valid);
}

void EditorSession::setMaskMode(bool enabled)
{
    if (enabled != m_maskMode) {
        m_maskMode = enabled;
        emit maskModeChanged(enabled);
    }
}

void EditorSession::setAutoMask(bool enabled)
{
    if (enabled == m_autoMask) {
        return;
    }
    m_autoMask = enabled;
    if (enabled) {
        // Protege todas as cores, exceto a de fundo.
        m_maskBeforeAuto = m_mask;
        core::ColorSet mask;
        for (int index = 0; index < palette().size(); ++index) {
            mask.set(static_cast<std::size_t>(index));
        }
        mask.reset(m_backgroundColor);
        setMask(mask);
    } else {
        setMask(m_maskBeforeAuto);
    }
    emit autoMaskChanged(enabled);
}

core::RgbaWriteRules EditorSession::rgbaTransferRules() const
{
    core::RgbaWriteRules rules;
    rules.skipTransparent = m_transparentEffect; // alfa 0 não é gravado
    return rules;
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

bool EditorSession::plot(core::AnyImage& image, core::Point point, ColorRole role) const
{
    // Pixels fora do padrão de tom ficam intactos (vale para os dois modos).
    if (!core::toneCovers(m_tone, point, m_toneShift)) {
        return false;
    }
    if (auto* indexed = std::get_if<core::IndexedImage>(&image)) {
        const core::ColorIndex color = role == ColorRole::Primary ? m_drawColor : m_backgroundColor;
        return indexed->setPixel(point, color, drawingRules());
    }
    // RGBA: substitui o pixel pela cor exata, inclusive o alfa (sem mistura).
    return std::get<core::RgbaImage>(image).setPixel(point, rgbaFor(role));
}

core::AnyImage EditorSession::blankLike(const core::AnyImage& image, core::Size size) const
{
    if (core::modeOf(image) == core::ColorMode::Indexed) {
        return core::IndexedImage(size, m_backgroundColor);
    }
    return core::RgbaImage(size); // transparente
}

// -----------------------------------------------------------------------------
// Cores RGBA
// -----------------------------------------------------------------------------
void EditorSession::setPrimaryRgba(const core::Rgba& color)
{
    setRgbaFor(ColorRole::Primary, color);
}

void EditorSession::setSecondaryRgba(const core::Rgba& color)
{
    setRgbaFor(ColorRole::Secondary, color);
}

const core::Rgba& EditorSession::rgbaFor(ColorRole role) const
{
    return role == ColorRole::Primary ? m_primaryRgba : m_secondaryRgba;
}

void EditorSession::setRgbaFor(ColorRole role, const core::Rgba& color)
{
    core::Rgba& target = role == ColorRole::Primary ? m_primaryRgba : m_secondaryRgba;
    if (target != color) {
        target = color;
        emit colorsChanged();
    }
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
    // O buffer adota o modo do documento de origem (normalmente já é o ativo).
    m_buffer->convertTo(document->colorMode(), document->palette());
    // Áreas da célula fora do canvas viram o pixel de fundo do modo.
    core::AnyImage region = blankLike(document->canvas(), cell.size());
    core::pasteImage(region, document->canvas(), core::Point { } - cell.topLeft());
    m_buffer->replace(region, UndoLabel { UndoOperation::GetCell });
    m_buffer->setSelection(std::nullopt);
    m_lastCell = cell;
    emit lastCellChanged(cell);
}

void EditorSession::putToCanvas(Document* document, const core::Rect& cell)
{
    if (document == nullptr) {
        return;
    }
    cancelTool();
    const core::Rect target = core::Rect::fromPointAndSize(cell.topLeft(), m_buffer->size())
                                  .intersected(core::boundsOf(document->canvas()));
    if (target.isEmpty()) {
        return;
    }
    // Se o documento for de outro modo, o conteúdo é convertido só para esta
    // gravação (o buffer em si não muda).
    const core::AnyImage source
        = core::convertImage(m_buffer->image(), document->colorMode(), document->palette());
    const core::Point offset = cell.topLeft() - target.topLeft();

    // "Depois" = canvas atual com o buffer colado segundo as regras do modo.
    const core::AnyImage before = core::copyRegion(document->canvas(), target);
    core::AnyImage after = before;
    if (auto* indexed = std::get_if<core::IndexedImage>(&after)) {
        indexed->paste(std::get<core::IndexedImage>(source), offset, transferRules());
    } else {
        std::get<core::RgbaImage>(after).paste(
            std::get<core::RgbaImage>(source), offset, rgbaTransferRules());
    }
    if (after == before) {
        return;
    }
    document->undoStack()->push(new RegionCommand(document, target.topLeft(), before,
        std::move(after), UndoLabel { UndoOperation::PutCell }));
}

void EditorSession::transferAll()
{
    if (m_activeDocument) {
        cancelTool();
        m_buffer->convertTo(m_activeDocument->colorMode(), m_activeDocument->palette());
        m_buffer->replace(m_activeDocument->canvas(), UndoLabel { UndoOperation::TransferAll });
    }
}

} // namespace spritenator::gui
