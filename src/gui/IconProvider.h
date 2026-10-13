// =============================================================================
// IconProvider.h
//
// Ponto ÚNICO de acesso aos ícones da aplicação. Todo o código da GUI pede
// ícones por NOME LÓGICO (ex.: "file_open"), nunca por caminho de arquivo.
//
// Os ícones atuais foram extraídos do Character Maker 1999 (ver
// tools/extract_icons.py) e são provisórios. Para trocá-los por arte própria
// basta substituir os PNGs em resources/icons mantendo os mesmos nomes:
//   - <nome>.png           -> imagem normal (obrigatória)
//   - <nome>_disabled.png  -> imagem para o estado desabilitado (opcional;
//                             se ausente, o Qt gera uma versão acinzentada)
// =============================================================================
#pragma once

#include <QIcon>
#include <QString>

namespace spritenator::gui {

/// Fornece ícones a partir dos recursos embutidos (prefixo ":/icons").
class IconProvider final {
public:
    IconProvider() = delete; // classe puramente estática

    /// Devolve o ícone com o nome lógico informado. Se não existir, devolve
    /// um QIcon nulo (o botão/menu simplesmente fica sem imagem).
    [[nodiscard]] static QIcon icon(const QString& name);

    /// Caminho do recurso correspondente a um nome lógico (útil em testes).
    [[nodiscard]] static QString resourcePath(const QString& name);
};

} // namespace spritenator::gui
