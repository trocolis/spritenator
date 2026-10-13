// =============================================================================
// CanvasEditor.cpp
// =============================================================================
#include "CanvasEditor.h"

#include "Document.h"
#include "EditorSession.h"
#include "ImageRendering.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace spritenator::gui {

namespace {

/// Espaçamento mínimo (em pixels de tela) para exibir uma grade: abaixo
/// disso as linhas cobririam a imagem.
constexpr double kMinGridSpacing = 4;

/// Zoom mínimo para contornar os pixels destacados (abaixo disso o contorno
/// esconderia o próprio destaque).
constexpr double kMinZoomForHighlightEdges = 4;

/// Tolerância nas comparações de zoom (evita que 0,5 * 2 "não seja" 1).
constexpr double kZoomEpsilon = 1e-6;

/// Um "clique" da roda do mouse (QWheelEvent::angleDelta).
constexpr double kWheelStep = 120;

// Cores das grades: semitransparentes para não esconder os pixels.
const QColor kMainGridColor(128, 128, 128, 90);
const QColor kSubGridColor(0, 160, 255, 150);

// Destaque: tom azulado sobre os pixels + contorno preto/branco.
const QColor kHighlightFill(0, 150, 255, 110);

} // namespace

CanvasEditor::CanvasEditor(Document* document, EditorSession* session, QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_document(document)
    , m_session(session)
{
    m_document->setParent(this); // o editor passa a ser dono do documento

    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus); // recebe Esc para cancelar operações
    setBackgroundRole(QPalette::Dark);
    viewport()->setMouseTracking(true); // movimento sem botão: destaque
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent); // pintamos tudo
    viewport()->setCursor(Qt::CrossCursor);
    // O botão direito desenha com a cor secundária: nada de menu de contexto.
    viewport()->setContextMenuPolicy(Qt::PreventContextMenu);

    // --- Documento ---
    connect(m_document, &Document::canvasChanged, this, [this](const core::Rect& area) {
        // +1 pixel de margem para as linhas de grade nas bordas da área.
        viewport()->update(viewportRectOf(area).adjusted(-1, -1, 1, 1));
        refreshHighlight(); // ex.: a região do balde mudou
    });
    connect(m_document, &Document::canvasResized, this, [this] {
        if (m_autoFit) {
            applyZoom(fitZoom(), std::nullopt);
        }
        updateScrollBars();
        updateTitle();
        refreshHighlight();
        viewport()->update();
    });
    connect(m_document, &Document::paletteChanged, viewport(), qOverload<>(&QWidget::update));
    connect(m_document, &Document::selectionChanged, viewport(), qOverload<>(&QWidget::update));
    connect(m_document, &Document::modifiedChanged, this, &QWidget::setWindowModified);
    connect(m_document, &Document::filePathChanged, this, &CanvasEditor::updateTitle);
    connect(m_document, &Document::colorModeChanged, this, [this] {
        updateTitle();
        viewport()->update();
    });

    // --- Sessão: grades e o que muda o destaque ---
    connect(m_session, &EditorSession::gridsChanged, viewport(), qOverload<>(&QWidget::update));
    connect(
        m_session, &EditorSession::gridLayoutChanged, viewport(), qOverload<>(&QWidget::update));
    connect(m_session, &EditorSession::toolChanged, this, &CanvasEditor::refreshHighlight);
    connect(m_session, &EditorSession::toneChanged, this, &CanvasEditor::refreshHighlight);
    connect(m_session, &EditorSession::maskChanged, this, &CanvasEditor::refreshHighlight);
    connect(
        m_session, &EditorSession::activeDocumentChanged, this, &CanvasEditor::refreshHighlight);

    updateScrollBars();
    updateTitle();
}

QSize CanvasEditor::sizeHint() const
{
    // A imagem inteira no zoom atual + moldura (limitada pelo QMdiArea).
    const int frame = 2 * frameWidth();
    return scaledSize().toSize() + QSize(frame, frame);
}

