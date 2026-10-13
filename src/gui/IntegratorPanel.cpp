// =============================================================================
// IntegratorPanel.cpp
// =============================================================================
#include "IntegratorPanel.h"

#include "ActionCatalog.h"
#include "EditorSession.h"
#include "widgets/ColorEditor.h"
#include "widgets/DrawAreaView.h"
#include "widgets/OrgAreaView.h"
#include "widgets/PaletteView.h"
#include "widgets/ToolPanel.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Cria um botão de ferramenta ligado a uma ação do catálogo.
QToolButton* makeActionButton(QAction* action, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(16, 16));
    return button;
}

} // namespace

IntegratorPanel::IntegratorPanel(EditorSession* session, ActionCatalog* actions, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(createLeftColumn(actions));
    splitter->addWidget(createRightColumn());
    splitter->setStretchFactor(0, 0); // coluna de controles com largura fixa
    splitter->setStretchFactor(1, 1); // área de desenho ocupa o restante
    splitter->setChildrenCollapsible(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);
}

QWidget* IntegratorPanel::createLeftColumn(ActionCatalog* actions)
{
    auto* column = new QWidget(this);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(4, 4, 4, 4);

    m_paletteView = new PaletteView(m_session, column);
    layout->addWidget(m_paletteView);

    // Botões de paleta (mesmos do painel do original, ligados às ações).
    auto* paletteButtons = new QGridLayout;
    paletteButtons->setSpacing(1);
    const ActionId buttonIds[] = {
        ActionId::PaletteTransparent,
        ActionId::PaletteLoad,
        ActionId::PaletteSave,
        ActionId::PaletteEdit,
        ActionId::PaletteMaskMode,
        ActionId::PaletteClearMask,
        ActionId::PaletteInvertMask,
        ActionId::PaletteAutoMask,
        ActionId::PaletteGradation,
    };
    int position = 0;
    for (const ActionId id : buttonIds) {
        paletteButtons->addWidget(makeActionButton(actions->action(id), column), 0, position++);
    }
    paletteButtons->setColumnStretch(position, 1);
    layout->addLayout(paletteButtons);

    layout->addWidget(new ColorEditor(m_session, column));
    layout->addWidget(new ToolPanel(m_session, column));
    layout->addStretch(1);

    column->setMinimumWidth(m_paletteView->minimumSizeHint().width() + 8);
    return column;
}

QWidget* IntegratorPanel::createRightColumn()
{
    auto* column = new QWidget(this);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 4, 4, 4);

    // Prévia em tamanho real.
    auto* preview = new OrgAreaView(m_session, column);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(preview);

    // Área de desenho com barras de rolagem.
    m_drawArea = new DrawAreaView(m_session, column);
    m_drawScroll = new QScrollArea(column);
    m_drawScroll->setWidget(m_drawArea);
    m_drawScroll->setWidgetResizable(false);
    m_drawScroll->setAlignment(Qt::AlignCenter);
    m_drawScroll->setBackgroundRole(QPalette::Dark);
    layout->addWidget(m_drawScroll, 1);

    return column;
}

} // namespace spritenator::gui
