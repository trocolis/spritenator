// =============================================================================
// EditorSession.cpp
// =============================================================================
#include "EditorSession.h"

#include "Document.h"
#include "tools/PenTool.h"
#include "tools/ShapeTool.h"
#include "tools/SimpleTools.h"

#include <QUndoGroup>
#include <QUndoStack>

#include <algorithm>
#include <cassert>
#include <variant>

namespace spritenator::gui {

EditorSession::EditorSession(QObject* parent)
    : QObject(parent)
    , m_undoGroup(new QUndoGroup(this))
    , m_defaultPalette(core::Palette::makeDefault(core::ColorDepth::Bits8))
{
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
        m_activeDocument->cancelEdit(); // não deixa um traço pela metade
        disconnect(m_activeDocument, nullptr, this, nullptr);
    }
    m_activeDocument = document;
    if (document != nullptr) {
        // Desfazer/refazer passam a agir sobre o documento ativo.
        if (!m_undoGroup->stacks().contains(document->undoStack())) {
            m_undoGroup->addStack(document->undoStack());
        }
        m_undoGroup->setActiveStack(document->undoStack());
        connect(document, &Document::paletteChanged, this, &EditorSession::syncPalette);
        connect(document, &Document::colorModeChanged, this, &EditorSession::syncColorMode);
    } else {
        m_undoGroup->setActiveStack(nullptr);
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

    emit paletteChanged();
    if (clamped) {
        emit colorsChanged();
    }
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

void EditorSession::setGridLayout(const core::GridLayout& layout)
{
    if (layout != m_gridLayout && !layout.mainGrid.isEmpty() && !layout.subGrid.isEmpty()) {
        m_gridLayout = layout;
        emit gridLayoutChanged(m_gridLayout);
    }
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

core::RgbaWriteRules EditorSession::rgbaPasteRules() const
{
    core::RgbaWriteRules rules;
    rules.skipTransparent = m_transparentEffect; // alfa 0 não é colado
    return rules;
}

core::WriteRules EditorSession::pasteRules() const
{
    core::WriteRules rules = drawingRules();
    if (m_transparentEffect) {
        // "Transparent Colour Effect": a cor de fundo não é colada.
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

bool EditorSession::canPlot(const core::AnyImage& image, core::Point point) const
{
    if (!core::boundsOf(image).contains(point) || !core::toneCovers(m_tone, point, m_toneShift)) {
        return false;
    }
    // Máscara: pixels de cores protegidas não podem ser alterados.
    if (const auto* indexed = std::get_if<core::IndexedImage>(&image)) {
        return !m_mask.test(indexed->pixel(point.x, point.y));
    }
    return true;
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

void EditorSession::setActiveColorRole(ColorRole role)
{
    if (role != m_activeColorRole) {
        m_activeColorRole = role;
        emit activeColorRoleChanged(role);
    }
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
// Célula para a animação
// -----------------------------------------------------------------------------
void EditorSession::setLastClickedPoint(core::Point point)
{
    const core::Rect cell = m_gridLayout.subGridCellAt(point);
    if (cell != m_lastCell) {
        m_lastCell = cell;
        emit lastCellChanged(cell);
    }
}

} // namespace spritenator::gui
