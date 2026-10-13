// =============================================================================
// PaletteView.h
//
// Grade com as cores da paleta (TPaletteArea no original). Botão esquerdo
// escolhe a cor de desenho; botão direito, a cor de fundo. As cores
// selecionadas são marcadas com molduras distintas.
//
// No modo máscara (F5) o comportamento muda, como no original:
//   - esquerdo: marca/desmarca a cor como protegida;
//   - direito: inverte a máscara; duplo clique direito: limpa a máscara.
// =============================================================================
#pragma once

#include <QWidget>

namespace spritenator::gui {

class EditorSession;

/// Seletor de cores da paleta.
class PaletteView final : public QWidget {
    Q_OBJECT

public:
    /// Quantidade de colunas da grade (16 x 16 para 256 cores).
    static constexpr int kColumns = 16;

    explicit PaletteView(EditorSession* session, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    /// Uma cor foi clicada (índice e botão). Emitido além da seleção normal,
    /// para que outros componentes (ex.: modo máscara) possam reagir.
    void colorClicked(int index, Qt::MouseButton button);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override; // dicas (tooltips) por cor

private:
    /// Índice da cor sob a posição (-1 se fora da grade).
    [[nodiscard]] int indexAt(const QPoint& position) const;
    /// Retângulo de uma célula da grade.
    [[nodiscard]] QRect cellRect(int index) const;
    /// Lado de cada célula em pixels (adapta-se à largura disponível).
    [[nodiscard]] int cellSize() const;
    [[nodiscard]] int rows() const;

    EditorSession* m_session;
};

} // namespace spritenator::gui
