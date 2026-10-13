# Spritenator

Editor de sprites com cores indexadas, multiplataforma (Linux, Windows e
macOS), escrito em **C++20** com **Qt 6**. É uma recriação do *Character Maker
1999* (Yoji Ojima), um editor clássico de *charsets* e *chipsets* para o RPG
Maker.

> Histórico de versões: veja o [CHANGELOG](CHANGELOG.md).

## Funcionalidades

- Imagens indexadas de **16 ou 256 cores**, em PNG e BMP. BMPs de 16 cores são gravados em 4 bits.
- Fluxo do original: o canvas é dividido em **células de personagem**:
  - o botão direito traz a célula para a **área de desenho** ampliada;
  - o esquerdo grava a área de desenho de volta;
  - as setas navegam entre os quadros.
- **12 ferramentas**: seleção, lupa, balde, lápis fino/grosso, reta, retângulo, retângulo arredondado e elipse (contorno ou preenchidos).
- **8 tons de retícula** (A–H), copiados pixel a pixel do original, com deslocamento de fase.
- **12 transformações**: deslocar, espelhar, simetria, girar 90°, rotação livre e esticar.
- **Paleta**:
  - edição RGB, gradação e máscara de cores protegidas (manual ou automática);
  - efeito de cor transparente;
  - editor de paleta (troca de cores, remoção de cores não usadas, 15 bits);
  - carga de paletas (`.pal` RIFF/JASC, `.act`, imagens), com remapeamento por cor mais próxima ou pontilhamento Floyd–Steinberg.
- **Janela de animação**, com prévia ao vivo e gerador do ciclo de caminhada 1-2-3-2.
- Desfazer/refazer ilimitado e unificado (área de desenho e canvas), área de transferência e entrada de texto sem suavização.
- Várias imagens abertas (MDI), arquivos recentes, arrastar e soltar, backup automático ao salvar e ajuda embutida (F1).

## Compilação

Requisitos:

- CMake ≥ 3.21
- Compilador com suporte a C++20 (GCC ≥ 11, Clang ≥ 14 ou MSVC 2022)
- Qt ≥ 6.7 (módulos Core, Gui, Widgets, Test e LinguistTools; no Arch Linux, `qt6-base` e `qt6-tools`)
- Opcional: as traduções do próprio Qt (`qt6-translations`), para os diálogos padrão em português

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/app/spritenator
```

Instalação e pacotes:

```sh
cmake --install build --prefix ~/.local     # executável, atalho .desktop e ícone
cd build && cpack                            # .tar.gz/.deb (Linux), .zip (Windows), .dmg (macOS)
```

Opções do CMake:

| Opção | Padrão | Descrição |
|---|---|---|
| `SPRITENATOR_BUILD_TESTS` | `ON` | Compila os testes automatizados |
| `SPRITENATOR_WARNINGS_AS_ERRORS` | `OFF` | Trata avisos do compilador como erros (ligado na CI) |

## Estrutura do projeto

```
src/
  core/   modelo de dados e algoritmos em C++ puro (sem Qt)
  io/     leitura e gravação de imagens, paletas e backups
  gui/    janelas, diálogos, widgets, ferramentas e comandos de desfazer
  app/    ponto de entrada (main) e recursos do executável
resources/
  icons/  ícones (PNG) embutidos no executável
  help/   manual do usuário (HTML) embutido, em inglês e português
  linux/  atalho .desktop
tests/    testes automatizados (Qt Test + CTest), por camada
translations/ traduções da interface (.ts do Qt Linguist)
tools/    scripts auxiliares (ex.: extração de ícones)
docs/     documentação técnica (ver ARCHITECTURE.md)
original/ artefatos extraídos da aplicação original, usados como referência
```

A arquitetura, os padrões de projeto e o fluxo de dados estão descritos em
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Convenções

- Identificadores (classes, funções e variáveis) em **inglês**; comentários em **português**.
- Classes em `PascalCase`, funções e variáveis em `camelCase`, membros com prefixo `m_`, constantes com prefixo `k`.
- Formatação definida em [`.clang-format`](.clang-format).
- Versionamento semântico `X.Y.Z`. A versão é definida **apenas** em `project(VERSION ...)` no `CMakeLists.txt` raiz e gerada em `spritenator/Version.h`. Cada versão recebe uma tag git `vX.Y.Z`.
- Mensagens de commit no padrão *Conventional Commits* (`feat:`, `fix:`, `docs:`, `chore:`...).
- Textos da interface escritos em inglês no código, sempre via `tr()`/`QCoreApplication::translate()`. As traduções ficam em `translations/` (veja abaixo).

## Traduções

A interface está disponível em **português do Brasil** e em inglês, o idioma
original. Por padrão o programa segue o idioma do sistema; para mudar, use
*Exibir > Idioma*. A troca vale na hora, sem reiniciar: menus, painéis, janelas
abertas, ajuda e até os nomes já registrados no histórico de desfazer mudam de
idioma.

- As traduções ficam em `translations/spritenator_<idioma>.ts` e podem ser editadas no Qt Linguist ou num editor de texto.
- A cada build, o `lrelease` compila os `.ts` em `.qm`, que vão embutidos no executável.
- Depois de alterar textos no código, rode `cmake --build build --target update_translations` para atualizar os `.ts`.
- Para um novo idioma, acrescente o `.ts` em `TS_FILES` (`src/CMakeLists.txt`) e, se quiser, o manual `resources/help/manual_<idioma>.html`.
- Textos de widgets que ficam abertos são definidos num método `retranslate()`, chamado na construção e no `changeEvent(QEvent::LanguageChange)`. Os comandos de desfazer guardam um `UndoLabel` (a operação), e não o texto pronto.
- O teste `test_translation` verifica se as traduções estão completas e se preservam os marcadores (`%1`, `%n`), os aceleradores (`&`) e as quebras de linha. Ele também garante que nenhum menu traduzido tem letras de atalho repetidas. Além disso, troca o idioma com o programa aberto e confere que nenhum texto do idioma anterior continua visível.

## Ícones

Os ícones atuais foram extraídos do programa original por
[`tools/extract_icons.py`](tools/extract_icons.py) e são **provisórios**. Para
substituí-los, sobrescreva os arquivos em `resources/icons/` mantendo os
mesmos nomes:

- `<nome>.png`: imagem normal;
- `<nome>_disabled.png`: imagem do estado desabilitado (opcional).

O código acessa os ícones apenas pelo nome lógico, por meio de
`gui/IconProvider`. Por isso não é preciso alterar nenhum código ou CMake. O
ícone do executável do Windows fica em `resources/app_icon.ico`.

## Créditos

*Character Maker 1999*: Copyright © 1998-2000 Yoji Ojima. Este projeto é uma
recriação independente, para fins de estudo e preservação.
