// =============================================================================
// CanvasSizeDialog.cpp
// =============================================================================
#include "CanvasSizeDialog.h"

#include "core/IndexedImage.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace spritenator::gui {

CanvasSizeDialog::CanvasSizeDialog(core::Size current, QWidget* parent)
    : QDialog(parent)
    , m_width(new QSpinBox(this))
    , m_height(new QSpinBox(this))
    , m_scale(new QCheckBox(tr("Scale the image to the new canvas size"), this))
{
    setWindowTitle(tr("Canvas Size"));
    for (QSpinBox* spin : { m_width, m_height }) {
        spin->setRange(1, core::IndexedImage::kMaxDimension);
        spin->setSuffix(tr(" px"));
    }
    m_width->setValue(current.width);
    m_height->setValue(current.height);

    auto* sizeBox = new QGroupBox(tr("Size"), this);
    auto* sizeLayout = new QFormLayout(sizeBox);
    sizeLayout->addRow(tr("Width:"), m_width);
    sizeLayout->addRow(tr("Height:"), m_height);

    auto* optionBox = new QGroupBox(tr("Option"), this);
    auto* optionLayout = new QVBoxLayout(optionBox);
    optionLayout->addWidget(m_scale);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(sizeBox);
    layout->addWidget(optionBox);
    layout->addWidget(buttons);
}

core::Size CanvasSizeDialog::newSize() const
{
    return { m_width->value(), m_height->value() };
}

bool CanvasSizeDialog::scaleContents() const
{
    return m_scale->isChecked();
}

} // namespace spritenator::gui
