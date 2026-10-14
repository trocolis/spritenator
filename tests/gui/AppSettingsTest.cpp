// =============================================================================
// AppSettingsTest.cpp — persistência das preferências e manual de ajuda.
// =============================================================================
#include "gui/AppSettings.h"
#include "gui/ActionCatalog.h"
#include "gui/HelpWindow.h"

#include <spritenator/Version.h>

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

using namespace spritenator;
using spritenator::gui::AppSettings;

class AppSettingsTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Configurações isoladas, apagadas antes de cada execução.
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("AppSettingsTest"));
        QSettings().clear();
    }

    /// Recentes: mais novo no topo, sem duplicatas, no máximo 4.
    void recentFilesAreBounded()
    {
        AppSettings settings;
        for (const char* name : { "a", "b", "c", "d", "e", "c" }) {
            settings.addRecentFile(QString::fromLatin1(name));
        }
        QCOMPARE(settings.recentFiles(),
            (QStringList { QStringLiteral("c"), QStringLiteral("e"), QStringLiteral("d"),
                QStringLiteral("b") }));
        settings.removeRecentFile(QStringLiteral("e"));
        QCOMPARE(settings.recentFiles().size(), 3);
    }

    void layoutAndViewRoundTrip()
    {
        AppSettings settings;
        // Sem grupo "grids", o antigo tamanho do personagem (1.x) vira a
        // sub-grade.
        QSettings().setValue(QStringLiteral("characterLayout/cellWidth"), 16);
        QSettings().setValue(QStringLiteral("characterLayout/cellHeight"), 20);
        QCOMPARE(settings.gridLayout().subGrid, (core::Size { 16, 20 }));
        QCOMPARE(settings.gridLayout().mainGrid, (core::Size { 1, 1 }));

        core::GridLayout layout;
        layout.mainGrid = { 2, 3 };
        layout.subGrid = { 32, 48 };
        settings.setGridLayout(layout);
        QCOMPARE(settings.gridLayout(), layout);

        gui::ViewSettings view;
        view.subGridVisible = true;
        view.canvasReversed = true;
        settings.setViewSettings(view);
        const gui::ViewSettings loaded = settings.viewSettings();
        QVERIFY(loaded.subGridVisible);
        QVERIFY(loaded.canvasReversed);

        settings.setBackupFolder(QStringLiteral("/tmp/backups"));
        QCOMPARE(settings.backupFolder(), QStringLiteral("/tmp/backups"));
    }

    /// Histórico de "Nova imagem": mais recente primeiro, sem duplicatas,
    /// no máximo 8, persistente e tolerante a valores corrompidos.
    void newImageHistory()
    {
        AppSettings settings;
        QVERIFY(settings.newImageHistory().isEmpty());

        const gui::NewImageEntry rgba { { 72, 128 }, core::ColorMode::Rgba,
            core::ColorDepth::Bits8 };
        const gui::NewImageEntry indexed16 { { 288, 256 }, core::ColorMode::Indexed,
            core::ColorDepth::Bits4 };
        settings.addNewImageEntry(rgba);
        settings.addNewImageEntry(indexed16);
        QCOMPARE(settings.newImageHistory(), (QList<gui::NewImageEntry> { indexed16, rgba }));

        // Repetir uma escolha a traz para o topo (sem duplicar). Em RGBA a
        // profundidade não importa.
        gui::NewImageEntry sameRgba = rgba;
        sameRgba.depth = core::ColorDepth::Bits1;
        settings.addNewImageEntry(sameRgba);
        QCOMPARE(settings.newImageHistory(), (QList<gui::NewImageEntry> { rgba, indexed16 }));

        // O mesmo tamanho em outro modo é outra entrada.
        const gui::NewImageEntry indexed2 { { 72, 128 }, core::ColorMode::Indexed,
            core::ColorDepth::Bits1 };
        settings.addNewImageEntry(indexed2);
        QCOMPARE(settings.newImageHistory().size(), 3);

        // Limite de 8: as mais antigas saem.
        for (int width = 1; width <= 10; ++width) {
            settings.addNewImageEntry(
                { { width, 1 }, core::ColorMode::Rgba, core::ColorDepth::Bits8 });
        }
        const auto history = settings.newImageHistory();
        QCOMPARE(history.size(), AppSettings::kMaxNewImageHistory);
        QCOMPARE(history.first().size, (core::Size { 10, 1 }));
        QCOMPARE(history.last().size, (core::Size { 3, 1 }));

        // Valores corrompidos no arquivo são ignorados.
        QSettings().setValue(QStringLiteral("newImage/history"),
            QStringList { QStringLiteral("abc"), QStringLiteral("0x5:rgba"),
                QStringLiteral("16x16:3"), QStringLiteral("16x16:4") });
        QCOMPARE(settings.newImageHistory(),
            (QList<gui::NewImageEntry> {
                { { 16, 16 }, core::ColorMode::Indexed, core::ColorDepth::Bits4 } }));
    }

    /// O manual embutido traz a versão e a tabela de atalhos gerada.
    void helpContainsVersionAndShortcuts()
    {
        QObject owner;
        const gui::ActionCatalog actions(&owner);
        const QString html = gui::HelpWindow::buildHtml(&actions);
        QVERIFY(html.contains(QString::fromLatin1(
            version::kString.data(), static_cast<qsizetype>(version::kString.size()))));
        QVERIFY(!html.contains(QStringLiteral("{{SHORTCUTS}}")));
        QVERIFY(html.contains(QStringLiteral("Prepare Shadow")));
        QVERIFY(html.contains(QKeySequence(Qt::Key_F8).toString(QKeySequence::NativeText)));
    }
};

QTEST_MAIN(AppSettingsTest)
#include "AppSettingsTest.moc"
