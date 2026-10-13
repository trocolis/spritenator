// =============================================================================
// NewImageDialog.cpp
// =============================================================================
#include "NewImageDialog.h"

#include "core/Image.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Predefinição de tamanho exibida no combo.
struct SizePreset {
    const char* label;
    core::Size size;
};

/// Tamanhos padrão do RPG Maker 2000 (ver "Canvas Sizes" no manual original).
constexpr SizePreset kPresets[] = {
    { QT_TRANSLATE_NOOP("spritenator::gui::NewImageDialog", "Custom"), { 0, 0 } },
    { QT_TRANSLATE_NOOP(
          "spritenator::gui::NewImageDialog", "RPG Maker charset - single (72 x 128)"),
        { 72, 128 } },
    { QT_TRANSLATE_NOOP(
          "spritenator::gui::NewImageDialog", "RPG Maker charset - full sheet (288 x 256)"),
        { 288, 256 } },
    { QT_TRANSLATE_NOOP("spritenator::gui::NewImageDialog", "RPG Maker chipset (480 x 256)"),
        { 480, 256 } },
    { QT_TRANSLATE_NOOP("spritenator::gui::NewImageDialog", "Screen (320 x 240)"), { 320, 240 } },
};

} // namespace

NewImageDialog::NewImageDialog(bool canCopyPalette, QWidget* parent)
    : QDialog(parent)
    , m_presets(new QComboBox(this))
    , m_width(new QSpinBox(this))
    , m_height(new QSpinBox(this))
    , m_colorsRgba(new QRadioButton(tr("RGBA (32 bit, true color with transparency)"), this))
    , m_colors256(new QRadioButton(tr("256 colors (8 bit)"), this))
    , m_colors16(new QRadioButton(tr("16 colors (4 bit)"), this))
    , m_colors2(new QRadioButton(tr("2 colors (1 bit)"), this))
    , m_copyPalette(new QCheckBox(tr("Copy current palette"), this))
{
    setWindowTitle(tr("New Image"));

    // --- Tamanho ---
    for (const SizePreset& preset : kPresets) {
        m_presets->addItem(tr(preset.label));
    }
    for (QSpinBox* spin : { m_width, m_height }) {
        spin->setRange(1, core::IndexedImage::kMaxDimension);
        spin->setSuffix(tr(" px"));
    }
    m_width->setValue(320); // valores padrão do original
    m_height->setValue(240);

    auto* sizeBox = new QGroupBox(tr("Canvas size"), this);
    auto* sizeLayout = new QFormLayout(sizeBox);
    sizeLayout->addRow(tr("Preset:"), m_presets);
    sizeLayout->addRow(tr("Width:"), m_width);
    sizeLayout->addRow(tr("Height:"), m_height);

    // --- Cores ---
    auto* colorBox = new QGroupBox(tr("Colors"), this);
    auto* colorLayout = new QVBoxLayout(colorBox);
    colorLayout->addWidget(m_colorsRgba);
    colorLayout->addWidget(m_colors256);
    colorLayout->addWidget(m_colors16);
    colorLayout->addWidget(m_colors2);
    m_colorsRgba->setChecked(true); // RGBA é a primeira opção e o padrão

    // --- Opções ---
    auto* optionBox = new QGroupBox(tr("Options"), this);
    auto* optionLayout = new QVBoxLayout(optionBox);
    optionLayout->addWidget(m_copyPalette);
    // Copiar a paleta só faz sentido para imagens indexadas.
    const auto updatePaletteOption = [this, canCopyPalette] {
        m_copyPalette->setEnabled(canCopyPalette && !m_colorsRgba->isChecked());
    };
    connect(m_colorsRgba, &QRadioButton::toggled, this, updatePaletteOption);
    updatePaletteOption();
    m_copyPalette->setToolTip(tr("Use the palette of the active image in the new image"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(sizeBox);
    layout->addWidget(colorBox);
    layout->addWidget(optionBox);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_presets, &QComboBox::activated, this, &NewImageDialog::applyPreset);

    // Editar o tamanho manualmente volta o combo para "Personalizado".
    for (QSpinBox* spin : { m_width, m_height }) {
        connect(spin, &QSpinBox::valueChanged, this, [this] {
            const core::Size current = imageSize();
            const auto& preset = kPresets[static_cast<std::size_t>(m_presets->currentIndex())];
            if (preset.size != current) {
                m_presets->setCurrentIndex(0);
            }
        });
    }
}

void NewImageDialog::applyPreset(int index)
{
    if (index <= 0 || index >= static_cast<int>(std::size(kPresets))) {
        return;
    }
    const core::Size size = kPresets[static_cast<std::size_t>(index)].size;
    m_width->setValue(size.width);
    m_height->setValue(size.height);
    m_presets->setCurrentIndex(index); // os setValue acima podem ter voltado p/ "Custom"
}

core::Size NewImageDialog::imageSize() const
{
    return { m_width->value(), m_height->value() };
}

core::ColorMode NewImageDialog::colorMode() const
{
    return m_colorsRgba->isChecked() ? core::ColorMode::Rgba : core::ColorMode::Indexed;
}

core::ColorDepth NewImageDialog::colorDepth() const
{
    if (m_colors2->isChecked()) {
        return core::ColorDepth::Bits1;
    }
    return m_colors16->isChecked() ? core::ColorDepth::Bits4 : core::ColorDepth::Bits8;
}

bool NewImageDialog::copyCurrentPalette() const
{
    return m_copyPalette->isEnabled() && m_copyPalette->isChecked();
}

} // namespace spritenator::gui
