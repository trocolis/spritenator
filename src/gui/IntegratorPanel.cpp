// =============================================================================
// IntegratorPanel.cpp
// =============================================================================
#include "IntegratorPanel.h"

#include "ActionCatalog.h"
#include "DrawBuffer.h"
#include "EditOperations.h"
#include "EditorSession.h"
#include "IconProvider.h"
#include "ImageRendering.h"
#include "dialogs/StretchDialog.h"
#include "widgets/ColorEditor.h"
#include "widgets/ColorWheel.h"
#include "widgets/DrawAreaView.h"
#include "widgets/OrgAreaView.h"
#include "widgets/PaletteView.h"
#include "widgets/ToolPanel.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

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

IntegratorPanel::IntegratorPanel(
    EditorSession* session, ActionCatalog* actions, EditOperations* operations, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_operations(operations)
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
    retranslate();
}

QWidget* IntegratorPanel::createLeftColumn(ActionCatalog* actions)
{
    auto* column = new QWidget(this);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(4, 4, 4, 4);

    // Ordem de cima para baixo: ferramentas, ajuste das cores e paleta.
    // Um pequeno espaço separa visualmente os três painéis.
    constexpr int kSectionSpacing = 8;

    // --- 1. Ferramentas e tons ---
    layout->addWidget(new ToolPanel(m_session, column));
    layout->addSpacing(kSectionSpacing);

    // --- 2. Ajuste das cores (amostras de desenho/fundo e controles R, G, B) ---
    layout->addWidget(new ColorEditor(m_session, column));
    layout->addSpacing(kSectionSpacing);

    // --- 3. Paleta: grade de cores e seus botões (os mesmos do original,
    //        ligados às ações do catálogo). Só existe no modo indexado. ---
    auto* paletteSection = new QWidget(column);
    auto* paletteLayout = new QVBoxLayout(paletteSection);
    paletteLayout->setContentsMargins(0, 0, 0, 0);
    m_paletteView = new PaletteView(m_session, paletteSection);
    paletteLayout->addWidget(m_paletteView);

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
        paletteButtons->addWidget(
            makeActionButton(actions->action(id), paletteSection), 0, position++);
    }
    paletteButtons->setColumnStretch(position, 1);
    paletteLayout->addLayout(paletteButtons);
    layout->addWidget(paletteSection);

    // --- 3'. Seletor circular (modo RGBA), no lugar da paleta ---
    m_colorWheel = new ColorWheel(column);
    layout->addWidget(m_colorWheel);

    // Clique no seletor: esquerdo = primária, direito = secundária; a cor
    // escolhida também se torna o mostrador ativo (as barras a mostram).
    connect(m_colorWheel, &ColorWheel::colorPicked, this,
        [this](const QColor& picked, Qt::MouseButton button) {
            const ColorRole role = colorRoleFor(button);
            core::Rgba color = toRgba(picked);
            // Mantém a opacidade da cor de destino — exceto se ela era
            // totalmente transparente (ex.: secundária padrão): aí fica opaca,
            // senão a cor escolhida seria invisível.
            const std::uint8_t alpha = m_session->rgbaFor(role).alpha;
            color.alpha = alpha == 0 ? 255 : alpha;
            m_session->setActiveColorRole(role);
            m_session->setRgbaFor(role, color);
        });
    // Cores alteradas por outros meios (barras, troca de mostrador) -> seletor.
    const auto syncWheel = [this] {
        m_colorWheel->setColor(toQColor(m_session->rgbaFor(m_session->activeColorRole())));
    };
    connect(m_session, &EditorSession::colorsChanged, m_colorWheel, syncWheel);
    connect(m_session, &EditorSession::activeColorRoleChanged, m_colorWheel, syncWheel);
    syncWheel();

    // Paleta no modo indexado; seletor circular no modo RGBA.
    const auto updateColorSection = [this, paletteSection] {
        const bool indexed = m_session->colorMode() == core::ColorMode::Indexed;
        paletteSection->setVisible(indexed);
        m_colorWheel->setVisible(!indexed);
    };
    connect(m_session, &EditorSession::colorModeChanged, this, updateColorSection);
    updateColorSection();
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
    layout->addWidget(createTransformBar(column));

    return column;
}

QWidget* IntegratorPanel::createTransformBar(QWidget* parent)
{
    // Mesma ordem e ícones dos botões do original (MenuButtonS1..S12). As
    // dicas são textos-fonte (QT_TR_NOOP), traduzidos em retranslate().
    struct TransformButton {
        const char* icon;
        const char* tip;
        std::function<void()> action;
    };
    const auto transform
        = [this](TransformKind kind) { return [this, kind] { m_operations->transform(kind); }; };
    const TransformButton buttons[] = {
        { "shift_up", QT_TR_NOOP("Shift up"), transform(TransformKind::ShiftUp) },
        { "shift_right", QT_TR_NOOP("Shift right"), transform(TransformKind::ShiftRight) },
        { "shift_down", QT_TR_NOOP("Shift down"), transform(TransformKind::ShiftDown) },
        { "shift_left", QT_TR_NOOP("Shift left"), transform(TransformKind::ShiftLeft) },
        { "flip_vertical", QT_TR_NOOP("Flip vertically"), transform(TransformKind::FlipVertical) },
        { "flip_horizontal", QT_TR_NOOP("Flip horizontally"),
            transform(TransformKind::FlipHorizontal) },
        { "mirror_vertical",
            QT_TR_NOOP("Vertical symmetry: copy the top half mirrored onto the bottom"),
            transform(TransformKind::MirrorVertical) },
        { "mirror_horizontal",
            QT_TR_NOOP("Horizontal symmetry: copy the left half mirrored onto the right"),
            transform(TransformKind::MirrorHorizontal) },
        { "rotate_cw", QT_TR_NOOP("Rotate 90 degrees clockwise"),
            transform(TransformKind::RotateClockwise) },
        { "rotate_ccw", QT_TR_NOOP("Rotate 90 degrees counterclockwise"),
            transform(TransformKind::RotateCounterClockwise) },
        { "rotate_free", QT_TR_NOOP("Rotate by any angle..."), [this] { rotateFreely(); } },
        { "stretch", QT_TR_NOOP("Stretch or shrink..."), [this] { stretchWorkArea(); } },
    };

    auto* bar = new QWidget(parent);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);
    for (const TransformButton& spec : buttons) {
        auto* button = new QToolButton(bar);
        button->setIcon(IconProvider::icon(QString::fromLatin1(spec.icon)));
        button->setIconSize(QSize(20, 20));
        button->setAutoRaise(true);
        connect(button, &QToolButton::clicked, this, spec.action);
        layout->addWidget(button);
        m_transformButtons.append({ button, spec.tip });
    }
    layout->addStretch(1);
    return bar;
}

void IntegratorPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void IntegratorPanel::retranslate()
{
    for (const auto& [button, tip] : std::as_const(m_transformButtons)) {
        button->setToolTip(tr(tip) + tr(" (selection or whole drawing area)"));
    }
}

void IntegratorPanel::rotateFreely()
{
    bool accepted = false;
    const int degrees = QInputDialog::getInt(this, tr("Free Rotation"),
        tr("Rotation angle (degrees, clockwise):"), 0, -360, 360, 1, &accepted);
    if (accepted && degrees % 360 != 0) {
        m_operations->rotate(degrees);
    }
}

void IntegratorPanel::stretchWorkArea()
{
    StretchDialog dialog(m_session->buffer()->workArea().size(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_operations->stretch(dialog.newSize());
    }
}

} // namespace spritenator::gui
