// =============================================================================
// Document.cpp
// =============================================================================
#include "Document.h"

#include "ImageRendering.h"
#include "commands/ImageCommands.h"
#include "io/GifFile.h"
#include "io/ImageFile.h"
#include "io/IoError.h"
#include "io/ProjectFile.h"

#include <QDir>
#include <QFileInfo>
#include <QUndoStack>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace spritenator::gui {

namespace {

/// Contador global de documentos sem nome (Untitled1, Untitled2...).
int g_untitledCounter = 0;

/// Cópia profunda de uma célula (para duplicar cenas e camadas).
CelPtr cloneCel(const CelPtr& cel)
{
    return std::make_shared<core::AnyImage>(*cel);
}

/// Imagem vazia do modo e tamanho do documento ("transparente": índice 0
/// ou RGBA com alfa 0).
CelPtr blankCel(const SpriteState& state)
{
    const core::AnyImage& sample = *state.cels.front().front();
    return std::make_shared<core::AnyImage>(
        core::blankImage(core::modeOf(sample), core::sizeOf(sample)));
}

/// Estado de uma camada e uma cena com a imagem informada.
SpriteState singleCelState(core::AnyImage image, core::Palette palette)
{
    SpriteState state;
    state.palette = std::move(palette);
    state.layers.push_back({ 1, Document::tr("Layer %1").arg(1), true, 255 });
    state.frames.push_back({ 2, 100 });
    state.cels = { { std::make_shared<core::AnyImage>(std::move(image)) } };
    state.nextId = 3;
    return state;
}

/// Converte os dados de um projeto/GIF lido do disco em estado.
SpriteState stateFrom(io::ProjectData data)
{
    SpriteState state;
    state.palette = std::move(data.palette);
    for (const io::LayerData& layer : data.layers) {
        state.layers.push_back(
            { state.nextId++, layer.name, layer.visible, layer.opacity, layer.locked });
    }
    for (const int duration : data.durations) {
        state.frames.push_back({ state.nextId++, duration });
    }
    for (auto& row : data.cels) {
        std::vector<CelPtr> cels;
        for (auto& image : row) {
            cels.push_back(std::make_shared<core::AnyImage>(std::move(image)));
        }
        state.cels.push_back(std::move(cels));
    }
    return state;
}

/// Junta uma cor (sem pré-multiplicação) sobre outra ("over").
QRgb blendOver(QRgb below, int red, int green, int blue, int alpha)
{
    if (alpha <= 0) {
        return below;
    }
    const int belowAlpha = qAlpha(below);
    const int outAlpha = alpha + belowAlpha * (255 - alpha) / 255;
    if (outAlpha == 0) {
        return 0;
    }
    const auto mix = [&](int top, int bottom) {
        return (top * alpha + bottom * belowAlpha * (255 - alpha) / 255) / outAlpha;
    };
    return qRgba(
        mix(red, qRed(below)), mix(green, qGreen(below)), mix(blue, qBlue(below)), outAlpha);
}

/// Compõe as camadas visíveis de uma cena na região "area" de "target"
/// (ARGB32 sem pré-multiplicação, do tamanho do canvas).
void compositeInto(QImage& target, const SpriteState& state, int frame, const core::Rect& area)
{
    const auto& cels = state.cels.at(static_cast<std::size_t>(frame));
    for (int y = area.top(); y < area.bottom(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(target.scanLine(y));
        for (int x = area.left(); x < area.right(); ++x) {
            QRgb color = 0;
            for (std::size_t index = 0; index < state.layers.size(); ++index) {
                const Layer& layer = state.layers[index];
                if (!layer.visible) {
                    continue;
                }
                const core::AnyImage& cel = *cels[index];
                if (const auto* indexed = std::get_if<core::IndexedImage>(&cel)) {
                    const core::ColorIndex value = indexed->pixel(x, y);
                    if (value == 0 && index > 0) {
                        continue; // índice 0 transparente nas camadas de cima
                    }
                    const core::Rgb rgb = state.palette.color(value);
                    color = qRgba(rgb.red, rgb.green, rgb.blue, 255);
                } else {
                    const core::Rgba rgba = std::get<core::RgbaImage>(cel).pixel(x, y);
                    color = blendOver(
                        color, rgba.red, rgba.green, rgba.blue, rgba.alpha * layer.opacity / 255);
                }
            }
            line[x] = color;
        }
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Criação e abertura
// -----------------------------------------------------------------------------
Document::Document(core::AnyImage canvas, core::Palette palette, QObject* parent)
    : Document(singleCelState(std::move(canvas), std::move(palette)), parent)
{
}

Document::Document(SpriteState state, QObject* parent)
    : QObject(parent)
    , m_state(std::move(state))
    , m_undoStack(new QUndoStack(this))
    , m_untitledNumber(++g_untitledCounter)
{
    assert(!m_state.layers.empty() && !m_state.frames.empty());
    rebuildRendered();

    // O estado "limpo" do QUndoStack é a nossa noção de "não modificado".
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
        [this](bool clean) { emit modifiedChanged(!clean); });
}

Document::~Document() = default;

SaveFormat Document::formatForPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("sprn")) {
        return SaveFormat::Project;
    }
    if (suffix == QLatin1String("gif")) {
        return SaveFormat::Gif;
    }
    return SaveFormat::Image;
}

Document* Document::load(const QString& path, QObject* parent)
{
    Document* document = nullptr;
    switch (formatForPath(path)) {
    case SaveFormat::Project:
        document = new Document(stateFrom(io::ProjectFile::load(path)), parent);
        break;
    case SaveFormat::Gif:
        document = new Document(stateFrom(io::GifFile::load(path)), parent);
        break;
    default: {
        io::ImageData data = io::ImageFile::load(path);
        document = new Document(std::move(data.image), data.palette, parent);
        break;
    }
    }
    document->m_filePath = QFileInfo(path).absoluteFilePath();
    document->m_saveOptions.format = formatForPath(path);
    return document;
}

QString Document::displayName() const
{
    if (m_filePath.isEmpty()) {
        return tr("Untitled%1").arg(m_untitledNumber);
    }
    return QFileInfo(m_filePath).fileName();
}

// -----------------------------------------------------------------------------
// Acesso às células
// -----------------------------------------------------------------------------
const core::AnyImage& Document::canvas() const
{
    return cel(m_frame, m_layer);
}

const core::AnyImage& Document::cel(int frame, int layer) const
{
    return *m_state.cels.at(static_cast<std::size_t>(frame)).at(static_cast<std::size_t>(layer));
}

core::AnyImage& Document::mutableCanvas()
{
    CelPtr& cel
        = m_state.cels.at(static_cast<std::size_t>(m_frame)).at(static_cast<std::size_t>(m_layer));
    if (cel.use_count() > 1) { // compartilhada com o histórico: separa
        cel = cloneCel(cel);
    }
    return *cel;
}

const core::IndexedImage& Document::indexedCanvas() const
{
    assert(colorMode() == core::ColorMode::Indexed);
    return std::get<core::IndexedImage>(canvas());
}

CelKey Document::activeCel() const
{
    return { m_state.frames.at(static_cast<std::size_t>(m_frame)).id,
        m_state.layers.at(static_cast<std::size_t>(m_layer)).id };
}

void Document::setCurrentFrame(int frame)
{
    frame = std::clamp(frame, 0, frameCount() - 1);
    if (frame == m_frame) {
        return;
    }
    if (m_editing) {
        cancelEdit(); // a edição era da célula anterior
    }
    m_frame = frame;
    rebuildRendered();
    emit canvasChanged(core::boundsOf(canvas()));
    emit currentCelChanged(m_frame, m_layer);
}

void Document::setCurrentLayer(int layer)
{
    layer = std::clamp(layer, 0, layerCount() - 1);
    if (layer == m_layer) {
        return;
    }
    if (m_editing) {
        cancelEdit();
    }
    m_layer = layer;
    emit currentCelChanged(m_frame, m_layer);
}

void Document::clampCurrent()
{
    m_frame = std::clamp(m_frame, 0, frameCount() - 1);
    m_layer = std::clamp(m_layer, 0, layerCount() - 1);
}

QImage Document::composite(int frame) const
{
    const core::Size size = core::sizeOf(canvas());
    QImage image(size.width, size.height, QImage::Format_ARGB32);
    compositeInto(image, m_state, frame, core::boundsOf(canvas()));
    return image;
}

core::AnyImage Document::flattened(int frame) const
{
    const auto& cels = m_state.cels.at(static_cast<std::size_t>(frame));
    if (colorMode() == core::ColorMode::Indexed) {
        core::IndexedImage result(core::sizeOf(canvas()), 0);
        for (std::size_t index = 0; index < m_state.layers.size(); ++index) {
            if (!m_state.layers[index].visible) {
                continue;
            }
            // A camada de baixo é opaca; nas de cima, o índice 0 é transparente.
            core::WriteRules rules;
            if (index > 0) {
                rules.transparentIndex = 0;
            }
            result.paste(std::get<core::IndexedImage>(*cels[index]), { 0, 0 }, rules);
        }
        return result;
    }
    const QImage image = composite(frame);
    core::RgbaImage result(core::sizeOf(canvas()));
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            result.setPixel({ x, y }, core::Rgba::fromArgb32(line[x]));
        }
    }
    return result;
}

