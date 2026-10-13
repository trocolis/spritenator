// =============================================================================
// HelpWindow.h
//
// Janela de ajuda (F1). Exibe o manual embutido (resources/help/manual.html)
// seguido de uma tabela de atalhos de teclado gerada a partir do
// ActionCatalog — assim a tabela nunca fica desatualizada em relação aos
// atalhos reais.
// =============================================================================
#pragma once

#include <QWidget>

class QTextBrowser;

namespace spritenator::gui {

class ActionCatalog;

/// Janela com o manual do usuário.
class HelpWindow final : public QWidget {
    Q_OBJECT

public:
    HelpWindow(const ActionCatalog* actions, QWidget* parent = nullptr);

    /// HTML completo exibido (exposto para testes).
    [[nodiscard]] static QString buildHtml(const ActionCatalog* actions);

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Reaplica título e conteúdo no idioma atual.
    void retranslate();

    const ActionCatalog* m_actions;
    QTextBrowser* m_browser = nullptr;
};

} // namespace spritenator::gui
