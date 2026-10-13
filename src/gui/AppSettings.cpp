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
constexpr auto kLayoutGroup = "characterLayout";
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
// Grade de personagens
// -----------------------------------------------------------------------------
core::CharacterLayout AppSettings::characterLayout() const
{
    QSettings settings;
    settings.beginGroup(key(kLayoutGroup));
    const core::CharacterLayout defaults;
    core::CharacterLayout layout;
    layout.cellSize.width = settings.value("cellWidth", defaults.cellSize.width).toInt();
    layout.cellSize.height = settings.value("cellHeight", defaults.cellSize.height).toInt();
    layout.subGridSize.width = settings.value("subGridWidth", defaults.subGridSize.width).toInt();
    layout.subGridSize.height
        = settings.value("subGridHeight", defaults.subGridSize.height).toInt();
    layout.doubleCanvas = settings.value("doubleCanvas", defaults.doubleCanvas).toBool();
    layout.halfCellSteps = settings.value("halfCellSteps", defaults.halfCellSteps).toBool();
    layout.keepMouseCursor = settings.value("keepMouseCursor", defaults.keepMouseCursor).toBool();
    settings.endGroup();

    // Proteção contra valores corrompidos no arquivo de configuração.
    if (layout.cellSize.isEmpty() || layout.subGridSize.isEmpty()) {
        return defaults;
    }
    return layout;
}

void AppSettings::setCharacterLayout(const core::CharacterLayout& layout)
{
    QSettings settings;
    settings.beginGroup(key(kLayoutGroup));
    settings.setValue("cellWidth", layout.cellSize.width);
    settings.setValue("cellHeight", layout.cellSize.height);
    settings.setValue("subGridWidth", layout.subGridSize.width);
    settings.setValue("subGridHeight", layout.subGridSize.height);
    settings.setValue("doubleCanvas", layout.doubleCanvas);
    settings.setValue("halfCellSteps", layout.halfCellSteps);
    settings.setValue("keepMouseCursor", layout.keepMouseCursor);
    settings.endGroup();
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
    view.zoom = settings.value("zoom", defaults.zoom).toInt();
    view.mainGridVisible = settings.value("mainGrid", defaults.mainGridVisible).toBool();
    view.subGridVisible = settings.value("subGrid", defaults.subGridVisible).toBool();
    view.transparentEffect
        = settings.value("transparentEffect", defaults.transparentEffect).toBool();
    view.canvasReversed = settings.value("canvasReversed", defaults.canvasReversed).toBool();
    view.splitterState = settings.value("splitter").toByteArray();
    settings.endGroup();
    return view;
}

void AppSettings::setViewSettings(const ViewSettings& view)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("view"));
    settings.setValue("zoom", view.zoom);
    settings.setValue("mainGrid", view.mainGridVisible);
    settings.setValue("subGrid", view.subGridVisible);
    settings.setValue("transparentEffect", view.transparentEffect);
    settings.setValue("canvasReversed", view.canvasReversed);
    settings.setValue("splitter", view.splitterState);
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
