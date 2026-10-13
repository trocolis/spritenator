// =============================================================================
// MainWindow.h
//
// Janela principal do Spritenator. Equivale ao "TCharacterRaiserMainForm"
// do original: contém o menu, a barra de ferramentas, a área MDI onde ficam
// as janelas de canvas e a barra de status.
// =============================================================================
#pragma once

#include "AppSettings.h"

#include <QMainWindow>

#include <cstddef>

class QLabel;
class QMdiArea;
class QMdiSubWindow;
class QMenu;
class QToolBar;

namespace spritenator::gui {

class ActionCatalog;
class CanvasWindow;
class Document;
enum class ActionId : std::size_t;

/// Janela principal da aplicação.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /// Abre o arquivo informado (ou ativa a janela se já estiver aberto).
    /// Devolve true em caso de sucesso; erros são mostrados ao usuário.
    bool openFile(const QString& path);

    /// Documento da janela MDI ativa (nulo se não houver).
    [[nodiscard]] Document* activeDocument() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    // --- Construção da interface ---
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void connectActions();

    /// Habilita/desabilita ações conforme o estado atual da aplicação.
    void updateActionStates();

    // --- Arquivo ---
    void newImage();
    void openImage();
    bool saveDocument(Document* document);
    bool saveDocumentAs(Document* document);
    void revertDocument();

    /// Pergunta se o usuário quer salvar um documento modificado. Devolve
    /// false se a operação em andamento (fechar/sair) deve ser cancelada.
    bool maybeSave(Document* document);

    /// Cria a janela-filha MDI para o documento e a exibe.
    CanvasWindow* addDocumentWindow(Document* document);

    /// Procura a janela de um arquivo já aberto (comparando caminhos absolutos).
    [[nodiscard]] QMdiSubWindow* findWindowForFile(const QString& path) const;

    /// Reconstrói as entradas de arquivos recentes no menu Arquivo.
    void updateRecentFilesMenu();

    // --- Janela ---
    [[nodiscard]] CanvasWindow* activeCanvasWindow() const;
    void tileWindows(Qt::Orientation orientation);
    void fitActiveWindow();

    // --- Ajuda ---
    void showAbout();

    /// Exibe uma mensagem de erro padronizada.
    void showError(const QString& message);

    /// Atalho para obter uma ação do catálogo.
    [[nodiscard]] QAction* action(ActionId id) const;

    AppSettings m_settings;
    ActionCatalog* m_actions = nullptr; ///< Todas as ações (pai: this).
    QMdiArea* m_mdiArea = nullptr; ///< Área com as janelas de canvas.
    QToolBar* m_toolBar = nullptr; ///< Barra de ferramentas principal.
    QMenu* m_fileMenu = nullptr;
    QAction* m_recentSeparator = nullptr; ///< Separador antes dos recentes.
    QList<QAction*> m_recentFileActions; ///< Entradas de arquivos recentes.
    QLabel* m_positionLabel = nullptr; ///< Posição do mouse no canvas.
};

} // namespace spritenator::gui
