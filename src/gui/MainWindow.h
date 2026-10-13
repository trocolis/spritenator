// =============================================================================
// MainWindow.h
//
// Janela principal do Spritenator. Equivale ao "TCharacterRaiserMainForm"
// do original: contém o menu, a barra de ferramentas, a área MDI onde ficam
// as janelas de canvas e a barra de status.
// =============================================================================
#pragma once

#include <QMainWindow>

#include <cstddef>

class QMdiArea;
class QToolBar;

namespace spritenator::gui {

class ActionCatalog;
enum class ActionId : std::size_t;

/// Janela principal da aplicação.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

private:
    // --- Construção da interface ---
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void connectActions();

    /// Habilita/desabilita ações conforme o estado atual da aplicação.
    void updateActionStates();

    // --- Tratadores de comandos ---
    void showAbout();

    /// Atalho para obter uma ação do catálogo.
    [[nodiscard]] QAction* action(ActionId id) const;

    ActionCatalog* m_actions = nullptr; ///< Todas as ações (pai: this).
    QMdiArea* m_mdiArea = nullptr; ///< Área com as janelas de canvas.
    QToolBar* m_toolBar = nullptr; ///< Barra de ferramentas principal.
};

} // namespace spritenator::gui
