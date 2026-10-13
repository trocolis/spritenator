// =============================================================================
// Document.cpp
// =============================================================================
#include "Document.h"

#include "ImageRendering.h"
#include "io/ImageFile.h"

#include <QFileInfo>
#include <QUndoStack>

#include <stdexcept>

namespace spritenator::gui {

namespace {

/// Contador global de documentos sem nome (Untitled1, Untitled2...).
int g_untitledCounter = 0;

} // namespace

Document::Document(core::IndexedImage canvas, core::Palette palette, QObject* parent)
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
// Aplicação de alterações
// -----------------------------------------------------------------------------
void Document::applyCanvasRegion(core::Point origin, const core::IndexedImage& pixels)
{
    m_canvas.paste(pixels, origin);
    const core::Rect area
        = core::Rect::fromPointAndSize(origin, pixels.size()).intersected(m_canvas.bounds());
    updateRenderedRegion(m_rendered, m_canvas, area);
    emit canvasChanged(area);
}

void Document::applyCanvas(const core::IndexedImage& canvas)
{
    const bool resized = canvas.size() != m_canvas.size();
    m_canvas = canvas;
    rebuildRendered();
    if (resized) {
        emit canvasResized(m_canvas.size());
    }
    emit canvasChanged(m_canvas.bounds());
}

void Document::applyPalette(const core::Palette& palette)
{
    m_palette = palette;
    m_rendered.setColorTable(colorTableOf(m_palette));
    emit paletteChanged();
}

void Document::rebuildRendered()
{
    m_rendered = renderIndexed(m_canvas, m_palette);
}

} // namespace spritenator::gui
