// =============================================================================
// AppSettings.cpp
//
// Cada método cria um QSettings local: é barato, thread-safe e sempre usa a
// organização/aplicação definidas no main() (QCoreApplication).
// =============================================================================
#include "AppSettings.h"

#include "core/Image.h"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>

#include <optional>

namespace spritenator::gui {

namespace {

// Chaves das configurações (centralizadas para evitar divergências).
constexpr auto kRecentFilesKey = "files/recent";
constexpr auto kLastDirectoryKey = "files/lastDirectory";
constexpr auto kBackupFolderKey = "files/backupFolder";
constexpr auto kGridGroup = "grids";
constexpr auto kNewImageHistoryKey = "newImage/history";
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
// Histórico do diálogo "Nova imagem"
//
// Cada entrada é gravada como texto: "LxA:rgba" ou "LxA:<bits>" (1, 4 ou 8),
// ex.: "72x128:rgba", "288x256:4". Entradas inválidas são ignoradas.
// -----------------------------------------------------------------------------
namespace {

QString encodeEntry(const NewImageEntry& entry)
{
    const QString mode = entry.mode == core::ColorMode::Rgba
        ? QStringLiteral("rgba")
        : QString::number(core::bitsPerPixelOf(entry.depth));
    return QStringLiteral("%1x%2:%3").arg(entry.size.width).arg(entry.size.height).arg(mode);
}

std::optional<NewImageEntry> decodeEntry(const QString& text)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^(\d+)x(\d+):(rgba|1|4|8)$)"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    NewImageEntry entry;
    entry.size = { match.captured(1).toInt(), match.captured(2).toInt() };
    if (entry.size.isEmpty() || entry.size.width > core::IndexedImage::kMaxDimension
        || entry.size.height > core::IndexedImage::kMaxDimension) {
        return std::nullopt;
    }
    const QString mode = match.captured(3);
    if (mode == QLatin1String("rgba")) {
        entry.mode = core::ColorMode::Rgba;
    } else {
        entry.mode = core::ColorMode::Indexed;
        entry.depth = mode == QLatin1String("1") ? core::ColorDepth::Bits1
            : mode == QLatin1String("4")         ? core::ColorDepth::Bits4
                                                 : core::ColorDepth::Bits8;
    }
    return entry;
}

} // namespace

QList<NewImageEntry> AppSettings::newImageHistory() const
{
    QList<NewImageEntry> history;
    for (const QString& text : QSettings().value(key(kNewImageHistoryKey)).toStringList()) {
        const auto entry = decodeEntry(text);
        if (entry && !history.contains(*entry) && history.size() < kMaxNewImageHistory) {
            history.append(*entry); // mantém a ordem gravada
        }
    }
    return history;
}

void AppSettings::addNewImageEntry(const NewImageEntry& entry)
{
    QStringList texts;
    for (const NewImageEntry& item : withEntry(newImageHistory(), entry)) {
        texts.append(encodeEntry(item));
    }
    QSettings().setValue(key(kNewImageHistoryKey), texts);
}

QList<NewImageEntry> AppSettings::withEntry(
    QList<NewImageEntry> history, const NewImageEntry& entry)
{
    history.removeAll(entry);
    history.prepend(entry);
    while (history.size() > kMaxNewImageHistory) {
        history.removeLast();
    }
    return history;
}

// -----------------------------------------------------------------------------
// Atalhos personalizados (grupo "shortcuts", formato portável do Qt)
// -----------------------------------------------------------------------------
QHash<QString, QKeySequence> AppSettings::customShortcuts() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    QHash<QString, QKeySequence> result;
    for (const QString& name : settings.childKeys()) {
        result.insert(name,
            QKeySequence::fromString(settings.value(name).toString(), QKeySequence::PortableText));
    }
    return result;
}

void AppSettings::setCustomShortcuts(const QHash<QString, QKeySequence>& shortcuts)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    settings.remove(QString()); // apaga o grupo inteiro antes de regravar
    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
        settings.setValue(it.key(), it.value().toString(QKeySequence::PortableText));
    }
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
QString AppSettings::theme() const
{
    return QSettings()
        .value(QStringLiteral("interface/theme"), QStringLiteral("system"))
        .toString();
}

void AppSettings::setTheme(const QString& theme)
{
    QSettings().setValue(QStringLiteral("interface/theme"), theme);
}

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
    view.wandTolerance = settings.value("wandTolerance", defaults.wandTolerance).toInt();
    view.wandContiguous = settings.value("wandContiguous", defaults.wandContiguous).toBool();
    view.brushRound = settings.value("brushRound", defaults.brushRound).toBool();
    view.brushSize = settings.value("brushSize", defaults.brushSize).toInt();
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
    settings.setValue("wandTolerance", view.wandTolerance);
    settings.setValue("wandContiguous", view.wandContiguous);
    settings.setValue("brushRound", view.brushRound);
    settings.setValue("brushSize", view.brushSize);
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
