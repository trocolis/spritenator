// =============================================================================
// BackupTest.cpp — testes do backup automático ao salvar.
// =============================================================================
#include "io/Backup.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace spritenator::io;

class BackupTest final : public QObject {
    Q_OBJECT

private slots:
    void fileNameHasTimestamp()
    {
        const QDateTime moment(QDate(2026, 10, 13), QTime(15, 30, 45));
        QCOMPARE(backupFileName(QStringLiteral("/tmp/heroi.png"), moment),
            QStringLiteral("heroi_20261013-153045.png"));
        QCOMPARE(backupFileName(QStringLiteral("semextensao"), moment),
            QStringLiteral("semextensao_20261013-153045"));
    }

    /// Sem arquivo original não há backup; com arquivo, a cópia é criada
    /// (inclusive a pasta) e backups no mesmo segundo não se sobrescrevem.
    void createsCopiesWithoutOverwriting()
    {
        QTemporaryDir directory;
        const QString original = directory.filePath(QStringLiteral("sprite.bmp"));
        const QString folder = directory.filePath(QStringLiteral("backups/nested"));
        const QDateTime moment(QDate(2026, 1, 2), QTime(3, 4, 5));

        QVERIFY(createBackup(original, folder, moment).isEmpty());

        QFile file(original);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("conteudo");
        file.close();

        const QString first = createBackup(original, folder, moment);
        const QString second = createBackup(original, folder, moment);
        QVERIFY(QFile::exists(first));
        QVERIFY(QFile::exists(second));
        QVERIFY(first != second);
        QFile copy(first);
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), QByteArray("conteudo"));
    }
};

QTEST_GUILESS_MAIN(BackupTest)
#include "BackupTest.moc"
