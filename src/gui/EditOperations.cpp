// =============================================================================
// EditOperations.cpp
// =============================================================================
#include "EditOperations.h"

#include "DrawBuffer.h"
#include "EditorSession.h"
#include "commands/ImageCommands.h"
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

namespace spritenator::gui {

namespace {

QString mimeType()
{
    return QString::fromLatin1(EditOperations::kIndexedMimeType);
}

/// Serializa os índices: largura, altura e bytes (formato privado).
QByteArray encodeIndexed(const core::IndexedImage& image)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << qint32(image.width()) << qint32(image.height());
    const auto pixels = image.data();
    stream.writeRawData(
        reinterpret_cast<const char*>(pixels.data()), static_cast<int>(pixels.size()));
    return bytes;
}

/// Inverso de encodeIndexed; devolve imagem vazia se os dados forem inválidos.
core::IndexedImage decodeIndexed(const QByteArray& bytes)
{
    QDataStream stream(bytes);
    qint32 width = 0;
    qint32 height = 0;
    stream >> width >> height;
    if (width <= 0 || height <= 0 || width > core::IndexedImage::kMaxDimension
        || height > core::IndexedImage::kMaxDimension) {
        return { };
    }
    core::IndexedImage image({ width, height });
    auto pixels = image.data();
    if (stream.readRawData(reinterpret_cast<char*>(pixels.data()), static_cast<int>(pixels.size()))
        != static_cast<int>(pixels.size())) {
        return { };
    }
    return image;
}

/// Converte uma imagem qualquer (de outro programa) para índices da paleta
/// atual, escolhendo a cor mais próxima de cada pixel.
core::IndexedImage indexFromForeignImage(const QImage& source, const core::Palette& palette)
{
    const QImage rgb = source.convertToFormat(QImage::Format_RGB32);
    core::IndexedImage image({ rgb.width(), rgb.height() });
    for (int y = 0; y < rgb.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); ++x) {
            const core::Rgb color { static_cast<std::uint8_t>(qRed(line[x])),
                static_cast<std::uint8_t>(qGreen(line[x])),
                static_cast<std::uint8_t>(qBlue(line[x])) };
            image.setPixel({ x, y }, palette.nearestIndex(color));
        }
    }
    return image;
}

} // namespace

EditOperations::EditOperations(EditorSession* session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
}

void EditOperations::applyToWorkArea(const UndoLabel& undoLabel,
    const std::function<core::IndexedImage(const core::IndexedImage&)>& function)
{
    m_session->cancelTool();
    DrawBuffer* buffer = m_session->buffer();
    const core::Rect area = buffer->workArea();
    core::IndexedImage before = buffer->image().copy(area);
    core::IndexedImage after = function(before);
    if (after == before) {
        return; // nada mudou: não polui o histórico
    }
    buffer->undoStack()->push(
        new RegionCommand(buffer, area.topLeft(), std::move(before), std::move(after), undoLabel));
}

// -----------------------------------------------------------------------------
// Área de transferência
// -----------------------------------------------------------------------------
void EditOperations::copy()
{
    const DrawBuffer* buffer = m_session->buffer();
    const core::IndexedImage region = buffer->image().copy(buffer->workArea());

    auto* mime = new QMimeData;
    mime->setData(mimeType(), encodeIndexed(region));
    // Imagem comum, para colar em outros programas.
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
    core::IndexedImage pasted;
    if (mime->hasFormat(mimeType())) {
        pasted = decodeIndexed(mime->data(mimeType()));
    } else if (mime->hasImage()) {
        pasted
            = indexFromForeignImage(qvariant_cast<QImage>(mime->imageData()), m_session->palette());
    }
    if (pasted.isEmpty()) {
        return;
    }

    m_session->cancelTool();
    DrawBuffer* buffer = m_session->buffer();
    const core::Point origin
        = buffer->selection() ? buffer->selection()->topLeft() : core::Point { };
    const core::Rect target
        = core::Rect::fromPointAndSize(origin, pasted.size()).intersected(buffer->image().bounds());
    if (target.isEmpty()) {
        return;
    }
    core::IndexedImage before = buffer->image().copy(target);
    core::IndexedImage after = before;
    after.paste(pasted, origin - target.topLeft(), m_session->drawingRules());
    buffer->undoStack()->push(new RegionCommand(buffer, target.topLeft(), std::move(before),
        std::move(after), UndoLabel { UndoOperation::Paste }));
    buffer->setSelection(target); // o conteúdo colado fica selecionado
}

void EditOperations::clear()
{
    const core::ColorIndex background = m_session->backgroundColor();
    const core::WriteRules rules = m_session->drawingRules();
    applyToWorkArea(UndoLabel { UndoOperation::Clear }, [&](const core::IndexedImage& region) {
        core::IndexedImage result = region;
        result.fillRect(result.bounds(), background, rules);
        return result;
    });
}

