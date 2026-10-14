// =============================================================================
// FlowLayout.h
//
// Layout "de fluxo": coloca os itens lado a lado, da esquerda para a direita,
// e quebra a linha quando não cabem mais — como texto num parágrafo. Com o
// painel largo tudo fica numa linha; estreitando, os itens descem.
//
// O Qt não traz um layout assim; esta é a versão do exemplo oficial
// ("Flow Layout"), com altura dependente da largura (heightForWidth) para
// que o painel cresça na vertical ao quebrar linhas.
// =============================================================================
#pragma once

#include <QLayout>
#include <QList>

namespace spritenator::gui {

/// Layout que quebra linha conforme a largura disponível.
class FlowLayout final : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int spacing = 1);
    ~FlowLayout() override;

    FlowLayout(const FlowLayout&) = delete;
    FlowLayout& operator=(const FlowLayout&) = delete;

    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    [[nodiscard]] QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize minimumSize() const override;
    [[nodiscard]] QSize sizeHint() const override;
    void setGeometry(const QRect& rect) override;

private:
    /// Posiciona os itens em "rect" (ou só calcula, se "testOnly") e devolve
    /// a altura usada.
    int doLayout(const QRect& rect, bool testOnly) const;

    QList<QLayoutItem*> m_items;
};

} // namespace spritenator::gui
