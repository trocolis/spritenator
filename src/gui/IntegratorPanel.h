// =============================================================================
// IntegratorPanel.h
//
// Painel lateral de edição (TIntegrator no original). Reúne:
//   - coluna esquerda: paleta, amostras/RGB, botões de paleta e ferramentas;
//   - coluna direita: prévia em tamanho real e a área de desenho ampliada.
//
// Apenas organiza widgets; o estado vive na EditorSession.
// =============================================================================
#pragma once

#include <QWidget>

class QScrollArea;
class QVBoxLayout;

namespace spritenator::gui {

class ActionCatalog;
class DrawAreaView;
class EditorSession;
class PaletteView;

/// Painel com a área de desenho e seus controles.
class IntegratorPanel final : public QWidget {
    Q_OBJECT

public:
    IntegratorPanel(EditorSession* session, ActionCatalog* actions, QWidget* parent = nullptr);

    [[nodiscard]] DrawAreaView* drawArea() const { return m_drawArea; }
    [[nodiscard]] PaletteView* paletteView() const { return m_paletteView; }

private:
    /// Cria a coluna esquerda (paleta, cores, botões).
    QWidget* createLeftColumn(ActionCatalog* actions);
    /// Cria a coluna direita (prévia + área de desenho).
    QWidget* createRightColumn();

    EditorSession* m_session;
    PaletteView* m_paletteView = nullptr;
    DrawAreaView* m_drawArea = nullptr;
    QScrollArea* m_drawScroll = nullptr;
};

} // namespace spritenator::gui