// -----------------------------------------------------------------------------
// Zoom
// -----------------------------------------------------------------------------
void CanvasEditor::setZoom(double zoom, std::optional<QPointF> anchor)
{
    m_autoFit = false; // o usuário escolheu um zoom: não ajusta mais sozinho
    applyZoom(zoom, anchor);
}

void CanvasEditor::zoomIn(std::optional<QPointF> anchor)
{
    // Próximo nível acima do atual (o zoom pode estar entre dois níveis,
    // depois de "ajustar à janela" ou de digitar um valor).
    for (const double level : kZoomLevels) {
        if (level > m_zoom + kZoomEpsilon) {
            setZoom(level, anchor);
            return;
        }
    }
    setZoom(kMaxZoom, anchor);
}

void CanvasEditor::zoomOut(std::optional<QPointF> anchor)
{
    for (auto level = std::rbegin(kZoomLevels); level != std::rend(kZoomLevels); ++level) {
        if (*level < m_zoom - kZoomEpsilon) {
            setZoom(*level, anchor);
            return;
        }
    }
    setZoom(kMinZoom, anchor);
}

void CanvasEditor::zoomToFit()
{
    m_autoFit = false;
    applyZoom(fitZoom(), std::nullopt);
}

double CanvasEditor::fitZoom() const
{
    const core::Size size = core::sizeOf(m_document->canvas());
    const QSize available = viewport()->size();
    if (size.isEmpty() || available.isEmpty()) {
        return 1;
    }
    double zoom = std::min(
        double(available.width()) / size.width, double(available.height()) / size.height);
    // Ampliando, usa um fator inteiro: todos os pixels ficam do mesmo
    // tamanho na tela, como a pixel art pede.
    if (zoom >= 1) {
        zoom = std::floor(zoom);
    }
    return std::clamp(zoom, kMinZoom, kMaxZoom);
}

void CanvasEditor::applyZoom(double zoom, std::optional<QPointF> anchor)
{
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (std::abs(zoom - m_zoom) < kZoomEpsilon) {
        return;
    }
    // Coordenada da imagem sob a âncora, que deve continuar sob ela.
    const QPointF anchorPoint = anchor.value_or(QRectF(viewport()->rect()).center());
    const QPointF origin = imageOrigin();
    const QPointF imagePosition = (anchorPoint - origin) / m_zoom;

    m_zoom = zoom;
    updateScrollBars();

    // Só há o que rolar quando a imagem não cabe (senão fica centralizada).
    horizontalScrollBar()->setValue(
        static_cast<int>(std::lround(imagePosition.x() * m_zoom - anchorPoint.x())));
    verticalScrollBar()->setValue(
        static_cast<int>(std::lround(imagePosition.y() * m_zoom - anchorPoint.y())));

    viewport()->update();
    if (m_mousePosition) {
        updateHighlight(imagePointAt(*m_mousePosition), true);
    }
    emit zoomChanged(m_zoom);
}

// -----------------------------------------------------------------------------
// Coordenadas
// -----------------------------------------------------------------------------
QSizeF CanvasEditor::scaledSize() const
{
    const core::Size size = core::sizeOf(m_document->canvas());
    return { size.width * m_zoom, size.height * m_zoom };
}

QPointF CanvasEditor::imageOrigin() const
{
    // Coordenadas inteiras: as bordas dos pixels e as linhas de grade ficam
    // alinhadas aos pixels da tela.
    const QSizeF scaled = scaledSize();
    const QSize available = viewport()->size();
    const auto axis = [](double content, int view, int scroll) {
        return content <= view ? std::floor((view - content) / 2) : double(-scroll);
    };
    return { axis(scaled.width(), available.width(), horizontalScrollBar()->value()),
        axis(scaled.height(), available.height(), verticalScrollBar()->value()) };
}

core::Point CanvasEditor::imagePointAt(const QPointF& viewportPosition) const
{
    const QPointF position = (viewportPosition - imageOrigin()) / m_zoom;
    return { static_cast<int>(std::floor(position.x())),
        static_cast<int>(std::floor(position.y())) };
}

