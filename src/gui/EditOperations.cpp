// =============================================================================
// EditOperations.cpp
//
// Todas as operações valem para os dois modos de cor. As transformações são
// funções genéricas do núcleo (core/Transform), aplicadas ao tipo concreto da
// imagem com std::visit; o "pixel de fundo" depende do modo: o índice da cor
// de fundo (indexada) ou transparente (RGBA).
// =============================================================================
#include "EditOperations.h"

#include "Document.h"
#include "EditorSession.h"
#include "SelectionTransform.h"
#include "commands/ImageCommands.h"
#include "core/ColorConversion.h"
#include "core/Transform.h"
#include "io/ImageFile.h"

#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QFontMetrics>
#include <QIODevice>
#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <QUndoStack>

#include <optional>
#include <type_traits>
#include <variant>

namespace spritenator::gui {

namespace {

QString mimeType()
{
    return QString::fromLatin1(EditOperations::kImageMimeType);
}

/// Códigos do modo de cor no formato privado da área de transferência.
constexpr qint32 kIndexedTag = 0;
constexpr qint32 kRgbaTag = 1;

/// Serializa a imagem: modo, largura, altura e os bytes dos pixels.
QByteArray encodeImage(const core::AnyImage& image)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    const core::Size size = core::sizeOf(image);
    const bool rgba = core::modeOf(image) == core::ColorMode::Rgba;
    stream << (rgba ? kRgbaTag : kIndexedTag) << qint32(size.width) << qint32(size.height);
    std::visit(
        [&](const auto& concrete) {
            const auto pixels = concrete.data();
            stream.writeRawData(reinterpret_cast<const char*>(pixels.data()),
                static_cast<int>(pixels.size_bytes()));
        },
        image);
    return bytes;
}

/// Inverso de encodeImage; vazio se os dados forem inválidos.
std::optional<core::AnyImage> decodeImage(const QByteArray& bytes)
{
    QDataStream stream(bytes);
    qint32 tag = -1;
    qint32 width = 0;
    qint32 height = 0;
    stream >> tag >> width >> height;
    constexpr int kLimit = core::IndexedImage::kMaxDimension;
    if ((tag != kIndexedTag && tag != kRgbaTag) || width <= 0 || height <= 0 || width > kLimit
        || height > kLimit) {
        return std::nullopt;
    }
    core::AnyImage image = core::blankImage(
        tag == kRgbaTag ? core::ColorMode::Rgba : core::ColorMode::Indexed, { width, height });
    const bool complete = std::visit(
        [&](auto& concrete) {
            auto pixels = concrete.data();
            const auto size = static_cast<int>(pixels.size_bytes());
            return stream.readRawData(reinterpret_cast<char*>(pixels.data()), size) == size;
        },
        image);
    return complete ? std::optional<core::AnyImage>(std::move(image)) : std::nullopt;
}

/// Converte uma imagem de outro programa para o modo do documento: RGBA é
/// copiada sem perdas; para indexada, cada cor vira a mais próxima da paleta
/// (pixels quase transparentes viram o índice 0).
core::AnyImage importForeignImage(
    const QImage& source, core::ColorMode mode, const core::Palette& palette)
{
    const io::ImageData data = io::ImageFile::fromQImage(source);
    // Indexada vinda de fora: interpretada com a PRÓPRIA paleta dela.
    const core::AnyImage asRgba
        = core::convertImage(data.image, core::ColorMode::Rgba, data.palette);
    return core::convertImage(asRgba, mode, palette);
}

/// Fora da seleção, devolve os pixels originais: "after" e "before" cobrem o
/// retângulo "area", e só os pixels selecionados ficam com o valor novo.
void keepOutside(core::AnyImage& after, const core::AnyImage& before, const core::Rect& area,
    const core::Selection& selection)
{
    if (selection.isEmpty() || selection.isRectangular()) {
        return; // retângulo: toda a área é selecionada
    }
    std::visit(
        [&](auto& target) {
            const auto& source = std::get<std::decay_t<decltype(target)>>(before);
            for (int y = 0; y < area.height; ++y) {
                for (int x = 0; x < area.width; ++x) {
                    if (!selection.contains({ area.x + x, area.y + y })) {
                        target.setPixel({ x, y }, source.pixel(x, y));
                    }
                }
            }
        },
        after);
}

/// Aplica uma transformação genérica (template) ao tipo concreto da imagem.
template <typename Function>
core::AnyImage mapImage(const core::AnyImage& image, Function&& function)
{
    return std::visit(
        [&](const auto& concrete) -> core::AnyImage { return function(concrete); }, image);
}

} // namespace

