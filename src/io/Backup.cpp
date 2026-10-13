// =============================================================================
// Backup.cpp
// =============================================================================
#include "Backup.h"

#include "IoError.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace spritenator::io {

QString backupFileName(const QString& filePath, const QDateTime& timestamp)
{
    const QFileInfo info(filePath);
    const QString stamp = timestamp.toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString suffix
        = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
    return info.completeBaseName() + QLatin1Char('_') + stamp + suffix;
}

QString createBackup(
    const QString& filePath, const QString& backupFolder, const QDateTime& timestamp)
{
    if (!QFileInfo::exists(filePath)) {
        return { }; // primeira gravação: não há versão anterior
    }
    QDir folder(backupFolder);
    if (!folder.exists() && !folder.mkpath(QStringLiteral("."))) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::Backup", "Cannot create the backup folder \"%1\".")
                .arg(backupFolder));
    }
    // Dois backups no mesmo segundo: acrescenta "-2", "-3"... em vez de sobrescrever.
    const QFileInfo name(backupFileName(filePath, timestamp));
    const QString suffix
        = name.suffix().isEmpty() ? QString() : QStringLiteral(".") + name.suffix();
    QString target = folder.filePath(name.fileName());
    for (int counter = 2; QFileInfo::exists(target); ++counter) {
        target = folder.filePath(
            QStringLiteral("%1-%2%3").arg(name.completeBaseName()).arg(counter).arg(suffix));
    }
    if (!QFile::copy(filePath, target)) {
        throw IoError(QCoreApplication::translate(
            "spritenator::io::Backup", "Cannot write the backup \"%1\".")
                .arg(target));
    }
    return target;
}

} // namespace spritenator::io
