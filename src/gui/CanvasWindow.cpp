// =============================================================================
// CanvasWindow.cpp
// =============================================================================
#include "CanvasWindow.h"

#include "CanvasView.h"
#include "Document.h"

#include <QScrollBar>

namespace spritenator::gui {

CanvasWindow::CanvasWindow(Document* document, QWidget* parent)
    : QScrollArea(parent)
    , m_document(document)
    , m_view(new CanvasView(document, this))
{
    m_document->setParent(this); // a janela passa a ser dona do documento

    setWidget(m_view);
    setWidgetResizable(false); // a view tem tamanho fixo (canvas x escala)
    setAlignment(Qt::AlignCenter);
    setBackgroundRole(QPalette::Dark);
    setAttribute(Qt::WA_DeleteOnClose);

    connect(m_document, &Document::modifiedChanged, this, &QWidget::setWindowModified);
    connect(m_document, &Document::filePathChanged, this, &CanvasWindow::updateTitle);
    connect(m_document, &Document::canvasResized, this, &CanvasWindow::updateTitle);
    updateTitle();
}

QSize CanvasWindow::sizeHint() const
{
    // Canvas + moldura da área de rolagem.
    const int frame = 2 * frameWidth();
    return m_view->sizeHint() + QSize(frame, frame);
}

void CanvasWindow::updateTitle()
{
    const auto size = m_document->canvas().size();
    setWindowTitle(QStringLiteral("%1[*] (%2x%3)")
            .arg(m_document->displayName())
            .arg(size.width)
            .arg(size.height));
    setWindowModified(m_document->isModified());
}

} // namespace spritenator::gui