EditOperations::EditOperations(EditorSession* session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
}

void EditOperations::applyToWorkArea(const UndoLabel& undoLabel,
    const std::function<core::AnyImage(const core::AnyImage&)>& function)
{
    m_session->cancelTool();
    m_session->commitTransform();
    Document* document = m_session->activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::Rect area = document->workArea();
    core::AnyImage before = core::copyRegion(document->canvas(), area);
    core::AnyImage after = function(before);
    keepOutside(after, before, area, document->selection());
    if (after == before) {
        return; // nada mudou: não polui o histórico
    }
    document->undoStack()->push(new RegionCommand(document, document->activeCel(), area.topLeft(),
        std::move(before), std::move(after), undoLabel));
}

/// Pixel usado para preencher lacunas (rotação) no modo do tipo informado.
template <typename Pixel> Pixel EditOperations::fillPixel() const
{
    if constexpr (std::is_same_v<Pixel, core::ColorIndex>) {
        return m_session->backgroundColor();
    } else {
        return core::Rgba { }; // transparente
    }
}

// -----------------------------------------------------------------------------
// Área de transferência
// -----------------------------------------------------------------------------
void EditOperations::copy()
{
    const Document* document = m_session->activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::Rect area = document->workArea();
    core::AnyImage region = core::copyRegion(document->canvas(), area);
    // Seleção de forma livre: o que fica fora dela vai como "vazio" (fundo
    // na indexada, transparente na RGBA).
    const core::Selection& selection = document->selection();
    if (!selection.isEmpty() && !selection.isRectangular()) {
        std::visit(
            [&](auto& image) {
                using Pixel = typename std::decay_t<decltype(image)>::PixelType;
                for (int y = 0; y < area.height; ++y) {
                    for (int x = 0; x < area.width; ++x) {
                        if (!selection.contains({ area.x + x, area.y + y })) {
                            image.setPixel({ x, y }, fillPixel<Pixel>());
                        }
                    }
                }
            },
            region);
    }

    auto* mime = new QMimeData;
    mime->setData(mimeType(), encodeImage(region));
    // Imagem comum (com alfa, se RGBA), para colar em outros programas.
    mime->setImageData(io::ImageFile::toQImage({ region, m_session->palette() }));
    QApplication::clipboard()->setMimeData(mime);
}

void EditOperations::cut()
{
    copy();
    clear();
}

bool EditOperations::canPaste()
{
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    return mime != nullptr && (mime->hasFormat(mimeType()) || mime->hasImage());
}