QPointF CanvasEditor::viewportPointOf(const QPointF& imagePosition) const
{
    return imageOrigin() + imagePosition * m_zoom;
}

QRect CanvasEditor::viewportRectOf(const core::Rect& area) const
{
    const QPointF topLeft = viewportPointOf(QPointF(area.x, area.y));
    return QRectF(topLeft, QSizeF(area.width * m_zoom, area.height * m_zoom)).toAlignedRect();
}

void CanvasEditor::updateScrollBars()
{
    const QSizeF scaled = scaledSize();
    const QSize available = viewport()->size();
    const auto setup = [](QScrollBar* bar, double content, int view) {
        bar->setRange(0, std::max(0, static_cast<int>(std::ceil(content)) - view));
        bar->setPageStep(view);
        bar->setSingleStep(std::max(1, view / 20));
    };
    setup(horizontalScrollBar(), scaled.width(), available.width());
    setup(verticalScrollBar(), scaled.height(), available.height());
}

// -----------------------------------------------------------------------------
// Título
// -----------------------------------------------------------------------------
void CanvasEditor::updateTitle()
{
    const auto size = core::sizeOf(m_document->canvas());
    // "RGBA" no título distingue os documentos em cores reais.
    const QString mode
        = m_document->colorMode() == core::ColorMode::Rgba ? QStringLiteral(", RGBA") : QString();
    setWindowTitle(QStringLiteral("%1[*] (%2x%3%4)")
            .arg(m_document->displayName())
            .arg(size.width)
            .arg(size.height)
            .arg(mode));
    setWindowModified(m_document->isModified());
}

void CanvasEditor::changeEvent(QEvent* event)
{
    // "SemTitulo1"/"Untitled1" depende do idioma.
    if (event->type() == QEvent::LanguageChange) {
        updateTitle();
    }
    QAbstractScrollArea::changeEvent(event);
}

// -----------------------------------------------------------------------------
// Pintura
// -----------------------------------------------------------------------------
void CanvasEditor::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    const QRect dirty = event->rect();
    painter.fillRect(dirty, palette().color(QPalette::Dark));

    const QImage& image = m_document->renderedImage();
    const QPointF origin = imageOrigin();
    const QRectF imageArea(origin, scaledSize());
    const QRect visible = imageArea.toAlignedRect().intersected(dirty);
    if (visible.isEmpty()) {
        return;
    }

    // Em RGBA, um xadrez por baixo mostra a transparência (casas do tamanho
    // de um pixel ampliado, entre 4 e 16 px de tela).
    if (m_document->colorMode() == core::ColorMode::Rgba) {
        const int cell = std::clamp(static_cast<int>(std::lround(m_zoom)), 4, 16);
        paintTransparencyGrid(painter, visible, cell);
    }

    // Só a parte visível da imagem (em pixels inteiros), ampliada sem
    // suavização ("vizinho mais próximo"), como a pixel art exige.
    const int left
        = std::max(0, static_cast<int>(std::floor((visible.left() - origin.x()) / m_zoom)));
    const int top
        = std::max(0, static_cast<int>(std::floor((visible.top() - origin.y()) / m_zoom)));
    const int right = std::min(
        image.width(), static_cast<int>(std::ceil((visible.right() + 1 - origin.x()) / m_zoom)));
    const int bottom = std::min(
        image.height(), static_cast<int>(std::ceil((visible.bottom() + 1 - origin.y()) / m_zoom)));
    if (right > left && bottom > top) {
        const QRect source(left, top, right - left, bottom - top);
        const QRectF target(origin + QPointF(left, top) * m_zoom,
            QSizeF(source.width() * m_zoom, source.height() * m_zoom));
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(target, image, source);
    }

    paintGrids(painter, visible);
    paintHighlight(painter);

    // Seleção: retângulo tracejado (preto + branco para contraste).
    if (const auto& selection = m_document->selection()) {
        const QRect area = viewportRectOf(*selection).adjusted(0, 0, -1, -1);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(area);
        painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
        painter.drawRect(area);
    }
}

