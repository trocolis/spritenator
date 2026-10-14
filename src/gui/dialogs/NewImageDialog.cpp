// =============================================================================
// NewImageDialog.cpp
// =============================================================================
#include "NewImageDialog.h"

#include "core/Image.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace spritenator::gui {

NewImageDialog::NewImageDialog(
    bool canCopyPalette, const QList<NewImageEntry>& history, QWidget* parent)
    : QDialog(parent)
    , m_entries(history)
    , m_canCopyPalette(canCopyPalette)
    , m_history(new QComboBox(this))
    , m_width(new QSpinBox(this))
    , m_height(new QSpinBox(this))
    , m_rgba(new QRadioButton(tr("RGBA (32 bit, true color with transparency)"), this))
    , m_indexed(new QRadioButton(tr("Indexed (palette)"), this))
    , m_indexedOptions(new QWidget(this))
    , m_colors256(new QRadioButton(tr("256 colors"), m_indexedOptions))
    , m_colors16(new QRadioButton(tr("16 colors"), m_indexedOptions))
    , m_colors2(new QRadioButton(tr("2 colors"), m_indexedOptions))
    , m_copyPalette(new QCheckBox(tr("Copy current palette"), m_indexedOptions))
{
    setWindowTitle(tr("New Image"));

    // --- Tamanho, com o histórico das escolhas anteriores ---
    for (const NewImageEntry& entry : std::as_const(m_entries)) {
        m_history->addItem(entryLabel(entry));
    }
    m_history->setEnabled(!m_entries.isEmpty());
    m_history->setPlaceholderText(
        m_entries.isEmpty() ? tr("None yet") : tr("Choose a previous size..."));
    m_history->setToolTip(tr("Sizes and color modes used before (most recent first)"));
    for (QSpinBox* spin : { m_width, m_height }) {
        spin->setRange(1, core::IndexedImage::kMaxDimension);
        spin->setSuffix(tr(" px"));
    }

    auto* sizeBox = new QGroupBox(tr("Canvas size"), this);
    auto* sizeLayout = new QFormLayout(sizeBox);
    sizeLayout->addRow(tr("Used before:"), m_history);
    sizeLayout->addRow(tr("Width:"), m_width);
    sizeLayout->addRow(tr("Height:"), m_height);

    // --- Cores: duas seções, RGBA e Indexada (256, 16 ou 2 cores) ---
    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_rgba);
    modeGroup->addButton(m_indexed);
    auto* depthGroup = new QButtonGroup(this);
    for (QRadioButton* depth : { m_colors256, m_colors16, m_colors2 }) {
        depthGroup->addButton(depth);
    }
    m_copyPalette->setToolTip(tr("Use the palette of the active image in the new image"));

    auto* depthRow = new QHBoxLayout;
    depthRow->addWidget(m_colors256);
    depthRow->addWidget(m_colors16);
    depthRow->addWidget(m_colors2);
    depthRow->addStretch(1);
    auto* indexedLayout = new QVBoxLayout(m_indexedOptions);
    indexedLayout->setContentsMargins(24, 0, 0, 0); // recuo: opções da seção Indexada
    indexedLayout->addLayout(depthRow);
    indexedLayout->addWidget(m_copyPalette);

    auto* colorBox = new QGroupBox(tr("Colors"), this);
    auto* colorLayout = new QVBoxLayout(colorBox);
    colorLayout->addWidget(m_rgba);
    colorLayout->addWidget(m_indexed);
    colorLayout->addWidget(m_indexedOptions);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(sizeBox);
    layout->addWidget(colorBox);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_history, &QComboBox::activated, this,
        [this](int index) { applyEntry(m_entries.at(index)); });
    connect(m_indexed, &QRadioButton::toggled, this, &NewImageDialog::updateIndexedOptions);

    // Qualquer edição que não corresponda à entrada mostrada a desmarca.
    for (QSpinBox* spin : { m_width, m_height }) {
        connect(spin, &QSpinBox::valueChanged, this, &NewImageDialog::syncHistorySelection);
    }
    for (QRadioButton* radio : { m_rgba, m_indexed, m_colors256, m_colors16, m_colors2 }) {
        connect(radio, &QRadioButton::toggled, this, &NewImageDialog::syncHistorySelection);
    }

    // Abre com a escolha mais recente (ou o padrão: 320 x 240, RGBA).
    applyEntry(m_entries.value(0, NewImageEntry { }));
}

QString NewImageDialog::entryLabel(const NewImageEntry& entry)
{
    QString mode;
    if (entry.mode == core::ColorMode::Rgba) {
        mode = tr("RGBA");
    } else {
        mode = tr("%1 colors").arg(core::colorCountOf(entry.depth));
    }
    return tr("%1 × %2 · %3").arg(entry.size.width).arg(entry.size.height).arg(mode);
}

void NewImageDialog::applyEntry(const NewImageEntry& entry)
{
    m_width->setValue(entry.size.width);
    m_height->setValue(entry.size.height);
    m_rgba->setChecked(entry.mode == core::ColorMode::Rgba);
    m_indexed->setChecked(entry.mode == core::ColorMode::Indexed);
    // A profundidade fica marcada mesmo em RGBA: é a usada se o usuário
    // trocar para Indexada (padrão: 256 cores).
    switch (entry.depth) {
    case core::ColorDepth::Bits1: m_colors2->setChecked(true); break;
    case core::ColorDepth::Bits4: m_colors16->setChecked(true); break;
    case core::ColorDepth::Bits8: m_colors256->setChecked(true); break;
    }
    updateIndexedOptions();
    syncHistorySelection();
}

void NewImageDialog::updateIndexedOptions()
{
    m_indexedOptions->setEnabled(m_indexed->isChecked());
    // Copiar a paleta exige uma imagem ativa com paleta.
    m_copyPalette->setEnabled(m_canCopyPalette);
}

void NewImageDialog::syncHistorySelection()
{
    m_history->setCurrentIndex(static_cast<int>(m_entries.indexOf(entry())));
}

NewImageEntry NewImageDialog::entry() const
{
    return { imageSize(), colorMode(), colorDepth() };
}

core::Size NewImageDialog::imageSize() const
{
    return { m_width->value(), m_height->value() };
}

core::ColorMode NewImageDialog::colorMode() const
{
    return m_indexed->isChecked() ? core::ColorMode::Indexed : core::ColorMode::Rgba;
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
    return colorMode() == core::ColorMode::Indexed && m_canCopyPalette
        && m_copyPalette->isChecked();
}

} // namespace spritenator::gui
