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