// -----------------------------------------------------------------------------
// Composição da cena exibida
// -----------------------------------------------------------------------------
void Document::rebuildRendered()
{
    m_rendered = composite(m_frame);
}

void Document::updateComposite(const core::Rect& area)
{
    const core::Rect clipped = area.intersected(core::boundsOf(canvas()));
    if (!clipped.isEmpty()) {
        compositeInto(m_rendered, m_state, m_frame, clipped);
    }
}

// -----------------------------------------------------------------------------
// Arquivo
// -----------------------------------------------------------------------------
bool Document::canRevert() const
{
    return hasFilePath() && m_saveOptions.format != SaveFormat::SpriteSheet
        && m_saveOptions.format != SaveFormat::Frames;
}

void Document::saveAs(const QString& path)
{
    saveAs(path, { formatForPath(path), 0 });
}

void Document::saveAs(const QString& path, const SaveOptions& options)
{
    std::vector<core::AnyImage> frames;
    std::vector<int> durations;
    const auto collectFrames = [&] {
        for (int frame = 0; frame < frameCount(); ++frame) {
            frames.push_back(flattened(frame));
            durations.push_back(m_state.frames[static_cast<std::size_t>(frame)].durationMs);
        }
    };

    switch (options.format) {
    case SaveFormat::Image: io::ImageFile::save(path, { flattened(m_frame), palette() }); break;
    case SaveFormat::Project: {
        io::ProjectData data;
        data.palette = palette();
        for (const Layer& layer : m_state.layers) {
            data.layers.push_back({ layer.name, layer.visible, layer.opacity, layer.locked });
        }
        for (const Frame& frame : m_state.frames) {
            data.durations.push_back(frame.durationMs);
        }
        for (const auto& row : m_state.cels) {
            std::vector<core::AnyImage> images;
            for (const CelPtr& cel : row) {
                images.push_back(*cel);
            }
            data.cels.push_back(std::move(images));
        }
        io::ProjectFile::save(path, data);
        break;
    }
    case SaveFormat::Gif:
        collectFrames();
        io::GifFile::save(path, frames, durations, palette());
        break;
    case SaveFormat::SpriteSheet: {
        collectFrames();
        // Cenas lado a lado, em "colunas" por linha.
        const core::Size cell = core::sizeOf(canvas());
        const int count = frameCount();
        const int columns
            = options.sheetColumns > 0 ? std::min(options.sheetColumns, count) : count;
        const int rows = (count + columns - 1) / columns;
        if (static_cast<long long>(cell.width) * columns > core::IndexedImage::kMaxDimension
            || static_cast<long long>(cell.height) * rows > core::IndexedImage::kMaxDimension) {
            throw io::IoError(tr("The sprite sheet would be larger than %1 pixels. "
                                 "Use more columns or fewer scenes.")
                    .arg(core::IndexedImage::kMaxDimension));
        }
        core::AnyImage sheet
            = core::blankImage(colorMode(), { cell.width * columns, cell.height * rows });
        for (int index = 0; index < count; ++index) {
            core::pasteImage(sheet, frames[static_cast<std::size_t>(index)],
                { (index % columns) * cell.width, (index / columns) * cell.height });
        }
        io::ImageFile::save(path, { sheet, palette() });
        break;
    }
    case SaveFormat::Frames: {
        collectFrames();
        // nome.png -> nome_001.png, nome_002.png...
        const QFileInfo info(path);
        const QString suffix = info.suffix().isEmpty() ? QStringLiteral("png") : info.suffix();
        for (std::size_t index = 0; index < frames.size(); ++index) {
            const QString name = QStringLiteral("%1_%2.%3")
                                     .arg(info.completeBaseName())
                                     .arg(index + 1, 3, 10, QLatin1Char('0'))
                                     .arg(suffix);
            io::ImageFile::save(info.dir().filePath(name), { frames[index], palette() });
        }
        break;
    }
    }

    m_saveOptions = options;
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
    saveAs(m_filePath, m_saveOptions);
}