void EditOperations::paste()
{
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (mime == nullptr) {
        return;
    }
    Document* document = m_session->activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::ColorMode mode = document->colorMode();
    std::optional<core::AnyImage> pasted;
    if (mime->hasFormat(mimeType())) {
        // Do próprio Spritenator: índices/cores exatos; se o modo for outro,
        // converte com a paleta atual.
        if (const auto decoded = decodeImage(mime->data(mimeType()))) {
            pasted = core::convertImage(*decoded, mode, m_session->palette());
        }
    } else if (mime->hasImage()) {
        pasted = importForeignImage(
            qvariant_cast<QImage>(mime->imageData()), mode, m_session->palette());
    }
    if (!pasted) {
        return;
    }

    m_session->cancelTool();
    m_session->commitTransform();
    const core::Point origin
        = document->hasSelection() ? document->selection().bounds().topLeft() : core::Point { };
    const core::Rect target = core::Rect::fromPointAndSize(origin, core::sizeOf(*pasted))
                                  .intersected(core::boundsOf(document->canvas()));
    if (target.isEmpty()) {
        return;
    }
    core::AnyImage before = core::copyRegion(document->canvas(), target);
    core::AnyImage after = before;
    const core::Point offset = origin - target.topLeft();
    // Regras de colar: máscara + "efeito de cor transparente" (F8).
    if (auto* indexed = std::get_if<core::IndexedImage>(&after)) {
        indexed->paste(std::get<core::IndexedImage>(*pasted), offset, m_session->pasteRules());
    } else {
        std::get<core::RgbaImage>(after).paste(
            std::get<core::RgbaImage>(*pasted), offset, m_session->rgbaPasteRules());
    }
    document->undoStack()->push(new RegionCommand(document, document->activeCel(), target.topLeft(),
        std::move(before), std::move(after), UndoLabel { UndoOperation::Paste }));
    document->setSelection(target); // o conteúdo colado fica selecionado
}

void EditOperations::clear()
{
    // Indexada: cor de fundo (respeitando a máscara); RGBA: transparente.
    applyToWorkArea(UndoLabel { UndoOperation::Clear }, [&](const core::AnyImage& region) {
        core::AnyImage result = region;
        if (auto* indexed = std::get_if<core::IndexedImage>(&result)) {
            indexed->fillRect(
                indexed->bounds(), m_session->backgroundColor(), m_session->drawingRules());
        } else {
            std::get<core::RgbaImage>(result).fill(core::Rgba { });
        }
        return result;
    });
}

void EditOperations::selectAll()
{
    m_session->commitTransform();
    if (Document* document = m_session->activeDocument()) {
        document->setSelection(core::boundsOf(document->canvas()));
    }
}

void EditOperations::deselect()
{
    m_session->cancelTool();
    m_session->commitTransform();
    if (Document* document = m_session->activeDocument()) {
        document->clearSelection();
    }
}

void EditOperations::invertSelection()
{
    m_session->commitTransform();
    if (Document* document = m_session->activeDocument()) {
        document->setSelection(document->selection().inverted(core::boundsOf(document->canvas())));
    }
}

void EditOperations::moveSelection(int dx, int dy)
{
    const Document* document = m_session->activeDocument();
    if (document == nullptr || !document->hasSelection()) {
        return;
    }
    SelectionTransform* transform = m_session->selectionTransform();
    if (transform->begin()) {
        transform->translate(dx, dy); // fica flutuando até ser aplicada
    }
}

void EditOperations::moveSelectionOutline(int dx, int dy)
{
    m_session->commitTransform();
    if (Document* document = m_session->activeDocument(); document && document->hasSelection()) {
        // Sem recorte ao canvas no meio do caminho: só a parte visível fica.
        document->setSelection(document->selection().translated({ dx, dy }));
    }
}

bool EditOperations::transformSelection(
    UndoOperation operation, const std::function<void(SelectionTransform&)>& change)
{
    const Document* document = m_session->activeDocument();
    if (document == nullptr || !document->hasSelection()) {
        return false;
    }
    SelectionTransform* transform = m_session->selectionTransform();
    const bool alreadyActive = transform->isActive() && transform->document() == document;
    if (!transform->begin()) {
        return false;
    }
    change(*transform);
    if (!alreadyActive) {
        transform->commit(operation); // operação avulsa: um passo de desfazer
    }
    return true;
}

