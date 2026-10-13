// =============================================================================
// main.cpp — ponto de entrada do Spritenator.
//
// Responsabilidades mínimas: configurar metadados da aplicação (usados pelo
// QSettings para localizar as configurações), instalar as traduções e exibir
// a janela principal.
// =============================================================================
#include "gui/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/Translations.h"

#include <spritenator/Version.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    // Metadados: definem também o caminho onde o QSettings grava as opções.
    QApplication::setApplicationName(QStringLiteral("Spritenator"));
    QApplication::setOrganizationName(QStringLiteral("Spritenator"));
    QApplication::setApplicationVersion(QString::fromUtf8(spritenator::version::kString.data(),
        static_cast<qsizetype>(spritenator::version::kString.size())));

    // Traduções: precisam ser instaladas ANTES de criar qualquer janela, pois
    // os textos são traduzidos no momento em que os widgets são construídos.
    const QString language = spritenator::gui::AppSettings().language();
    spritenator::gui::Translations::install(
        application, spritenator::gui::Translations::localeFor(language));

    spritenator::gui::MainWindow mainWindow;
    mainWindow.show();

    return QApplication::exec();
}
