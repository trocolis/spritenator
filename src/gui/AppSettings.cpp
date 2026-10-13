// =============================================================================
// AppSettings.cpp
//
// Cada método cria um QSettings local: é barato, thread-safe e sempre usa a
// organização/aplicação definidas no main() (QCoreApplication).
// =============================================================================
#include "AppSettings.h"

#include <QDir>
#include <QSettings>

namespace spritenator::gui {

namespace {

// Chaves das configurações (centralizadas para evitar divergências).
constexpr auto kRecentFilesKey = "files/recent";
constexpr auto kLastDirectoryKey = "files/lastDirectory";
constexpr auto kBackupFolderKey = "files/backupFolder";
constexpr auto kGridGroup = "grids";
/// Grupo da versão 1.x ("tamanho do personagem"), lido só para migração.
constexpr auto kOldLayoutGroup = "characterLayout";
constexpr auto kLanguageKey = "interface/language";
constexpr auto kGeometryKey = "mainWindow/geometry";
constexpr auto kStateKey = "mainWindow/state";

QString key(const char* name)
{
    return QString::fromLatin1(name);
}

} // namespace

// -----------------------------------------------------------------------------
// Arquivos recentes
// -----------------------------------------------------------------------------
QStringList AppSettings::recentFiles() const
{
    return QSettings().value(key(kRecentFilesKey)).toStringList();
}

void AppSettings::addRecentFile(const QString& path)
{
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    QSettings().setValue(key(kRecentFilesKey), files);
}

void AppSettings::removeRecentFile(const QString& path)
{
    QStringList files = recentFiles();
    files.removeAll(path);
    QSettings().setValue(key(kRecentFilesKey), files);
}

// -----------------------------------------------------------------------------
// Diretórios
// -----------------------------------------------------------------------------
QString AppSettings::lastDirectory() const
{
    return QSettings().value(key(kLastDirectoryKey), QDir::homePath()).toString();
}

void AppSettings::setLastDirectory(const QString& directory)
{
    QSettings().setValue(key(kLastDirectoryKey), directory);
}

QString AppSettings::backupFolder() const
{
    return QSettings().value(key(kBackupFolderKey)).toString();
}

void AppSettings::setBackupFolder(const QString& folder)
{
    QSettings().setValue(key(kBackupFolderKey), folder);
}

// -----------------------------------------------------------------------------
// Grades
// -----------------------------------------------------------------------------
core::GridLayout AppSettings::gridLayout() const
{
    QSettings settings;
    core::GridLayout defaults;
    // Migração da 1.x: o antigo "tamanho do personagem" vira a sub-grade.
    settings.beginGroup(key(kOldLayoutGroup));
    defaults.subGrid.width = settings.value("cellWidth", defaults.subGrid.width).toInt();
    defaults.subGrid.height = settings.value("cellHeight", defaults.subGrid.height).toInt();
    settings.endGroup();
    if (defaults.subGrid.isEmpty()) {
        defaults.subGrid = core::GridLayout { }.subGrid;
    }

    settings.beginGroup(key(kGridGroup));
    core::GridLayout layout;
    layout.mainGrid.width = settings.value("mainWidth", defaults.mainGrid.width).toInt();
    layout.mainGrid.height = settings.value("mainHeight", defaults.mainGrid.height).toInt();
    layout.subGrid.width = settings.value("subWidth", defaults.subGrid.width).toInt();
    layout.subGrid.height = settings.value("subHeight", defaults.subGrid.height).toInt();
    settings.endGroup();

    // Proteção contra valores corrompidos no arquivo de configuração.
    if (layout.mainGrid.isEmpty() || layout.subGrid.isEmpty()) {
        return defaults;
    }
    return layout;
}

void AppSettings::setGridLayout(const core::GridLayout& layout)
{
    QSettings settings;
    settings.beginGroup(key(kGridGroup));
    settings.setValue("mainWidth", layout.mainGrid.width);
    settings.setValue("mainHeight", layout.mainGrid.height);
    settings.setValue("subWidth", layout.subGrid.width);
    settings.setValue("subHeight", layout.subGrid.height);
    settings.endGroup();
}

// -----------------------------------------------------------------------------
// Idioma
// -----------------------------------------------------------------------------
QString AppSettings::language() const
{
    return QSettings().value(key(kLanguageKey)).toString();
}

void AppSettings::setLanguage(const QString& languageCode)
{
    QSettings().setValue(key(kLanguageKey), languageCode);
}

// -----------------------------------------------------------------------------
// Preferências de exibição
// -----------------------------------------------------------------------------
ViewSettings AppSettings::viewSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("view"));
    const ViewSettings defaults;
    ViewSettings view;
    view.mainGridVisible = settings.value("mainGrid", defaults.mainGridVisible).toBool();
    view.subGridVisible = settings.value("subGrid", defaults.subGridVisible).toBool();
    view.transparentEffect
        = settings.value("transparentEffect", defaults.transparentEffect).toBool();
    view.canvasReversed = settings.value("canvasReversed", defaults.canvasReversed).toBool();
    // Chave nova na 2.0: o painel mudou de largura e a proporção antiga não
    // serve mais.
    view.splitterState = settings.value("panelSplitter").toByteArray();
    settings.endGroup();
    return view;
}

void AppSettings::setViewSettings(const ViewSettings& view)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("view"));
    settings.setValue("mainGrid", view.mainGridVisible);
    settings.setValue("subGrid", view.subGridVisible);
    settings.setValue("transparentEffect", view.transparentEffect);
    settings.setValue("canvasReversed", view.canvasReversed);
    settings.setValue("panelSplitter", view.splitterState);
    settings.endGroup();
}

// -----------------------------------------------------------------------------
// Janela principal
// -----------------------------------------------------------------------------
QByteArray AppSettings::mainWindowGeometry() const
{
    return QSettings().value(key(kGeometryKey)).toByteArray();
}

void AppSettings::setMainWindowGeometry(const QByteArray& geometry)
{
    QSettings().setValue(key(kGeometryKey), geometry);
}

QByteArray AppSettings::mainWindowState() const
{
    return QSettings().value(key(kStateKey)).toByteArray();
}

void AppSettings::setMainWindowState(const QByteArray& state)
{
    QSettings().setValue(key(kStateKey), state);
}

} // namespace spritenator::gui
