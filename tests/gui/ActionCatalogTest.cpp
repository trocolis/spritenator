// =============================================================================
// ActionCatalogTest.cpp
//
// Verifica a integridade do catálogo de ações e dos recursos de ícones:
//   - toda ação existe e tem texto;
//   - não há atalhos de teclado duplicados (conflitos silenciosos no Qt);
//   - todo ícone referenciado existe no recurso embutido;
//   - a versão gerada pelo CMake está no formato X.Y.Z.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/IconProvider.h"

#include <spritenator/Version.h>

#include <QAction>
#include <QActionGroup>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

using spritenator::gui::ActionCatalog;
using spritenator::gui::ActionId;
using spritenator::gui::IconProvider;
using spritenator::gui::kActionCount;

class ActionCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    /// Todas as ações devem existir e possuir texto visível.
    void everyActionHasText()
    {
        QObject owner;
        const ActionCatalog catalog(&owner);
        for (std::size_t index = 0; index < kActionCount; ++index) {
            const QAction* action = catalog.action(static_cast<ActionId>(index));
            QVERIFY(action != nullptr);
            QVERIFY2(!action->text().isEmpty(), qPrintable(QString::number(index)));
        }
    }

    /// Dois comandos com o mesmo atalho geram ambiguidade no Qt.
    void shortcutsAreUnique()
    {
        QObject owner;
        const ActionCatalog catalog(&owner);
        QSet<QString> seen;
        for (std::size_t index = 0; index < kActionCount; ++index) {
            const QKeySequence shortcut = catalog.action(static_cast<ActionId>(index))->shortcut();
            if (shortcut.isEmpty()) {
                continue;
            }
            const QString key = shortcut.toString(QKeySequence::PortableText);
            QVERIFY2(!seen.contains(key), qPrintable(QStringLiteral("Atalho repetido: ") + key));
            seen.insert(key);
        }
    }

    /// Ícones essenciais devem estar embutidos nos recursos.
    void iconResourcesExist()
    {
        const QStringList required = {
            QStringLiteral("app_icon"),
            QStringLiteral("file_new"),
            QStringLiteral("file_open"),
            QStringLiteral("file_save"),
            QStringLiteral("zoom_12"),
            QStringLiteral("tool_select"),
            QStringLiteral("tone_a"),
            QStringLiteral("stretch"),
            QStringLiteral("mask_auto"),
        };
        for (const QString& name : required) {
            QVERIFY2(QFile::exists(IconProvider::resourcePath(name)), qPrintable(name));
            QVERIFY(!IconProvider::icon(name).isNull());
        }
        // Um nome inexistente produz ícone nulo, sem falhar.
        QVERIFY(IconProvider::icon(QStringLiteral("does_not_exist")).isNull());
    }

    /// Apenas um fator de zoom está selecionado por padrão.
    void zoomGroupIsExclusive()
    {
        QObject owner;
        const ActionCatalog catalog(&owner);
        int checked = 0;
        for (const QAction* zoom : catalog.zoomGroup()->actions()) {
            checked += zoom->isChecked() ? 1 : 0;
        }
        QCOMPARE(checked, 1);
        QCOMPARE(ActionCatalog::zoomFactorOf(ActionId::ViewZoom12), 12);
        QCOMPARE(ActionCatalog::zoomFactorOf(ActionId::FileNew), 0);
    }

    /// A versão gerada segue o versionamento semântico X.Y.Z.
    void versionIsSemantic()
    {
        const QString versionText = QString::fromLatin1(spritenator::version::kString.data(),
            static_cast<qsizetype>(spritenator::version::kString.size()));
        static const QRegularExpression pattern(QStringLiteral(R"(^\d+\.\d+\.\d+$)"));
        QVERIFY(pattern.match(versionText).hasMatch());
        QCOMPARE(versionText,
            QStringLiteral("%1.%2.%3")
                .arg(spritenator::version::kMajor)
                .arg(spritenator::version::kMinor)
                .arg(spritenator::version::kPatch));
    }
};

QTEST_MAIN(ActionCatalogTest)
#include "ActionCatalogTest.moc"
