// =============================================================================
// TimelinePanel.cpp
// =============================================================================
#include "TimelinePanel.h"

#include "ActionCatalog.h"
#include "Document.h"
#include "EditorSession.h"
#include "IconProvider.h"
#include "TimelineGrid.h"

#include <QAction>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace spritenator::gui {

namespace {

constexpr int kMinFps = 1;
constexpr int kMaxFps = 60;

QToolButton* makeButton(QAction* action, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(16, 16));
    return button;
}

/// Separador vertical entre grupos de botões.
QFrame* makeSeparator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// Opacidade da camada (0..255) <-> porcentagem exibida (0..100).
int percentOf(int opacity)
{
    return static_cast<int>(std::lround(opacity * 100.0 / 255.0));
}

int opacityOf(int percent)
{
    return static_cast<int>(std::lround(percent * 255.0 / 100.0));
}

} // namespace

TimelinePanel::TimelinePanel(EditorSession* session, ActionCatalog* actions, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_actions(actions)
    , m_grid(new TimelineGrid(this))
    , m_fps(new QSpinBox(this))
    , m_opacity(new QSpinBox(this))
    , m_playTimer(new QTimer(this))
{
    // --- Barra das camadas (esquerda) e das cenas (direita) ---
    auto* bar = new QHBoxLayout;
    bar->setSpacing(1);
    for (const ActionId id : { ActionId::LayerNew, ActionId::LayerDelete, ActionId::LayerMoveUp,
             ActionId::LayerMoveDown, ActionId::LayerDuplicate, ActionId::LayerMergeDown }) {
        bar->addWidget(makeButton(actions->action(id), this));
    }
    bar->addStretch(1);
    for (const ActionId id : { ActionId::FrameNew, ActionId::FrameDelete, ActionId::FrameDuplicate,
             ActionId::FrameMoveLeft, ActionId::FrameMoveRight }) {
        bar->addWidget(makeButton(actions->action(id), this));
    }
    bar->addWidget(makeSeparator(this));
    for (const ActionId id : { ActionId::FrameFirst, ActionId::FramePrevious, ActionId::PlayPause,
             ActionId::FrameNext, ActionId::FrameLast }) {
        bar->addWidget(makeButton(actions->action(id), this));
    }
    bar->addWidget(makeSeparator(this));
    bar->addWidget(makeButton(actions->action(ActionId::OnionSkin), this));
    bar->addWidget(makeButton(actions->action(ActionId::PlaybackPingPong), this));
    m_fps->setRange(kMinFps, kMaxFps);
    bar->addWidget(m_fps);

    // Opacidade da camada atual, no canto da grade (como no exemplo).
    m_opacity->setRange(0, 100);
    m_grid->setCornerWidget(m_opacity);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    layout->addLayout(bar);
    layout->addWidget(m_grid, 1);

    // --- Grade -> documento ---
    const auto withDocument = [this](auto&& function) {
        return [this, function](auto... arguments) {
            finishPendingWork();
            if (Document* document = m_document.data()) {
                function(*document, arguments...);
            }
        };
    };
    connect(m_grid, &TimelineGrid::frameClicked, this,
        withDocument([](Document& document, int frame) { document.setCurrentFrame(frame); }));
    connect(m_grid, &TimelineGrid::layerClicked, this,
        withDocument([](Document& document, int layer) { document.setCurrentLayer(layer); }));
    connect(m_grid, &TimelineGrid::cellClicked, this,
        withDocument([](Document& document, int frame, int layer) {
            document.setCurrentFrame(frame);
            document.setCurrentLayer(layer);
        }));
    connect(m_grid, &TimelineGrid::visibilityToggled, this,
        withDocument([](Document& document, int layer) {
            document.setLayerVisible(
                layer, !document.layers()[static_cast<std::size_t>(layer)].visible);
        }));
    connect(
        m_grid, &TimelineGrid::lockToggled, this, withDocument([](Document& document, int layer) {
            document.setLayerLocked(
                layer, !document.layers()[static_cast<std::size_t>(layer)].locked);
        }));
    connect(m_grid, &TimelineGrid::renameRequested, this,
        withDocument([](Document& document, int layer, const QString& name) {
            document.renameLayer(layer, name);
        }));
    connect(m_fps, &QSpinBox::valueChanged, this, [this](int fps) {
        Document* document = m_document.data();
        if (!m_updating && document != nullptr) {
            document->setAllFrameDurations(static_cast<int>(std::lround(1000.0 / fps)));
        }
    });
    connect(m_opacity, &QSpinBox::valueChanged, this, [this](int percent) {
        Document* document = m_document.data();
        if (!m_updating && document != nullptr) {
            document->setLayerOpacity(document->currentLayer(), opacityOf(percent));
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
    onDocument(ActionId::FrameFirst, [](Document& document) { document.setCurrentFrame(0); });
    onDocument(ActionId::FrameLast,
        [](Document& document) { document.setCurrentFrame(document.frameCount() - 1); });
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
    connect(m_actions->action(ActionId::LayerLock), &QAction::triggered, this, [this](bool locked) {
        if (Document* document = m_document.data()) {
            document->setLayerLocked(document->currentLayer(), locked);
        }
    });
    connect(m_actions->action(ActionId::PlayPause), &QAction::toggled, this,
        &TimelinePanel::setPlaying);
    connect(m_actions->action(ActionId::OnionSkin), &QAction::toggled, m_session,
        &EditorSession::setOnionSkin);
    connect(m_actions->action(ActionId::PlaybackPingPong), &QAction::toggled, this,
        &TimelinePanel::updateLoopIcon);

    // --- Reprodução ---
    m_playTimer->setSingleShot(true);
    connect(m_playTimer, &QTimer::timeout, this, &TimelinePanel::playNextFrame);

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
    m_fps->setSuffix(tr(" FPS"));
    m_fps->setToolTip(tr("Playback speed: scenes per second (the same for all scenes)"));
    m_opacity->setPrefix(tr("Opacity: "));
    m_opacity->setSuffix(QStringLiteral("%"));
    m_opacity->setToolTip(tr("Opacity of the current layer (RGBA images only)"));
    m_grid->setToolTip(tr("Click a cell to choose the scene and the layer; the eye shows or hides "
                          "the layer, the padlock protects it; double click the name to rename"));
    updateLoopIcon();
}

void TimelinePanel::updateLoopIcon()
{
    QAction* pingPong = m_actions->action(ActionId::PlaybackPingPong);
    pingPong->setIcon(IconProvider::icon(
        pingPong->isChecked() ? QStringLiteral("ping_pong") : QStringLiteral("loop")));
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
    m_grid->setDocument(document);
    if (document != nullptr) {
        connect(document, &Document::structureChanged, this, &TimelinePanel::syncCurrent);
        connect(document, &Document::currentCelChanged, this, &TimelinePanel::syncCurrent);
        connect(document, &Document::colorModeChanged, this, &TimelinePanel::syncCurrent);
    }
    syncCurrent();
}

void TimelinePanel::syncCurrent()
{
    m_updating = true;
    const Document* document = m_document.data();
    if (document != nullptr) {
        const int frame = document->currentFrame();
        const Layer& layer = document->layers()[static_cast<std::size_t>(document->currentLayer())];
        const int duration = document->frames()[static_cast<std::size_t>(frame)].durationMs;
        m_fps->setValue(static_cast<int>(std::lround(1000.0 / duration)));
        m_opacity->setValue(percentOf(layer.opacity));
        m_actions->action(ActionId::LayerLock)->setChecked(layer.locked);
    }
    m_fps->setEnabled(document != nullptr);
    // A opacidade só existe em RGBA (nas indexadas o índice 0 é transparente).
    m_opacity->setEnabled(document != nullptr && document->colorMode() == core::ColorMode::Rgba);
    m_updating = false;
    updateActions();
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
    enable(ActionId::FrameFirst, document && frame > 0);
    enable(ActionId::FrameLast, document && frame < frames - 1);
    enable(ActionId::FramePrevious, frames > 1);
    enable(ActionId::FrameNext, frames > 1);
    enable(ActionId::PlayPause, frames > 1);
    enable(ActionId::OnionSkin, document != nullptr);
    enable(ActionId::PlaybackPingPong, document != nullptr);
    enable(ActionId::LayerNew, document != nullptr);
    enable(ActionId::LayerDuplicate, document != nullptr);
    enable(ActionId::LayerDelete, layers > 1);
    enable(ActionId::LayerMoveUp, document && layer < layers - 1);
    enable(ActionId::LayerMoveDown, document && layer > 0);
    enable(ActionId::LayerMergeDown, document && layer > 0);
    enable(ActionId::LayerAbove, document && layer < layers - 1);
    enable(ActionId::LayerBelow, document && layer > 0);
    enable(ActionId::LayerLock, document != nullptr);
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
    m_direction = 1;
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
        const int count = document->frameCount();
        int next = document->currentFrame() + m_direction;
        if (m_actions->action(ActionId::PlaybackPingPong)->isChecked()) {
            // Vai e volta: inverte o sentido ao chegar a uma das pontas.
            if (next < 0 || next >= count) {
                m_direction = -m_direction;
                next = document->currentFrame() + m_direction;
            }
        } else if (next >= count) {
            next = 0; // repete do começo
        }
        document->setCurrentFrame(std::clamp(next, 0, count - 1));
    }
    m_playTimer->start(
        document->frames()[static_cast<std::size_t>(document->currentFrame())].durationMs);
}

} // namespace spritenator::gui