// -----------------------------------------------------------------------------
// Transformações
// -----------------------------------------------------------------------------
void EditOperations::transform(TransformKind kind)
{
    // Cada caso: rótulo de desfazer + transformação genérica do núcleo.
    const auto apply = [this](UndoOperation operation, auto&& function) {
        applyToWorkArea(UndoLabel { operation },
            [&](const core::AnyImage& image) { return mapImage(image, function); });
    };
    const auto rotate90 = [this](bool clockwise) {
        return [this, clockwise](const auto& image) {
            using Pixel = typename std::decay_t<decltype(image)>::PixelType;
            return core::rotated90(image, clockwise, fillPixel<Pixel>());
        };
    };
    // Com seleção, giros e espelhos levam a forma junto (transformação livre).
    switch (kind) {
    case TransformKind::FlipVertical:
        if (transformSelection(UndoOperation::FlipVertically,
                [](SelectionTransform& transform) { transform.flip(false); })) {
            return;
        }
        break;
    case TransformKind::FlipHorizontal:
        if (transformSelection(UndoOperation::FlipHorizontally,
                [](SelectionTransform& transform) { transform.flip(true); })) {
            return;
        }
        break;
    case TransformKind::RotateClockwise:
        if (transformSelection(UndoOperation::RotateRight,
                [](SelectionTransform& transform) { transform.rotate90(true); })) {
            return;
        }
        break;
    case TransformKind::RotateCounterClockwise:
        if (transformSelection(UndoOperation::RotateLeft,
                [](SelectionTransform& transform) { transform.rotate90(false); })) {
            return;
        }
        break;
    default: break;
    }

    switch (kind) {
    case TransformKind::ShiftUp:
        apply(
            UndoOperation::ShiftUp, [](const auto& image) { return core::shifted(image, 0, -1); });
        break;
    case TransformKind::ShiftRight:
        apply(UndoOperation::ShiftRight,
            [](const auto& image) { return core::shifted(image, 1, 0); });
        break;
    case TransformKind::ShiftDown:
        apply(
            UndoOperation::ShiftDown, [](const auto& image) { return core::shifted(image, 0, 1); });
        break;
    case TransformKind::ShiftLeft:
        apply(UndoOperation::ShiftLeft,
            [](const auto& image) { return core::shifted(image, -1, 0); });
        break;
    case TransformKind::FlipVertical:
        apply(UndoOperation::FlipVertically,
            [](const auto& image) { return core::flippedVertically(image); });
        break;
    case TransformKind::FlipHorizontal:
        apply(UndoOperation::FlipHorizontally,
            [](const auto& image) { return core::flippedHorizontally(image); });
        break;
    case TransformKind::MirrorVertical:
        apply(UndoOperation::VerticalSymmetry,
            [](const auto& image) { return core::mirroredVertically(image); });
        break;
    case TransformKind::MirrorHorizontal:
        apply(UndoOperation::HorizontalSymmetry,
            [](const auto& image) { return core::mirroredHorizontally(image); });
        break;
    case TransformKind::RotateClockwise: apply(UndoOperation::RotateRight, rotate90(true)); break;
    case TransformKind::RotateCounterClockwise:
        apply(UndoOperation::RotateLeft, rotate90(false));
        break;
    }
}

void EditOperations::rotate(double degrees)
{
    if (transformSelection(UndoOperation::FreeRotation,
            [degrees](SelectionTransform& transform) { transform.rotateBy(degrees); })) {
        return;
    }
    applyToWorkArea(UndoLabel { UndoOperation::FreeRotation }, [&](const core::AnyImage& region) {
        return mapImage(region, [&](const auto& image) {
            using Pixel = typename std::decay_t<decltype(image)>::PixelType;
            return core::rotated(image, degrees, fillPixel<Pixel>());
        });
    });
}

