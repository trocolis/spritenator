// =============================================================================
// CanvasEditor.cpp
// =============================================================================
#include "CanvasEditor.h"

#include "Document.h"
#include "EditorSession.h"
#include "IconProvider.h"
#include "ImageRendering.h"
#include "SelectionTransform.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <numbers>

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

// Alças de transformação.
const QColor kTransformBoxColor(52, 116, 214);
constexpr double kHandleHalfSize = 3.5; ///< alça: quadrado de 7 px
constexpr double kHandleHitRadius = 6; ///< tolerância do clique numa alça
constexpr double kRotateZone = 24; ///< distância dos cantos que ativa a rotação
constexpr double kSnapDegrees = 15; ///< Shift: ângulo em múltiplos de 15°

double distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

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
    // Cenas e camadas: outra cena (ou outra estrutura) invalida o papel de seda.
    const auto invalidateOnion = [this] {
        m_onionFrame = -1;
        viewport()->update();
    };
    connect(m_document, &Document::structureChanged, this, invalidateOnion);
    connect(m_document, &Document::currentCelChanged, this, invalidateOnion);
    connect(m_session, &EditorSession::onionSkinChanged, this, invalidateOnion);
    connect(m_document, &Document::selectionChanged, this, [this] {
        updateSelectionEdges();
        viewport()->update();
    });
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
    connect(m_session, &EditorSession::toolChanged, this, [this] {
        refreshHighlight();
        refreshToolPath(); // o contorno da ferramenta anterior some
        viewport()->update(); // a caixa de transformação aparece ou some
    });
    connect(m_session->selectionTransform(), &SelectionTransform::changed, viewport(),
        qOverload<>(&QWidget::update));
    connect(m_session, &EditorSession::toneChanged, this, &CanvasEditor::refreshHighlight);
    connect(m_session, &EditorSession::brushChanged, this, &CanvasEditor::refreshHighlight);
    connect(m_session, &EditorSession::maskChanged, this, &CanvasEditor::refreshHighlight);
    connect(
        m_session, &EditorSession::activeDocumentChanged, this, &CanvasEditor::refreshHighlight);

    updateScrollBars();
    updateSelectionEdges();
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

    // Em RGBA (ou com várias camadas), um xadrez por baixo mostra a
    // transparência (casas do tamanho de um pixel ampliado, entre 4 e 16 px).
    if (m_document->colorMode() == core::ColorMode::Rgba || m_document->layerCount() > 1) {
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
        paintOnionSkin(painter, target, source);
    }

    paintGrids(painter, visible);
    paintHighlight(painter);

    paintSelection(painter);
    paintToolPath(painter);
    paintTransformBox(painter);
}

// -----------------------------------------------------------------------------
// Papel de seda: a cena anterior e a próxima, translúcidas, sobre a atual
// -----------------------------------------------------------------------------
void CanvasEditor::paintOnionSkin(QPainter& painter, const QRectF& target, const QRect& source)
{
    const int frames = m_document->frameCount();
    if (!m_session->onionSkin() || frames < 2) {
        return;
    }
    const int current = m_document->currentFrame();
    if (m_onionFrame != current) { // cache: só recompõe ao mudar de cena
        m_onionPrevious = current > 0 ? m_document->composite(current - 1) : QImage();
        m_onionNext = current + 1 < frames ? m_document->composite(current + 1) : QImage();
        m_onionFrame = current;
    }
    constexpr double kOpacity = 0.3;
    painter.save();
    painter.setOpacity(kOpacity);
    for (const QImage& neighbour : { m_onionPrevious, m_onionNext }) {
        if (!neighbour.isNull()) {
            painter.drawImage(target, neighbour, source);
        }
    }
    painter.restore();
}

// -----------------------------------------------------------------------------
// Alças de transformação: mover (dentro), escalar (alças) e girar (perto dos
// cantos, do lado de fora) — como nos editores de imagem modernos
// -----------------------------------------------------------------------------
QPointF CanvasEditor::imageCoordinatesAt(const QPointF& viewportPosition) const
{
    return (viewportPosition - imageOrigin()) / m_zoom;
}

