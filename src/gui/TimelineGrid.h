// =============================================================================
// TimelineGrid.h
//
// A grade da linha do tempo: CAMADAS nas linhas (a de cima primeiro) e CENAS
// nas colunas. À esquerda de cada camada: olho (visível), cadeado
// (bloqueada) e nome; no alto, o número de cada cena. Cada célula mostra se
// tem desenho, e a célula atual fica destacada.
//
// A coluna das camadas e a linha dos números ficam fixas; as células rolam.
// O widget só desenha e avisa os cliques (sinais); quem muda o documento é o
// TimelinePanel.
// =============================================================================
#pragma once

#include <QAbstractScrollArea>
#include <QPointer>

#include <vector>

class QLineEdit;
class QTimer;

namespace spritenator::gui {

class Document;

/// Grade camadas x cenas.
class TimelineGrid final : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit TimelineGrid(QWidget* parent = nullptr);

    /// Documento exibido (nulo = grade vazia).
    void setDocument(Document* document);

    /// Widget colocado no canto superior esquerdo (a opacidade da camada).
    void setCornerWidget(QWidget* widget);

    // --- Geometria, em coordenadas do viewport (expostas para testes) ---
    [[nodiscard]] QRect cellRect(int frame, int layer) const;
    [[nodiscard]] QRect eyeRect(int layer) const;
    [[nodiscard]] QRect lockRect(int layer) const;
    [[nodiscard]] QRect nameRect(int layer) const;
    [[nodiscard]] QRect frameHeaderRect(int frame) const;
    /// Verdadeiro se a célula tem desenho (cache atualizado com atraso).
    [[nodiscard]] bool cellFilled(int frame, int layer) const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void frameClicked(int frame);
    void cellClicked(int frame, int layer);
    void layerClicked(int layer);
    void visibilityToggled(int layer);
    void lockToggled(int layer);
    void renameRequested(int layer, const QString& name);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    /// Linha (de cima para baixo) <-> índice da camada (0 = a de baixo).
    [[nodiscard]] int rowOf(int layer) const;
    [[nodiscard]] int layerAtRow(int row) const;
    void updateScrollBars();
    /// Recalcula quais células têm desenho (todas ou só a atual).
    void refreshFilled(bool all);
    /// Garante que a célula atual esteja à vista.
    void ensureCurrentVisible();
    void startRename(int layer);

    QPointer<Document> m_document;
    std::vector<std::vector<bool>> m_filled; ///< [cena][camada]
    QTimer* m_filledTimer = nullptr; ///< agrupa as atualizações ao desenhar
    QWidget* m_corner = nullptr;
    QLineEdit* m_renameEditor = nullptr;
    int m_renameLayer = -1;
};

} // namespace spritenator::gui
