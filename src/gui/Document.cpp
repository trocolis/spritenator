// =============================================================================
// Document.cpp
// =============================================================================
#include "Document.h"

#include "ImageRendering.h"
#include "commands/ImageCommands.h"
#include "io/ImageFile.h"

#include <QFileInfo>
#include <QUndoStack>

#include <cassert>
#include <stdexcept>
#include <variant>

namespace spritenator::gui {

namespace {

/// Contador global de documentos sem nome (Untitled1, Untitled2...).
int g_untitledCounter = 0;

} // namespace

Document::Document(core::AnyImage canvas, core::Palette palette, QObject* parent)
    : QObject(parent)
    , m_canvas(std::move(canvas))
    , m_palette(std::move(palette))
    , m_undoStack(new QUndoStack(this))
    , m_untitledNumber(++g_untitledCounter)
{
    rebuildRendered();

    // O estado "limpo" do QUndoStack é a nossa noção de "não modificado".
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
        [this](bool clean) { emit modifiedChanged(!clean); });
}

Document::~Document() = default;

Document* Document::load(const QString& path, QObject* parent)
{
    io::ImageData data = io::ImageFile::load(path);
    auto* document = new Document(std::move(data.image), data.palette, parent);
    document->m_filePath = QFileInfo(path).absoluteFilePath();
    return document;
}

QString Document::displayName() const
{
    if (m_filePath.isEmpty()) {
        return tr("Untitled%1").arg(m_untitledNumber);
    }
    return QFileInfo(m_filePath).fileName();
}

const core::IndexedImage& Document::indexedCanvas() const
{
    assert(colorMode() == core::ColorMode::Indexed);
    return std::get<core::IndexedImage>(m_canvas);
}

bool Document::isModified() const
{
    return !m_undoStack->isClean();
}

// -----------------------------------------------------------------------------
// Arquivo
// -----------------------------------------------------------------------------
void Document::saveAs(const QString& path)
{
    io::ImageFile::save(path, { m_canvas, m_palette });

    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (absolute != m_filePath) {
        m_filePath = absolute;
        emit filePathChanged(m_filePath);
    }
    m_undoStack->setClean();
}

void Document::save()
{
    if (m_filePath.isEmpty()) {
        throw std::logic_error("Document::save chamado sem caminho de arquivo");
    }
    saveAs(m_filePath);
}

void Document::revert()
{
    if (m_filePath.isEmpty()) {
        return;
    }
    io::ImageData data = io::ImageFile::load(m_filePath);

    // O histórico refere-se ao conteúdo antigo: precisa ser descartado.
    m_undoStack->clear();
    applyPalette(data.palette);
    applyCanvas(data.image);
    m_undoStack->setClean();
}

// -----------------------------------------------------------------------------
// Edição ao vivo
// -----------------------------------------------------------------------------
void Document::beginEdit()
{
    m_snapshot = m_canvas;
    m_dirty = { };
    m_editing = true;
}

void Document::editLive(const LiveEdit& edit)
{
    if (!m_editing) {
        beginEdit(); // tolerância: permite editar sem chamar beginEdit
    }
    const core::Rect area = edit(m_canvas).intersected(core::boundsOf(m_canvas));
    if (area.isEmpty()) {
        return;
    }
    m_dirty = m_dirty.united(area);
    updateRenderedRegion(m_rendered, m_canvas, area);
    emit canvasChanged(area);
}

void Document::restoreSnapshot()
{
    if (!m_editing || m_dirty.isEmpty()) {
        return;
    }
    // Copia de volta apenas a área tocada até agora.
    core::pasteImage(m_canvas, core::copyRegion(m_snapshot, m_dirty), m_dirty.topLeft());
    updateRenderedRegion(m_rendered, m_canvas, m_dirty);
    emit canvasChanged(m_dirty);
}

void Document::commitEdit(const UndoLabel& undoLabel)
{
    if (!m_editing) {
        return;
    }
    m_editing = false;
    if (m_dirty.isEmpty()) {
        return;
    }
    core::AnyImage before = core::copyRegion(m_snapshot, m_dirty);
    core::AnyImage after = core::copyRegion(m_canvas, m_dirty);
    m_snapshot = core::emptyImage(colorMode()); // libera memória
    if (before == after) {
        return; // ex.: traço desenhado com a mesma cor que já estava lá
    }
    // A edição já está aplicada: o comando não deve reaplicá-la no push.
    m_undoStack->push(new RegionCommand(
        this, m_dirty.topLeft(), std::move(before), std::move(after), undoLabel, true));
}

void Document::cancelEdit()
{
    restoreSnapshot();
    m_editing = false;
    m_snapshot = core::emptyImage(colorMode());
    m_dirty = { };
}

// -----------------------------------------------------------------------------
// Seleção
// -----------------------------------------------------------------------------
void Document::setSelection(const std::optional<core::Rect>& selection)
{
    std::optional<core::Rect> clipped;
    if (selection) {
        const core::Rect area = selection->intersected(core::boundsOf(m_canvas));
        if (!area.isEmpty()) {
            clipped = area;
        }
    }
    if (clipped != m_selection) {
        m_selection = clipped;
        emit selectionChanged();
    }
}

core::Rect Document::workArea() const
{
    return m_selection.value_or(core::boundsOf(m_canvas));
}

// -----------------------------------------------------------------------------
// Aplicação de alterações
// -----------------------------------------------------------------------------
void Document::applyCanvasRegion(core::Point origin, const core::AnyImage& pixels)
{
    core::pasteImage(m_canvas, pixels, origin);
    const core::Rect area = core::Rect::fromPointAndSize(origin, core::sizeOf(pixels))
                                .intersected(core::boundsOf(m_canvas));
    updateRenderedRegion(m_rendered, m_canvas, area);
    emit canvasChanged(area);
}

void Document::applyCanvas(const core::AnyImage& canvas)
{
    const bool resized = core::sizeOf(canvas) != core::sizeOf(m_canvas);
    const bool modeChanged = canvas.index() != m_canvas.index();
    m_canvas = canvas;
    rebuildRendered();
    if (resized) {
        setSelection(std::nullopt); // a seleção antiga pode não caber mais
        emit canvasResized(core::sizeOf(m_canvas));
    }
    if (modeChanged) {
        emit colorModeChanged(colorMode());
    }
    emit canvasChanged(core::boundsOf(m_canvas));
}

void Document::applyPalette(const core::Palette& palette)
{
    m_palette = palette;
    if (colorMode() == core::ColorMode::Indexed) { // RGBA não usa tabela de cores
        m_rendered.setColorTable(colorTableOf(m_palette));
    }
    emit paletteChanged();
}

void Document::rebuildRendered()
{
    m_rendered = renderImage(m_canvas, m_palette);
}

} // namespace spritenator::gui
