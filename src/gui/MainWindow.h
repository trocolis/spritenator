// =============================================================================
// MainWindow.h
//
// Janela principal do Spritenator. Inspirada no "TCharacterRaiserMainForm"
// do original: contém o menu, a barra de ferramentas, o painel lateral, a
// área de documentos (abas, uma por imagem aberta) e a barra de status com o
// zoom do editor ativo.
// =============================================================================
#pragma once

#include "AppSettings.h"
#include "commands/UndoLabel.h"
#include "core/AnyImage.h"
#include "core/Geometry.h"
#include "core/Image.h"
#include "core/Palette.h"

#include <QMainWindow>

#include <cstddef>

class QComboBox;
class QLabel;
class QMdiArea;
class QMdiSubWindow;
class QMenu;
class QSplitter;
class QToolBar;

namespace spritenator::gui {

class ActionCatalog;
class AnimationWindow;
class CanvasEditor;
class Document;
class HelpWindow;
class EditOperations;
class EditorSession;
class IntegratorPanel;
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

    /// Documento da aba ativa (nulo se não houver).
    [[nodiscard]] Document* activeDocument() const;
    /// Editor da aba ativa (nulo se não houver).
    [[nodiscard]] CanvasEditor* activeEditor() const;

    /// Estado do editor e catálogo de ações (expostos para testes de integração).
    [[nodiscard]] EditorSession* session() const { return m_session; }
    [[nodiscard]] ActionCatalog* actions() const { return m_actions; }

protected:
    void changeEvent(QEvent* event) override;
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
    void setBackupFolder();
    /// Guarda a versão anterior do arquivo na pasta de backup (se definida).
    /// Devolve false se o usuário preferir cancelar a gravação após um erro.
    bool backupBeforeSaving(const QString& path);

    /// Pergunta se o usuário quer salvar um documento modificado. Devolve
    /// false se a operação em andamento (fechar/sair) deve ser cancelada.
    bool maybeSave(Document* document);

    /// Cria o editor (aba) do documento e o exibe.
    CanvasEditor* addDocumentWindow(Document* document);

    /// Torna o documento da aba atual o documento ativo da sessão.
    void syncActiveDocument();

    /// Procura a janela de um arquivo já aberto (comparando caminhos absolutos).
    [[nodiscard]] QMdiSubWindow* findWindowForFile(const QString& path) const;

    /// Reconstrói as entradas de arquivos recentes no menu Arquivo.
    void updateRecentFilesMenu();

    // --- Janela ---
    /// Troca os lados do Integrator e da área MDI ("Reverse Canvas").
    void setCanvasReversed(bool reversed);

    // --- Editar ---
    void drawText();

    // --- Paleta ---
    void loadPalette();
    void savePalette();
    void editPalette();
    void applyGradation();
    /// Aplica imagem + paleta novas ao documento como um passo de desfazer.
    void commitDocumentState(Document* document, const core::AnyImage& canvas,
        const core::Palette& palette, const UndoLabel& undoLabel);

    // --- Canvas ---
    void resizeCanvas();
    void clearCanvas();

    // --- Exibir ---
    /// Cria o submenu Exibir > Idioma (sistema, inglês e traduções embutidas).
    void createLanguageMenu(QMenu* viewMenu);
    /// Cria o submenu Exibir > Tema (sistema, claro, escuro).
    void createThemeMenu(QMenu* viewMenu);
    /// Cria um menu cujo título (texto-fonte em inglês, marcado com
    /// QT_TR_NOOP) é traduzido em retranslateUi().
    QMenu* addTranslatedMenu(QWidget* parent, const char* title);
    /// Reaplica todos os textos da janela no idioma atual.
    void retranslateUi();
    void showAnimationWindow();
    void editGrids();
    /// Editar > Atalhos de teclado.
    void editShortcuts();
    /// Aplica os atalhos personalizados gravados nas preferências.
    void applyCustomShortcuts();
    /// Liga o indicador de zoom ao editor ativo.
    void trackActiveEditorZoom();
    /// Mostra o zoom (fator) no indicador da barra de status.
    void showZoom(double zoom);
    /// Aplica o zoom digitado/escolhido no indicador (em %).
    void applyZoomText(const QString& text);
    /// Sincroniza as ações de desfazer/refazer com o grupo de pilhas.
    void updateUndoActions();
    /// Mostra uma posição de pixel na barra de status.
    void showPosition(const core::Point& pixel);

    // --- Ajuda ---
    void showHelp();
    void showAbout();

    // --- Preferências ---
    void restoreViewSettings();
    void saveViewSettings();

    /// Exibe uma mensagem de erro padronizada.
    void showError(const QString& message);

    /// Atalho para obter uma ação do catálogo.
    [[nodiscard]] QAction* action(ActionId id) const;

    AppSettings m_settings;
    EditorSession* m_session = nullptr; ///< Estado central do editor.
    EditOperations* m_operations = nullptr; ///< Operações de edição do documento.
    ActionCatalog* m_actions = nullptr; ///< Todas as ações (pai: this).
    QSplitter* m_mainSplitter = nullptr; ///< Integrator | área MDI.
    IntegratorPanel* m_integrator = nullptr; ///< Painel com ferramentas e cores.
    QMdiArea* m_mdiArea = nullptr; ///< Área com as abas dos documentos.
    QToolBar* m_toolBar = nullptr; ///< Barra de ferramentas principal.
    QMenu* m_fileMenu = nullptr;
    QAction* m_recentSeparator = nullptr; ///< Separador antes dos recentes.
    QList<QAction*> m_recentFileActions; ///< Entradas de arquivos recentes.
    QLabel* m_positionLabel = nullptr; ///< Posição do mouse na imagem.
    QComboBox* m_zoomCombo = nullptr; ///< Zoom do editor ativo, em %.
    QMetaObject::Connection m_zoomConnection; ///< editor ativo -> indicador
    AnimationWindow* m_animationWindow = nullptr; ///< Criada sob demanda (F2).
    HelpWindow* m_helpWindow = nullptr; ///< Criada sob demanda (F1).
    QList<QPair<QMenu*, const char*>> m_translatedMenus; ///< menu e título-fonte
    QAction* m_systemLanguageAction = nullptr; ///< item "padrão do sistema"
    QList<QPair<QAction*, const char*>> m_themeActions; ///< item e texto-fonte
};

} // namespace spritenator::gui
