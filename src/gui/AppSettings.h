// =============================================================================
// AppSettings.h
//
// Acesso tipado às configurações persistentes (QSettings). Concentrar as
// chaves aqui evita strings espalhadas pelo código e erros de digitação.
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/GridLayout.h"
#include "core/Palette.h"

#include <QByteArray>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

namespace spritenator::gui {

/// Preferências de exibição restauradas ao abrir a aplicação.
struct ViewSettings {
    bool mainGridVisible = true;
    bool subGridVisible = false;
    bool transparentEffect = false;
    bool canvasReversed = false;
    int wandTolerance = 15; ///< tolerância da varinha mágica (0..100)
    bool wandContiguous = true; ///< varinha: só a região conectada
    bool brushRound = false; ///< ponta do lápis redonda (senão, quadrada)
    int brushSize = 1; ///< tamanho da ponta do lápis (1..64)
    QByteArray splitterState; ///< proporção Integrator | área MDI
};

/// Escolha feita no diálogo "Nova imagem": tamanho + modo de cor (o histórico
/// do diálogo guarda estas combinações).
struct NewImageEntry {
    core::Size size { 320, 240 };
    core::ColorMode mode = core::ColorMode::Rgba;
    /// Só tem sentido no modo indexado (256, 16 ou 2 cores).
    core::ColorDepth depth = core::ColorDepth::Bits8;

    friend bool operator==(const NewImageEntry& a, const NewImageEntry& b)
    {
        // No modo RGBA a profundidade não conta.
        return a.size == b.size && a.mode == b.mode
            && (a.mode == core::ColorMode::Rgba || a.depth == b.depth);
    }
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

    // --- Histórico do diálogo "Nova imagem" (mais recente primeiro) ---
    /// Quantidade máxima de entradas guardadas.
    static constexpr int kMaxNewImageHistory = 8;
    [[nodiscard]] QList<NewImageEntry> newImageHistory() const;
    /// Coloca a entrada no topo (sem duplicatas, no máximo 8) e grava.
    void addNewImageEntry(const NewImageEntry& entry);
    /// Regra do histórico, sem gravar: a entrada vai para o topo, repetições
    /// saem e a lista é limitada a kMaxNewImageHistory.
    [[nodiscard]] static QList<NewImageEntry> withEntry(
        QList<NewImageEntry> history, const NewImageEntry& entry);

    // --- Atalhos de teclado personalizados ---
    /// Atalhos que diferem do padrão: chave estável da ação -> atalho (um
    /// atalho vazio significa "sem atalho").
    [[nodiscard]] QHash<QString, QKeySequence> customShortcuts() const;
    void setCustomShortcuts(const QHash<QString, QKeySequence>& shortcuts);

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
