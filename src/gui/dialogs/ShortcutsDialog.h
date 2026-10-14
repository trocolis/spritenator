// =============================================================================
// ShortcutsDialog.h
//
// Diálogo "Atalhos de teclado": lista todas as ações do programa com o atalho
// atual e permite trocar (ou remover) cada um, restaurar o padrão de uma ação
// ou de todas. Avisa quando um atalho já pertence a outra ação.
//
// As mudanças só valem ao confirmar (OK): então o diálogo informa, para cada
// ação, o atalho escolhido (ver shortcuts()).
// =============================================================================
#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QList>

class QKeySequenceEdit;
class QPushButton;
class QTreeWidget;

namespace spritenator::gui {

class ActionCatalog;

/// Edição dos atalhos de teclado.
class ShortcutsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ShortcutsDialog(const ActionCatalog* actions, QWidget* parent = nullptr);

    /// Atalho escolhido para cada ação, na ordem de ActionId.
    [[nodiscard]] const QList<QKeySequence>& shortcuts() const { return m_shortcuts; }

    /// Árvore com as ações (exposta para testes).
    [[nodiscard]] QTreeWidget* list() const { return m_list; }
    /// Define o atalho da ação na linha "row" (como se digitado no editor).
    /// Devolve false se o usuário recusou tirar o atalho de outra ação.
    bool assign(int row, const QKeySequence& sequence);

private:
    /// Atualiza o texto da coluna de atalho de uma linha.
    void refreshRow(int row);
    /// Linha atual mudou: mostra o atalho dela no editor.
    void showCurrent();

    QList<QKeySequence> m_shortcuts;
    QTreeWidget* m_list = nullptr;
    QKeySequenceEdit* m_editor = nullptr;
    QPushButton* m_clear = nullptr;
    QPushButton* m_default = nullptr;
    QPushButton* m_defaultAll = nullptr;
};

} // namespace spritenator::gui
