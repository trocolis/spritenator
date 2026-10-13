// =============================================================================
// DrawAreaView.cpp
// =============================================================================
#include "DrawAreaView.h"

#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace spritenator::gui {

namespace {

/// Zoom mínimo para exibir a grade de pixels (abaixo disso ela polui a imagem).
constexpr int kMinZoomForPixelGrid = 4;

// Cores das grades: semitransparentes para não esconder os pixels.
const QColor kPixelGridColor(128, 128, 128, 90);
const QColor kSubGridColor(0, 160, 255, 150);
const QColor kCellGridColor(255, 64, 64, 200);

} // namespace

DrawAreaView::DrawAreaView(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus); // recebe Esc para cancelar operações
    setCursor(Qt::CrossCursor);

    DrawBuffer* buffer = m_session->buffer();
    connect(buffer, &DrawBuffer::changed, this, [this](const core::Rect& area) {
        const int zoom = m_session->zoom();
        // +1 pixel de margem para as linhas de grade nas bordas da área.
        update(QRect(area.x * zoom, area.y * zoom, area.width * zoom + 1, area.height * zoom + 1));
    });
    connect(buffer, &DrawBuffer::resized, this, &DrawAreaView::updateSize);
    connect(buffer, &DrawBuffer::selectionChanged, this, qOverload<>(&QWidget::update));
    connect(m_session, &EditorSession::zoomChanged, this, &DrawAreaView::updateSize);
    connect(m_session, &EditorSession::gridsChanged, this, qOverload<>(&QWidget::update));
    connect(m_session, &EditorSession::characterLayoutChanged, this, qOverload<>(&QWidget::update));
    updateSize();
}

QSize DrawAreaView::sizeHint() const
{
    const core::Size size = m_session->buffer()->image().size();
    const int zoom = m_session->zoom();
    return { size.width * zoom + 1, size.height * zoom + 1 }; // +1: última linha da grade
}

void DrawAreaView::updateSize()
{
    setFixedSize(sizeHint());
    update();
}

core::Point DrawAreaView::toBuffer(const QPointF& position) const
{
    const double zoom = m_session->zoom();
    return { static_cast<int>(std::floor(position.x() / zoom)),
        static_cast<int>(std::floor(position.y() / zoom)) };
}

// -----------------------------------------------------------------------------
// Pintura
// -----------------------------------------------------------------------------
void DrawAreaView::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    const QRect dirty = event->rect();
    const int zoom = m_session->zoom();
    const DrawBuffer* buffer = m_session->buffer();

    // Fundo (a coluna/linha extra da grade fica fora da imagem).
    painter.fillRect(dirty, palette().color(QPalette::Dark));

    // Imagem ampliada, sem suavização.
    const QImage& image = buffer->renderedImage();
    painter.drawImage(QRect(0, 0, image.width() * zoom, image.height() * zoom), image);

    paintGrids(painter, dirty);

    // Seleção: retângulo tracejado (preto + branco para contraste).
    if (const auto& selection = buffer->selection()) {
        const QRect area(selection->x * zoom, selection->y * zoom, selection->width * zoom,
            selection->height * zoom);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(area);
        painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
        painter.drawRect(area);
    }

    // Elementos temporários da ferramenta atual.
    m_session->tool().paintOverlay(painter, zoom);
}

void DrawAreaView::paintGrids(QPainter& painter, const QRect& dirty) const
{
    const int zoom = m_session->zoom();
    const core::Size size = m_session->buffer()->image().size();
    const core::CharacterLayout& layout = m_session->characterLayout();
    const int widthPx = size.width * zoom;
    const int heightPx = size.height * zoom;

    // Desenha linhas verticais/horizontais a cada "step" pixels do buffer,
    // limitando-se à área suja para não desperdiçar tempo.
    const auto drawLines = [&](int stepX, int stepY, const QColor& color) {
        painter.setPen(QPen(color, 1));
        for (int x = 0; x <= size.width; x += stepX) {
            const int px = x * zoom;
            if (px >= dirty.left() - 1 && px <= dirty.right() + 1) {
                painter.drawLine(px, 0, px, heightPx);
            }
        }
        for (int y = 0; y <= size.height; y += stepY) {
            const int py = y * zoom;
            if (py >= dirty.top() - 1 && py <= dirty.bottom() + 1) {
                painter.drawLine(0, py, widthPx, py);
            }
        }
    };

    if (m_session->mainGridVisible() && zoom >= kMinZoomForPixelGrid) {
        drawLines(1, 1, kPixelGridColor);
    }
    if (m_session->subGridVisible()) {
        drawLines(layout.subGridSize.width, layout.subGridSize.height, kSubGridColor);
    }
    // Contorno das células: útil quando o buffer contém vários quadros
    // (após "Transferir tudo"). Só aparece se houver mais de uma célula.
    if (size.width > layout.cellSize.width || size.height > layout.cellSize.height) {
        drawLines(layout.cellSize.width, layout.cellSize.height, kCellGridColor);
    }
}

// -----------------------------------------------------------------------------
// Mouse e teclado: repassados à ferramenta atual
// -----------------------------------------------------------------------------
void DrawAreaView::mousePressEvent(QMouseEvent* event)
{
    if (m_activeButton != Qt::NoButton) {
        return; // ignora um segundo botão durante um traço
    }
    m_activeButton = event->button();
    m_session->tool().press(*m_session, toBuffer(event->position()), event->button());
    update();
}

void DrawAreaView::mouseMoveEvent(QMouseEvent* event)
{
    const core::Point pixel = toBuffer(event->position());
    emit pixelHovered(pixel);
    if (m_activeButton != Qt::NoButton) {
        m_session->tool().move(*m_session, pixel);
        update(); // atualiza elementos temporários (ex.: retângulo de seleção)
    }
}

void DrawAreaView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != m_activeButton) {
        return;
    }
    m_activeButton = Qt::NoButton;
    m_session->tool().release(*m_session, toBuffer(event->position()));
    update();
}

void DrawAreaView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_session->cancelTool();
        m_activeButton = Qt::NoButton;
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace spritenator::gui
