// =============================================================================
// NewImageDialogTest.cpp — diálogo "Nova imagem": histórico de escolhas
// anteriores (tamanho + modo de cor) e as seções RGBA / Indexada.
// =============================================================================
#include "gui/dialogs/NewImageDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QTest>

using namespace spritenator;
using gui::NewImageDialog;
using gui::NewImageEntry;

namespace {

const NewImageEntry kCharset { { 72, 128 }, core::ColorMode::Indexed, core::ColorDepth::Bits4 };
const NewImageEntry kSheet { { 288, 256 }, core::ColorMode::Rgba, core::ColorDepth::Bits8 };

/// Botão de opção com o texto informado.
QRadioButton* radio(const NewImageDialog& dialog, const QString& text)
{
    for (QRadioButton* button : dialog.findChildren<QRadioButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

QSpinBox* spin(const NewImageDialog& dialog, int index)
{
    return dialog.findChildren<QSpinBox*>().at(index);
}

} // namespace

class NewImageDialogTest final : public QObject {
    Q_OBJECT

private slots:
    /// Sem histórico: padrão 320 x 240 em RGBA; o combo fica desabilitado e
    /// não há predefinições.
    void defaultsWithoutHistory()
    {
        const NewImageDialog dialog(true, { });
        QCOMPARE(dialog.imageSize(), (core::Size { 320, 240 }));
        QCOMPARE(dialog.colorMode(), core::ColorMode::Rgba);
        QVERIFY(!dialog.historyCombo()->isEnabled());
        QCOMPARE(dialog.historyCombo()->count(), 0);
        QVERIFY(!dialog.copyCurrentPalette());
    }

    /// Abre com a escolha mais recente; escolher outra entrada preenche tudo.
    void historyFillsSizeAndColors()
    {
        NewImageDialog dialog(true, { kCharset, kSheet });
        QCOMPARE(dialog.historyCombo()->count(), 2);
        QCOMPARE(dialog.historyCombo()->itemText(0), QStringLiteral("72 × 128 · 16 colors"));
        QCOMPARE(dialog.historyCombo()->itemText(1), QStringLiteral("288 × 256 · RGBA"));
        QCOMPARE(dialog.entry(), kCharset);
        QCOMPARE(dialog.historyCombo()->currentIndex(), 0);

        dialog.historyCombo()->setCurrentIndex(1);
        emit dialog.historyCombo()->activated(1);
        QCOMPARE(dialog.entry(), kSheet);
        QCOMPARE(dialog.colorMode(), core::ColorMode::Rgba);
    }

    /// Editar os valores desmarca a entrada do histórico; voltar a eles a
    /// marca de novo.
    void editingClearsHistorySelection()
    {
        const NewImageDialog dialog(false, { kCharset });
        spin(dialog, 0)->setValue(100);
        QCOMPARE(dialog.historyCombo()->currentIndex(), -1);
        spin(dialog, 0)->setValue(72);
        QCOMPARE(dialog.historyCombo()->currentIndex(), 0);
        radio(dialog, QStringLiteral("2 colors"))->setChecked(true);
        QCOMPARE(dialog.historyCombo()->currentIndex(), -1);
    }

    /// Seção Indexada: 256, 16 e 2 cores, só habilitadas fora do modo RGBA;
    /// copiar a paleta só vale no modo indexado.
    void indexedSectionFollowsMode()
    {
        const NewImageDialog dialog(true, { });
        QRadioButton* colors16 = radio(dialog, QStringLiteral("16 colors"));
        QVERIFY(colors16 != nullptr);
        QVERIFY(radio(dialog, QStringLiteral("256 colors")) != nullptr);
        QVERIFY(radio(dialog, QStringLiteral("2 colors")) != nullptr);
        QVERIFY(!colors16->isEnabled());
        auto* copy = dialog.findChild<QCheckBox*>();
        copy->setChecked(true);
        QVERIFY(!dialog.copyCurrentPalette());

        radio(dialog, QStringLiteral("Indexed (palette)"))->setChecked(true);
        QVERIFY(colors16->isEnabled());
        colors16->setChecked(true);
        QCOMPARE(dialog.colorMode(), core::ColorMode::Indexed);
        QCOMPARE(dialog.colorDepth(), core::ColorDepth::Bits4);
        QVERIFY(dialog.copyCurrentPalette());
    }
};

QTEST_MAIN(NewImageDialogTest)
#include "NewImageDialogTest.moc"