void CanvasEditor::paintGrids(QPainter& painter, const QRect& visible) const
{
    const core::Size size = core::sizeOf(m_document->canvas());
    const core::GridLayout& layout = m_session->gridLayout();
    const QPointF origin = imageOrigin();

    // Linhas a cada "step" pixels da imagem, apenas dentro da área visível.
    // Uma grade que ficaria densa demais na tela não é desenhada.
    const auto drawLines = [&](core::Size step, const QColor& color) {
        if (step.width * m_zoom < kMinGridSpacing || step.height * m_zoom < kMinGridSpacing) {
            return;
        }
        painter.setPen(QPen(color, 1));
        // Faixa de linhas visível em cada eixo (índices de linha da grade).
        const auto range = [&](int from, int to, double start, int stepSize, int limit) {
            const int first
                = std::max(0, static_cast<int>(std::floor((from - start) / m_zoom / stepSize)));
            const int last = std::min(limit / stepSize + (limit % stepSize != 0 ? 1 : 0),
                static_cast<int>(std::ceil((to + 1 - start) / m_zoom / stepSize)));
            return std::pair { first, last };
        };
        const auto [firstColumn, lastColumn]
            = range(visible.left(), visible.right(), origin.x(), step.width, size.width);
        for (int column = firstColumn; column <= lastColumn; ++column) {
            // A última linha fica na borda direita da imagem.
            const int x = std::min(column * step.width, size.width);
            const int px = static_cast<int>(std::lround(origin.x() + x * m_zoom));
            painter.drawLine(px, visible.top(), px, visible.bottom());
        }
        const auto [firstRow, lastRow]
            = range(visible.top(), visible.bottom(), origin.y(), step.height, size.height);
        for (int row = firstRow; row <= lastRow; ++row) {
            const int y = std::min(row * step.height, size.height);
            const int py = static_cast<int>(std::lround(origin.y() + y * m_zoom));
            painter.drawLine(visible.left(), py, visible.right(), py);
        }
    };

    if (m_session->mainGridVisible()) {
        drawLines(layout.mainGrid, kMainGridColor);
    }
    if (m_session->subGridVisible()) {
        drawLines(layout.subGrid, kSubGridColor);
    }
}

void CanvasEditor::paintHighlight(QPainter& painter) const
{
    if (m_highlight.empty()) {
        return;
    }
    // Preenchimento: a imagem de sobreposição ampliada sobre a área.
    const QPointF topLeft = viewportPointOf(QPointF(m_highlightBounds.x, m_highlightBounds.y));
    const QRectF target(
        topLeft, QSizeF(m_highlightBounds.width * m_zoom, m_highlightBounds.height * m_zoom));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_highlightImage);

    // Contorno: só com zoom suficiente, e só as bordas visíveis entre um
    // pixel destacado e um não destacado.
    if (m_zoom < kMinZoomForHighlightEdges) {
        return;
    }
    const auto highlighted = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < m_highlightImage.width() && y < m_highlightImage.height()
            && qAlpha(m_highlightImage.pixel(x, y)) != 0;
    };
    const QRect clip = viewport()->rect();
    const int firstX
        = std::max(0, static_cast<int>(std::floor((clip.left() - topLeft.x()) / m_zoom)));
    const int firstY
        = std::max(0, static_cast<int>(std::floor((clip.top() - topLeft.y()) / m_zoom)));
    const int lastX = std::min(m_highlightImage.width() - 1,
        static_cast<int>(std::ceil((clip.right() - topLeft.x()) / m_zoom)));
    const int lastY = std::min(m_highlightImage.height() - 1,
        static_cast<int>(std::ceil((clip.bottom() - topLeft.y()) / m_zoom)));

    // Borda de cada pixel em tela: [x0, x1) x [y0, y1).
    const auto edge = [&](int index, double start) {
        return static_cast<int>(std::lround(start + index * m_zoom));
    };
    QList<QLine> lines;
    for (int y = firstY; y <= lastY; ++y) {
        for (int x = firstX; x <= lastX; ++x) {
            if (!highlighted(x, y)) {
                continue;
            }
            const int x0 = edge(x, topLeft.x());
            const int x1 = edge(x + 1, topLeft.x()) - 1;
            const int y0 = edge(y, topLeft.y());
            const int y1 = edge(y + 1, topLeft.y()) - 1;
            if (!highlighted(x - 1, y)) {
                lines.append({ x0, y0, x0, y1 });
            }
            if (!highlighted(x + 1, y)) {
                lines.append({ x1, y0, x1, y1 });
            }
            if (!highlighted(x, y - 1)) {
                lines.append({ x0, y0, x1, y0 });
            }
            if (!highlighted(x, y + 1)) {
                lines.append({ x0, y1, x1, y1 });
            }
        }
    }
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLines(lines);
    painter.setPen(QPen(Qt::white, 1, Qt::DotLine));
    painter.drawLines(lines);
}

