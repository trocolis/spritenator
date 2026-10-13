// =============================================================================
// AnimationWindow.cpp
// =============================================================================
#include "AnimationWindow.h"

#include "Document.h"
#include "EditorSession.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace spritenator::gui {

// -----------------------------------------------------------------------------
// AnimationPreview
// -----------------------------------------------------------------------------
AnimationPreview::AnimationPreview(
    EditorSession* session, const core::Animation* animation, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_animation(animation)
{
    setMinimumSize(96, 96);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void AnimationPreview::setFramePosition(std::size_t position)
{
    m_position = position;
    update();
}

void AnimationPreview::setScale(int scale)
{
    m_scale = std::max(1, scale);
    updateGeometry();
    update();
}

QSize AnimationPreview::sizeHint() const
{
    const core::Size cell = m_session->characterLayout().cellSize;
    return QSize(cell.width, cell.height) * m_scale + QSize(16, 16);
}

void AnimationPreview::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Dark));

    const Document* document = m_session->activeDocument();
    if (document == nullptr || m_animation->isEmpty()) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
            document == nullptr ? tr("No image open") : tr("No frames"));
        return;
    }
    const auto& frames = m_animation->frames();
    const core::Rect& frame = frames.at(std::min(m_position, frames.size() - 1));
    const QRect source(frame.x, frame.y, frame.width, frame.height);
    const QSize scaled = source.size() * m_scale;
    const QPoint origin((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
    // Partes do quadro fora do canvas simplesmente não aparecem.
    painter.drawImage(QRect(origin, scaled), document->renderedImage(), source);
}

// -----------------------------------------------------------------------------
// AnimationWindow
// -----------------------------------------------------------------------------
AnimationWindow::AnimationWindow(EditorSession* session, QWidget* parent)
    : QWidget(parent, Qt::Tool) // janela auxiliar: fica sobre a principal
    , m_session(session)
    , m_frameList(new QListWidget(this))
    , m_wait(new QSpinBox(this))
    , m_scale(new QComboBox(this))
    , m_addCell(new QPushButton(tr("&Add Cell"), this))
    , m_walkCycle(new QPushButton(tr("&Walk Cycle"), this))
    , m_playButton(new QPushButton(tr("&Pause"), this))
    , m_timer(new QTimer(this))
{
    setWindowTitle(tr("Animation"));
    m_preview = new AnimationPreview(m_session, &m_animation, this);

    // --- Controles ---
    m_wait->setRange(core::Animation::kMinWaitMs, core::Animation::kMaxWaitMs);
    m_wait->setSingleStep(10);
    m_wait->setValue(core::Animation::kDefaultWaitMs);
    m_wait->setSuffix(tr(" ms"));
    for (const int factor : { 1, 2, 3, 4 }) {
        m_scale->addItem(QStringLiteral("%1x").arg(factor), factor);
    }
    m_scale->setCurrentIndex(1);
    m_playButton->setCheckable(true);
    m_addCell->setToolTip(
        tr("Add the cell last transferred to the drawing area (right-click on the canvas)"));
    m_walkCycle->setToolTip(
        tr("Add the RPG Maker walk cycle (frames 1-2-3-2) of the last transferred cell"));

    auto* removeButton = new QPushButton(tr("&Remove"), this);
    auto* clearButton = new QPushButton(tr("&Delete All"), this);

    auto* settings = new QFormLayout;
    settings->addRow(tr("Wait:"), m_wait);
    settings->addRow(tr("Zoom:"), m_scale);

    auto* frameButtons = new QGridLayout;
    frameButtons->addWidget(m_addCell, 0, 0);
    frameButtons->addWidget(m_walkCycle, 0, 1);
    frameButtons->addWidget(removeButton, 1, 0);
    frameButtons->addWidget(clearButton, 1, 1);

    auto* framesBox = new QGroupBox(tr("Frames"), this);
    auto* framesLayout = new QVBoxLayout(framesBox);
    framesLayout->addWidget(m_frameList, 1);
    framesLayout->addLayout(frameButtons);
    framesLayout->addLayout(settings);
    framesLayout->addWidget(m_playButton);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_preview, 1);
    layout->addWidget(framesBox);

    // --- Ligações ---
    connect(m_addCell, &QPushButton::clicked, this, &AnimationWindow::addLastCell);
    connect(m_walkCycle, &QPushButton::clicked, this, &AnimationWindow::addWalkCycle);
    connect(removeButton, &QPushButton::clicked, this, &AnimationWindow::removeSelectedFrame);
    connect(clearButton, &QPushButton::clicked, this, &AnimationWindow::clearFrames);
    connect(m_playButton, &QPushButton::toggled, this, &AnimationWindow::updatePlayback);
    connect(m_wait, &QSpinBox::valueChanged, this, [this](int value) {
        m_animation.setWaitMs(value);
        m_timer->setInterval(m_animation.waitMs());
    });
    connect(m_scale, &QComboBox::currentIndexChanged, this,
        [this] { m_preview->setScale(m_scale->currentData().toInt()); });
    connect(m_timer, &QTimer::timeout, this, &AnimationWindow::advanceFrame);
    // Clicar num quadro da lista o exibe (útil com a reprodução pausada).
    connect(m_frameList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            m_preview->setFramePosition(static_cast<std::size_t>(row));
        }
    });

    // A prévia acompanha edições no documento ativo e a troca de documento.
    connect(m_session, &EditorSession::activeDocumentChanged, this, [this](Document* document) {
        if (document != nullptr) {
            connect(document, &Document::canvasChanged, m_preview, qOverload<>(&QWidget::update),
                Qt::UniqueConnection);
        }
        m_preview->update();
    });
    if (Document* document = m_session->activeDocument()) {
        connect(document, &Document::canvasChanged, m_preview, qOverload<>(&QWidget::update),
            Qt::UniqueConnection);
    }
    const auto updateButtons = [this] {
        const bool hasCell = m_session->lastCell().has_value();
        m_addCell->setEnabled(hasCell);
        m_walkCycle->setEnabled(hasCell);
    };
    connect(m_session, &EditorSession::lastCellChanged, this, updateButtons);
    updateButtons();

    m_timer->setInterval(m_animation.waitMs());
    m_preview->setScale(2);
    refreshFrames();
}

