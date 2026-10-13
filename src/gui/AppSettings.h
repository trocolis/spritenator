// =============================================================================
// AppSettings.h
//
// Acesso tipado às configurações persistentes (QSettings). Concentrar as
// chaves aqui evita strings espalhadas pelo código e erros de digitação.
// =============================================================================
#pragma once

#include "core/GridLayout.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace spritenator::gui {

/// Preferências de exibição restauradas ao abrir a aplicação.
struct ViewSettings {
    bool mainGridVisible = true;
    bool subGridVisible = false;
    bool transparentEffect = false;
    bool canvasReversed = false;
    QByteArray splitterState; ///< proporção Integrator | área MDI
};

/// Leitura e gravação das preferências da aplicação.
class AppSettings final {
public:
    /// Quantidade de arquivos recentes no menu Arquivo (igual ao original).
    static constexpr int kMaxRecentFiles = 4;

    // --- Arquivos recentes ---
    [[nodiscard]] QStringList recentFiles() const;
    /// Coloca o arquivo no topo da lista (removendo duplicatas).
    void addRecentFile(const QString& path);
    void removeRecentFile(const QString& path);

    // --- Último diretório usado em diálogos de arquivo ---
    [[nodiscard]] QString lastDirectory() const;
    void setLastDirectory(const QString& directory);

    // --- Pasta de backup (vazia = backup desativado) ---
    [[nodiscard]] QString backupFolder() const;
    void setBackupFolder(const QString& folder);

    // --- Grades (principal e sub-grade) ---
    [[nodiscard]] core::GridLayout gridLayout() const;
    void setGridLayout(const core::GridLayout& layout);

    // --- Idioma da interface ("pt_BR", "en"...; vazio = idioma do sistema) ---
    [[nodiscard]] QString language() const;
    void setLanguage(const QString& languageCode);

    // --- Preferências de exibição ---
    [[nodiscard]] ViewSettings viewSettings() const;
    void setViewSettings(const ViewSettings& settings);

    // --- Geometria e estado da janela principal ---
    [[nodiscard]] QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);
    [[nodiscard]] QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);
};

} // namespace spritenator::gui
