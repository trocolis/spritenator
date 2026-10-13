// =============================================================================
// TextInputDialog.cpp
// =============================================================================
#include "TextInputDialog.h"

#include "gui/EditOperations.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Limites de tamanho da fonte (iguais aos do original).
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 64;
/// Ampliação da prévia para enxergar os pixels.
constexpr int kPreviewScale = 3;

} // namespace

TextInputDialog::TextInputDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontName(new QFontComboBox(this))
    , m_size(new QSpinBox(this))
    , m_bold(new QCheckBox(tr("Bold"), this))
    , m_italic(new QCheckBox(tr("Italic"), this))
    , m_underline(new QCheckBox(tr("Underline"), this))
    , m_strikeOut(new QCheckBox(tr("Strikeout"), this))
    , m_text(new QPlainTextEdit(this))
    , m_preview(new QLabel(this))
{
    setWindowTitle(tr("Text Input"));

    m_size->setRange(kMinFontSize, kMaxFontSize);
    m_size->setValue(kMinFontSize);
    m_size->setSuffix(tr(" px"));

    auto* fontBox = new QGroupBox(tr("Font"), this);
    auto* fontLayout = new QFormLayout(fontBox);
    fontLayout->addRow(tr("Name:"), m_fontName);
    fontLayout->addRow(tr("Size:"), m_size);
    auto* styles = new QHBoxLayout;
    for (QCheckBox* style : { m_bold, m_italic, m_underline, m_strikeOut }) {
        styles->addWidget(style);
    }
    fontLayout->addRow(tr("Style:"), styles);

    auto* textBox = new QGroupBox(tr("Text"), this);
    auto* textLayout = new QVBoxLayout(textBox);
    textLayout->addWidget(m_text);
    m_text->setMaximumBlockCount(64);

    auto* previewBox = new QGroupBox(tr("Preview"), this);
    auto* previewLayout = new QVBoxLayout(previewBox);
    m_preview->setMinimumHeight(48);
    m_preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    previewLayout->addWidget(m_preview);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(fontBox);
    layout->addWidget(textBox);
    layout->addWidget(previewBox);
    layout->addWidget(buttons);

    // Qualquer alteração atualiza a prévia.
    connect(m_fontName, &QFontComboBox::currentFontChanged, this, &TextInputDialog::updatePreview);
    connect(m_size, &QSpinBox::valueChanged, this, &TextInputDialog::updatePreview);
    for (QCheckBox* style : { m_bold, m_italic, m_underline, m_strikeOut }) {
        connect(style, &QCheckBox::toggled, this, &TextInputDialog::updatePreview);
    }
    connect(m_text, &QPlainTextEdit::textChanged, this, &TextInputDialog::updatePreview);
    updatePreview();
}

QString TextInputDialog::text() const
{
    return m_text->toPlainText();
}

QFont TextInputDialog::selectedFont() const
{
    QFont font = m_fontName->currentFont();
    font.setPixelSize(m_size->value()); // tamanho em PIXELS, não em pontos
    font.setBold(m_bold->isChecked());
    font.setItalic(m_italic->isChecked());
    font.setUnderline(m_underline->isChecked());
    font.setStrikeOut(m_strikeOut->isChecked());
    return font;
}

void TextInputDialog::updatePreview()
{
    const core::IndexedImage mask = EditOperations::renderText(text(), selectedFont());
    if (mask.isEmpty()) {
        m_preview->clear();
        return;
    }
    // Preto sobre branco, ampliado sem suavização.
    QImage image(mask.width(), mask.height(), QImage::Format_RGB32);
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            image.setPixel(x, y, mask.pixel(x, y) != 0 ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
        }
    }
    m_preview->setPixmap(QPixmap::fromImage(
        image.scaled(image.size() * kPreviewScale, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
}

} // namespace spritenator::gui
