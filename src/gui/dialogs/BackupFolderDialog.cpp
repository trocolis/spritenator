// =============================================================================
// BackupFolderDialog.cpp
// =============================================================================
#include "BackupFolderDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace spritenator::gui {

BackupFolderDialog::BackupFolderDialog(const QString& folder, QWidget* parent)
    : QDialog(parent)
    , m_enabled(new QCheckBox(tr("Keep a backup of the previous version when saving"), this))
    , m_path(new QLineEdit(folder, this))
    , m_browse(new QPushButton(tr("&Browse..."), this))
{
    setWindowTitle(tr("Backup Folder"));
    m_enabled->setChecked(!folder.isEmpty());
    m_path->setPlaceholderText(tr("Folder for the backup copies"));

    auto* explanation = new QLabel(
        tr("Before a file is overwritten, a copy of its previous version is stored in this "
           "folder with the date and time in its name."),
        this);
    explanation->setWordWrap(true);

    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(m_browse);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_enabled->isChecked() && m_path->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Choose a folder or disable the backup."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(explanation);
    layout->addWidget(m_enabled);
    layout->addLayout(pathRow);
    layout->addWidget(buttons);
    resize(460, sizeHint().height());

    connect(m_enabled, &QCheckBox::toggled, this, &BackupFolderDialog::updateEnabledState);
    connect(m_browse, &QPushButton::clicked, this, &BackupFolderDialog::browse);
    updateEnabledState();
}

QString BackupFolderDialog::folder() const
{
    return m_enabled->isChecked() ? QDir::cleanPath(m_path->text().trimmed()) : QString();
}

void BackupFolderDialog::browse()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Backup Folder"), m_path->text().isEmpty() ? QDir::homePath() : m_path->text());
    if (!chosen.isEmpty()) {
        m_path->setText(chosen);
    }
}

void BackupFolderDialog::updateEnabledState()
{
    m_path->setEnabled(m_enabled->isChecked());
    m_browse->setEnabled(m_enabled->isChecked());
}

} // namespace spritenator::gui
