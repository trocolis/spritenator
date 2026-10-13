// =============================================================================
// OrgAreaView.h
//
// Prévia em tamanho original da área de desenho (TOrgArea no original): mostra
// o buffer em 1x (ou 2x com a opção "dobrar canvas"), para julgar o resultado
// como ele aparecerá no jogo.
// =============================================================================
#pragma once

#include <QWidget>

namespace spritenator::gui {

class EditorSession;

/// Prévia 1:1 do buffer da área de desenho.
class OrgAreaView final : public QWidget {
    Q_OBJECT

public:
    explicit OrgAreaView(EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// Fator de exibição (1 ou 2, conforme a grade de personagens).
    [[nodiscard]] int scale() const;

    EditorSession* m_session;
};

} // namespace spritenator::gui