void Document::revert()
{
    if (!canRevert()) {
        return;
    }
    std::unique_ptr<Document> reloaded(load(m_filePath));
    // O histórico refere-se ao conteúdo antigo: precisa ser descartado.
    m_undoStack->clear();
    applyState(reloaded->m_state);
    m_undoStack->setClean();
}

bool Document::isModified() const
{
    return !m_undoStack->isClean();
}

// -----------------------------------------------------------------------------
// Edição ao vivo
// -----------------------------------------------------------------------------
void Document::beginEdit()
{
    mutableCanvas(); // separa a célula do histórico antes de começar
    m_snapshot = canvas();
    m_dirty = { };
    m_editing = true;
}

void Document::editLive(const LiveEdit& edit)
{
    if (!m_editing) {
        beginEdit(); // tolerância: permite editar sem chamar beginEdit
    }
    core::AnyImage& image = mutableCanvas();
    const core::Rect area = edit(image).intersected(core::boundsOf(image));
    if (area.isEmpty()) {
        return;
    }
    m_dirty = m_dirty.united(area);
    updateComposite(area);
    emit canvasChanged(area);
}

void Document::restoreSnapshot()
{
    if (!m_editing || m_dirty.isEmpty()) {
        return;
    }
    // Copia de volta apenas a área tocada até agora.
    core::pasteImage(mutableCanvas(), core::copyRegion(m_snapshot, m_dirty), m_dirty.topLeft());
    updateComposite(m_dirty);
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
    core::AnyImage after = core::copyRegion(canvas(), m_dirty);
    m_snapshot = core::emptyImage(colorMode()); // libera memória
    if (before == after) {
        return; // ex.: traço desenhado com a mesma cor que já estava lá
    }
    // A edição já está aplicada: o comando não deve reaplicá-la no push.
    m_undoStack->push(new RegionCommand(this, activeCel(), m_dirty.topLeft(), std::move(before),
        std::move(after), undoLabel, true));
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
void Document::setSelection(const core::Selection& selection)
{
    const core::Selection clipped = selection.intersected(core::boundsOf(canvas()));
    if (clipped != m_selection) {
        m_selection = clipped;
        emit selectionChanged();
    }
}

core::Rect Document::workArea() const
{
    return hasSelection() ? m_selection.bounds() : core::boundsOf(canvas());
}

// -----------------------------------------------------------------------------
// Operações de linha do tempo
// -----------------------------------------------------------------------------
int Document::takeId(SpriteState& state) const
{
    return state.nextId++;
}

void Document::changeState(SpriteState after, UndoOperation operation, bool mergeable)
{
    if (m_editing) {
        cancelEdit();
    }
    m_undoStack->push(new SpriteStateCommand(
        this, m_state, std::move(after), UndoLabel { operation }, mergeable));
}

void Document::addFrame()
{
    SpriteState next = m_state;
    const auto position = static_cast<std::ptrdiff_t>(m_frame + 1);
    std::vector<CelPtr> cels;
    for (std::size_t layer = 0; layer < next.layers.size(); ++layer) {
        cels.push_back(blankCel(next));
    }
    const int duration = m_state.frames[static_cast<std::size_t>(m_frame)].durationMs;
    next.frames.insert(next.frames.begin() + position, Frame { takeId(next), duration });
    next.cels.insert(next.cels.begin() + position, std::move(cels));
    changeState(std::move(next), UndoOperation::AddFrame);
    setCurrentFrame(m_frame + 1);
}

void Document::duplicateFrame()
{
    SpriteState next = m_state;
    const auto position = static_cast<std::ptrdiff_t>(m_frame + 1);
    std::vector<CelPtr> cels;
    for (const CelPtr& cel : m_state.cels[static_cast<std::size_t>(m_frame)]) {
        cels.push_back(cloneCel(cel));
    }
    Frame frame = m_state.frames[static_cast<std::size_t>(m_frame)];
    frame.id = takeId(next);
    next.frames.insert(next.frames.begin() + position, frame);
    next.cels.insert(next.cels.begin() + position, std::move(cels));
    changeState(std::move(next), UndoOperation::DuplicateFrame);
    setCurrentFrame(m_frame + 1);
}

void Document::removeFrame()
{
    if (frameCount() <= 1) {
        return;
    }
    SpriteState next = m_state;
    next.frames.erase(next.frames.begin() + m_frame);
    next.cels.erase(next.cels.begin() + m_frame);
    changeState(std::move(next), UndoOperation::DeleteFrame);
}

void Document::moveFrame(int offset)
{
    const int target = m_frame + offset;
    if (offset == 0 || target < 0 || target >= frameCount()) {
        return;
    }
    SpriteState next = m_state;
    std::swap(next.frames[static_cast<std::size_t>(m_frame)],
        next.frames[static_cast<std::size_t>(target)]);
    std::swap(
        next.cels[static_cast<std::size_t>(m_frame)], next.cels[static_cast<std::size_t>(target)]);
    changeState(std::move(next), UndoOperation::MoveFrame);
    setCurrentFrame(target);
}

void Document::setFrameDuration(int frame, int durationMs)
{
    durationMs = std::clamp(durationMs, kMinFrameDuration, kMaxFrameDuration);
    if (frame < 0 || frame >= frameCount()
        || m_state.frames[static_cast<std::size_t>(frame)].durationMs == durationMs) {
        return;
    }
    SpriteState next = m_state;
    next.frames[static_cast<std::size_t>(frame)].durationMs = durationMs;
    changeState(std::move(next), UndoOperation::FrameDuration, true);
}

void Document::addLayer()
{
    SpriteState next = m_state;
    const auto position = static_cast<std::ptrdiff_t>(m_layer + 1);
    const int id = takeId(next);
    next.layers.insert(next.layers.begin() + position,
        Layer { id, tr("Layer %1").arg(layerCount() + 1), true, 255 });
    for (auto& row : next.cels) {
        row.insert(row.begin() + position, blankCel(m_state));
    }
    changeState(std::move(next), UndoOperation::AddLayer);
    setCurrentLayer(m_layer + 1);
}

void Document::duplicateLayer()
{
    SpriteState next = m_state;
    const auto position = static_cast<std::ptrdiff_t>(m_layer + 1);
    Layer layer = m_state.layers[static_cast<std::size_t>(m_layer)];
    layer.id = takeId(next);
    layer.name = tr("%1 copy").arg(layer.name);
    next.layers.insert(next.layers.begin() + position, layer);
    for (auto& row : next.cels) {
        row.insert(row.begin() + position, cloneCel(row[static_cast<std::size_t>(m_layer)]));
    }
    changeState(std::move(next), UndoOperation::DuplicateLayer);
    setCurrentLayer(m_layer + 1);
}

void Document::removeLayer()
{
    if (layerCount() <= 1) {
        return;
    }
    SpriteState next = m_state;
    next.layers.erase(next.layers.begin() + m_layer);
    for (auto& row : next.cels) {
        row.erase(row.begin() + m_layer);
    }
    changeState(std::move(next), UndoOperation::DeleteLayer);
}

void Document::moveLayer(int offset)
{
    const int target = m_layer + offset;
    if (offset == 0 || target < 0 || target >= layerCount()) {
        return;
    }
    SpriteState next = m_state;
    std::swap(next.layers[static_cast<std::size_t>(m_layer)],
        next.layers[static_cast<std::size_t>(target)]);
    for (auto& row : next.cels) {
        std::swap(row[static_cast<std::size_t>(m_layer)], row[static_cast<std::size_t>(target)]);
    }
    changeState(std::move(next), UndoOperation::MoveLayer);
    setCurrentLayer(target);
}

void Document::mergeLayerDown()
{
    if (m_layer == 0) {
        return;
    }
    SpriteState next = m_state;
    const auto upper = static_cast<std::size_t>(m_layer);
    const auto lower = upper - 1;
    const Layer& top = m_state.layers[upper];
    for (auto& row : next.cels) {
        core::AnyImage merged = *row[lower];
        if (top.visible) {
            if (auto* indexed = std::get_if<core::IndexedImage>(&merged)) {
                core::WriteRules rules;
                rules.transparentIndex = 0; // o índice 0 de cima não cobre
                indexed->paste(std::get<core::IndexedImage>(*row[upper]), { 0, 0 }, rules);
            } else {
                auto& target = std::get<core::RgbaImage>(merged);
                const auto& source = std::get<core::RgbaImage>(*row[upper]);
                for (int y = 0; y < target.height(); ++y) {
                    for (int x = 0; x < target.width(); ++x) {
                        const core::Rgba above = source.pixel(x, y);
                        const QRgb mixed = blendOver(target.pixel(x, y).toArgb32(), above.red,
                            above.green, above.blue, above.alpha * top.opacity / 255);
                        target.setPixel({ x, y }, core::Rgba::fromArgb32(mixed));
                    }
                }
            }
        }
        row[lower] = std::make_shared<core::AnyImage>(std::move(merged));
        row.erase(row.begin() + static_cast<std::ptrdiff_t>(upper));
    }
    next.layers.erase(next.layers.begin() + static_cast<std::ptrdiff_t>(upper));
    changeState(std::move(next), UndoOperation::MergeLayer);
    setCurrentLayer(static_cast<int>(lower));
}

void Document::setLayerVisible(int layer, bool visible)
{
    if (layer < 0 || layer >= layerCount()
        || m_state.layers[static_cast<std::size_t>(layer)].visible == visible) {
        return;
    }
    SpriteState next = m_state;
    next.layers[static_cast<std::size_t>(layer)].visible = visible;
    changeState(std::move(next), UndoOperation::LayerProperties);
}

void Document::setLayerLocked(int layer, bool locked)
{
    if (layer < 0 || layer >= layerCount()
        || m_state.layers[static_cast<std::size_t>(layer)].locked == locked) {
        return;
    }
    SpriteState next = m_state;
    next.layers[static_cast<std::size_t>(layer)].locked = locked;
    changeState(std::move(next), UndoOperation::LayerProperties);
}

bool Document::canEditActiveCel() const
{
    return !m_state.layers.at(static_cast<std::size_t>(m_layer)).locked;
}

bool Document::celHasContent(int frame, int layer) const
{
    // "Vazia" = só o pixel transparente (índice 0 ou alfa 0).
    return std::visit(
        [](const auto& image) {
            using Pixel = typename std::decay_t<decltype(image)>::PixelType;
            const auto pixels = image.data();
            if constexpr (std::is_same_v<Pixel, core::ColorIndex>) {
                return std::any_of(
                    pixels.begin(), pixels.end(), [](auto value) { return value != 0; });
            } else {
                return std::any_of(pixels.begin(), pixels.end(),
                    [](const auto& value) { return value.alpha != 0; });
            }
        },
        cel(frame, layer));
}

void Document::setAllFrameDurations(int durationMs)
{
    durationMs = std::clamp(durationMs, kMinFrameDuration, kMaxFrameDuration);
    SpriteState next = m_state;
    bool changed = false;
    for (Frame& frame : next.frames) {
        changed = changed || frame.durationMs != durationMs;
        frame.durationMs = durationMs;
    }
    if (changed) {
        changeState(std::move(next), UndoOperation::FrameDuration, true);
    }
}

void Document::setLayerOpacity(int layer, int opacity)
{
    opacity = std::clamp(opacity, 0, 255);
    if (layer < 0 || layer >= layerCount()
        || m_state.layers[static_cast<std::size_t>(layer)].opacity == opacity) {
        return;
    }
    SpriteState next = m_state;
    next.layers[static_cast<std::size_t>(layer)].opacity = opacity;
    changeState(std::move(next), UndoOperation::LayerOpacity, true);
}

void Document::renameLayer(int layer, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (layer < 0 || layer >= layerCount() || trimmed.isEmpty()
        || m_state.layers[static_cast<std::size_t>(layer)].name == trimmed) {
        return;
    }
    SpriteState next = m_state;
    next.layers[static_cast<std::size_t>(layer)].name = trimmed;
    changeState(std::move(next), UndoOperation::LayerProperties);
}

void Document::transformCels(
    const CelFunction& function, const core::Palette& palette, const UndoLabel& undoLabel)
{
    SpriteState next = m_state;
    next.palette = palette;
    bool changed = palette != m_state.palette;
    for (auto& row : next.cels) {
        for (std::size_t layer = 0; layer < row.size(); ++layer) {
            CelPtr& cel = row[layer];
            core::AnyImage result = function(*cel, static_cast<int>(layer));
            if (result != *cel) {
                cel = std::make_shared<core::AnyImage>(std::move(result));
                changed = true;
            }
        }
    }
    if (!changed) {
        return;
    }
    if (m_editing) {
        cancelEdit();
    }
    m_undoStack->push(new SpriteStateCommand(this, m_state, std::move(next), undoLabel));
}

// -----------------------------------------------------------------------------
// Aplicação de alterações (comandos)
// -----------------------------------------------------------------------------
void Document::applyCelRegion(const CelKey& key, core::Point origin, const core::AnyImage& pixels)
{
    // A célula alterada passa a ser a ativa (desfazer leva à cena dela).
    for (int frame = 0; frame < frameCount(); ++frame) {
        if (m_state.frames[static_cast<std::size_t>(frame)].id != key.frameId) {
            continue;
        }
        for (int layer = 0; layer < layerCount(); ++layer) {
            if (m_state.layers[static_cast<std::size_t>(layer)].id == key.layerId) {
                setCurrentFrame(frame);
                setCurrentLayer(layer);
            }
        }
    }
    if (activeCel() != key) {
        return; // a célula não existe mais (não deveria acontecer)
    }
    core::pasteImage(mutableCanvas(), pixels, origin);
    const core::Rect area = core::Rect::fromPointAndSize(origin, core::sizeOf(pixels))
                                .intersected(core::boundsOf(canvas()));
    updateComposite(area);
    emit canvasChanged(area);
}

void Document::applyState(const SpriteState& state)
{
    if (m_editing) {
        cancelEdit();
    }
    const core::Size oldSize = core::sizeOf(canvas());
    const core::ColorMode oldMode = colorMode();
    const bool paletteChangedNow = state.palette != m_state.palette;
    m_state = state;
    clampCurrent();
    rebuildRendered();
    const core::Size size = core::sizeOf(canvas());
    if (size != oldSize) {
        clearSelection(); // a seleção antiga pode não caber mais
        emit canvasResized(size);
    }
    if (colorMode() != oldMode) {
        emit colorModeChanged(colorMode());
    }
    if (paletteChangedNow) {
        emit paletteChanged();
    }
    emit structureChanged();
    emit currentCelChanged(m_frame, m_layer);
    emit canvasChanged(core::boundsOf(canvas()));
}

void Document::applyPalette(const core::Palette& palette)
{
    m_state.palette = palette;
    if (colorMode() == core::ColorMode::Indexed) { // RGBA não usa a paleta
        rebuildRendered();
        emit canvasChanged(core::boundsOf(canvas()));
    }
    emit paletteChanged();
}

} // namespace spritenator::gui
