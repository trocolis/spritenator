// =============================================================================
// UndoLabel.cpp
// =============================================================================
#include "UndoLabel.h"

#include "ImageCommands.h"

#include <QCoreApplication>
#include <QUndoGroup>
#include <QUndoStack>

#include <array>

namespace spritenator::gui {

namespace {

/// Contexto de tradução comum a todos os nomes de operação.
constexpr auto kContext = "spritenator::gui::UndoLabel";

/// Textos-fonte (inglês), na ordem de UndoOperation. QT_TRANSLATE_NOOP só
/// marca o texto para o lupdate; a tradução acontece em UndoLabel::text().
constexpr std::array kSources = {
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Pen"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Fill"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Line"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Rectangle"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Filled Rectangle"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Rounded Rectangle"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Filled Rounded Rectangle"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Ellipse"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Filled Ellipse"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Get Cell"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Put Cell"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Transfer All"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Paste"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Clear"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Prepare Shadow"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Text"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Shift Up"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Shift Right"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Shift Down"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Shift Left"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Flip Vertically"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Flip Horizontally"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Vertical Symmetry"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Horizontal Symmetry"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Rotate Right"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Rotate Left"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Free Rotation"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Stretch"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Transform Selection"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Canvas Size"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Clear All"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Character Size"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Edit Color %1"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Gradation"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Load Palette"),
    QT_TRANSLATE_NOOP("spritenator::gui::UndoLabel", "Edit Palette"),
};

// Toda operação nova precisa de um nome: a tabela acompanha o enum.
static_assert(kSources.size() == static_cast<std::size_t>(UndoOperation::EditPalette) + 1,
    "UndoLabel: kSources deve ter um nome para cada UndoOperation");

} // namespace

QString UndoLabel::text() const
{
    const QString name
        = QCoreApplication::translate(kContext, kSources.at(static_cast<std::size_t>(operation)));
    return argument >= 0 ? name.arg(argument) : name;
}

void retranslateUndoHistory(const QUndoGroup* group)
{
    for (QUndoStack* stack : group->stacks()) {
        for (int index = 0; index < stack->count(); ++index) {
            // command() devolve ponteiro constante, mas o comando pertence à
            // pilha e não é constante de fato: mudar só o texto é seguro.
            if (const auto* labeled = dynamic_cast<const LabeledCommand*>(stack->command(index))) {
                const_cast<LabeledCommand*>(labeled)->retranslate();
            }
        }
    }
}

} // namespace spritenator::gui