void AnimationWindow::addLastCell()
{
    if (const auto& cell = m_session->lastCell()) {
        m_animation.addFrame(*cell);
        refreshFrames();
    }
}

void AnimationWindow::addWalkCycle()
{
    if (const auto& cell = m_session->lastCell()) {
        m_animation.addFrames(core::walkCycleFrames(*cell));
        refreshFrames();
    }
}

void AnimationWindow::removeSelectedFrame()
{
    const int row = m_frameList->currentRow();
    if (row >= 0) {
        m_animation.removeFrame(static_cast<std::size_t>(row));
        refreshFrames();
    }
}

void AnimationWindow::clearFrames()
{
    m_animation.clear();
    refreshFrames();
}

void AnimationWindow::advanceFrame()
{
    m_preview->setFramePosition(m_animation.nextPosition(m_preview->framePosition()));
}

void AnimationWindow::refreshFrames()
{
    m_frameList->clear();
    int number = 1;
    for (const core::Rect& frame : m_animation.frames()) {
        m_frameList->addItem(tr("%1: (%2, %3)").arg(number++).arg(frame.x).arg(frame.y));
    }
    m_preview->setFramePosition(0);
    // Começa a tocar automaticamente quando a lista ganha quadros.
    if (!m_animation.isEmpty() && !m_playButton->isChecked()) {
        m_playButton->setChecked(true);
    }
    updatePlayback();
}

void AnimationWindow::updatePlayback()
{
    const bool playing = m_playButton->isChecked() && m_animation.size() > 1;
    m_playButton->setText(m_playButton->isChecked() ? tr("&Pause") : tr("&Play"));
    if (playing) {
        m_timer->start();
    } else {
        m_timer->stop();
    }
}

} // namespace spritenator::gui
