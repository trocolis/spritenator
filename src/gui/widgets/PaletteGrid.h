// =============================================================================
// PaletteGrid.h
//
// Grade de paleta independente da sessão, usada pelos diálogos de editar e
// carregar paleta. Exibe uma paleta qualquer e um intervalo selecionado
// (âncora..atual): clique escolhe uma cor; Shift+clique estende o intervalo.
// =============================================================================
#pragma once

#include "core/Palette.h"

#include <QWidget>

#include <algorithm>

namespace spritenator::gui {

/// Exibição e seleção de cores de uma paleta avulsa.
class PaletteGrid final : public QWidget {
    Q_OBJECT

public:
    explicit PaletteGrid(QWidget* parent = nullptr);

    /// Define a paleta exibida (todas as entradas ficam visíveis). O nome
    /// evita conflito com QWidget::setPalette (cores do tema do widget).
    void setColorPalette(const core::Palette& palette);
    [[nodiscard]] const core::Palette& colorPalette() const { return m_palette; }

    /// Quantas entradas exibir (ex.: só as cores lidas de um arquivo).
    void setVisibleCount(int count);

    /// Intervalo selecionado (âncora e cor atual, em qualquer ordem).
    [[nodiscard]] int anchor() const { return m_anchor; }
    [[nodiscard]] int current() const { return m_current; }
    [[nodiscard]] int selectionFirst() const { return std::min(m_anchor, m_current); }
    [[nodiscard]] int selectionLast() const { return std::max(m_anchor, m_current); }
    void setSelection(int anchor, int current);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void selectionChanged();
    void colorDoubleClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] int indexAt(const QPoint& position) const;
    [[nodiscard]] QRect cellRect(int index) const;
    [[nodiscard]] int cellSize() const;
    [[nodiscard]] int rows() const;

    core::Palette m_palette;
    int m_visibleCount = 256;
    int m_anchor = 0;
    int m_current = 0;
};

} // namespace spritenator::gui
