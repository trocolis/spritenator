// =============================================================================
// ColorEditor.cpp
// =============================================================================
#include "ColorEditor.h"

#include "ColorSwatch.h"
#include "gui/Document.h"
#include "gui/ImageRendering.h"
#include "gui/commands/ImageCommands.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Índice de array de um canal.
std::size_t at(ColorEditor::Channel channel)
{
    return static_cast<std::size_t>(channel);
}

/// Valor do canal numa cor RGBA.
int channelOf(const core::Rgba& color, ColorEditor::Channel channel)
{
    switch (channel) {
    case ColorEditor::Red: return color.red;
    case ColorEditor::Green: return color.green;
    case ColorEditor::Blue: return color.blue;
    default: return color.alpha;
    }
}

/// Cor RGBA com um canal alterado.
core::Rgba withChannel(core::Rgba color, ColorEditor::Channel channel, int value)
{
    const auto channelValue = static_cast<std::uint8_t>(value);
    switch (channel) {
    case ColorEditor::Red: color.red = channelValue; break;
    case ColorEditor::Green: color.green = channelValue; break;
    case ColorEditor::Blue: color.blue = channelValue; break;
    default: color.alpha = channelValue; break;
    }
    return color;
}

} // namespace

ColorEditor::ColorEditor(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_primarySwatch(new ColorSwatch(this))
    , m_secondarySwatch(new ColorSwatch(this))
{
    // --- Mostradores: clicar torna o mostrador ativo ---
    auto* swatches = new QHBoxLayout;
    swatches->addWidget(m_primarySwatch);
    swatches->addWidget(m_secondarySwatch);
    connect(m_primarySwatch, &ColorSwatch::clicked, this,
        [this] { m_session->setActiveColorRole(ColorRole::Primary); });
    connect(m_secondarySwatch, &ColorSwatch::clicked, this,
        [this] { m_session->setActiveColorRole(ColorRole::Secondary); });

    // --- Barras de canal ---
    auto* channels = new QGridLayout;
    for (int index = 0; index < ChannelCount; ++index) {
        const auto channel = static_cast<Channel>(index);
        auto* label = new QLabel(this);
        auto* slider = new QSlider(Qt::Horizontal, this);
        auto* spin = new QSpinBox(this);
        slider->setRange(0, 255);
        spin->setRange(0, 255);
        channels->addWidget(label, index, 0);
        channels->addWidget(slider, index, 1);
        channels->addWidget(spin, index, 2);

        // Barra e caixa numérica refletem o mesmo valor.
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
        connect(slider, &QSlider::valueChanged, this,
            [this, channel](int value) { applyChannel(channel, value); });

        m_channelLabels.at(at(channel)) = label;
        m_sliders.at(at(channel)) = slider;
        m_spins.at(at(channel)) = spin;
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(swatches);
    layout->addLayout(channels);

    connect(m_session, &EditorSession::colorsChanged, this, &ColorEditor::refresh);
    connect(m_session, &EditorSession::paletteChanged, this, &ColorEditor::refresh);
    connect(m_session, &EditorSession::activeDocumentChanged, this, &ColorEditor::refresh);
    connect(m_session, &EditorSession::colorModeChanged, this, &ColorEditor::refresh);
    connect(m_session, &EditorSession::activeColorRoleChanged, this, &ColorEditor::refresh);
    retranslate();
    refresh();
}

ColorSwatch* ColorEditor::swatch(ColorRole role) const
{
    return role == ColorRole::Primary ? m_primarySwatch : m_secondarySwatch;
}

QSlider* ColorEditor::slider(Channel channel) const
{
    return m_sliders.at(at(channel));
}

void ColorEditor::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void ColorEditor::retranslate()
{
    m_primarySwatch->setToolTip(
        tr("Primary color (left button). Click to edit it with the sliders."));
    m_secondarySwatch->setToolTip(
        tr("Secondary color (right button). Click to edit it with the sliders."));
    const QString names[] = { tr("R"), tr("G"), tr("B"), tr("A") };
    for (std::size_t channel = 0; channel < ChannelCount; ++channel) {
        m_channelLabels.at(channel)->setText(names[channel]);
    }
    m_sliders.at(at(Alpha))->setToolTip(tr("Opacity: 0 = transparent, 255 = opaque"));
}

void ColorEditor::refresh()
{
    m_updating = true;
    const bool rgba = m_session->colorMode() == core::ColorMode::Rgba;
    const ColorRole active = m_session->activeColorRole();
    m_primarySwatch->setActive(active == ColorRole::Primary);
    m_secondarySwatch->setActive(active == ColorRole::Secondary);

    core::Rgba editing;
    if (rgba) {
        // RGBA: os mostradores mostram as cores reais (não há índice).
        m_primarySwatch->setColor(toQColor(m_session->primaryRgba()));
        m_secondarySwatch->setColor(toQColor(m_session->secondaryRgba()));
        m_primarySwatch->setText({ });
        m_secondarySwatch->setText({ });
        editing = m_session->rgbaFor(active);
    } else {
        const core::Palette& palette = m_session->palette();
        m_primarySwatch->setColor(toQColor(palette.color(m_session->drawColor())));
        m_secondarySwatch->setColor(toQColor(palette.color(m_session->backgroundColor())));
        m_primarySwatch->setText(QString::number(m_session->drawColor()));
        m_secondarySwatch->setText(QString::number(m_session->backgroundColor()));
        const int index
            = active == ColorRole::Primary ? m_session->drawColor() : m_session->backgroundColor();
        editing = core::Rgba::opaque(palette.color(index));
    }

    for (int index = 0; index < ChannelCount; ++index) {
        const auto channel = static_cast<Channel>(index);
        m_sliders.at(at(channel))->setValue(channelOf(editing, channel));
    }
    // A barra A só existe no modo RGBA (paletas não têm transparência).
    for (QWidget* widget : std::initializer_list<QWidget*> {
             m_channelLabels.at(at(Alpha)), m_sliders.at(at(Alpha)), m_spins.at(at(Alpha)) }) {
        widget->setVisible(rgba);
    }
    // Sem documento não há paleta editável (cores RGBA sempre são).
    const bool editable = rgba || m_session->activeDocument() != nullptr;
    for (std::size_t channel = 0; channel < ChannelCount; ++channel) {
        m_sliders.at(channel)->setEnabled(editable);
        m_spins.at(channel)->setEnabled(editable);
    }
    m_updating = false;
}

void ColorEditor::applyChannel(Channel channel, int value)
{
    if (m_updating) {
        return;
    }
    const ColorRole role = m_session->activeColorRole();
    if (m_session->colorMode() == core::ColorMode::Rgba) {
        // RGBA: edita a cor ativa diretamente (não há paleta nem histórico).
        m_session->setRgbaFor(role, withChannel(m_session->rgbaFor(role), channel, value));
        return;
    }

    Document* document = m_session->activeDocument();
    if (document == nullptr || channel == Alpha) {
        return;
    }
    const int index
        = role == ColorRole::Primary ? m_session->drawColor() : m_session->backgroundColor();
    core::Palette after = document->palette();
    const core::Rgba current = core::Rgba::opaque(after.color(index));
    const core::Rgba changed = withChannel(current, channel, value);
    if (changed == current) {
        return;
    }
    after.setColor(index, changed.rgb());
    // mergeKey = índice da cor: ajustes seguidos na mesma cor viram 1 passo.
    document->undoStack()->push(new PaletteCommand(document, document->palette(), after,
        UndoLabel { UndoOperation::EditColor, index }, index));
}

} // namespace spritenator::gui
