// =============================================================================
// ToolPanel.h
//
// Botões de ferramentas e de tons do Integrator (os MenuButtonTool1..12 e
// MenuButtonTone1..9 do original). Os botões são exclusivos: exatamente uma
// ferramenta e um tom ficam marcados, sempre espelhando a EditorSession.
// =============================================================================
#pragma once

#include <QWidget>

class QButtonGroup;
class QToolButton;

namespace spritenator::gui {

class EditorSession;

/// Seletor de ferramenta e de tom.
class ToolPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ToolPanel(EditorSession* session, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Marca os botões correspondentes ao estado atual da sessão.
    void syncFromSession();
    /// Reaplica as dicas no idioma atual.
    void retranslate();

    EditorSession* m_session;
    QButtonGroup* m_toolGroup = nullptr;
    QButtonGroup* m_toneGroup = nullptr;
    QToolButton* m_shiftButton = nullptr;
};

} // namespace spritenator::gui