bool CanvasEditor::showsTransformBox() const
{
    if (!isSelectionTool(m_session->toolKind()) || m_session->activeDocument() != m_document) {
        return false;
    }
    const SelectionTransform* transform = m_session->selectionTransform();
    return (transform->isActive() && transform->document() == m_document)
        || m_document->hasSelection();
}

QList<QPointF> CanvasEditor::transformBox() const
{
    if (!showsTransformBox()) {
        return { };
    }
    const SelectionTransform* transform = m_session->selectionTransform();
    core::Rect bounds = m_document->selection().bounds();
    core::Affine affine;
    if (transform->isActive() && transform->document() == m_document) {
        bounds = transform->originalBounds();
        affine = transform->affine();
    }
    QList<QPointF> corners;
    for (const auto& [x, y] : core::transformedCorners(affine, bounds)) {
        corners.append(viewportPointOf(QPointF(x, y)));
    }
    return corners;
}

QList<QPointF> CanvasEditor::transformHandles() const
{
    QList<QPointF> handles = transformBox();
    if (handles.isEmpty()) {
        return handles;
    }
    for (int index = 0; index < 4; ++index) { // meios dos lados
        handles.append((handles.at(index) + handles.at((index + 1) % 4)) / 2);
    }
    return handles;
}

int CanvasEditor::handleAt(const QPointF& position) const
{
    const QList<QPointF> handles = transformHandles();
    for (int index = 0; index < handles.size(); ++index) {
        if (distance(handles.at(index), position) <= kHandleHitRadius) {
            return index;
        }
    }
    return -1;
}

CanvasEditor::TransformHit CanvasEditor::transformHitAt(const QPointF& position) const
{
    const QList<QPointF> box = transformBox();
    if (box.isEmpty()) {
        return TransformHit::None;
    }
    if (handleAt(position) >= 0) {
        return TransformHit::Scale;
    }
    if (!QPolygonF(box).containsPoint(position, Qt::OddEvenFill)) {
        // Fora da caixa, perto de um canto: girar.
        for (const QPointF& corner : box) {
            if (distance(corner, position) <= kRotateZone) {
                return TransformHit::Rotate;
            }
        }
        return TransformHit::None;
    }
    // Dentro: move, se o pixel está selecionado (ou durante a transformação).
    const SelectionTransform* transform = m_session->selectionTransform();
    if ((transform->isActive() && transform->document() == m_document)
        || m_document->selection().contains(imagePointAt(position))) {
        return TransformHit::Move;
    }
    return TransformHit::None;
}

void CanvasEditor::paintTransformBox(QPainter& painter) const
{
    const QList<QPointF> handles = transformHandles();
    if (handles.isEmpty()) {
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kTransformBoxColor, 1));
    painter.drawPolygon(QPolygonF(handles.mid(0, 4)));
    painter.setPen(QPen(QColor(27, 29, 46), 1));
    painter.setBrush(Qt::white);
    for (const QPointF& handle : handles) {
        painter.drawRect(QRectF(handle - QPointF(kHandleHalfSize, kHandleHalfSize),
            QSizeF(kHandleHalfSize * 2, kHandleHalfSize * 2)));
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
}

bool CanvasEditor::beginTransformDrag(const QPointF& position, Qt::KeyboardModifiers modifiers)
{
    const TransformHit hit = transformHitAt(position);
    // Shift/Ctrl dentro da seleção: somar/subtrair (vai para a ferramenta).
    if (hit == TransformHit::None
        || (hit == TransformHit::Move
            && (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) != 0)) {
        return false;
    }
    SelectionTransform* transform = m_session->selectionTransform();
    if (!transform->begin()) {
        return false;
    }
    m_drag = hit;
    m_dragHandle = handleAt(position);
    m_dragStart = imageCoordinatesAt(position);
    m_dragParams = transform->params();
    // Lado da alça nos eixos locais (girados) da seleção.
    m_dragDirX = 0;
    m_dragDirY = 0;
    if (hit == TransformHit::Scale) {
        const core::Rect bounds = transform->originalBounds();
        const double radians = m_dragParams.angle * std::numbers::pi / 180.0;
        const QPointF center(bounds.x + bounds.width / 2.0 + m_dragParams.dx,
            bounds.y + bounds.height / 2.0 + m_dragParams.dy);
        const QPointF offset = imageCoordinatesAt(transformHandles().at(m_dragHandle)) - center;
        const double localX = offset.x() * std::cos(radians) + offset.y() * std::sin(radians);
        const double localY = -offset.x() * std::sin(radians) + offset.y() * std::cos(radians);
        constexpr double kTolerance = 1e-6;
        m_dragDirX = localX > kTolerance ? 1 : (localX < -kTolerance ? -1 : 0);
        m_dragDirY = localY > kTolerance ? 1 : (localY < -kTolerance ? -1 : 0);
    }
    return true;
}

