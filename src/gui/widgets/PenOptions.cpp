// =============================================================================
// PenOptions.cpp
// =============================================================================
#include "PenOptions.h"

#include "gui/EditorSession.h"
#include "gui/IconProvider.h"

#include <QButtonGroup>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>

namespace spritenator::gui {

namespace {

QToolButton* makeTipButton(const QString& icon, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setIcon(IconProvider::icon(icon));
    button->setIconSize(QSize(16, 16));
    button->setCheckable(true);
    button->setAutoRaise(true);
    return button;
}

} // namespace

PenOptions::PenOptions(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_tipLabel(new QLabel(this))
    , m_square(makeTipButton(QStringLiteral("brush_square"), this))
    , m_round(makeTipButton(QStringLiteral("brush_round"), this))
    , m_sizeLabel(new QLabel(this))
    , m_slider(new QSlider(Qt::Horizontal, this))
    , m_spin(new QSpinBox(this))
{
    auto* tips = new QButtonGroup(this);
    tips->addButton(m_square);
    tips->addButton(m_round);
    m_slider->setRange(core::kMinBrushSize, core::kMaxBrushSize);
    m_spin->setRange(core::kMinBrushSize, core::kMaxBrushSize);

    auto* tipRow = new QHBoxLayout;
    tipRow->setSpacing(1);
    tipRow->addWidget(m_square);
    tipRow->addWidget(m_round);
    tipRow->addStretch(1);
    auto* sizeRow = new QHBoxLayout;
    sizeRow->addWidget(m_slider, 1);
    sizeRow->addWidget(m_spin);
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tipLabel, 0, 0);
    layout->addLayout(tipRow, 0, 1);
    layout->addWidget(m_sizeLabel, 1, 0);
    layout->addLayout(sizeRow, 1, 1);

    connect(m_square, &QToolButton::clicked, m_session,
        [this] { m_session->setBrushShape(core::BrushShape::Square); });
    connect(m_round, &QToolButton::clicked, m_session,
        [this] { m_session->setBrushShape(core::BrushShape::Round); });
    // Barra e caixa numérica refletem o mesmo valor; ambos vão para a sessão.
    connect(m_slider, &QSlider::valueChanged, m_spin, &QSpinBox::setValue);
    connect(m_spin, &QSpinBox::valueChanged, m_slider, &QSlider::setValue);
    connect(m_slider, &QSlider::valueChanged, m_session, &EditorSession::setBrushSize);
    connect(m_session, &EditorSession::brushChanged, this, &PenOptions::syncFromSession);
    connect(m_session, &EditorSession::toolChanged, this, &PenOptions::syncFromSession);
    retranslate();
    syncFromSession();
}

void PenOptions::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void PenOptions::retranslate()
{
    m_tipLabel->setText(tr("Tip:"));
    m_square->setToolTip(tr("Square tip"));
    m_round->setToolTip(tr("Round tip"));
    m_sizeLabel->setText(tr("Size:"));
    m_spin->setSuffix(tr(" px"));
    const QString tip = tr("Size of the pen tip, in pixels ([ and ] change it)");
    m_slider->setToolTip(tip);
    m_spin->setToolTip(tip);
}

void PenOptions::syncFromSession()
{
    setVisible(m_session->toolKind() == ToolKind::Pen);
    const QSignalBlocker sliderBlocker(m_slider);
    const QSignalBlocker spinBlocker(m_spin);
    m_square->setChecked(m_session->brushShape() == core::BrushShape::Square);
    m_round->setChecked(m_session->brushShape() == core::BrushShape::Round);
    m_slider->setValue(m_session->brushSize());
    m_spin->setValue(m_session->brushSize());
}

} // namespace spritenator::gui
