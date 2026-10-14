// =============================================================================
// ToolPanel.cpp
// =============================================================================
#include "ToolPanel.h"

#include "FlowLayout.h"
#include "gui/EditorSession.h"
#include "gui/IconProvider.h"

#include <QButtonGroup>
#include <QEvent>
#include <QToolButton>

namespace spritenator::gui {

namespace {

/// Descrição de um botão de ferramenta (ícone + dica).
struct ToolSpec {
    ToolKind kind;
    const char* icon;
    const char* tip;
};

// Mesma ordem dos botões do original (4 colunas x 3 linhas).
constexpr ToolSpec kToolSpecs[] = {
    // Linha 1: seleções; Shift soma e Ctrl subtrai da seleção existente.
    { ToolKind::Select, "tool_select",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel",
            "Rectangular selection (Shift: square; when starting, Shift adds and Ctrl "
            "subtracts)") },
    { ToolKind::SelectEllipse, "tool_select_ellipse",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel",
            "Oval selection (Shift: circle; when starting, Shift adds and Ctrl subtracts)") },
    { ToolKind::SelectPolygon, "tool_select_polygon",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel",
            "Point-to-point selection: click each corner; close with a double click, "
            "the first point, the right button or Enter. Backspace removes the last point") },
    { ToolKind::SelectLasso, "tool_select_lasso",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel",
            "Freehand selection: draw the outline while holding the button") },
    { ToolKind::MagicWand, "tool_magic_wand",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel",
            "Magic wand: select the area of similar color (tolerance below the tools)") },
    { ToolKind::Fill, "tool_fill",
        QT_TRANSLATE_NOOP(
            "spritenator::gui::ToolPanel", "Fill the adjacent area of the same color") },
    { ToolKind::Pen, "tool_pen",
        QT_TRANSLATE_NOOP(
            "spritenator::gui::ToolPanel", "Pen (choose the tip and the size below the tools)") },
    { ToolKind::Line, "tool_line",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Straight line") },
    { ToolKind::Rectangle, "tool_rect",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Rectangle") },
    { ToolKind::RoundRectangle, "tool_round_rect",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Rounded rectangle") },
    { ToolKind::Ellipse, "tool_ellipse",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Ellipse") },
    { ToolKind::FilledRectangle, "tool_rect_filled",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Filled rectangle") },
    { ToolKind::FilledRoundRectangle, "tool_round_rect_filled",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Filled rounded rectangle") },
    { ToolKind::FilledEllipse, "tool_ellipse_filled",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Filled ellipse") },
};

QToolButton* makeIconButton(const QString& icon, QSize iconSize, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setIcon(IconProvider::icon(icon));
    button->setIconSize(iconSize);
    button->setAutoRaise(true);
    return button;
}

} // namespace

// -----------------------------------------------------------------------------
// ToolPanel: ferramentas
// -----------------------------------------------------------------------------
ToolPanel::ToolPanel(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_toolGroup(new QButtonGroup(this))
{
    // Em fluxo: uma linha só se couber; senão, quebra conforme a largura.
    auto* layout = new FlowLayout(this);
    for (const ToolSpec& spec : kToolSpecs) {
        QToolButton* button = makeIconButton(QString::fromLatin1(spec.icon), { 20, 20 }, this);
        button->setCheckable(true);
        m_toolGroup->addButton(button, static_cast<int>(spec.kind));
        layout->addWidget(button);
    }
    connect(m_toolGroup, &QButtonGroup::idClicked, this,
        [this](int id) { m_session->setToolKind(static_cast<ToolKind>(id)); });
    connect(m_session, &EditorSession::toolChanged, this, &ToolPanel::syncFromSession);
    retranslate();
}

QAbstractButton* ToolPanel::button(ToolKind kind) const
{
    return m_toolGroup->button(static_cast<int>(kind));
}

void ToolPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void ToolPanel::retranslate()
{
    for (const ToolSpec& spec : kToolSpecs) {
        m_toolGroup->button(static_cast<int>(spec.kind))->setToolTip(tr(spec.tip));
    }
    syncFromSession();
}

void ToolPanel::syncFromSession()
{
    if (QAbstractButton* tool = m_toolGroup->button(static_cast<int>(m_session->toolKind()))) {
        tool->setChecked(true);
    }
}

// -----------------------------------------------------------------------------
// TonePanel: tons (opções de preenchimento)
// -----------------------------------------------------------------------------
TonePanel::TonePanel(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_toneGroup(new QButtonGroup(this))
{
    auto* layout = new FlowLayout(this);
    for (int index = 0; index < core::kToneCount; ++index) {
        const QChar letter(QLatin1Char(static_cast<char>('a' + index)));
        QToolButton* button = makeIconButton(QStringLiteral("tone_") + letter, { 20, 16 }, this);
        button->setCheckable(true);
        m_toneGroup->addButton(button, index);
        layout->addWidget(button);
    }
    m_shiftButton = makeIconButton(QStringLiteral("tone_shift"), { 20, 16 }, this);
    layout->addWidget(m_shiftButton);

    connect(m_toneGroup, &QButtonGroup::idClicked, this,
        [this](int id) { m_session->setTone(static_cast<core::Tone>(id)); });
    connect(m_shiftButton, &QToolButton::clicked, m_session, &EditorSession::cycleToneShift);
    connect(m_session, &EditorSession::toneChanged, this, &TonePanel::syncFromSession);
    retranslate();
}

void TonePanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void TonePanel::retranslate()
{
    for (int index = 0; index < core::kToneCount; ++index) {
        const QChar letter(QLatin1Char(static_cast<char>('A' + index)));
        m_toneGroup->button(index)->setToolTip(tr("Draw with tone %1").arg(letter));
    }
    syncFromSession(); // a dica do deslocamento inclui o estado atual
}

void TonePanel::syncFromSession()
{
    if (QAbstractButton* tone = m_toneGroup->button(static_cast<int>(m_session->tone()))) {
        tone->setChecked(true);
    }
    m_shiftButton->setToolTip(
        tr("Shift the origin of the tone pattern (current: %1)").arg(m_session->toneShift()));
}

} // namespace spritenator::gui