void CanvasEditor::continueTransformDrag(const QPointF& position, Qt::KeyboardModifiers modifiers)
{
    SelectionTransform* transform = m_session->selectionTransform();
    if (!transform->isActive()) {
        m_drag = TransformHit::None;
        return;
    }
    const QPointF point = imageCoordinatesAt(position);
    const core::Rect bounds = transform->originalBounds();
    const QPointF originalCenter(bounds.x + bounds.width / 2.0, bounds.y + bounds.height / 2.0);
    const QPointF center = originalCenter + QPointF(m_dragParams.dx, m_dragParams.dy);
    core::TransformParams params = m_dragParams;

    switch (m_drag) {
    case TransformHit::Move:
        // Deslocamentos inteiros: os pixels continuam alinhados à grade.
        params.dx += std::round(point.x() - m_dragStart.x());
        params.dy += std::round(point.y() - m_dragStart.y());
        break;
    case TransformHit::Rotate: {
        const double before
            = std::atan2(m_dragStart.y() - center.y(), m_dragStart.x() - center.x());
        const double after = std::atan2(point.y() - center.y(), point.x() - center.x());
        double angle = m_dragParams.angle + (after - before) * 180.0 / std::numbers::pi;
        if (modifiers.testFlag(Qt::ShiftModifier)) {
            angle = std::round(angle / kSnapDegrees) * kSnapDegrees;
        }
        angle = std::remainder(angle, 360.0); // (-180, 180]
        params.angle = angle;
        break;
    }
    case TransformHit::Scale: {
        // Eixos locais da seleção girada: u (largura) e v (altura).
        const double radians = m_dragParams.angle * std::numbers::pi / 180.0;
        const QPointF u(std::cos(radians), std::sin(radians));
        const QPointF v(-std::sin(radians), std::cos(radians));
        const QPointF offset = point - center;
        const double mouseX = offset.x() * u.x() + offset.y() * u.y();
        const double mouseY = offset.x() * v.x() + offset.y() * v.y();
        const double width = bounds.width * std::abs(m_dragParams.sx);
        const double height = bounds.height * std::abs(m_dragParams.sy);
        // O lado oposto à alça fica parado.
        double newWidth = width;
        double newHeight = height;
        if (m_dragDirX != 0) {
            newWidth = std::max(1.0, (mouseX + m_dragDirX * width / 2) * m_dragDirX);
        }
        if (m_dragDirY != 0) {
            newHeight = std::max(1.0, (mouseY + m_dragDirY * height / 2) * m_dragDirY);
        }
        if (modifiers.testFlag(Qt::ShiftModifier) && m_dragDirX != 0 && m_dragDirY != 0) {
            const double ratio = std::max(newWidth / width, newHeight / height);
            newWidth = width * ratio; // Shift nos cantos: mantém a proporção
            newHeight = height * ratio;
        }
        const double turns = m_dragParams.angle / 90.0;
        if (std::abs(turns - std::round(turns)) < 1e-9) {
            newWidth = std::max(1.0, std::round(newWidth)); // pixels inteiros
            newHeight = std::max(1.0, std::round(newHeight));
        }
        const double localCenterX = m_dragDirX * (newWidth - width) / 2;
        const double localCenterY = m_dragDirY * (newHeight - height) / 2;
        const QPointF newCenter = center + u * localCenterX + v * localCenterY;
        params.sx = (m_dragParams.sx < 0 ? -1.0 : 1.0) * newWidth / bounds.width;
        params.sy = (m_dragParams.sy < 0 ? -1.0 : 1.0) * newHeight / bounds.height;
        params.dx = newCenter.x() - originalCenter.x();
        params.dy = newCenter.y() - originalCenter.y();
        break;
    }
    case TransformHit::None: break;
    }
    transform->setParams(params);
}

