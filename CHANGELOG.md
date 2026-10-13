# Changelog

Todas as mudanças relevantes do projeto são documentadas aqui.

O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto adota o [Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [0.3.0] - 2026-10-13

### Adicionado
- Biblioteca `spritenator_io` para leitura e gravação de arquivos.
- `ImageFile`: leitura de BMP/PNG indexados, em tons de cinza ou em cores reais (paleta exata até 256 cores; acima disso, redução com pontilhamento).
- Gravação de PNG indexado e de BMP indexado com codificador próprio (4 bits para 16 cores, 8 bits para 256 cores), sempre com gravação atômica (`QSaveFile`).
- `PaletteFile`: paletas Microsoft RIFF `.pal`, JASC-PAL `.pal` e Adobe `.act` (leitura e gravação), e leitura da paleta de imagens BMP/PNG.
- `IoError`, a exceção com mensagem pronta para o usuário.
- Testes de ida e volta para todos os formatos e testes de erro.

## [0.2.0] - 2026-10-13

### Adicionado
- Biblioteca `spritenator_core` em C++ puro, sem dependência do Qt.
- `Point`, `Size` e `Rect` (retângulos semiabertos), com interseção, união e criação a partir de cantos.
- `Rgb`, com empacotamento, quantização para 15 bits e distância perceptual entre cores.
- `Palette` de 16 ou 256 cores: paleta padrão, cor mais próxima, conversão para 15 bits e gradação.
- `IndexedImage`, com cópia e colagem recortadas, redimensionamento do canvas, cores usadas e remapeamento.
- `WriteRules`, que implementa a cor transparente da origem e a máscara de cores protegidas do destino.
- `CharacterLayout`, com a grade de células de personagem, meia célula e sub-grade.
- Testes unitários do núcleo.

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
