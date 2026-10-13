// =============================================================================
// PaletteView.cpp
// =============================================================================
#include "PaletteView.h"

#include "gui/EditorSession.h"
#include "gui/ImageRendering.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>

namespace spritenator::gui {

namespace {

constexpr int kMinCellSize = 10;
constexpr int kMaxCellSize = 16;

} // namespace

PaletteView::PaletteView(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(m_session, &EditorSession::paletteChanged, this, [this] {
        updateGeometry(); // 16 <-> 256 cores muda a altura
        update();
    });
    connect(m_session, &EditorSession::colorsChanged, this, qOverload<>(&QWidget::update));
}

int PaletteView::rows() const
{
    return (m_session->palette().size() + kColumns - 1) / kColumns;
}

int PaletteView::cellSize() const
{
    return std::clamp((width() - 1) / kColumns, kMinCellSize, kMaxCellSize);
}

QSize PaletteView::sizeHint() const
{
    return { kColumns * kMaxCellSize + 1, rows() * kMaxCellSize + 1 };
}

QSize PaletteView::minimumSizeHint() const
{
    return { kColumns * kMinCellSize + 1, rows() * kMinCellSize + 1 };
}

QRect PaletteView::cellRect(int index) const
{
    const int size = cellSize();
    return { (index % kColumns) * size, (index / kColumns) * size, size, size };
}

int PaletteView::indexAt(const QPoint& position) const
{
    const int size = cellSize();
    const int column = position.x() / size;
    const int row = position.y() / size;
    if (position.x() < 0 || position.y() < 0 || column >= kColumns) {
        return -1;
    }
    const int index = row * kColumns + column;
    return m_session->palette().isValidIndex(index) ? index : -1;
}

void PaletteView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const core::Palette& palette = m_session->palette();

    for (int index = 0; index < palette.size(); ++index) {
        const QRect cell = cellRect(index);
        painter.fillRect(cell, toQColor(palette.color(index)));
        painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
        painter.drawRect(cell);
    }

    painter.setBrush(Qt::NoBrush);
    // Cor de fundo: moldura tracejada.
    const QRect background = cellRect(m_session->backgroundColor()).adjusted(1, 1, -1, -1);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(background);
    painter.setPen(QPen(Qt::white, 1, Qt::DotLine));
    painter.drawRect(background);

    // Cor de desenho: moldura dupla sólida (mais destacada).
    const QRect draw = cellRect(m_session->drawColor());
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(draw);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(draw.adjusted(1, 1, -1, -1));
}

void PaletteView::mousePressEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (index < 0) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_session->setDrawColor(index);
    } else if (event->button() == Qt::RightButton) {
        m_session->setBackgroundColor(index);
    }
    emit colorClicked(index, event->button());
}

bool PaletteView::event(QEvent* event)
{
    // Dica com o índice e o valor RGB da cor sob o mouse.
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const int index = indexAt(help->pos());
        if (index >= 0) {
            const core::Rgb& color = m_session->palette().color(index);
            QToolTip::showText(help->globalPos(),
                tr("Color %1\nR: %2  G: %3  B: %4")
                    .arg(index)
                    .arg(color.red)
                    .arg(color.green)
                    .arg(color.blue),
                this, cellRect(index));
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

} // namespace spritenator::gui