// -----------------------------------------------------------------------------
// Destaque dos pixels sob o mouse
// -----------------------------------------------------------------------------
void CanvasEditor::refreshHighlight()
{
    updateHighlight(m_hoverPixel, true);
}

void CanvasEditor::updateHighlight(std::optional<core::Point> pixel, bool force)
{
    if (!force && pixel == m_hoverPixel) {
        return;
    }
    m_hoverPixel = pixel;

    // Apaga o destaque anterior (com margem para o contorno).
    if (!m_highlight.empty()) {
        viewport()->update(viewportRectOf(m_highlightBounds).adjusted(-1, -1, 1, 1));
    }
    m_highlight.clear();
    m_highlightImage = QImage();
    m_highlightBounds = { };

    // Só o editor do documento ativo destaca (as ferramentas agem nele).
    if (!pixel || m_session->activeDocument() != m_document) {
        return;
    }
    const core::AnyImage& canvas = m_document->canvas();
    for (const core::Point point : m_session->tool().footprint(*m_session, *pixel)) {
        // Mostra só o que de fato mudaria: dentro da imagem, coberto pelo
        // tom e fora da máscara.
        if (m_session->canPlot(canvas, point)) {
            m_highlight.push_back(point);
            m_highlightBounds = m_highlightBounds.united({ point.x, point.y, 1, 1 });
        }
    }
    if (m_highlight.empty()) {
        return;
    }

    // Sobreposição do tamanho da área envolvente: cor de destaque onde há
    // pixel afetado, transparente no resto.
    m_highlightImage = QImage(
        m_highlightBounds.width, m_highlightBounds.height, QImage::Format_ARGB32_Premultiplied);
    m_highlightImage.fill(Qt::transparent);
    const QRgb fill = qPremultiply(kHighlightFill.rgba());
    for (const core::Point point : m_highlight) {
        m_highlightImage.setPixel(
            point.x - m_highlightBounds.x, point.y - m_highlightBounds.y, fill);
    }
    viewport()->update(viewportRectOf(m_highlightBounds).adjusted(-1, -1, 1, 1));
}

// -----------------------------------------------------------------------------
// Mouse: repassado à ferramenta atual (a lupa e a vista são tratadas aqui)
// -----------------------------------------------------------------------------
void CanvasEditor::mousePressEvent(QMouseEvent* event)
{
    const QPointF position = event->position();

    // Botão do meio: arrasta a vista.
    if (event->button() == Qt::MiddleButton && m_activeButton == Qt::NoButton) {
        m_panning = true;
        m_panStart = position;
        m_panScrollStart = { horizontalScrollBar()->value(), verticalScrollBar()->value() };
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (m_activeButton != Qt::NoButton || m_panning) {
        return; // ignora um segundo botão durante um traço
    }
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
        return;
    }

    // Lupa: o zoom é deste editor, ancorado no ponto clicado.
    if (m_session->toolKind() == ToolKind::Zoom) {
        if (event->button() == Qt::LeftButton) {
            zoomIn(position);
        } else {
            zoomOut(position);
        }
        return;
    }

    // As ferramentas agem no documento ativo: garante que seja este.
    m_session->setActiveDocument(m_document);
    const core::Point pixel = imagePointAt(position);
    if (core::boundsOf(m_document->canvas()).contains(pixel)) {
        m_session->setLastClickedPoint(pixel); // célula para a animação
    }
    m_activeButton = event->button();
    m_session->tool().press(*m_session, pixel, event->button());
}

void CanvasEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Um clique duplo rápido é, para as ferramentas, apenas um novo clique.
    mousePressEvent(event);
}

void CanvasEditor::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF position = event->position();
    m_mousePosition = position;
    if (m_panning) {
        const QPointF delta = position - m_panStart;
        horizontalScrollBar()->setValue(m_panScrollStart.x() - static_cast<int>(delta.x()));
        verticalScrollBar()->setValue(m_panScrollStart.y() - static_cast<int>(delta.y()));
        return;
    }
    const core::Point pixel = imagePointAt(position);
    emit pixelHovered(pixel);
    if (m_activeButton != Qt::NoButton) {
        m_session->tool().move(*m_session, pixel);
    }
    updateHighlight(pixel);
}

void CanvasEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        viewport()->setCursor(Qt::CrossCursor);
        return;
    }
    if (event->button() != m_activeButton) {
        return;
    }
    m_activeButton = Qt::NoButton;
    m_session->tool().release(*m_session, imagePointAt(event->position()));
}

bool CanvasEditor::viewportEvent(QEvent* event)
{
    // A saída do mouse chega ao viewport, não à área de rolagem.
    if (event->type() == QEvent::Leave) {
        m_mousePosition.reset();
        updateHighlight(std::nullopt);
    }
    return QAbstractScrollArea::viewportEvent(event);
}

// -----------------------------------------------------------------------------
// Roda do mouse: zoom; com Ctrl, rolagem (Ctrl+Shift: horizontal)
// -----------------------------------------------------------------------------
void CanvasEditor::wheelEvent(QWheelEvent* event)
{
    // Alguns sistemas entregam Shift+roda como movimento horizontal.
    const QPoint angle = event->angleDelta();
    const int delta = angle.y() != 0 ? angle.y() : angle.x();
    if (delta == 0) {
        event->ignore();
        return;
    }
    event->accept();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (modifiers.testFlag(Qt::ControlModifier)) {
        QScrollBar* bar
            = modifiers.testFlag(Qt::ShiftModifier) ? horizontalScrollBar() : verticalScrollBar();
        const double lines = delta / kWheelStep * QApplication::wheelScrollLines();
        bar->setValue(bar->value() - static_cast<int>(std::lround(lines * bar->singleStep())));
        return;
    }

    // Zoom: um nível por "clique" da roda; frações (touchpads) se acumulam.
    m_wheelRemainder += delta / kWheelStep;
    while (m_wheelRemainder >= 1) {
        zoomIn(event->position());
        m_wheelRemainder -= 1;
    }
    while (m_wheelRemainder <= -1) {
        zoomOut(event->position());
        m_wheelRemainder += 1;
    }
}

// -----------------------------------------------------------------------------
// Teclado, rolagem e redimensionamento
// -----------------------------------------------------------------------------
void CanvasEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_session->cancelTool();
        m_activeButton = Qt::NoButton;
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void CanvasEditor::scrollContentsBy(int /*dx*/, int /*dy*/)
{
    viewport()->update();
    // A imagem andou sob o mouse parado: o pixel destacado mudou.
    if (m_mousePosition) {
        updateHighlight(imagePointAt(*m_mousePosition), true);
    }
}

void CanvasEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    if (m_autoFit) {
        applyZoom(fitZoom(), std::nullopt);
    }
    updateScrollBars();
}

} // namespace spritenator::gui
