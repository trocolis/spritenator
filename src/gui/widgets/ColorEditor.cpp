// =============================================================================
// ColorEditor.cpp
// =============================================================================
#include "ColorEditor.h"

#include "gui/Document.h"
#include "gui/EditorSession.h"
#include "gui/ImageRendering.h"
#include "gui/commands/ImageCommands.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Monta o texto/estilo de uma amostra de cor.
void paintSwatch(QLabel* swatch, const QColor& color, int index)
{
    // Texto em preto ou branco conforme a luminosidade, para manter contraste.
    const QColor text = color.lightness() > 128 ? Qt::black : Qt::white;
    swatch->setStyleSheet(QStringLiteral("background-color: %1; color: %2; border: 1px solid gray;")
            .arg(color.name(), text.name()));
    swatch->setText(QString::number(index));
}

} // namespace

ColorEditor::ColorEditor(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_drawSwatch(new QLabel(this))
    , m_backgroundSwatch(new QLabel(this))
{
    // --- Amostras ---
    for (QLabel* swatch : { m_drawSwatch, m_backgroundSwatch }) {
        swatch->setAlignment(Qt::AlignCenter);
        swatch->setMinimumSize(40, 28);
    }
    m_drawSwatch->setToolTip(tr("Drawing color (select with the left button)"));
    m_backgroundSwatch->setToolTip(tr("Background color (select with the right button)"));

    auto* swatches = new QHBoxLayout;
    swatches->addWidget(m_drawSwatch);
    swatches->addWidget(m_backgroundSwatch);

    // --- Controles RGB ---
    auto* channels = new QGridLayout;
    const QString names[] = { tr("R"), tr("G"), tr("B") };
    for (int channel = 0; channel < 3; ++channel) {
        auto* slider = new QSlider(Qt::Horizontal, this);
        auto* spin = new QSpinBox(this);
        slider->setRange(0, 255);
        spin->setRange(0, 255);
        channels->addWidget(new QLabel(names[channel], this), channel, 0);
        channels->addWidget(slider, channel, 1);
        channels->addWidget(spin, channel, 2);

        // Controle deslizante e caixa numérica refletem o mesmo valor.
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
        connect(slider, &QSlider::valueChanged, this,
            [this, channel](int value) { applyChannel(channel, value); });

        m_sliders.at(static_cast<std::size_t>(channel)) = slider;
        m_spins.at(static_cast<std::size_t>(channel)) = spin;
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(swatches);
    layout->addLayout(channels);

    connect(m_session, &EditorSession::colorsChanged, this, &ColorEditor::refresh);
    connect(m_session, &EditorSession::paletteChanged, this, &ColorEditor::refresh);
    connect(m_session, &EditorSession::activeDocumentChanged, this, &ColorEditor::refresh);
    refresh();
}

void ColorEditor::refresh()
{
    m_updating = true;
    const core::Palette& palette = m_session->palette();
    const core::Rgb draw = palette.color(m_session->drawColor());
    paintSwatch(m_drawSwatch, toQColor(draw), m_session->drawColor());
    paintSwatch(m_backgroundSwatch, toQColor(palette.color(m_session->backgroundColor())),
        m_session->backgroundColor());

    const int values[] = { draw.red, draw.green, draw.blue };
    for (std::size_t channel = 0; channel < 3; ++channel) {
        m_sliders.at(channel)->setValue(values[channel]);
    }
    // Sem documento não há paleta editável.
    const bool editable = m_session->activeDocument() != nullptr;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        m_sliders.at(channel)->setEnabled(editable);
        m_spins.at(channel)->setEnabled(editable);
    }
    m_updating = false;
}

void ColorEditor::applyChannel(int channel, int value)
{
    Document* document = m_session->activeDocument();
    if (m_updating || document == nullptr) {
        return;
    }
    const int index = m_session->drawColor();
    core::Palette after = document->palette();
    core::Rgb color = after.color(index);
    const auto channelValue = static_cast<std::uint8_t>(value);
    switch (channel) {
    case 0: color.red = channelValue; break;
    case 1: color.green = channelValue; break;
    default: color.blue = channelValue; break;
    }
    if (color == after.color(index)) {
        return;
    }
    after.setColor(index, color);
    // mergeKey = índice da cor: ajustes seguidos na mesma cor viram 1 passo.
    document->undoStack()->push(new PaletteCommand(
        document, document->palette(), after, tr("Edit Color %1").arg(index), index));
}

} // namespace spritenator::gui
