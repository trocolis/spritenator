// =============================================================================
// Translations.cpp
// =============================================================================
#include "Translations.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QTranslator>

namespace spritenator::gui {

namespace {

/// Pasta dos .qm do Spritenator no sistema de recursos.
constexpr auto kResourceFolder = ":/i18n";

/// Prefixo dos arquivos de tradução (spritenator_pt_BR.qm...).
constexpr auto kFilePrefix = "spritenator";

/// Carrega um tradutor e o instala; descarta-o se o arquivo não existir.
bool loadTranslator(QCoreApplication& application, const QLocale& locale, const QString& prefix,
    const QStringList& folders)
{
    auto* translator = new QTranslator(&application);
    for (const QString& folder : folders) {
        if (translator->load(locale, prefix, QStringLiteral("_"), folder)) {
            QCoreApplication::installTranslator(translator);
            return true;
        }
    }
    delete translator;
    return false;
}

} // namespace

QStringList Translations::availableLanguages()
{
    // Cada spritenator_<código>.qm embutido corresponde a um idioma.
    QStringList languages;
    const QStringList files
        = QDir(QString::fromLatin1(kResourceFolder))
              .entryList({ QString::fromLatin1(kFilePrefix) + QStringLiteral("_*.qm") });
    for (const QString& file : files) {
        // "spritenator_pt_BR.qm" -> "pt_BR"
        languages.append(file.mid(static_cast<qsizetype>(qstrlen(kFilePrefix)) + 1).chopped(3));
    }
    return languages;
}

QLocale Translations::localeFor(const QString& languageCode)
{
    return languageCode.isEmpty() ? QLocale::system() : QLocale(languageCode);
}

bool Translations::install(QCoreApplication& application, const QLocale& locale)
{
    QLocale::setDefault(locale);

    // Traduções do Qt: pasta de instalação do Qt (Linux) ou a pasta
    // "translations" ao lado do executável (pacotes do Windows/macOS).
    const QStringList qtFolders = {
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
    };
    loadTranslator(application, locale, QStringLiteral("qtbase"), qtFolders);

    return loadTranslator(application, locale, QString::fromLatin1(kFilePrefix),
        { QString::fromLatin1(kResourceFolder) });
}

} // namespace spritenator::gui
