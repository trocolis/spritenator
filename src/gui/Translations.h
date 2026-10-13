// =============================================================================
// Translations.h
//
// Instalação das traduções da interface. Dois tradutores são carregados:
//   - o do Spritenator (":/i18n/spritenator_<idioma>.qm", embutido no
//     executável — ver qt_add_translations em src/CMakeLists.txt);
//   - o do próprio Qt ("qtbase_<idioma>.qm"), que traduz os diálogos padrão
//     (botões "Salvar"/"Cancelar", seletor de arquivos, nomes de teclas).
//
// O idioma pode ser trocado com o programa aberto: apply() remove os
// tradutores anteriores e instala os novos. O Qt então envia
// QEvent::LanguageChange a todas as janelas, e cada componente reaplica seus
// textos no próprio retranslate() (ver MainWindow::changeEvent).
//
// O idioma vem das preferências (menu Exibir > Idioma); vazio = idioma do
// sistema. Textos sem tradução caem automaticamente no inglês original.
// =============================================================================
#pragma once

#include <QLocale>
#include <QString>
#include <QStringList>

namespace spritenator::gui {

/// Carregamento e troca das traduções (classe puramente estática).
class Translations final {
public:
    Translations() = delete;

    /// Códigos dos idiomas com tradução embutida (além do inglês original).
    [[nodiscard]] static QStringList availableLanguages();

    /// Localidade correspondente a um código ("pt_BR", "en"); vazio = sistema.
    [[nodiscard]] static QLocale localeFor(const QString& languageCode);

    /// Substitui as traduções atuais pelas do idioma informado e define a
    /// localidade padrão (QLocale::setDefault), usada também pela ajuda.
    /// Requer uma QCoreApplication existente. Devolve true se a tradução do
    /// Spritenator foi carregada (false para o inglês, o idioma original).
    static bool apply(const QLocale& locale);
};

} // namespace spritenator::gui
