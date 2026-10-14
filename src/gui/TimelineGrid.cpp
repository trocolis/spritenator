// =============================================================================
// TimelineGrid.cpp
// =============================================================================
#include "TimelineGrid.h"

#include "Document.h"
#include "IconProvider.h"

#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>

namespace spritenator::gui {

namespace {

constexpr int kRowHeight = 26;
constexpr int kHeaderHeight = 28;
constexpr int kIconWidth = 22; ///< colunas do olho e do cadeado
constexpr int kNameWidth = 150;
constexpr int kLeftWidth = 2 * kIconWidth + kNameWidth;
constexpr int kCellWidth = 30;
constexpr int kIconSize = 16;
/// Atraso para recalcular as células com desenho enquanto se desenha.
constexpr int kFilledDelayMs = 150;

} // namespace

TimelineGrid::TimelineGrid(QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_filledTimer(new QTimer(this))
    , m_renameEditor(new QLineEdit(viewport()))
{
    setFrameShape(QFrame::StyledPanel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setMouseTracking(false);
    m_renameEditor->hide();
    connect(m_renameEditor, &QLineEdit::editingFinished, this, [this] {
        if (m_renameLayer >= 0 && m_renameEditor->isVisible()) {
            const int layer = m_renameLayer;
            m_renameLayer = -1;
            m_renameEditor->hide();
            emit renameRequested(layer, m_renameEditor->text());
        }
    });
    m_filledTimer->setSingleShot(true);
    m_filledTimer->setInterval(kFilledDelayMs);
    connect(m_filledTimer, &QTimer::timeout, this, [this] { refreshFilled(false); });
}

QSize TimelineGrid::sizeHint() const
{
    return { kLeftWidth + 12 * kCellWidth, kHeaderHeight + 4 * kRowHeight + 4 };
}

QSize TimelineGrid::minimumSizeHint() const
{
    return { kLeftWidth + 2 * kCellWidth, kHeaderHeight + 2 * kRowHeight + 4 };
}

void TimelineGrid::setCornerWidget(QWidget* widget)
{
    m_corner = widget;
    widget->setParent(viewport());
    widget->setGeometry(2 * kIconWidth, 2, kNameWidth - 4, kHeaderHeight - 4);
    widget->show();
}

void TimelineGrid::setDocument(Document* document)
{
    if (Document* previous = m_document.data()) {
        disconnect(previous, nullptr, this, nullptr);
    }
    m_document = document;
    m_renameEditor->hide();
    m_renameLayer = -1;
    if (document != nullptr) {
        connect(document, &Document::structureChanged, this, [this] {
            refreshFilled(true);
            updateScrollBars();
            viewport()->update();
        });
        connect(document, &Document::currentCelChanged, this, [this] {
            ensureCurrentVisible();
            viewport()->update();
        });
        // Desenhar muda só a célula atual: atualiza com atraso (agrupa traços).
        connect(document, &Document::canvasChanged, m_filledTimer, qOverload<>(&QTimer::start));
    }
    refreshFilled(true);
    updateScrollBars();
    viewport()->update();
}

// -----------------------------------------------------------------------------
// Geometria
// -----------------------------------------------------------------------------
int TimelineGrid::rowOf(int layer) const
{
    const Document* document = m_document.data();
    return document != nullptr ? document->layerCount() - 1 - layer : 0;
}

int TimelineGrid::layerAtRow(int row) const
{
    const Document* document = m_document.data();
    return document != nullptr ? document->layerCount() - 1 - row : -1;
}

QRect TimelineGrid::frameHeaderRect(int frame) const
{
    return { kLeftWidth + frame * kCellWidth - horizontalScrollBar()->value(), 0, kCellWidth,
        kHeaderHeight };
}

QRect TimelineGrid::cellRect(int frame, int layer) const
{
    return { kLeftWidth + frame * kCellWidth - horizontalScrollBar()->value(),
        kHeaderHeight + rowOf(layer) * kRowHeight - verticalScrollBar()->value(), kCellWidth,
        kRowHeight };
}

QRect TimelineGrid::eyeRect(int layer) const
{
    return { 0, kHeaderHeight + rowOf(layer) * kRowHeight - verticalScrollBar()->value(),
        kIconWidth, kRowHeight };
}

QRect TimelineGrid::lockRect(int layer) const
{
    return eyeRect(layer).translated(kIconWidth, 0);
}

QRect TimelineGrid::nameRect(int layer) const
{
    const QRect eye = eyeRect(layer);
    return { 2 * kIconWidth, eye.y(), kNameWidth, kRowHeight };
}

bool TimelineGrid::cellFilled(int frame, int layer) const
{
    if (frame < 0 || frame >= static_cast<int>(m_filled.size())) {
        return false;
    }
    const auto& row = m_filled[static_cast<std::size_t>(frame)];
    return layer >= 0 && layer < static_cast<int>(row.size())
        && row[static_cast<std::size_t>(layer)];
}

void TimelineGrid::updateScrollBars()
{
    const Document* document = m_document.data();
    const int frames = document != nullptr ? document->frameCount() : 0;
    const int layers = document != nullptr ? document->layerCount() : 0;
    const QSize area = viewport()->size();
    horizontalScrollBar()->setRange(
        0, std::max(0, frames * kCellWidth - (area.width() - kLeftWidth)));
    horizontalScrollBar()->setPageStep(std::max(1, area.width() - kLeftWidth));
    horizontalScrollBar()->setSingleStep(kCellWidth);
    verticalScrollBar()->setRange(
        0, std::max(0, layers * kRowHeight - (area.height() - kHeaderHeight)));
    verticalScrollBar()->setPageStep(std::max(1, area.height() - kHeaderHeight));
    verticalScrollBar()->setSingleStep(kRowHeight);
}

void TimelineGrid::ensureCurrentVisible()
{
    const Document* document = m_document.data();
    if (document == nullptr) {
        return;
    }
    const int left = document->currentFrame() * kCellWidth;
    const int width = viewport()->width() - kLeftWidth;
    QScrollBar* horizontal = horizontalScrollBar();
    if (left < horizontal->value()) {
        horizontal->setValue(left);
    } else if (left + kCellWidth > horizontal->value() + width) {
        horizontal->setValue(left + kCellWidth - width);
    }
    const int top = rowOf(document->currentLayer()) * kRowHeight;
    const int height = viewport()->height() - kHeaderHeight;
    QScrollBar* vertical = verticalScrollBar();
    if (top < vertical->value()) {
        vertical->setValue(top);
    } else if (top + kRowHeight > vertical->value() + height) {
        vertical->setValue(top + kRowHeight - height);
    }
}

void TimelineGrid::refreshFilled(bool all)
{
    const Document* document = m_document.data();
    if (document == nullptr) {
        m_filled.clear();
        return;
    }
    if (all || static_cast<int>(m_filled.size()) != document->frameCount()) {
        m_filled.assign(static_cast<std::size_t>(document->frameCount()),
            std::vector<bool>(static_cast<std::size_t>(document->layerCount()), false));
        for (int frame = 0; frame < document->frameCount(); ++frame) {
            for (int layer = 0; layer < document->layerCount(); ++layer) {
                m_filled[static_cast<std::size_t>(frame)][static_cast<std::size_t>(layer)]
                    = document->celHasContent(frame, layer);
            }
        }
    } else {
        const int frame = document->currentFrame();
        const int layer = document->currentLayer();
        auto& row = m_filled[static_cast<std::size_t>(frame)];
        if (layer < static_cast<int>(row.size())) {
            row[static_cast<std::size_t>(layer)] = document->celHasContent(frame, layer);
        }
    }
    viewport()->update();
}

// -----------------------------------------------------------------------------
// Pintura
// -----------------------------------------------------------------------------
void TimelineGrid::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(viewport());
    const QPalette& colors = palette();
    const QRect area = viewport()->rect();
    painter.fillRect(area, colors.color(QPalette::Base));
    const Document* document = m_document.data();
    if (document == nullptr) {
        return;
    }
    const int frames = document->frameCount();
    const int layers = document->layerCount();
    const QColor text = colors.color(QPalette::Text);
    const QColor highlight = colors.color(QPalette::Highlight);
    QColor line = text;
    line.setAlpha(40);
    QColor empty = text;
    empty.setAlpha(18);
    QColor filled = text;
    filled.setAlpha(80);

    // --- Células (por baixo dos cabeçalhos fixos) ---
    painter.setClipRect(QRect(kLeftWidth, kHeaderHeight, area.width(), area.height()));
    for (int layer = 0; layer < layers; ++layer) {
        const bool visible = document->layers()[static_cast<std::size_t>(layer)].visible;
        for (int frame = 0; frame < frames; ++frame) {
            const QRect cell = cellRect(frame, layer);
            if (!cell.intersects(area)) {
                continue;
            }
            QColor fill = cellFilled(frame, layer) ? filled : empty;
            if (!visible) {
                fill.setAlpha(fill.alpha() / 2); // camada oculta: mais apagada
            }
            painter.fillRect(cell.adjusted(1, 1, -1, -1), fill);
            if (frame == document->currentFrame() && layer == document->currentLayer()) {
                painter.setPen(QPen(highlight, 2));
                painter.drawRect(cell.adjusted(1, 1, -2, -2));
            }
        }
    }
    // Coluna da cena atual: um leve destaque em todas as camadas.
    QColor column = highlight;
    column.setAlpha(40);
    const QRect current = cellRect(document->currentFrame(), 0);
    painter.fillRect(QRect(current.x(), kHeaderHeight, kCellWidth, layers * kRowHeight), column);

    // --- Coluna das camadas (fixa na horizontal) ---
    painter.setClipRect(QRect(0, kHeaderHeight, kLeftWidth, area.height()));
    painter.fillRect(
        QRect(0, kHeaderHeight, kLeftWidth, area.height()), colors.color(QPalette::Window));
    for (int layer = 0; layer < layers; ++layer) {
        const Layer& info = document->layers()[static_cast<std::size_t>(layer)];
        const QRect name = nameRect(layer);
        if (layer == document->currentLayer()) {
            QColor selected = highlight;
            selected.setAlpha(90);
            painter.fillRect(QRect(0, name.y(), kLeftWidth, kRowHeight), selected);
        }
        const auto drawIcon = [&](const QRect& box, const char* icon, bool dim) {
            painter.setOpacity(dim ? 0.35 : 1.0);
            const QRect target(box.center().x() - kIconSize / 2 + 1,
                box.center().y() - kIconSize / 2 + 1, kIconSize, kIconSize);
            IconProvider::icon(QString::fromLatin1(icon)).paint(&painter, target);
            painter.setOpacity(1.0);
        };
        drawIcon(eyeRect(layer), info.visible ? "eye_open" : "eye_closed", !info.visible);
        drawIcon(lockRect(layer), info.locked ? "lock_closed" : "lock_open", !info.locked);
        QFont font = painter.font();
        font.setBold(layer == document->currentLayer());
        painter.setFont(font);
        painter.setPen(info.visible ? text : colors.color(QPalette::Disabled, QPalette::Text));
        painter.drawText(name.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
            painter.fontMetrics().elidedText(info.name, Qt::ElideRight, name.width() - 8));
        painter.setPen(line);
        painter.drawLine(0, name.bottom(), kLeftWidth, name.bottom());
    }

    // --- Linha dos números das cenas (fixa na vertical) ---
    painter.setClipRect(QRect(kLeftWidth, 0, area.width(), kHeaderHeight));
    painter.fillRect(
        QRect(kLeftWidth, 0, area.width(), kHeaderHeight), colors.color(QPalette::Window));
    QFont font = painter.font();
    font.setBold(false);
    painter.setFont(font);
    for (int frame = 0; frame < frames; ++frame) {
        const QRect header = frameHeaderRect(frame);
        if (!header.intersects(area)) {
            continue;
        }
        if (frame == document->currentFrame()) {
            painter.setPen(QPen(highlight, 2));
            painter.drawRect(header.adjusted(2, 2, -2, -2));
        }
        painter.setPen(text);
        painter.drawText(header, Qt::AlignCenter, QString::number(frame + 1));
    }

    // --- Canto: rótulos das colunas do olho e do cadeado ---
    painter.setClipping(false);
    painter.fillRect(QRect(0, 0, kLeftWidth, kHeaderHeight), colors.color(QPalette::Window));
    painter.setOpacity(0.6);
    for (const auto& [x, icon] :
        { std::pair { 0, "eye_open" }, std::pair { kIconWidth, "lock_closed" } }) {
        IconProvider::icon(QString::fromLatin1(icon))
            .paint(&painter, QRect(x + 3, (kHeaderHeight - kIconSize) / 2, kIconSize, kIconSize));
    }
    painter.setOpacity(1.0);
    painter.setPen(line);
    painter.drawLine(0, kHeaderHeight - 1, area.width(), kHeaderHeight - 1);
    painter.drawLine(kLeftWidth - 1, 0, kLeftWidth - 1, area.height());
}

void TimelineGrid::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void TimelineGrid::scrollContentsBy(int /*dx*/, int /*dy*/)
{
    m_renameEditor->hide();
    m_renameLayer = -1;
    viewport()->update();
}

// -----------------------------------------------------------------------------
// Mouse
// -----------------------------------------------------------------------------
void TimelineGrid::mousePressEvent(QMouseEvent* event)
{
    const Document* document = m_document.data();
    if (document == nullptr || event->button() != Qt::LeftButton) {
        return;
    }
    const QPoint position = event->position().toPoint();
    // Linha dos números: escolhe a cena.
    if (position.y() < kHeaderHeight) {
        if (position.x() >= kLeftWidth) {
            const int frame
                = (position.x() - kLeftWidth + horizontalScrollBar()->value()) / kCellWidth;
            if (frame < document->frameCount()) {
                emit frameClicked(frame);
            }
        }
        return;
    }
    const int row = (position.y() - kHeaderHeight + verticalScrollBar()->value()) / kRowHeight;
    const int layer = layerAtRow(row);
    if (layer < 0 || layer >= document->layerCount()) {
        return;
    }
    if (position.x() < kIconWidth) {
        emit visibilityToggled(layer);
    } else if (position.x() < 2 * kIconWidth) {
        emit lockToggled(layer);
    } else if (position.x() < kLeftWidth) {
        emit layerClicked(layer);
    } else {
        const int frame = (position.x() - kLeftWidth + horizontalScrollBar()->value()) / kCellWidth;
        if (frame < document->frameCount()) {
            emit cellClicked(frame, layer);
        }
    }
}

void TimelineGrid::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    const Document* document = m_document.data();
    if (document != nullptr && position.y() >= kHeaderHeight && position.x() >= 2 * kIconWidth
        && position.x() < kLeftWidth) {
        const int row = (position.y() - kHeaderHeight + verticalScrollBar()->value()) / kRowHeight;
        const int layer = layerAtRow(row);
        if (layer >= 0 && layer < document->layerCount()) {
            startRename(layer); // clique duplo no nome: renomear
            return;
        }
    }
    mousePressEvent(event);
}

void TimelineGrid::startRename(int layer)
{
    const Document* document = m_document.data();
    if (document == nullptr) {
        return;
    }
    m_renameLayer = layer;
    m_renameEditor->setGeometry(nameRect(layer).adjusted(1, 1, -1, -1));
    m_renameEditor->setText(document->layers()[static_cast<std::size_t>(layer)].name);
    m_renameEditor->selectAll();
    m_renameEditor->show();
    m_renameEditor->setFocus();
}

} // namespace spritenator::gui
