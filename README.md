# Spritenator

Editor de sprites com cores indexadas, multiplataforma, escrito em **C++20**
com **Qt 6**. É uma recriação do *Character Maker 1999* (Yoji Ojima), um
editor clássico de *charsets* e *chipsets* para o RPG Maker.

> Histórico de versões: veja o [CHANGELOG](CHANGELOG.md).

## Compilação

Requisitos:

- CMake ≥ 3.21
- Compilador com suporte a C++20 (GCC ≥ 11, Clang ≥ 14 ou MSVC 2022)
- Qt ≥ 6.2 (módulos Core, Gui, Widgets e Test)

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/app/spritenator
```

Opções do CMake:

| Opção | Padrão | Descrição |
|---|---|---|
| `SPRITENATOR_BUILD_TESTS` | `ON` | Compila os testes automatizados |
| `SPRITENATOR_WARNINGS_AS_ERRORS` | `OFF` | Trata avisos do compilador como erros |

## Estrutura do projeto

```
src/
  core/   modelo de dados e algoritmos em C++ puro (sem Qt)
  io/     leitura e gravação de imagens e paletas
  gui/    janelas, diálogos e widgets (Qt Widgets)
  app/    ponto de entrada (main)
resources/
  icons/  ícones (PNG) embutidos no executável
tests/    testes automatizados (Qt Test + CTest)
tools/    scripts auxiliares (ex.: extração de ícones)
original/ artefatos extraídos da aplicação original, usados como referência
```

## Convenções

- Identificadores (classes, funções e variáveis) em **inglês**; comentários em **português**.
- Classes em `PascalCase`, funções e variáveis em `camelCase`, membros com prefixo `m_`, constantes com prefixo `k`.
- Formatação definida em [`.clang-format`](.clang-format).
- Versionamento semântico `X.Y.Z`. A versão é definida **apenas** em `project(VERSION ...)` no `CMakeLists.txt` raiz e gerada em `spritenator/Version.h`. Cada versão recebe uma tag git `vX.Y.Z`.
- Mensagens de commit no padrão *Conventional Commits* (`feat:`, `fix:`, `docs:`, `chore:`...).

## Ícones

Os ícones atuais foram extraídos do programa original por
[`tools/extract_icons.py`](tools/extract_icons.py) e são **provisórios**. Para
substituí-los, sobrescreva os arquivos em `resources/icons/` mantendo os
mesmos nomes:

- `<nome>.png`: imagem normal;
- `<nome>_disabled.png`: imagem do estado desabilitado (opcional).

O código acessa os ícones apenas pelo nome lógico, por meio de
`gui/IconProvider`. Por isso não é preciso alterar nenhum código ou CMake.

## Créditos

*Character Maker 1999*: Copyright © 1998-2000 Yoji Ojima. Este projeto é uma
recriação independente, para fins de estudo e preservação.
