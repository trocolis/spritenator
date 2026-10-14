// =============================================================================
// TimelinePanel.cpp
// =============================================================================
#include "TimelinePanel.h"

#include "ActionCatalog.h"
#include "Document.h"
#include "EditorSession.h"
#include "ImageRendering.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Lado das miniaturas das cenas, em pixels.
constexpr int kThumbnailSize = 48;
/// Atraso para atualizar a miniatura enquanto se desenha (agrupa os traços).
constexpr int kThumbnailDelayMs = 150;

QToolButton* makeButton(QAction* action, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(16, 16));
    return button;
}

/// Miniatura: a cena reduzida (sem suavização se ampliar), sobre o xadrez.
QPixmap thumbnailOf(const QImage& composite)
{
    QPixmap pixmap(kThumbnailSize, kThumbnailSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    const QSize scaled
        = composite.size().scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio);
    const QRect target(
        QPoint((kThumbnailSize - scaled.width()) / 2, (kThumbnailSize - scaled.height()) / 2),
        scaled);
    paintTransparencyGrid(painter, target, 4);
    const bool enlarge = scaled.width() >= composite.width();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !enlarge);
    painter.drawImage(target, composite);
    return pixmap;
}

} // namespace

TimelinePanel::TimelinePanel(EditorSession* session, ActionCatalog* actions, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_actions(actions)
    , m_framesLabel(new QLabel(this))
    , m_layersLabel(new QLabel(this))
    , m_durationLabel(new QLabel(this))
    , m_opacityLabel(new QLabel(this))
    , m_frames(new QListWidget(this))
    , m_layers(new QListWidget(this))
    , m_duration(new QSpinBox(this))
    , m_opacity(new QSlider(Qt::Horizontal, this))
    , m_playTimer(new QTimer(this))
    , m_thumbnailTimer(new QTimer(this))
{
    // --- Barra de cenas: botões + miniaturas em linha ---
    m_frames->setViewMode(QListView::IconMode);
    m_frames->setFlow(QListView::LeftToRight);
    m_frames->setWrapping(false);
    m_frames->setMovement(QListView::Static);
    m_frames->setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    m_frames->setSpacing(2);
    m_frames->setFixedHeight(kThumbnailSize + 46);
    m_frames->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_duration->setRange(kMinFrameDuration, kMaxFrameDuration);
    m_duration->setSingleStep(10);

    auto* frameButtons = new QHBoxLayout;
    frameButtons->setSpacing(1);
    frameButtons->addWidget(m_framesLabel);
    for (const ActionId id : { ActionId::FrameNew, ActionId::FrameDuplicate, ActionId::FrameDelete,
             ActionId::FrameMoveLeft, ActionId::FrameMoveRight, ActionId::PlayPause,
             ActionId::OnionSkin }) {
        frameButtons->addWidget(makeButton(actions->action(id), this));
    }
    frameButtons->addSpacing(8);
    frameButtons->addWidget(m_durationLabel);
    frameButtons->addWidget(m_duration);
    frameButtons->addStretch(1);

    // --- Barra de camadas: botões + camadas em linha (a de cima primeiro) ---
    m_layers->setFlow(QListView::LeftToRight);
    m_layers->setWrapping(false);
    m_layers->setSpacing(2);
    m_layers->setFixedHeight(42);
    m_layers->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_opacity->setRange(0, 255);
    m_opacity->setMaximumWidth(140);

    auto* layerButtons = new QHBoxLayout;
    layerButtons->setSpacing(1);
    layerButtons->addWidget(m_layersLabel);
    for (const ActionId id : { ActionId::LayerNew, ActionId::LayerDuplicate, ActionId::LayerDelete,
             ActionId::LayerMoveUp, ActionId::LayerMoveDown, ActionId::LayerMergeDown }) {
        layerButtons->addWidget(makeButton(actions->action(id), this));
    }
    layerButtons->addSpacing(8);
    layerButtons->addWidget(m_opacityLabel);
    layerButtons->addWidget(m_opacity);
    layerButtons->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    layout->addLayout(frameButtons);
    layout->addWidget(m_frames);
    layout->addLayout(layerButtons);
    layout->addWidget(m_layers);

    // --- Lista -> documento ---
    connect(m_frames, &QListWidget::currentRowChanged, this, [this](int row) {
        Document* document = m_document.data();
        if (!m_updating && document != nullptr && row >= 0) {
            finishPendingWork();
            document->setCurrentFrame(row);
        }
    });
    connect(m_layers, &QListWidget::currentRowChanged, this, [this](int row) {
        Document* document = m_document.data();
        if (!m_updating && document != nullptr && row >= 0) {
            finishPendingWork();
            document->setCurrentLayer(layerForRow(row));
        }
    });
    connect(m_layers, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        Document* document = m_document.data();
        if (m_updating || document == nullptr) {
            return;
        }
        // Lê tudo antes: a mudança no documento reconstrói a lista (e apaga
        // este item).
        const int layer = layerForRow(m_layers->row(item));
        const bool visible = item->checkState() == Qt::Checked;
        const QString name = item->text();
        document->setLayerVisible(layer, visible);
        document->renameLayer(layer, name);
    });
    connect(m_duration, &QSpinBox::valueChanged, this, [this](int value) {
        Document* document = m_document.data();
        if (!m_updating && document != nullptr) {
            document->setFrameDuration(document->currentFrame(), value);
        }
    });
    connect(m_opacity, &QSlider::valueChanged, this, [this](int value) {
        Document* document = m_document.data();
        if (!m_updating && document != nullptr) {
            document->setLayerOpacity(document->currentLayer(), value);
        }
    });

    // --- Ações (também no menu Animação) ---
    const auto onDocument = [this](ActionId id, auto&& function) {
        connect(m_actions->action(id), &QAction::triggered, this, [this, function] {
            setPlaying(false);
            finishPendingWork();
            // Lido depois: finishPendingWork pode mexer no documento.
            if (Document* document = m_document.data()) {
                function(*document);
            }
        });
    };
    onDocument(ActionId::FrameNew, [](Document& document) { document.addFrame(); });
    onDocument(ActionId::FrameDuplicate, [](Document& document) { document.duplicateFrame(); });
    onDocument(ActionId::FrameDelete, [](Document& document) { document.removeFrame(); });
    onDocument(ActionId::FrameMoveLeft, [](Document& document) { document.moveFrame(-1); });
    onDocument(ActionId::FrameMoveRight, [](Document& document) { document.moveFrame(1); });
    onDocument(ActionId::FramePrevious, [](Document& document) {
        const int count = document.frameCount(); // circular: da primeira vai à última
        document.setCurrentFrame((document.currentFrame() + count - 1) % count);
    });
    onDocument(ActionId::FrameNext, [](Document& document) {
        document.setCurrentFrame((document.currentFrame() + 1) % document.frameCount());
    });
    onDocument(ActionId::LayerNew, [](Document& document) { document.addLayer(); });
    onDocument(ActionId::LayerDuplicate, [](Document& document) { document.duplicateLayer(); });
    onDocument(ActionId::LayerDelete, [](Document& document) { document.removeLayer(); });
    onDocument(ActionId::LayerMoveUp, [](Document& document) { document.moveLayer(1); });
    onDocument(ActionId::LayerMoveDown, [](Document& document) { document.moveLayer(-1); });
    onDocument(ActionId::LayerMergeDown, [](Document& document) { document.mergeLayerDown(); });
    onDocument(ActionId::LayerAbove,
        [](Document& document) { document.setCurrentLayer(document.currentLayer() + 1); });
    onDocument(ActionId::LayerBelow,
        [](Document& document) { document.setCurrentLayer(document.currentLayer() - 1); });
    connect(m_actions->action(ActionId::PlayPause), &QAction::toggled, this,
        &TimelinePanel::setPlaying);
    connect(m_actions->action(ActionId::OnionSkin), &QAction::toggled, m_session,
        &EditorSession::setOnionSkin);

    // --- Reprodução e miniaturas ---
    m_playTimer->setSingleShot(true);
    connect(m_playTimer, &QTimer::timeout, this, &TimelinePanel::playNextFrame);
    m_thumbnailTimer->setSingleShot(true);
    m_thumbnailTimer->setInterval(kThumbnailDelayMs);
    connect(m_thumbnailTimer, &QTimer::timeout, this, &TimelinePanel::refreshPendingThumbnails);

    connect(m_session, &EditorSession::activeDocumentChanged, this, &TimelinePanel::attach);
    retranslate();
    attach(m_session->activeDocument());
}

void TimelinePanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void TimelinePanel::retranslate()
{
    m_framesLabel->setText(tr("Scenes"));
    m_layersLabel->setText(tr("Layers"));
    m_durationLabel->setText(tr("Duration:"));
    m_duration->setSuffix(tr(" ms"));
    m_duration->setToolTip(tr("How long the current scene is shown when playing"));
    m_opacityLabel->setText(tr("Opacity:"));
    m_opacity->setToolTip(tr("Opacity of the current layer (RGBA images only)"));
    m_layers->setToolTip(tr(
        "Click to choose the layer to draw on; the box shows or hides it; double click renames"));
    rebuild(); // textos das miniaturas ("Cena 1"...)
}

// -----------------------------------------------------------------------------
// Documento acompanhado
// -----------------------------------------------------------------------------
void TimelinePanel::attach(Document* document)
{
    setPlaying(false);
    if (Document* previous = m_document.data()) {
        disconnect(previous, nullptr, this, nullptr);
    }
    m_document = document;
    if (document != nullptr) {
        connect(document, &Document::structureChanged, this, &TimelinePanel::rebuild);
        connect(document, &Document::currentCelChanged, this, &TimelinePanel::syncCurrent);
        connect(document, &Document::colorModeChanged, this, &TimelinePanel::syncCurrent);
        connect(document, &Document::paletteChanged, this, [this] {
            for (int frame = 0; frame < m_frames->count(); ++frame) {
                m_frames->item(frame)->setData(Qt::UserRole, true); // todas pendentes
            }
            m_thumbnailTimer->start();
        });
        connect(document, &Document::canvasChanged, this, [this] {
            const Document* current = m_document.data();
            if (current == nullptr) {
                return;
            }
            // Só a cena exibida muda ao desenhar: a miniatura dela fica pendente.
            if (QListWidgetItem* item = m_frames->item(current->currentFrame())) {
                item->setData(Qt::UserRole, true);
                m_thumbnailTimer->start();
            }
        });
    }
    rebuild();
}

