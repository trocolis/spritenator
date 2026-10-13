// =============================================================================
// StretchDialog.cpp
// =============================================================================
#include "StretchDialog.h"

#include "core/IndexedImage.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace spritenator::gui {

StretchDialog::StretchDialog(core::Size current, QWidget* parent)
    : QDialog(parent)
    , m_original(current)
    , m_width(new QSpinBox(this))
    , m_height(new QSpinBox(this))
    , m_keepAspect(new QCheckBox(tr("Keep aspect ratio"), this))
{
    setWindowTitle(tr("Stretch"));
    for (QSpinBox* spin : { m_width, m_height }) {
        spin->setRange(1, core::IndexedImage::kMaxDimension);
        spin->setSuffix(tr(" px"));
    }
    m_width->setValue(current.width);
    m_height->setValue(current.height);

    auto* form = new QFormLayout;
    form->addRow(tr("Width:"), m_width);
    form->addRow(tr("Height:"), m_height);
    form->addRow(QString(), m_keepAspect);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    // Com "manter proporção", alterar um lado recalcula o outro.
    const auto keepRatio = [this](QSpinBox* changed, QSpinBox* other, int from, int to) {
        if (m_updating || !m_keepAspect->isChecked() || from <= 0) {
            return;
        }
        m_updating = true;
        const double ratio = static_cast<double>(changed->value()) / from;
        other->setValue(std::max(1, static_cast<int>(std::lround(to * ratio))));
        m_updating = false;
    };
    connect(m_width, &QSpinBox::valueChanged, this,
        [this, keepRatio] { keepRatio(m_width, m_height, m_original.width, m_original.height); });
    connect(m_height, &QSpinBox::valueChanged, this,
        [this, keepRatio] { keepRatio(m_height, m_width, m_original.height, m_original.width); });
}

core::Size StretchDialog::newSize() const
{
    return { m_width->value(), m_height->value() };
}

} // namespace spritenator::gui