void EditOperations::selectAll()
{
    m_session->buffer()->setSelection(m_session->buffer()->image().bounds());
}

// -----------------------------------------------------------------------------
// Transformações
// -----------------------------------------------------------------------------
void EditOperations::transform(TransformKind kind)
{
    const core::ColorIndex fill = m_session->backgroundColor();
    switch (kind) {
    case TransformKind::ShiftUp:
        applyToWorkArea(UndoLabel { UndoOperation::ShiftUp },
            [](const auto& image) { return core::shifted(image, 0, -1); });
        break;
    case TransformKind::ShiftRight:
        applyToWorkArea(UndoLabel { UndoOperation::ShiftRight },
            [](const auto& image) { return core::shifted(image, 1, 0); });
        break;
    case TransformKind::ShiftDown:
        applyToWorkArea(UndoLabel { UndoOperation::ShiftDown },
            [](const auto& image) { return core::shifted(image, 0, 1); });
        break;
    case TransformKind::ShiftLeft:
        applyToWorkArea(UndoLabel { UndoOperation::ShiftLeft },
            [](const auto& image) { return core::shifted(image, -1, 0); });
        break;
    case TransformKind::FlipVertical:
        applyToWorkArea(UndoLabel { UndoOperation::FlipVertically }, core::flippedVertically);
        break;
    case TransformKind::FlipHorizontal:
        applyToWorkArea(UndoLabel { UndoOperation::FlipHorizontally }, core::flippedHorizontally);
        break;
    case TransformKind::MirrorVertical:
        applyToWorkArea(UndoLabel { UndoOperation::VerticalSymmetry }, core::mirroredVertically);
        break;
    case TransformKind::MirrorHorizontal:
        applyToWorkArea(
            UndoLabel { UndoOperation::HorizontalSymmetry }, core::mirroredHorizontally);
        break;
    case TransformKind::RotateClockwise:
        applyToWorkArea(UndoLabel { UndoOperation::RotateRight },
            [fill](const auto& image) { return core::rotated90(image, true, fill); });
        break;
    case TransformKind::RotateCounterClockwise:
        applyToWorkArea(UndoLabel { UndoOperation::RotateLeft },
            [fill](const auto& image) { return core::rotated90(image, false, fill); });
        break;
    }
}

void EditOperations::rotate(double degrees)
{
    const core::ColorIndex fill = m_session->backgroundColor();
    applyToWorkArea(UndoLabel { UndoOperation::FreeRotation },
        [&](const core::IndexedImage& image) { return core::rotated(image, degrees, fill); });
}

void EditOperations::stretch(core::Size newSize)
{
    if (newSize.isEmpty()) {
        return;
    }
    m_session->cancelTool();
    DrawBuffer* buffer = m_session->buffer();
    const core::Rect oldArea = buffer->workArea();
    const core::Rect newArea = core::Rect::fromPointAndSize(oldArea.topLeft(), newSize);
    // A região afetada cobre a área antiga e a nova (recortadas ao buffer).
    const core::Rect affected = oldArea.united(newArea).intersected(buffer->image().bounds());

    core::IndexedImage before = buffer->image().copy(affected);
    core::IndexedImage after = before;
    const core::Point offset = oldArea.topLeft() - affected.topLeft();
    after.fillRect(
        core::Rect::fromPointAndSize(offset, oldArea.size()), m_session->backgroundColor());
    after.paste(core::scaled(buffer->image().copy(oldArea), newSize), offset);
    if (after == before) {
        return;
    }
    buffer->undoStack()->push(new RegionCommand(buffer, affected.topLeft(), std::move(before),
        std::move(after), UndoLabel { UndoOperation::Stretch }));
    buffer->setSelection(newArea);
}

// -----------------------------------------------------------------------------
// Sombra e texto
// -----------------------------------------------------------------------------
void EditOperations::prepareShadow()
{
    const core::ColorIndex background = m_session->backgroundColor();
    const core::ColorIndex shadow = m_session->drawColor();
    applyToWorkArea(UndoLabel { UndoOperation::PrepareShadow },
        [&](const core::IndexedImage& image) { return core::shadowed(image, background, shadow); });
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
    DrawBuffer* buffer = m_session->buffer();
    const core::Point origin = buffer->workArea().topLeft();
    const core::ColorIndex color = m_session->drawColor();

    buffer->beginEdit();
    buffer->editLive([&](core::IndexedImage& image) {
        for (int y = 0; y < mask.height(); ++y) {
            for (int x = 0; x < mask.width(); ++x) {
                if (mask.pixel(x, y) != 0) {
                    m_session->plot(image, origin + core::Point { x, y }, color);
                }
            }
        }
        return core::Rect::fromPointAndSize(origin, mask.size());
    });
    buffer->commitEdit(UndoLabel { UndoOperation::Text });
}

} // namespace spritenator::gui
