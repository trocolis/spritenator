// =============================================================================
// AnimationWindow.cpp
// =============================================================================
#include "AnimationWindow.h"

#include "Document.h"
#include "EditorSession.h"
#include "ImageRendering.h"

#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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
    if (document->colorMode() == core::ColorMode::Rgba) {
        paintTransparencyGrid(painter, QRect(origin, scaled), 4);
    }
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
    , m_addCell(new QPushButton(this))
    , m_walkCycle(new QPushButton(this))
    , m_removeButton(new QPushButton(this))
    , m_clearButton(new QPushButton(this))
    , m_playButton(new QPushButton(this))
    , m_framesBox(new QGroupBox(this))
    , m_waitLabel(new QLabel(this))
    , m_zoomLabel(new QLabel(this))
    , m_timer(new QTimer(this))
{
    // Os textos são definidos em retranslate() (também na troca de idioma).
    m_preview = new AnimationPreview(m_session, &m_animation, this);

    // --- Controles ---
    m_wait->setRange(core::Animation::kMinWaitMs, core::Animation::kMaxWaitMs);
    m_wait->setSingleStep(10);
    m_wait->setValue(core::Animation::kDefaultWaitMs);
    for (const int factor : { 1, 2, 3, 4 }) {
        m_scale->addItem(QStringLiteral("%1x").arg(factor), factor);
    }
    m_scale->setCurrentIndex(1);
    m_playButton->setCheckable(true);

    auto* settings = new QFormLayout;
    settings->addRow(m_waitLabel, m_wait);
    settings->addRow(m_zoomLabel, m_scale);

    auto* frameButtons = new QGridLayout;
    frameButtons->addWidget(m_addCell, 0, 0);
    frameButtons->addWidget(m_walkCycle, 0, 1);
    frameButtons->addWidget(m_removeButton, 1, 0);
    frameButtons->addWidget(m_clearButton, 1, 1);

    auto* framesLayout = new QVBoxLayout(m_framesBox);
    framesLayout->addWidget(m_frameList, 1);
    framesLayout->addLayout(frameButtons);
    framesLayout->addLayout(settings);
    framesLayout->addWidget(m_playButton);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_preview, 1);
    layout->addWidget(m_framesBox);

    // --- Ligações ---
    connect(m_addCell, &QPushButton::clicked, this, &AnimationWindow::addLastCell);
    connect(m_walkCycle, &QPushButton::clicked, this, &AnimationWindow::addWalkCycle);
    connect(m_removeButton, &QPushButton::clicked, this, &AnimationWindow::removeSelectedFrame);
    connect(m_clearButton, &QPushButton::clicked, this, &AnimationWindow::clearFrames);
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
    retranslate();
    refreshFrames();
}

void AnimationWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void AnimationWindow::retranslate()
{
    setWindowTitle(tr("Animation"));
    m_addCell->setText(tr("&Add Cell"));
    m_addCell->setToolTip(
        tr("Add the cell last transferred to the drawing area (right-click on the canvas)"));
    m_walkCycle->setText(tr("&Walk Cycle"));
    m_walkCycle->setToolTip(
        tr("Add the RPG Maker walk cycle (frames 1-2-3-2) of the last transferred cell"));
    m_removeButton->setText(tr("&Remove"));
    m_clearButton->setText(tr("&Delete All"));
    m_framesBox->setTitle(tr("Frames"));
    m_waitLabel->setText(tr("Wait:"));
    m_zoomLabel->setText(tr("Zoom:"));
    m_wait->setSuffix(tr(" ms"));
    // Itens da lista: só o texto muda (a seleção e a reprodução continuam).
    for (int row = 0; row < m_frameList->count(); ++row) {
        m_frameList->item(row)->setText(frameLabel(row));
    }
    m_playButton->setText(m_playButton->isChecked() ? tr("&Pause") : tr("&Play"));
    m_preview->update(); // "Nenhum quadro" etc. são desenhados na pintura
}

QString AnimationWindow::frameLabel(int row) const
{
    const core::Rect& frame = m_animation.frames().at(static_cast<std::size_t>(row));
    return tr("%1: (%2, %3)").arg(row + 1).arg(frame.x).arg(frame.y);
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
    for (int row = 0; row < static_cast<int>(m_animation.size()); ++row) {
        m_frameList->addItem(frameLabel(row));
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
