// =============================================================================
// CanvasView.cpp
// =============================================================================
#include "CanvasView.h"

#include "Document.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace spritenator::gui {

CanvasView::CanvasView(Document* document, QWidget* parent)
    : QWidget(parent)
    , m_document(document)
{
    setMouseTracking(true); // recebe movimento mesmo sem botão pressionado
    setAttribute(Qt::WA_OpaquePaintEvent); // pintamos todos os pixels: evita limpar o fundo
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::ClickFocus); // setas do teclado navegam entre células

    // Qualquer alteração do documento redesenha apenas a área afetada.
    connect(m_document, &Document::canvasChanged, this,
        [this](const core::Rect& area) { update(toWidget(area)); });
    connect(m_document, &Document::canvasResized, this, &CanvasView::updateGeometryFromDocument);
    connect(m_document, &Document::paletteChanged, this, qOverload<>(&QWidget::update));

    updateGeometryFromDocument();
}

void CanvasView::setScale(int scale)
{
    scale = std::max(1, scale);
    if (scale != m_scale) {
        m_scale = scale;
        updateGeometryFromDocument();
    }
}

void CanvasView::setCharacterLayout(const core::CharacterLayout& layout)
{
    m_layout = layout;
    setCursor(layout.keepMouseCursor ? Qt::ArrowCursor : Qt::CrossCursor);
    setScale(layout.doubleCanvas ? 2 : 1);
    update();
}

void CanvasView::setHoveredCell(const std::optional<core::Rect>& cell)
{
    if (cell == m_hoveredCell) {
        return;
    }
    // Redesenha a posição antiga e a nova do cursor (com margem da borda).
    if (m_hoveredCell) {
        update(toWidget(*m_hoveredCell).adjusted(-2, -2, 2, 2));
    }
    m_hoveredCell = cell;
    if (m_hoveredCell) {
        update(toWidget(*m_hoveredCell).adjusted(-2, -2, 2, 2));
    }
}

QSize CanvasView::sizeHint() const
{
    const core::Size size = m_document->canvas().size();
    return { size.width * m_scale, size.height * m_scale };
}

void CanvasView::updateGeometryFromDocument()
{
    setFixedSize(sizeHint());
    update();
}

core::Point CanvasView::toCanvas(const QPointF& position) const
{
    return { static_cast<int>(std::floor(position.x() / m_scale)),
        static_cast<int>(std::floor(position.y() / m_scale)) };
}

QRect CanvasView::toWidget(const core::Rect& area) const
{
    return { area.x * m_scale, area.y * m_scale, area.width * m_scale, area.height * m_scale };
}

// -----------------------------------------------------------------------------
// Pintura
// -----------------------------------------------------------------------------
void CanvasView::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // Ampliação por "vizinho mais próximo": sem suavização, como pixel art exige.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QRect dirty = event->rect();
    const QRectF source(dirty.x() / double(m_scale), dirty.y() / double(m_scale),
        dirty.width() / double(m_scale), dirty.height() / double(m_scale));
    painter.drawImage(QRectF(dirty), m_document->renderedImage(), source);

    // Cursor de célula: contorno duplo (preto/branco) visível sobre qualquer cor.
    if (m_hoveredCell) {
        const QRect cell = toWidget(*m_hoveredCell);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(cell.adjusted(0, 0, -1, -1));
        painter.setPen(QPen(Qt::white, 1, Qt::DotLine));
        painter.drawRect(cell.adjusted(0, 0, -1, -1));
    }
}

// -----------------------------------------------------------------------------
// Mouse
// -----------------------------------------------------------------------------
void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    const core::Point pixel = toCanvas(event->position());
    setHoveredCell(m_layout.cellAt(pixel));
    emit pixelHovered(pixel);
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    const core::Rect cell = m_layout.cellAt(toCanvas(event->position()));
    setHoveredCell(cell);
    if (event->button() == Qt::LeftButton) {
        emit putRequested(cell);
    } else if (event->button() == Qt::RightButton) {
        emit getRequested(cell);
    }
}

void CanvasView::leaveEvent(QEvent* event)
{
    // Com o foco no canvas o cursor permanece, para a navegação por teclado.
    if (!hasFocus()) {
        setHoveredCell(std::nullopt);
    }
    QWidget::leaveEvent(event);
}

// -----------------------------------------------------------------------------
// Teclado: as setas movem o cursor de célula e trazem a nova célula para a
// área de desenho — percorre os quadros de uma animação rapidamente.
// -----------------------------------------------------------------------------
void CanvasView::keyPressEvent(QKeyEvent* event)
{
    int columns = 0;
    int rows = 0;
    switch (event->key()) {
    case Qt::Key_Left: columns = -1; break;
    case Qt::Key_Right: columns = 1; break;
    case Qt::Key_Up: rows = -1; break;
    case Qt::Key_Down: rows = 1; break;
    default: QWidget::keyPressEvent(event); return;
    }
    const core::Rect current = m_hoveredCell.value_or(m_layout.cellAt({ 0, 0 }));
    const core::Rect next = m_layout.neighbour(current, columns, rows);
    // Só navega se a nova célula ainda tocar o canvas.
    if (next.intersected(m_document->canvas().bounds()).isEmpty()) {
        return;
    }
    setHoveredCell(next);
    emit getRequested(next);
}

} // namespace spritenator::gui
