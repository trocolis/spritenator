// =============================================================================
// CanvasWindow.h
//
// Conteúdo de uma janela-filha MDI: uma área com rolagem contendo a
// CanvasView de um documento. A janela é dona do documento.
// =============================================================================
#pragma once

#include <QScrollArea>

namespace spritenator::gui {

class CanvasView;
class Document;

/// Janela de canvas exibida dentro do QMdiArea.
class CanvasWindow final : public QScrollArea {
    Q_OBJECT

public:
    /// Assume a posse do documento (passa a ser filho desta janela).
    explicit CanvasWindow(Document* document, QWidget* parent = nullptr);

    [[nodiscard]] Document* document() const { return m_document; }
    [[nodiscard]] CanvasView* view() const { return m_view; }

    /// Tamanho ideal: o canvas inteiro visível, sem barras de rolagem.
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Atualiza o título: "nome[*] (LxA)". O "[*]" vira "*" se modificado.
    void updateTitle();

    Document* m_document = nullptr;
    CanvasView* m_view = nullptr;
};

} // namespace spritenator::gui
