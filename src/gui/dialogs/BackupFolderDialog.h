// =============================================================================
// BackupFolderDialog.h
//
// Diálogo "Definir pasta de backup" (TFormSetBackUpFolder no original). Com
// uma pasta definida, cada "Salvar" que sobrescreve um arquivo guarda antes
// uma cópia da versão anterior nessa pasta (ver io/Backup).
// =============================================================================
#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;

namespace spritenator::gui {

/// Escolha da pasta de backup automático.
class BackupFolderDialog final : public QDialog {
    Q_OBJECT

public:
    /// "folder" vazio = backup desativado.
    explicit BackupFolderDialog(const QString& folder, QWidget* parent = nullptr);

    /// Pasta escolhida; vazia se o backup estiver desativado.
    [[nodiscard]] QString folder() const;

private:
    void browse();
    void updateEnabledState();

    QCheckBox* m_enabled = nullptr;
    QLineEdit* m_path = nullptr;
    QPushButton* m_browse = nullptr;
};

} // namespace spritenator::gui
