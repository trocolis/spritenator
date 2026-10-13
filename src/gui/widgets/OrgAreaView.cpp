// =============================================================================
// OrgAreaView.cpp
// =============================================================================
#include "OrgAreaView.h"

#include "gui/DrawBuffer.h"
#include "gui/EditorSession.h"
#include "gui/ImageRendering.h"

#include <QEvent>
#include <QPainter>

namespace spritenator::gui {

OrgAreaView::OrgAreaView(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    retranslate();
    const auto refresh = [this] {
        updateGeometry();
        update();
    };
    connect(m_session->buffer(), &DrawBuffer::changed, this, qOverload<>(&QWidget::update));
    connect(m_session->buffer(), &DrawBuffer::resized, this, refresh);
    connect(m_session, &EditorSession::characterLayoutChanged, this, refresh);
}

void OrgAreaView::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void OrgAreaView::retranslate()
{
    setToolTip(tr("Actual size preview of the drawing area"));
}

int OrgAreaView::scale() const
{
    return m_session->characterLayout().doubleCanvas ? 2 : 1;
}

QSize OrgAreaView::sizeHint() const
{
    const core::Size size = m_session->buffer()->size();
    return QSize(size.width, size.height) * scale() + QSize(8, 8);
}

QSize OrgAreaView::minimumSizeHint() const
{
    return { 32, 32 };
}

void OrgAreaView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Dark));

    // Centraliza a imagem no espaço disponível.
    const QImage& image = m_session->buffer()->renderedImage();
    const QSize scaled = image.size() * scale();
    const QPoint origin((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
    if (m_session->buffer()->colorMode() == core::ColorMode::Rgba) {
        paintTransparencyGrid(painter, QRect(origin, scaled), 4);
    }
    painter.drawImage(QRect(origin, scaled), image);
}

} // namespace spritenator::gui
