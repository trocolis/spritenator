# Spritenator

Editor de sprites (cores indexadas ou RGBA), multiplataforma (Linux, Windows e
macOS), escrito em **C++20** com **Qt 6**. É levemente inspirado no *Character
Maker 1999* (Yoji Ojima), um editor clássico de *charsets* e *chipsets* para o
RPG Maker: mantém as ferramentas, os tons, as transformações e a paleta do
original, mas edita a imagem inteira diretamente, com zoom livre.

> Histórico de versões: veja o [CHANGELOG](CHANGELOG.md).

## Funcionalidades

- Imagens **RGBA** (32 bits, cores reais com transparência), salvas em PNG e exibidas sobre um fundo xadrez, com **seletor de cor circular** (anel de matizes + triângulo HSV) e barra de opacidade.
- Imagens indexadas de **2, 16 ou 256 cores**, em PNG e BMP. Os BMPs são gravados em 1, 4 ou 8 bits, conforme o número de cores.
- **Edição direta** da imagem inteira, uma aba por documento:
  - zoom de 12,5% a 3200% pela roda do mouse (ancorado no ponteiro), pela lupa, pelo menu *Exibir* ou digitando o valor em % na barra de status;
  - Ctrl+roda rola a imagem (Ctrl+Shift+roda na horizontal) e o botão do meio arrasta a vista;
  - **destaque** dos pixels que a ferramenta atual afetaria, sob o mouse (inclusive a região do balde), respeitando o tom e a máscara.
- **Grades**: a principal tem padrão 1×1 (a grade de pixels), é configurável e some quando ficaria densa demais; a sub-grade tem o "tamanho do personagem" (ex.: 24×32).
- **14 ferramentas**: seleções retangular, oval, ponto a ponto, livre (laço) e varinha mágica (com tolerância), balde, lápis (ponta quadrada ou redonda, de 1 a 64 px), reta, retângulo, retângulo arredondado e elipse (contorno ou preenchidos).
- **Painel lateral em seções** (ferramentas e suas opções, preenchimento, transformações e cores), separadas por linhas, que quebram linha conforme a largura.
- A ponta do lápis vale também para a reta e as figuras.
- **Tema** das janelas: o do sistema, claro ou escuro (*Exibir > Tema*), com os ícones ajustados ao fundo escuro.
- **Seleções de forma livre**: Shift soma e Ctrl subtrai; a pintura fica restrita à seleção; transformação livre com alças (mover, escalar, girar), setas do teclado e atalhos (R, E, Del...) configuráveis em *Editar > Atalhos de teclado*.
- **8 tons de retícula** (A–H), copiados pixel a pixel do original, com deslocamento de fase.
- **12 transformações**: deslocar, espelhar, simetria, girar 90°, rotação livre e esticar.
- **Paleta**:
  - edição RGB, gradação e máscara de cores protegidas (manual ou automática);
  - efeito de cor transparente;
  - editor de paleta (troca de cores, remoção de cores não usadas, 15 bits);
  - carga de paletas (`.pal` RIFF/JASC, `.act`, imagens), com remapeamento por cor mais próxima ou pontilhamento Floyd–Steinberg.
- **Linha do tempo com cenas e camadas** (como no Aseprite): miniaturas das cenas, duração de cada uma, reprodução no próprio editor, papel de seda (*onion skin*) e camadas comuns a todas as cenas, com visibilidade, opacidade e mesclagem.
- **Salvar** como projeto (`.sprn`, sem perdas), **GIF animado**, **sprite sheet** ou **imagens separadas**; abrir GIFs animados importa as cenas.
- Desfazer/refazer ilimitado por documento, área de transferência (com o efeito de cor transparente ao colar) e entrada de texto sem suavização.
- Várias imagens abertas em abas, arquivos recentes, arrastar e soltar, backup automático ao salvar e ajuda embutida (F1).

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
original/ aplicação original e artefatos extraídos dela (apenas locais, fora do git)
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

Os ícones são pixel art própria, descrita como texto em
[`tools/make_icons.py`](tools/make_icons.py): cada ícone é um mapa ASCII (um
caractere por pixel, numa paleta única) ou uma figura gerada por código. Os
tons são desenhados a partir dos próprios padrões de `src/core/Tone.h`. Para
mudar um ícone, edite o script e rode:

```sh
python3 tools/make_icons.py                      # requer Pillow
python3 tools/make_icons.py --preview prancha.png  # + prancha ampliada para revisão
```

O script gera:

- `resources/icons/<nome>.png` e `<nome>_disabled.png` (variante desabilitada automática);
- `resources/app_icon.ico` (Windows), `resources/app_icon.icns` (macOS) e `resources/linux/icons/<tamanho>.png` (Linux).

O código acessa os ícones apenas pelo nome lógico, por meio de
`gui/IconProvider`, então não é preciso alterar nenhum código ou CMake.

## Créditos

*Character Maker 1999*: Copyright © 1998-2000 Yoji Ojima. O Spritenator é um
projeto independente, inspirado nele; nenhum recurso do programa original
(ícones, textos ou imagens) faz parte deste repositório.