void CanvasEditor::updateTransformCursor(const QPointF& position)
{
    switch (transformHitAt(position)) {
    case TransformHit::Move: viewport()->setCursor(Qt::SizeAllCursor); return;
    case TransformHit::Rotate: {
        // Cursor próprio (o Qt não tem um de rotação), ampliado para 32 px.
        static const QCursor rotate = [] {
            const QPixmap base(IconProvider::resourcePath(QStringLiteral("cursor_rotate")));
            return QCursor(
                base.scaled(base.size() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation));
        }();
        viewport()->setCursor(rotate);
        return;
    }
    case TransformHit::Scale: {
        // Seta de redimensionar na direção da alça (a caixa pode estar girada).
        const QList<QPointF> box = transformBox();
        const QPointF center = (box.at(0) + box.at(2)) / 2;
        const QPointF direction = transformHandles().at(handleAt(position)) - center;
        double degrees = std::atan2(direction.y(), direction.x()) * 180.0 / std::numbers::pi;
        degrees = std::fmod(degrees + 360.0, 180.0);
        static constexpr Qt::CursorShape kShapes[]
            = { Qt::SizeHorCursor, Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor };
        const auto index = static_cast<std::size_t>(std::lround(degrees / 45.0)) % 4;
        viewport()->setCursor(kShapes[index]);
        return;
    }
    case TransformHit::None: viewport()->setCursor(Qt::CrossCursor); return;
    }
}

// -----------------------------------------------------------------------------
// Contorno da seleção ("formigas"): segue a forma exata da máscara
// -----------------------------------------------------------------------------
void CanvasEditor::updateSelectionEdges()
{
    // As bordas entre pixels selecionados e não selecionados, em coordenadas
    // da imagem (cantos dos pixels), unindo trechos consecutivos numa linha
    // só — uma seleção retangular vira apenas 4 segmentos.
    m_selectionEdges.clear();
    const core::Selection& selection = m_document->selection();
    if (selection.isEmpty()) {
        return;
    }
    const core::Rect bounds = selection.bounds();
    // Bordas horizontais: entre a linha y-1 e a linha y.
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        int runStart = 0;
        bool inRun = false;
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            const bool edge = x < bounds.right()
                && selection.contains({ x, y - 1 }) != selection.contains({ x, y });
            if (edge && !inRun) {
                runStart = x;
                inRun = true;
            } else if (!edge && inRun) {
                m_selectionEdges.append(QLine(runStart, y, x, y));
                inRun = false;
            }
        }
    }
    // Bordas verticais: entre a coluna x-1 e a coluna x.
    for (int x = bounds.left(); x <= bounds.right(); ++x) {
        int runStart = 0;
        bool inRun = false;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
            const bool edge = y < bounds.bottom()
                && selection.contains({ x - 1, y }) != selection.contains({ x, y });
            if (edge && !inRun) {
                runStart = y;
                inRun = true;
            } else if (!edge && inRun) {
                m_selectionEdges.append(QLine(x, runStart, x, y));
                inRun = false;
            }
        }
    }
}

