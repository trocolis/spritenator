# Changelog

Todas as mudanças relevantes do projeto são documentadas aqui.

O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto adota o [Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [0.1.0] - 2026-10-13

### Adicionado
- Estrutura do projeto em CMake com C++20 e Qt 6, separada em camadas (`core`, `io`, `gui`, `app`).
- Cabeçalho `spritenator/Version.h` gerado a partir de `project(VERSION ...)`.
- Janela principal com área MDI, menus e barra de ferramentas iguais aos do original.
- `ActionCatalog`, que concentra todas as ações com os atalhos do Character Maker 1999.
- `IconProvider` e ícones provisórios extraídos do original (`tools/extract_icons.py`).
- Diálogo "Sobre" com a versão.
- Testes com Qt Test/CTest: integridade das ações, atalhos únicos, ícones e formato da versão.
- README, `.clang-format` e `.editorconfig`.
