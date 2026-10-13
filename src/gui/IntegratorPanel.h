// =============================================================================
// IntegratorPanel.h
//
// Painel lateral de edição (TIntegrator no original). Reúne:
//   - coluna esquerda, de cima para baixo: ferramentas e tons, ajuste das
//     cores (mostradores e barras R, G, B, A) e, conforme o modo de cor, a
//     paleta com seus botões (indexado) ou o seletor circular (RGBA);
//   - coluna direita: prévia em tamanho real e a área de desenho ampliada.
//
// Apenas organiza widgets; o estado vive na EditorSession.
// =============================================================================
#pragma once

#include <QList>
#include <QPair>
#include <QWidget>

class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace spritenator::gui {

class ActionCatalog;
class ColorWheel;
class DrawAreaView;
class EditOperations;
class EditorSession;
class PaletteView;

/// Painel com a área de desenho e seus controles.
class IntegratorPanel final : public QWidget {
    Q_OBJECT

public:
    IntegratorPanel(EditorSession* session, ActionCatalog* actions, EditOperations* operations,
        QWidget* parent = nullptr);

    [[nodiscard]] DrawAreaView* drawArea() const { return m_drawArea; }
    [[nodiscard]] PaletteView* paletteView() const { return m_paletteView; }
    [[nodiscard]] ColorWheel* colorWheel() const { return m_colorWheel; }

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Reaplica as dicas no idioma atual.
    void retranslate();

    /// Cria a coluna esquerda (ferramentas, ajuste das cores e paleta).
    QWidget* createLeftColumn(ActionCatalog* actions);
    /// Cria a coluna direita (prévia + área de desenho + transformações).
    QWidget* createRightColumn();
    /// Cria a barra com os 12 botões de transformação.
    QWidget* createTransformBar(QWidget* parent);

    /// Pede o ângulo e aplica a rotação livre.
    void rotateFreely();
    /// Pede o novo tamanho e estica a região de trabalho.
    void stretchWorkArea();

    EditorSession* m_session;
    EditOperations* m_operations;
    PaletteView* m_paletteView = nullptr;
    ColorWheel* m_colorWheel = nullptr;
    DrawAreaView* m_drawArea = nullptr;
    QScrollArea* m_drawScroll = nullptr;
    QList<QPair<QToolButton*, const char*>> m_transformButtons; ///< botão e dica-fonte
};

} // namespace spritenator::gui
