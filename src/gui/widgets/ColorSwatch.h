// =============================================================================
// ColorSwatch.h
//
// Mostrador de uma cor (primária ou secundária). Exibe:
//   - a cor sobre um fundo xadrez, para que o alfa fique visível;
//   - opcionalmente um texto (o índice da paleta, no modo indexado);
//   - uma moldura destacada quando é o mostrador ATIVO (o que as barras
//     R, G, B, A editam).
// Clicar no mostrador emite clicked() — o ColorEditor o torna ativo.
// =============================================================================
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

namespace spritenator::gui {

/// Amostra de cor clicável.
class ColorSwatch final : public QWidget {
    Q_OBJECT

public:
    explicit ColorSwatch(QWidget* parent = nullptr);

    void setColor(const QColor& color);
    [[nodiscard]] const QColor& color() const { return m_color; }

    /// Texto sobre a cor (vazio = nenhum).
    void setText(const QString& text);
    [[nodiscard]] const QString& text() const { return m_text; }

    /// Destaque de mostrador ativo.
    void setActive(bool active);
    [[nodiscard]] bool isActive() const { return m_active; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QColor m_color = Qt::black;
    QString m_text;
    bool m_active = false;
};

} // namespace spritenator::gui
