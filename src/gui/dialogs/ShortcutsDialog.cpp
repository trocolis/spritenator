// =============================================================================
// ShortcutsDialog.cpp
// =============================================================================
#include "ShortcutsDialog.h"

#include "gui/ActionCatalog.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Texto da ação sem o "&" dos aceleradores nem as reticências.
QString plainText(const QAction* action)
{
    QString text = action->text();
    text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
    if (text.endsWith(QStringLiteral("..."))) {
        text.chop(3);
    }
    return text;
}

} // namespace

ShortcutsDialog::ShortcutsDialog(const ActionCatalog* actions, QWidget* parent)
    : QDialog(parent)
    , m_list(new QTreeWidget(this))
    , m_editor(new QKeySequenceEdit(this))
    , m_clear(new QPushButton(tr("&Remove"), this))
    , m_default(new QPushButton(tr("&Default"), this))
    , m_defaultAll(new QPushButton(tr("Restore &All Defaults"), this))
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(560, 520);

    m_list->setHeaderLabels({ tr("Command"), tr("Shortcut") });
    m_list->setRootIsDecorated(false);
    m_list->setAlternatingRowColors(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (std::size_t index = 0; index < kActionCount; ++index) {
        const QAction* action = actions->action(static_cast<ActionId>(index));
        m_shortcuts.append(action->shortcut());
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, plainText(action));
        item->setIcon(0, action->icon());
        item->setToolTip(0, action->statusTip());
    }
    for (int row = 0; row < m_list->topLevelItemCount(); ++row) {
        refreshRow(row);
    }
    m_editor->setMaximumSequenceLength(1); // um atalho por ação, sem "acordes"
    m_editor->setClearButtonEnabled(true);

    auto* editRow = new QHBoxLayout;
    editRow->addWidget(new QLabel(tr("Shortcut:"), this));
    editRow->addWidget(m_editor, 1);
    editRow->addWidget(m_clear);
    editRow->addWidget(m_default);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->addButton(m_defaultAll, QDialogButtonBox::ResetRole);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(editRow);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &ShortcutsDialog::showCurrent);
    connect(m_editor, &QKeySequenceEdit::editingFinished, this, [this] {
        const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
        if (row >= 0 && !assign(row, m_editor->keySequence())) {
            showCurrent(); // recusado: volta a mostrar o atalho atual
        }
    });
    connect(m_clear, &QPushButton::clicked, this, [this] {
        const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
        if (row >= 0) {
            assign(row, { });
            showCurrent();
        }
    });
    connect(m_default, &QPushButton::clicked, this, [this] {
        const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
        if (row >= 0) {
            assign(row, ActionCatalog::defaultShortcut(static_cast<ActionId>(row)));
            showCurrent();
        }
    });
    connect(m_defaultAll, &QPushButton::clicked, this, [this] {
        for (int row = 0; row < m_shortcuts.size(); ++row) {
            m_shortcuts[row] = ActionCatalog::defaultShortcut(static_cast<ActionId>(row));
            refreshRow(row);
        }
        showCurrent();
    });
    m_list->setCurrentItem(m_list->topLevelItem(0));
}

bool ShortcutsDialog::assign(int row, const QKeySequence& sequence)
{
    if (row < 0 || row >= m_shortcuts.size()) {
        return false;
    }
    if (!sequence.isEmpty()) {
        // Atalho de outra ação: pergunta se deve passar para esta.
        const auto owner = std::find(m_shortcuts.begin(), m_shortcuts.end(), sequence);
        const int ownerRow = static_cast<int>(owner - m_shortcuts.begin());
        if (owner != m_shortcuts.end() && ownerRow != row) {
            const auto answer = QMessageBox::question(this, windowTitle(),
                tr("%1 is already the shortcut of \"%2\". Use it for \"%3\" instead?")
                    .arg(sequence.toString(QKeySequence::NativeText),
                        m_list->topLevelItem(ownerRow)->text(0),
                        m_list->topLevelItem(row)->text(0)));
            if (answer != QMessageBox::Yes) {
                return false;
            }
            m_shortcuts[ownerRow] = { };
            refreshRow(ownerRow);
        }
    }
    m_shortcuts[row] = sequence;
    refreshRow(row);
    return true;
}

void ShortcutsDialog::refreshRow(int row)
{
    QTreeWidgetItem* item = m_list->topLevelItem(row);
    item->setText(1, m_shortcuts.at(row).toString(QKeySequence::NativeText));
    // Atalhos alterados aparecem em negrito.
    QFont font = item->font(1);
    font.setBold(m_shortcuts.at(row) != ActionCatalog::defaultShortcut(static_cast<ActionId>(row)));
    item->setFont(1, font);
}

void ShortcutsDialog::showCurrent()
{
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    const QSignalBlocker blocker(m_editor);
    m_editor->setKeySequence(row >= 0 ? m_shortcuts.at(row) : QKeySequence());
}

} // namespace spritenator::gui