void EditOperations::stretch(core::Size newSize)
{
    if (newSize.isEmpty()) {
        return;
    }
    // Com seleção: escala a forma junto, mantendo o canto superior esquerdo.
    if (transformSelection(UndoOperation::Stretch, [newSize](SelectionTransform& transform) {
            const core::Rect bounds = transform.originalBounds();
            transform.scaleTo(newSize);
            core::TransformParams params = transform.params();
            params.dx = (newSize.width - bounds.width) / 2.0;
            params.dy = (newSize.height - bounds.height) / 2.0;
            transform.setParams(params);
        })) {
        return;
    }
    m_session->cancelTool();
    m_session->commitTransform();
    Document* document = m_session->activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::Rect oldArea = document->workArea();
    const core::Rect newArea = core::Rect::fromPointAndSize(oldArea.topLeft(), newSize);
    // A região afetada cobre a área antiga e a nova (recortadas ao canvas).
    const core::Rect affected
        = oldArea.united(newArea).intersected(core::boundsOf(document->canvas()));

    core::AnyImage before = core::copyRegion(document->canvas(), affected);
    core::AnyImage after = before;
    const core::Point offset = oldArea.topLeft() - affected.topLeft();
    // A área antiga é limpa (pixel de fundo do modo) e recebe o conteúdo escalado.
    core::pasteImage(after, m_session->blankLike(before, oldArea.size()), offset);
    const core::AnyImage scaledContent = mapImage(core::copyRegion(document->canvas(), oldArea),
        [&](const auto& image) { return core::scaled(image, newSize); });
    core::pasteImage(after, scaledContent, offset);
    if (after == before) {
        return;
    }
    document->undoStack()->push(
        new RegionCommand(document, document->activeCel(), affected.topLeft(), std::move(before),
            std::move(after), UndoLabel { UndoOperation::Stretch }));
    document->setSelection(newArea);
}

// -----------------------------------------------------------------------------
// Sombra e texto
// -----------------------------------------------------------------------------
void EditOperations::prepareShadow()
{
    applyToWorkArea(UndoLabel { UndoOperation::PrepareShadow }, [&](const core::AnyImage& region) {
        if (const auto* indexed = std::get_if<core::IndexedImage>(&region)) {
            return core::AnyImage(
                core::shadowed(*indexed, m_session->backgroundColor(), m_session->drawColor()));
        }
        // RGBA: pixels não transparentes recebem a cor primária (alfa mantido).
        return core::AnyImage(
            core::shadowed(std::get<core::RgbaImage>(region), m_session->primaryRgba()));
    });
}

core::IndexedImage EditOperations::renderText(const QString& text, const QFont& font)
{
    if (text.isEmpty()) {
        return { };
    }
    // Sem suavização: em pixel art cada pixel é "tudo ou nada".
    QFont crisp = font;
    crisp.setStyleStrategy(QFont::NoAntialias);
    const QFontMetrics metrics(crisp);
    const QRect bounds
        = metrics.boundingRect(QRect(0, 0, 4096, 4096), Qt::AlignLeft | Qt::AlignTop, text);
    if (bounds.isEmpty()) {
        return { };
    }

    QImage canvas(bounds.size(), QImage::Format_RGB32);
    canvas.fill(Qt::black);
    {
        QPainter painter(&canvas);
        painter.setFont(crisp);
        painter.setPen(Qt::white);
        painter.setRenderHint(QPainter::TextAntialiasing, false);
        painter.drawText(QRect(QPoint(0, 0), bounds.size()), Qt::AlignLeft | Qt::AlignTop, text);
    }

    core::IndexedImage mask({ canvas.width(), canvas.height() }, 0);
    for (int y = 0; y < canvas.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
        for (int x = 0; x < canvas.width(); ++x) {
            if (qGray(line[x]) > 127) {
                mask.setPixel({ x, y }, 1);
            }
        }
    }
    return mask;
}

void EditOperations::drawText(const QString& text, const QFont& font)
{
    const core::IndexedImage mask = renderText(text, font);
    if (mask.isEmpty()) {
        return;
    }
    m_session->cancelTool();
    m_session->commitTransform();
    Document* document = m_session->activeDocument();
    if (document == nullptr) {
        return;
    }
    const core::Point origin = document->workArea().topLeft();

    document->beginEdit();
    document->editLive([&](core::AnyImage& image) {
        for (int y = 0; y < mask.height(); ++y) {
            for (int x = 0; x < mask.width(); ++x) {
                if (mask.pixel(x, y) != 0) {
                    m_session->plot(image, origin + core::Point { x, y }, ColorRole::Primary);
                }
            }
        }
        return core::Rect::fromPointAndSize(origin, mask.size());
    });
    document->commitEdit(UndoLabel { UndoOperation::Text });
}

} // namespace spritenator::gui
