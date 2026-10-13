// =============================================================================
// IntegratorPanel.h
//
// Painel lateral de edição (inspirado no TIntegrator do original). Uma
// coluna com, de cima para baixo: ferramentas e tons, transformações,
// ajuste das cores (mostradores e barras R, G, B, A) e, conforme o modo de
// cor, a paleta com seus botões (indexado) ou o seletor circular (RGBA).
//
// A edição acontece diretamente nos editores de imagem (CanvasEditor).
//
// Apenas organiza widgets; o estado vive na EditorSession.
// =============================================================================
#pragma once

#include <QList>
#include <QPair>
#include <QWidget>

class QToolButton;

namespace spritenator::gui {

class ActionCatalog;
class ColorWheel;
class EditOperations;
class EditorSession;
class PaletteView;

/// Painel com as ferramentas e as cores.
class IntegratorPanel final : public QWidget {
    Q_OBJECT

public:
    IntegratorPanel(EditorSession* session, ActionCatalog* actions, EditOperations* operations,
        QWidget* parent = nullptr);

    [[nodiscard]] PaletteView* paletteView() const { return m_paletteView; }
    [[nodiscard]] ColorWheel* colorWheel() const { return m_colorWheel; }

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Reaplica as dicas no idioma atual.
    void retranslate();

    /// Monta a coluna: ferramentas, transformações, cores e paleta.
    void buildColumn(ActionCatalog* actions);
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
    QList<QPair<QToolButton*, const char*>> m_transformButtons; ///< botão e dica-fonte
};

} // namespace spritenator::gui
