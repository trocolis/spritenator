// =============================================================================
// Backup.h
//
// Backup automático ao salvar ("Set Backup Folder" no original): antes de um
// arquivo existente ser sobrescrito, uma cópia da versão anterior é guardada
// na pasta de backup com data e hora no nome, por exemplo:
//
//   heroi.png  ->  <pasta>/heroi_20261013-153045.png
// =============================================================================
#pragma once

#include <QDateTime>
#include <QString>

namespace spritenator::io {

/// Nome do arquivo de backup para "filePath" no instante "timestamp".
[[nodiscard]] QString backupFileName(const QString& filePath, const QDateTime& timestamp);

/// Copia "filePath" para "backupFolder" (criando a pasta se preciso).
/// Devolve o caminho da cópia, ou uma string vazia se o arquivo original
/// ainda não existir (nada a preservar). Lança IoError em caso de falha.
QString createBackup(const QString& filePath, const QString& backupFolder,
    const QDateTime& timestamp = QDateTime::currentDateTime());

} // namespace spritenator::io