void CanvasEditor::paintSelection(QPainter& painter) const
{
    if (m_selectionEdges.isEmpty()) {
        return;
    }
    const QRect visible = viewport()->rect().adjusted(-2, -2, 2, 2);
    QList<QLineF> lines;
    lines.reserve(m_selectionEdges.size());
    for (const QLine& edge : m_selectionEdges) {
        // Canto dos pixels na tela (sem suavização: a linha de 1 px fica na
        // coluna/linha de tela da borda).
        const QPointF from = viewportPointOf(edge.p1());
        const QPointF to = viewportPointOf(edge.p2());
        if (QRectF(from, to).normalized().adjusted(-1, -1, 1, 1).intersects(visible)) {
            lines.append(QLineF(from, to));
        }
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLines(lines);
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
    painter.drawLines(lines);
}

void CanvasEditor::paintToolPath(QPainter& painter) const
{
    // Contorno em construção (polígono/laço): linha pelos centros dos pixels.
    const std::vector<core::Point> path = m_session->tool().previewPath();
    if (path.empty() || m_session->activeDocument() != m_document) {
        return;
    }
    QPolygonF polyline;
    for (const core::Point point : path) {
        polyline.append(viewportPointOf(QPointF(point.x + 0.5, point.y + 0.5)));
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawPolyline(polyline);
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
    painter.drawPolyline(polyline);
    // Vértices: quadradinhos (o primeiro maior, indica onde fechar).
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(Qt::white);
    for (qsizetype index = 0; index < polyline.size(); ++index) {
        const double half = index == 0 ? 3 : 2;
        painter.drawRect(QRectF(polyline[index] - QPointF(half, half), QSizeF(half * 2, half * 2)));
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
    const Tool& tool = m_session->tool();
    const core::Rect imageBounds = core::boundsOf(canvas);
    for (const core::Point point : tool.footprint(*m_session, *pixel)) {
        // Mostra só o que de fato mudaria: dentro da imagem e, nas
        // ferramentas que pintam, coberto pelo tom, fora da máscara e dentro
        // da seleção.
        if (tool.paints() ? m_session->canPlot(canvas, point) : imageBounds.contains(point)) {
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
// Mouse: repassado à ferramenta atual (a vista e as alças são tratadas aqui)
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

    // As ferramentas agem no documento ativo: garante que seja este.
    m_session->setActiveDocument(m_document);

    // Ferramentas de seleção: alças/interior da seleção transformam; um
    // clique fora aplica a transformação em andamento e começa outra seleção.
    if (isSelectionTool(m_session->toolKind()) && !m_session->tool().isBusy()) {
        if (event->button() == Qt::LeftButton && beginTransformDrag(position, event->modifiers())) {
            m_activeButton = Qt::LeftButton;
            return;
        }
        m_session->commitTransform();
    }
    const core::Point pixel = imagePointAt(position);
    m_activeButton = event->button();
    m_session->setInputModifiers(event->modifiers());
    m_session->tool().press(*m_session, pixel, event->button());
    refreshToolPath();
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
    m_session->setInputModifiers(event->modifiers());
    if (m_drag != TransformHit::None) {
        continueTransformDrag(position, event->modifiers());
        return;
    }
    if (m_activeButton != Qt::NoButton) {
        m_session->tool().move(*m_session, pixel);
    } else if (m_session->activeDocument() == m_document) {
        m_session->tool().hover(*m_session, pixel);
        updateTransformCursor(position);
    }
    refreshToolPath();
    updateHighlight(pixel);
}

void CanvasEditor::refreshToolPath()
{
    // O contorno em construção muda a cada evento: redesenha enquanto existir
    // (e uma última vez quando some).
    const bool hasPath = !m_session->tool().previewPath().empty();
    if (hasPath || m_hadToolPath) {
        viewport()->update();
    }
    m_hadToolPath = hasPath;
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
    if (m_drag != TransformHit::None) {
        // Fim do arraste: a transformação continua flutuando até ser aplicada
        // (Enter, clique fora ou troca de ferramenta).
        m_drag = TransformHit::None;
        updateTransformCursor(event->position());
        return;
    }
    m_session->setInputModifiers(event->modifiers());
    m_session->tool().release(*m_session, imagePointAt(event->position()));
    refreshToolPath();
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
    Tool& tool = m_session->tool();
    SelectionTransform* transform = m_session->selectionTransform();
    const bool transforming = transform->isActive() && transform->document() == m_document;
    if (event->key() == Qt::Key_Escape) {
        // Esc: primeiro cancela a operação em andamento (transformação,
        // traço, figura, polígono aberto); sem nenhuma, desfaz a seleção.
        if (transforming) {
            transform->cancel();
            m_drag = TransformHit::None;
            m_activeButton = Qt::NoButton;
        } else if (m_activeButton != Qt::NoButton || tool.isBusy()) {
            m_session->cancelTool();
            m_activeButton = Qt::NoButton;
        } else {
            m_document->clearSelection();
        }
        refreshToolPath();
        return;
    }
    if (transforming && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        transform->commit(); // Enter aplica a transformação
        return;
    }
    if (m_session->activeDocument() == m_document && tool.keyPress(*m_session, event->key())) {
        refreshToolPath();
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