void TimelinePanel::rebuild()
{
    m_updating = true;
    m_frames->clear();
    m_layers->clear();
    if (const Document* document = m_document.data()) {
        for (int frame = 0; frame < document->frameCount(); ++frame) {
            auto* item = new QListWidgetItem(QString::number(frame + 1), m_frames);
            item->setTextAlignment(Qt::AlignHCenter);
            item->setToolTip(tr("Scene %1 (%2 ms)")
                    .arg(frame + 1)
                    .arg(document->frames()[static_cast<std::size_t>(frame)].durationMs));
            refreshThumbnail(frame);
        }
        for (int layer = document->layerCount() - 1; layer >= 0; --layer) {
            const Layer& info = document->layers()[static_cast<std::size_t>(layer)];
            auto* item = new QListWidgetItem(info.name, m_layers);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
            item->setCheckState(info.visible ? Qt::Checked : Qt::Unchecked);
        }
    }
    m_updating = false;
    syncCurrent();
}

void TimelinePanel::syncCurrent()
{
    m_updating = true;
    const Document* document = m_document.data();
    if (document != nullptr) {
        const int frame = document->currentFrame();
        const int layer = document->currentLayer();
        m_frames->setCurrentRow(frame);
        m_layers->setCurrentRow(rowForLayer(layer));
        m_duration->setValue(document->frames()[static_cast<std::size_t>(frame)].durationMs);
        m_opacity->setValue(document->layers()[static_cast<std::size_t>(layer)].opacity);
    }
    m_duration->setEnabled(document != nullptr);
    // A opacidade só existe em RGBA (nas indexadas o índice 0 é transparente).
    const bool rgba = document != nullptr && document->colorMode() == core::ColorMode::Rgba;
    m_opacity->setEnabled(rgba);
    m_opacityLabel->setEnabled(rgba);
    m_updating = false;
    updateActions();
}

