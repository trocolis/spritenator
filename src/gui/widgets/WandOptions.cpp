// =============================================================================
// WandOptions.cpp
// =============================================================================
#include "WandOptions.h"

#include "gui/EditorSession.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace spritenator::gui {

WandOptions::WandOptions(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_label(new QLabel(this))
    , m_slider(new QSlider(Qt::Horizontal, this))
    , m_spin(new QSpinBox(this))
    , m_contiguous(new QCheckBox(this))
{
    m_slider->setRange(0, 100);
    m_spin->setRange(0, 100);
    m_spin->setSuffix(QStringLiteral("%"));

    auto* row = new QHBoxLayout;
    row->addWidget(m_label);
    row->addWidget(m_slider, 1);
    row->addWidget(m_spin);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(row);
    layout->addWidget(m_contiguous);

    // Barra e caixa numérica refletem o mesmo valor; ambos vão para a sessão.
    connect(m_slider, &QSlider::valueChanged, m_spin, &QSpinBox::setValue);
    connect(m_spin, &QSpinBox::valueChanged, m_slider, &QSlider::setValue);
    connect(m_slider, &QSlider::valueChanged, m_session, &EditorSession::setWandTolerance);
    connect(m_contiguous, &QCheckBox::toggled, m_session, &EditorSession::setWandContiguous);
    connect(m_session, &EditorSession::wandOptionsChanged, this, &WandOptions::syncFromSession);
    connect(m_session, &EditorSession::toolChanged, this, &WandOptions::syncFromSession);
    retranslate();
    syncFromSession();
}

void WandOptions::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void WandOptions::retranslate()
{
    m_label->setText(tr("Tolerance:"));
    const QString tip = tr("How different a color may be to be selected by the magic wand "
                           "(0% = only the same color)");
    m_slider->setToolTip(tip);
    m_spin->setToolTip(tip);
    m_contiguous->setText(tr("Contiguous"));
    m_contiguous->setToolTip(
        tr("Select only the area connected to the clicked pixel (otherwise, the whole image)"));
}

void WandOptions::syncFromSession()
{
    setVisible(m_session->toolKind() == ToolKind::MagicWand);
    const QSignalBlocker sliderBlocker(m_slider);
    const QSignalBlocker spinBlocker(m_spin);
    const QSignalBlocker boxBlocker(m_contiguous);
    m_slider->setValue(m_session->wandTolerance());
    m_spin->setValue(m_session->wandTolerance());
    m_contiguous->setChecked(m_session->wandContiguous());
}

} // namespace spritenator::gui
