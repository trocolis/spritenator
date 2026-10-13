// =============================================================================
// ToolPanel.cpp
// =============================================================================
#include "ToolPanel.h"

#include "gui/EditorSession.h"
#include "gui/IconProvider.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
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
    { ToolKind::Select, "tool_select",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Select a rectangular area") },
    { ToolKind::Zoom, "tool_zoom",
        QT_TRANSLATE_NOOP(
            "spritenator::gui::ToolPanel", "Zoom: left button magnifies, right button reduces") },
    { ToolKind::Fill, "tool_fill",
        QT_TRANSLATE_NOOP(
            "spritenator::gui::ToolPanel", "Fill the adjacent area of the same color") },
    { ToolKind::PenThin, "tool_pen_thin",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Thin pen (1 pixel)") },
    { ToolKind::PenThick, "tool_pen_thick",
        QT_TRANSLATE_NOOP("spritenator::gui::ToolPanel", "Thick pen (3 pixels)") },
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

constexpr int kToolColumns = 4;
constexpr int kToneColumns = 3; // 3 x 3: oito tons + botão de deslocamento

/// Cria um botão de ícone sem texto.
QToolButton* makeIconButton(
    const QString& icon, const QString& tip, QSize iconSize, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setIcon(IconProvider::icon(icon));
    button->setIconSize(iconSize);
    button->setToolTip(tip);
    button->setAutoRaise(true);
    return button;
}

} // namespace

ToolPanel::ToolPanel(EditorSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_toolGroup(new QButtonGroup(this))
    , m_toneGroup(new QButtonGroup(this))
{
    // --- Ferramentas ---
    auto* toolGrid = new QGridLayout;
    toolGrid->setSpacing(1);
    int position = 0;
    for (const ToolSpec& spec : kToolSpecs) {
        QToolButton* button
            = makeIconButton(QString::fromLatin1(spec.icon), tr(spec.tip), { 20, 20 }, this);
        button->setCheckable(true);
        m_toolGroup->addButton(button, static_cast<int>(spec.kind));
        toolGrid->addWidget(button, position / kToolColumns, position % kToolColumns);
        ++position;
    }

    // --- Tons A..H + deslocamento ---
    auto* toneGrid = new QGridLayout;
    toneGrid->setSpacing(1);
    for (int index = 0; index < core::kToneCount; ++index) {
        const QChar letter(QLatin1Char(static_cast<char>('a' + index)));
        QToolButton* button = makeIconButton(QStringLiteral("tone_") + letter,
            tr("Draw with tone %1").arg(letter.toUpper()), { 20, 16 }, this);
        button->setCheckable(true);
        m_toneGroup->addButton(button, index);
        toneGrid->addWidget(button, index / kToneColumns, index % kToneColumns);
    }
    m_shiftButton = makeIconButton(
        QStringLiteral("tone_shift"), tr("Shift the origin of the tone pattern"), { 20, 16 }, this);
    toneGrid->addWidget(
        m_shiftButton, core::kToneCount / kToneColumns, core::kToneCount % kToneColumns);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolGrid);
    layout->addSpacing(8);
    layout->addLayout(toneGrid);
    layout->addStretch(1);

    // --- Ligações com a sessão ---
    connect(m_toolGroup, &QButtonGroup::idClicked, this,
        [this](int id) { m_session->setToolKind(static_cast<ToolKind>(id)); });
    connect(m_toneGroup, &QButtonGroup::idClicked, this,
        [this](int id) { m_session->setTone(static_cast<core::Tone>(id)); });
    connect(m_shiftButton, &QToolButton::clicked, m_session, &EditorSession::cycleToneShift);
    connect(m_session, &EditorSession::toolChanged, this, &ToolPanel::syncFromSession);
    connect(m_session, &EditorSession::toneChanged, this, &ToolPanel::syncFromSession);
    syncFromSession();
}

void ToolPanel::syncFromSession()
{
    if (QAbstractButton* tool = m_toolGroup->button(static_cast<int>(m_session->toolKind()))) {
        tool->setChecked(true);
    }
    if (QAbstractButton* tone = m_toneGroup->button(static_cast<int>(m_session->tone()))) {
        tone->setChecked(true);
    }
    m_shiftButton->setToolTip(
        tr("Shift the origin of the tone pattern (current: %1)").arg(m_session->toneShift()));
}

} // namespace spritenator::gui
