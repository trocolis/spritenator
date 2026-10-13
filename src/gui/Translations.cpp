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

/// Tradutores instalados por apply(), removidos na próxima troca de idioma.
/// Pertencem à QCoreApplication (pai), então nunca vazam memória.
QList<QTranslator*>& installedTranslators()
{
    static QList<QTranslator*> translators;
    return translators;
}

/// Carrega um tradutor e o instala; descarta-o se o arquivo não existir.
bool loadTranslator(const QLocale& locale, const QString& prefix, const QStringList& folders)
{
    auto* translator = new QTranslator(QCoreApplication::instance());
    for (const QString& folder : folders) {
        if (translator->load(locale, prefix, QStringLiteral("_"), folder)) {
            // Cada instalação faz o Qt enviar QEvent::LanguageChange às janelas.
            QCoreApplication::installTranslator(translator);
            installedTranslators().append(translator);
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

bool Translations::apply(const QLocale& locale)
{
    // Remove as traduções anteriores (troca de idioma em tempo de execução).
    // A localidade precisa ser definida ANTES, pois cada remoção/instalação
    // dispara a retradução imediata das janelas (que consultam QLocale()).
    QLocale::setDefault(locale);
    for (QTranslator* translator : installedTranslators()) {
        QCoreApplication::removeTranslator(translator);
        delete translator;
    }
    installedTranslators().clear();

    // Traduções do Qt: pasta de instalação do Qt (Linux) ou a pasta
    // "translations" ao lado do executável (pacotes do Windows/macOS).
    const QStringList qtFolders = {
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
    };
    loadTranslator(locale, QStringLiteral("qtbase"), qtFolders);

    // Instalar e remover tradutores já faz o Qt enviar QEvent::LanguageChange
    // a todas as janelas (inclusive ao voltar para o inglês, só removendo).
    return loadTranslator(
        locale, QString::fromLatin1(kFilePrefix), { QString::fromLatin1(kResourceFolder) });
}

} // namespace spritenator::gui
