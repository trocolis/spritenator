// =============================================================================
// GridDialog.cpp
// =============================================================================
#include "GridDialog.h"

#include "core/Image.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

// Valores sugeridos nos combos; qualquer valor pode ser digitado.
const QStringList kMainGridSizes = { QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("4"),
    QStringLiteral("8"), QStringLiteral("16") };
// Tamanhos de personagem do Character Maker 1999.
const QStringList kSubGridSizes
    = { QStringLiteral("16"), QStringLiteral("20"), QStringLiteral("24"), QStringLiteral("32"),
          QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("192") };

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

GridDialog::GridDialog(const core::GridLayout& layout, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Grids"));

    // --- Grade principal ---
    m_mainWidth = makeSizeCombo(kMainGridSizes, layout.mainGrid.width, this);
    m_mainHeight = makeSizeCombo(kMainGridSizes, layout.mainGrid.height, this);
    auto* mainBox = new QGroupBox(tr("Main grid"), this);
    auto* mainLayout = new QFormLayout(mainBox);
    mainLayout->addRow(tr("Width:"), m_mainWidth);
    mainLayout->addRow(tr("Height:"), m_mainHeight);

    // --- Sub-grade (tamanho do personagem) ---
    m_subWidth = makeSizeCombo(kSubGridSizes, layout.subGrid.width, this);
    m_subHeight = makeSizeCombo(kSubGridSizes, layout.subGrid.height, this);
    auto* subBox = new QGroupBox(tr("Sub grid (character size)"), this);
    auto* subLayout = new QFormLayout(subBox);
    subLayout->addRow(tr("Width:"), m_subWidth);
    subLayout->addRow(tr("Height:"), m_subHeight);

    auto* note = new QLabel(tr("The main grid is hidden while its lines would be too close "
                               "together on screen. The sub grid also defines the animation "
                               "frames."),
        this);
    note->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GridDialog::acceptIfValid);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layoutBox = new QVBoxLayout(this);
    layoutBox->addWidget(mainBox);
    layoutBox->addWidget(subBox);
    layoutBox->addWidget(note);
    layoutBox->addWidget(buttons);
}

core::GridLayout GridDialog::layout() const
{
    core::GridLayout result;
    result.mainGrid = { valueOf(m_mainWidth), valueOf(m_mainHeight) };
    result.subGrid = { valueOf(m_subWidth), valueOf(m_subHeight) };
    return result;
}

void GridDialog::acceptIfValid()
{
    const core::GridLayout chosen = layout();
    if (chosen.mainGrid.isEmpty() || chosen.subGrid.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("All sizes must be positive numbers."));
        return;
    }
    accept();
}

} // namespace spritenator::gui
