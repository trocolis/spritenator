// =============================================================================
// AppSettings.h
//
// Acesso tipado às configurações persistentes (QSettings). Concentrar as
// chaves aqui evita strings espalhadas pelo código e erros de digitação.
// =============================================================================
#pragma once

#include "core/CharacterLayout.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace spritenator::gui {

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

    // --- Grade de personagens ---
    [[nodiscard]] core::CharacterLayout characterLayout() const;
    void setCharacterLayout(const core::CharacterLayout& layout);

    // --- Geometria e estado da janela principal ---
    [[nodiscard]] QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);
    [[nodiscard]] QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);
};

} // namespace spritenator::gui
