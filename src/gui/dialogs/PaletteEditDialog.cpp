// =============================================================================
// PaletteEditDialog.cpp
// =============================================================================
#include "PaletteEditDialog.h"

#include "gui/ImageRendering.h"
#include "gui/widgets/PaletteGrid.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

namespace spritenator::gui {

PaletteEditDialog::PaletteEditDialog(
    const core::Palette& palette, const core::IndexedImage& canvas, QWidget* parent)
    : QDialog(parent)
    , m_palette(palette)
    , m_canvas(canvas)
    , m_remap(core::identityRemap())
    , m_grid(new PaletteGrid(this))
    , m_info(new QLabel(this))
{
    setWindowTitle(tr("Edit Palette"));
    m_grid->setColorPalette(m_palette);

    // --- Botões de operação ---
    struct ButtonSpec {
        QString text;
        QString tip;
        std::function<void()> action;
    };
    const ButtonSpec specs[] = {
        { tr("&Edit Color..."), tr("Change the current color (also: double-click)"),
            [this] { editColor(m_grid->current()); } },
        { tr("&Copy"), tr("Copy the selected colors"), [this] { copyColors(); } },
        { tr("&Paste"), tr("Paste the copied colors starting at the current color"),
            [this] { pasteColors(); } },
        { tr("E&xchange"), tr("Swap the first and last selected colors; the image keeps its look"),
            [this] { swapColors(); } },
        { tr("&Gradient"), tr("Interpolate the colors between the ends of the selection"),
            [this] { makeGradient(); } },
        { tr("Remove &Unused Colors"),
            tr("Move the colors used by the image to the beginning and clear the rest"),
            [this] { removeUnused(); } },
        { tr("&15-bit Colors"), tr("Reduce every color to 5 bits per channel (RPG Maker 2000)"),
            [this] { convertTo15Bit(); } },
        { tr("&Reset"), tr("Replace the palette with the default palette"),
            [this] { resetToDefault(); } },
    };
    auto* buttonGrid = new QGridLayout;
    int position = 0;
    for (const ButtonSpec& spec : specs) {
        auto* button = new QPushButton(spec.text, this);
        button->setToolTip(spec.tip);
        button->setAutoDefault(false); // Enter confirma o diálogo, não a operação
        buttonGrid->addWidget(button, position / 2, position % 2);
        connect(button, &QPushButton::clicked, this, spec.action);
        ++position;
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* top = new QHBoxLayout;
    top->addWidget(m_grid, 1);
    top->addLayout(buttonGrid);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_info);
    layout->addWidget(buttons);

    connect(m_grid, &PaletteGrid::selectionChanged, this, &PaletteEditDialog::refresh);
    connect(m_grid, &PaletteGrid::colorDoubleClicked, this, &PaletteEditDialog::editColor);
    refresh();
}

void PaletteEditDialog::refresh()
{
    const int first = m_grid->selectionFirst();
    const int last = m_grid->selectionLast();
    const core::Rgb& color = m_palette.color(m_grid->current());
    QString text = tr("Color %1: R %2  G %3  B %4")
                       .arg(m_grid->current())
                       .arg(color.red)
                       .arg(color.green)
                       .arg(color.blue);
    if (first != last) {
        text += tr("   |   Selection: %1 - %2").arg(first).arg(last);
    }
    m_info->setText(text);
}

// -----------------------------------------------------------------------------
// Operações
// -----------------------------------------------------------------------------
void PaletteEditDialog::editColor(int index)
{
    const QColor chosen
        = QColorDialog::getColor(toQColor(m_palette.color(index)), this, tr("Color %1").arg(index));
    if (chosen.isValid()) {
        m_palette.setColor(index, toRgb(chosen));
        m_grid->setColorPalette(m_palette);
        refresh();
    }
}

void PaletteEditDialog::copyColors()
{
    m_copiedColors.clear();
    for (int index = m_grid->selectionFirst(); index <= m_grid->selectionLast(); ++index) {
        m_copiedColors.push_back(m_palette.color(index));
    }
}

void PaletteEditDialog::pasteColors()
{
    if (m_copiedColors.empty()) {
        return;
    }
    const auto count = static_cast<int>(m_copiedColors.size());
    m_palette = core::mergeColors(m_palette, m_copiedColors, 0, count, m_grid->current());
    m_grid->setColorPalette(m_palette);
    refresh();
}

void PaletteEditDialog::swapColors()
{
    const int a = m_grid->anchor();
    const int b = m_grid->current();
    if (a == b) {
        QMessageBox::information(this, windowTitle(),
            tr("Select two colors: click the first one and Shift+click the second one."));
        return;
    }
    const core::Rgb colorA = m_palette.color(a);
    m_palette.setColor(a, m_palette.color(b));
    m_palette.setColor(b, colorA);
    applyRemap(core::swapRemap(a, b)); // pixels acompanham a troca
    m_grid->setColorPalette(m_palette);
    refresh();
}

void PaletteEditDialog::makeGradient()
{
    m_palette.interpolate(m_grid->selectionFirst(), m_grid->selectionLast());
    m_grid->setColorPalette(m_palette);
    refresh();
}

void PaletteEditDialog::removeUnused()
{
    const core::CompactResult result = core::removeUnusedColors(m_palette, m_canvas.usedColors());
    m_palette = result.palette;
    applyRemap(result.remap);
    m_grid->setColorPalette(m_palette);
    refresh();
    QMessageBox::information(
        this, windowTitle(), tr("%n unused color(s) removed.", nullptr, result.removedCount));
}

void PaletteEditDialog::convertTo15Bit()
{
    m_palette.convertTo15Bit();
    m_grid->setColorPalette(m_palette);
    refresh();
}

void PaletteEditDialog::resetToDefault()
{
    m_palette = core::Palette::makeDefault(m_palette.depth());
    m_grid->setColorPalette(m_palette);
    refresh();
}

void PaletteEditDialog::applyRemap(const core::RemapTable& table)
{
    m_canvas.remap(table);
    // Composição: primeiro o remapeamento acumulado, depois o novo.
    for (core::ColorIndex& value : m_remap) {
        value = table[value];
    }
}

} // namespace spritenator::gui
