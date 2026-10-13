// =============================================================================
// LoadPaletteDialog.cpp
// =============================================================================
#include "LoadPaletteDialog.h"

#include "gui/widgets/PaletteGrid.h"
#include "io/IoError.h"
#include "io/PaletteFile.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace spritenator::gui {

LoadPaletteDialog::LoadPaletteDialog(
    const core::Palette& current, const QString& startDirectory, QWidget* parent)
    : QDialog(parent)
    , m_current(current)
    , m_startDirectory(startDirectory)
    , m_fileLabel(new QLabel(tr("(no file)"), this))
    , m_loadedGrid(new PaletteGrid(this))
    , m_resultGrid(new PaletteGrid(this))
    , m_readAll(new QRadioButton(tr("Read all colors"), this))
    , m_readRange(new QRadioButton(tr("Read a range"), this))
    , m_from(new QSpinBox(this))
    , m_count(new QSpinBox(this))
    , m_destination(new QSpinBox(this))
    , m_keepIndices(new QRadioButton(tr("Keep pixel indices (the image colors may change)"), this))
    , m_nearest(new QRadioButton(tr("Replace each color with the nearest new color"), this))
    , m_dither(new QRadioButton(tr("Reproduce the colors with error diffusion (dithering)"), this))
    , m_avoidSystem(new QCheckBox(tr("Do not use the 20 system colors"), this))
    , m_transparentZero(new QCheckBox(tr("Treat palette index 0 as the transparent color"), this))
{
    setWindowTitle(tr("Load Palette"));

    // --- Arquivo e prévias ---
    auto* browseButton = new QPushButton(tr("&Browse..."), this);
    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(m_fileLabel, 1);
    fileRow->addWidget(browseButton);

    auto* previews = new QGridLayout;
    previews->addWidget(new QLabel(tr("Colors in the file:"), this), 0, 0);
    previews->addWidget(new QLabel(tr("Resulting palette:"), this), 0, 1);
    previews->addWidget(m_loadedGrid, 1, 0);
    previews->addWidget(m_resultGrid, 1, 1);

    auto* fileBox = new QGroupBox(tr("Palette file"), this);
    auto* fileLayout = new QVBoxLayout(fileBox);
    fileLayout->addLayout(fileRow);
    fileLayout->addLayout(previews);

    // --- Posição de leitura ---
    m_from->setRange(0, 255);
    m_count->setRange(1, 256);
    m_count->setValue(256);
    m_destination->setRange(0, 255);
    m_readAll->setChecked(true);
    auto* rangeRow = new QHBoxLayout;
    rangeRow->addWidget(m_readRange);
    rangeRow->addWidget(new QLabel(tr("from"), this));
    rangeRow->addWidget(m_from);
    rangeRow->addWidget(new QLabel(tr("count"), this));
    rangeRow->addWidget(m_count);
    rangeRow->addWidget(new QLabel(tr("to position"), this));
    rangeRow->addWidget(m_destination);
    rangeRow->addStretch(1);
    auto* positionBox = new QGroupBox(tr("Reading position"), this);
    auto* positionLayout = new QVBoxLayout(positionBox);
    positionLayout->addWidget(m_readAll);
    positionLayout->addLayout(rangeRow);

    // --- Tratamento dos pixels ---
    m_nearest->setChecked(true);
    m_avoidSystem->setChecked(true);
    m_transparentZero->setChecked(true);
    auto* methodBox = new QGroupBox(tr("How to apply the colors"), this);
    auto* methodLayout = new QVBoxLayout(methodBox);
    for (QWidget* widget : std::initializer_list<QWidget*> {
             m_keepIndices, m_nearest, m_dither, m_avoidSystem, m_transparentZero }) {
        methodLayout->addWidget(widget);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_loaded.empty()) {
            QMessageBox::warning(this, windowTitle(), tr("Choose a palette file first."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(fileBox);
    layout->addWidget(positionBox);
    layout->addWidget(methodBox);
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, &LoadPaletteDialog::browse);
    for (QRadioButton* radio : { m_readAll, m_readRange, m_keepIndices, m_nearest, m_dither }) {
        connect(radio, &QRadioButton::toggled, this, &LoadPaletteDialog::refresh);
    }
    for (QSpinBox* spin : { m_from, m_count, m_destination }) {
        connect(spin, &QSpinBox::valueChanged, this, &LoadPaletteDialog::refresh);
    }
    refresh();
}

void LoadPaletteDialog::browse()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Load Palette"), m_startDirectory,
        io::PaletteFile::openFilters().join(QStringLiteral(";;")));
    if (path.isEmpty()) {
        return;
    }
    try {
        m_loaded = io::PaletteFile::load(path);
    } catch (const io::IoError& error) {
        QMessageBox::critical(this, windowTitle(), error.message());
        return;
    }
    m_filePath = path;
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_from->setMaximum(static_cast<int>(m_loaded.size()) - 1);
    refresh();
}

core::Palette LoadPaletteDialog::resultPalette() const
{
    const auto loadedCount = static_cast<int>(m_loaded.size());
    if (m_readAll->isChecked()) {
        return core::mergeColors(m_current, m_loaded, 0, loadedCount, 0);
    }
    return core::mergeColors(
        m_current, m_loaded, m_from->value(), m_count->value(), m_destination->value());
}

core::PaletteApplyOptions LoadPaletteDialog::applyOptions() const
{
    core::PaletteApplyOptions options;
    if (m_keepIndices->isChecked()) {
        options.method = core::RemapMethod::KeepIndices;
    } else if (m_dither->isChecked()) {
        options.method = core::RemapMethod::Dither;
    } else {
        options.method = core::RemapMethod::NearestColor;
    }
    options.avoidSystemColors = m_avoidSystem->isChecked();
    options.keepTransparentZero = m_transparentZero->isChecked();
    return options;
}

void LoadPaletteDialog::refresh()
{
    // Campos do intervalo só fazem sentido no modo "intervalo".
    for (QSpinBox* spin : { m_from, m_count, m_destination }) {
        spin->setEnabled(m_readRange->isChecked());
    }
    // As opções de remapeamento não se aplicam a "manter índices".
    const bool remapping = !m_keepIndices->isChecked();
    m_avoidSystem->setEnabled(remapping && m_current.size() == core::Palette::kMaxColors);
    m_transparentZero->setEnabled(remapping);

    if (m_loaded.empty()) {
        m_loadedGrid->setEnabled(false);
        m_resultGrid->setColorPalette(m_current);
        return;
    }
    // Prévia das cores lidas (exibidas numa paleta de 256 entradas).
    core::Palette loaded(core::ColorDepth::Bits8);
    const auto count = std::min<int>(static_cast<int>(m_loaded.size()), loaded.size());
    for (int index = 0; index < count; ++index) {
        loaded.setColor(index, m_loaded.at(static_cast<std::size_t>(index)));
    }
    m_loadedGrid->setEnabled(true);
    m_loadedGrid->setColorPalette(loaded);
    m_loadedGrid->setVisibleCount(count);
    m_resultGrid->setColorPalette(resultPalette());
}

} // namespace spritenator::gui
