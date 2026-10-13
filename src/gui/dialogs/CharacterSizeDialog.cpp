// =============================================================================
// CharacterSizeDialog.cpp
// =============================================================================
#include "CharacterSizeDialog.h"

#include "core/Image.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QMessageBox>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

// Valores sugeridos nos combos (os mesmos do original); qualquer valor
// pode ser digitado.
const QStringList kCellSizes = { QStringLiteral("16"), QStringLiteral("20"), QStringLiteral("24"),
    QStringLiteral("32"), QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("192") };
const QStringList kSubGridSizes = { QStringLiteral("8"), QStringLiteral("10"), QStringLiteral("12"),
    QStringLiteral("16"), QStringLiteral("32"), QStringLiteral("64"), QStringLiteral("96") };

/// Cria um combo editável que aceita apenas inteiros positivos.
QComboBox* makeSizeCombo(const QStringList& items, int value, QWidget* parent)
{
    auto* combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItems(items);
    combo->setValidator(new QIntValidator(1, core::IndexedImage::kMaxDimension, combo));
    combo->setCurrentText(QString::number(value));
    return combo;
}

/// Valor inteiro de um combo (0 se inválido).
int valueOf(const QComboBox* combo)
{
    return combo->currentText().toInt();
}

} // namespace

CharacterSizeDialog::CharacterSizeDialog(const core::CharacterLayout& layout, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Character Size"));

    // --- Tamanho da célula ---
    m_cellWidth = makeSizeCombo(kCellSizes, layout.cellSize.width, this);
    m_cellHeight = makeSizeCombo(kCellSizes, layout.cellSize.height, this);
    auto* cellBox = new QGroupBox(tr("Character basic size"), this);
    auto* cellLayout = new QFormLayout(cellBox);
    cellLayout->addRow(tr("Width:"), m_cellWidth);
    cellLayout->addRow(tr("Height:"), m_cellHeight);

    // --- Sub-grade ---
    m_subGridWidth = makeSizeCombo(kSubGridSizes, layout.subGridSize.width, this);
    m_subGridHeight = makeSizeCombo(kSubGridSizes, layout.subGridSize.height, this);
    auto* subGridBox = new QGroupBox(tr("Sub grid spacing"), this);
    auto* subGridLayout = new QFormLayout(subGridBox);
    subGridLayout->addRow(tr("Width:"), m_subGridWidth);
    subGridLayout->addRow(tr("Height:"), m_subGridHeight);

    // --- Opções ---
    m_doubleCanvas = new QCheckBox(tr("Show the canvas at double size"), this);
    m_halfCellSteps = new QCheckBox(tr("Move the cell cursor in half-character steps"), this);
    m_keepMouseCursor = new QCheckBox(tr("Do not change the mouse cursor shape"), this);
    m_doubleCanvas->setChecked(layout.doubleCanvas);
    m_halfCellSteps->setChecked(layout.halfCellSteps);
    m_keepMouseCursor->setChecked(layout.keepMouseCursor);
    auto* optionBox = new QGroupBox(tr("Options"), this);
    auto* optionLayout = new QVBoxLayout(optionBox);
    optionLayout->addWidget(m_doubleCanvas);
    optionLayout->addWidget(m_halfCellSteps);
    optionLayout->addWidget(m_keepMouseCursor);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CharacterSizeDialog::acceptIfValid);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(cellBox);
    mainLayout->addWidget(subGridBox);
    mainLayout->addWidget(optionBox);
    mainLayout->addWidget(buttons);
}

core::CharacterLayout CharacterSizeDialog::layout() const
{
    core::CharacterLayout result;
    result.cellSize = { valueOf(m_cellWidth), valueOf(m_cellHeight) };
    result.subGridSize = { valueOf(m_subGridWidth), valueOf(m_subGridHeight) };
    result.doubleCanvas = m_doubleCanvas->isChecked();
    result.halfCellSteps = m_halfCellSteps->isChecked();
    result.keepMouseCursor = m_keepMouseCursor->isChecked();
    return result;
}

void CharacterSizeDialog::acceptIfValid()
{
    const core::CharacterLayout chosen = layout();
    if (chosen.cellSize.isEmpty() || chosen.subGridSize.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("All sizes must be positive numbers."));
        return;
    }
    accept();
}

} // namespace spritenator::gui
