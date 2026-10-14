// =============================================================================
// IconProvider.h
//
// Ponto ÚNICO de acesso aos ícones da aplicação. Todo o código da GUI pede
// ícones por NOME LÓGICO (ex.: "file_open"), nunca por caminho de arquivo.
//
// Os ícones são pixel art própria, gerada por tools/make_icons.py (cada ícone
// é descrito ali como texto). Os PNGs ficam em resources/icons:
//   - <nome>.png           -> imagem normal (obrigatória)
//   - <nome>_disabled.png  -> imagem para o estado desabilitado (opcional;
//                             se ausente, o Qt gera uma versão acinzentada)
//
// Os ícones são desenhados por um QIconEngine próprio, que:
//   - amplia por fatores inteiros, sem suavização (pixel art nítida em HiDPI);
//   - no tema escuro, troca a cor do contorno (quase preta) por um tom claro,
//     para que os ícones não sumam no fundo escuro. A decisão é tomada na
//     hora de desenhar, então a troca de tema vale sem recriar os ícones.
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

    /// Cor do contorno dos ícones e a cor que a substitui no tema escuro.
    [[nodiscard]] static QColor outlineColor();
    [[nodiscard]] static QColor darkThemeOutlineColor();
};

} // namespace spritenator::gui