void TimelinePanel::refreshThumbnail(int frame)
{
    QListWidgetItem* item = m_frames->item(frame);
    const Document* document = m_document.data();
    if (item == nullptr || document == nullptr) {
        return;
    }
    item->setIcon(QIcon(thumbnailOf(document->composite(frame))));
    item->setData(Qt::UserRole, false);
}

void TimelinePanel::refreshPendingThumbnails()
{
    for (int frame = 0; frame < m_frames->count(); ++frame) {
        if (m_frames->item(frame)->data(Qt::UserRole).toBool()) {
            refreshThumbnail(frame);
        }
    }
}

int TimelinePanel::rowForLayer(int layer) const
{
    const Document* document = m_document.data();
    return document != nullptr ? document->layerCount() - 1 - layer : -1;
}

int TimelinePanel::layerForRow(int row) const
{
    const Document* document = m_document.data();
    return document != nullptr ? document->layerCount() - 1 - row : -1;
}

void TimelinePanel::finishPendingWork()
{
    m_session->cancelTool();
    m_session->commitTransform();
}

// -----------------------------------------------------------------------------
// Ações e reprodução
// -----------------------------------------------------------------------------
void TimelinePanel::updateActions()
{
    const Document* document = m_document.data();
    const int frames = document ? document->frameCount() : 0;
    const int layers = document ? document->layerCount() : 0;
    const int frame = document ? document->currentFrame() : 0;
    const int layer = document ? document->currentLayer() : 0;
    const auto enable
        = [this](ActionId id, bool enabled) { m_actions->action(id)->setEnabled(enabled); };
    enable(ActionId::FrameNew, document != nullptr);
    enable(ActionId::FrameDuplicate, document != nullptr);
    enable(ActionId::FrameDelete, frames > 1);
    enable(ActionId::FrameMoveLeft, document && frame > 0);
    enable(ActionId::FrameMoveRight, document && frame < frames - 1);
    enable(ActionId::FramePrevious, frames > 1);
    enable(ActionId::FrameNext, frames > 1);
    enable(ActionId::PlayPause, frames > 1);
    enable(ActionId::OnionSkin, document != nullptr);
    enable(ActionId::LayerNew, document != nullptr);
    enable(ActionId::LayerDuplicate, document != nullptr);
    enable(ActionId::LayerDelete, layers > 1);
    enable(ActionId::LayerMoveUp, document && layer < layers - 1);
    enable(ActionId::LayerMoveDown, document && layer > 0);
    enable(ActionId::LayerMergeDown, document && layer > 0);
    enable(ActionId::LayerAbove, document && layer < layers - 1);
    enable(ActionId::LayerBelow, document && layer > 0);
    if (frames <= 1) {
        setPlaying(false);
    }
}

bool TimelinePanel::isPlaying() const
{
    return m_actions->action(ActionId::PlayPause)->isChecked();
}

void TimelinePanel::setPlaying(bool playing)
{
    QAction* play = m_actions->action(ActionId::PlayPause);
    if (play->isChecked() != playing) {
        play->setChecked(playing); // volta aqui pelo sinal toggled
        return;
    }
    if (const Document* document = m_document.data(); playing && document != nullptr) {
        m_playTimer->start(
            document->frames()[static_cast<std::size_t>(document->currentFrame())].durationMs);
    } else {
        m_playTimer->stop();
    }
}

void TimelinePanel::playNextFrame()
{
    Document* document = m_document.data();
    if (document == nullptr || !isPlaying()) {
        return;
    }
    // Não troca de cena no meio de um traço: tenta de novo logo depois.
    if (!document->isEditing()) {
        document->setCurrentFrame((document->currentFrame() + 1) % document->frameCount());
    }
    m_playTimer->start(
        document->frames()[static_cast<std::size_t>(document->currentFrame())].durationMs);
}

} // namespace spritenator::gui
