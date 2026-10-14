// =============================================================================
// ToolPanel.h
//
// Seções do painel lateral com botões exclusivos, sempre espelhando a
// EditorSession:
//   - ToolPanel: as ferramentas (seleções, balde, lápis, figuras);
//   - TonePanel: os tons de retícula A..H e o deslocamento do padrão
//     (as opções de preenchimento).
// Os botões ficam num FlowLayout: numa linha só se couberem, senão quebram.
// =============================================================================
#pragma once

#include "gui/tools/Tool.h"

#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QToolButton;

namespace spritenator::gui {

class EditorSession;

/// Seletor de ferramenta.
class ToolPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ToolPanel(EditorSession* session, QWidget* parent = nullptr);

    /// Botão de uma ferramenta (nulo se ela não existe no painel).
    [[nodiscard]] QAbstractButton* button(ToolKind kind) const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void syncFromSession();
    void retranslate();

    EditorSession* m_session;
    QButtonGroup* m_toolGroup = nullptr;
};

/// Seletor de tom (opções de preenchimento).
class TonePanel final : public QWidget {
    Q_OBJECT

public:
    explicit TonePanel(EditorSession* session, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    void syncFromSession();
    void retranslate();

    EditorSession* m_session;
    QButtonGroup* m_toneGroup = nullptr;
    QToolButton* m_shiftButton = nullptr;
};

} // namespace spritenator::gui
