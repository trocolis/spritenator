# Changelog

Todas as mudanças relevantes do projeto são documentadas aqui.

O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto adota o [Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [0.8.0] - 2026-10-13

### Adicionado
- Máscara de cores protegidas: pixels dessas cores não são alterados pelo desenho nem pela gravação no canvas.
  - Modo máscara (F5): esquerdo marca a cor, direito inverte e duplo clique direito limpa.
  - Limpar (F6), Inverter (F7) e Máscara automática (F9), que protege tudo exceto a cor de fundo e restaura a máscara anterior ao desligar.
  - As cores protegidas aparecem marcadas com um "X".
- Gradação (Ctrl+D): interpola a paleta entre a cor de desenho e a de fundo.
- Editor de paleta (Shift+E), com uma intervenção no histórico ao confirmar:
  - editar cor, copiar e colar cores;
  - trocar duas cores sem alterar a aparência da imagem;
  - gradiente, remoção de cores não usadas (com remapeamento), cores de 15 bits e restaurar paleta.
- Carregar paleta (Shift+L), com prévia das cores lidas e da paleta resultante:
  - leitura total ou por intervalo ("N cores a partir de X na posição Y");
  - pixels: manter índices, cor mais próxima ou difusão de erro (Floyd–Steinberg);
  - opções de evitar as 20 cores do sistema e tratar o índice 0 como transparente;
  - o buffer da área de desenho é convertido junto com o canvas.
- Salvar paleta (Shift+S) em RIFF `.pal`, JASC `.pal` ou `.act`.
- `core/PaletteOps`: remapeamento, compactação, troca, mesclagem e pontilhamento.
- `PaletteGrid`, uma grade de paleta reutilizável nos diálogos.
- Testes de integração da janela principal, que disparam as ações reais dos menus, e testes de `PaletteOps`.

### Corrigido
- As ações de Recortar, Copiar, Colar, Limpar, Selecionar tudo, Preparar sombra e Entrada de texto não estavam ligadas aos menus na 0.7.0.
- Um documento recém-aberto agora sempre se torna o documento ativo, independentemente do foco da janela.

## [0.7.0] - 2026-10-13

### Adicionado
- Editar: Recortar, Copiar, Colar, Limpar e Selecionar tudo na área de desenho.
  - Dentro do Spritenator o formato privado preserva os índices de cor; para outros programas é publicada uma imagem comum.
  - Imagens coladas de fora são convertidas para a cor mais próxima da paleta.
- Barra com as 12 transformações do original, aplicadas à seleção ou à área inteira:
  - deslocamento circular nas 4 direções;
  - espelhamentos e simetrias horizontal e vertical;
  - rotação de 90° nos dois sentidos e rotação livre;
  - esticar/encolher, com opção de manter a proporção.
- "Prepare Shadow" (Ctrl+Q): tudo que não é cor de fundo vira a cor de desenho.
- "Text Input" (Ctrl+T): fonte, tamanho e estilos, com prévia ampliada. O texto é rasterizado sem suavização e respeita tom e máscara.
- `core/Transform`: transformações puras com mapeamento inverso (sem buracos).
- Testes das transformações e das operações de edição (área de transferência, seleção, esticar, sombra e texto).

## [0.6.0] - 2026-10-13

### Adicionado
- Todas as 12 ferramentas do original no painel de ferramentas:
  - seleção retangular e lupa (esquerdo amplia, direito reduz);
  - balde de tinta e lápis fino/grosso;
  - reta, retângulo, retângulo arredondado e elipse, em contorno ou preenchidos, com efeito elástico durante o arraste.
- Tons de retícula A–H (`core/Tone`), copiados pixel a pixel dos ícones originais, e o botão de deslocamento de tom (16 fases). Os tons se aplicam a todas as ferramentas de desenho.
- Núcleo de rasterização:
  - retângulo;
  - elipse pelo algoritmo de Zingl (diâmetros pares e ímpares, elipses achatadas);
  - retângulo arredondado;
  - preenchimento por inundação em varredura de linhas (sem recursão).
- Testes de figuras, simetria da elipse, preenchimento, cobertura e deslocamento dos tons, efeito elástico, balde com tom e seleção.

## [0.5.0] - 2026-10-13

### Adicionado
- Painel Integrator ao lado da área MDI (`Janela > Reverse Canvas` troca os lados), com:
  - área de desenho ampliada (zoom 1x a 12x), grade de pixels, sub-grade e contorno das células;
  - prévia em tamanho real (`OrgAreaView`);
  - paleta clicável (esquerdo = cor de desenho, direito = cor de fundo) com dicas de índice e RGB;
  - amostras das cores e controles R/G/B que editam a paleta; arrastar um controle gera um único passo de desfazer.
- `DrawBuffer`: buffer da área de desenho compartilhado entre documentos, com edição ao vivo e commit no histórico.
- `EditorSession`: estado central do editor (cores, zoom, grades, grade de personagens, ferramenta, regras de escrita).
- Transferência entre canvas e área de desenho:
  - botão direito na célula traz o quadro e o esquerdo grava o quadro;
  - as setas navegam entre as células;
  - F3 transfere o canvas inteiro.
- "Transparent Color Effect" (F8): a cor de fundo não é gravada no canvas.
- Diálogo "Character Size" (célula, sub-grade e opções), persistido nas configurações.
- Desfazer/refazer unificado (`QUndoGroup`): desfaz sempre a última edição, seja do buffer ou do canvas, e o menu mostra o nome da operação.
- Interface `ImageTarget` e comandos `RegionCommand`, `ReplaceImageCommand`, `PaletteCommand` (com fusão) e `DocumentStateCommand`.
- Sistema de ferramentas (padrão Strategy) com lápis fino e grosso; Esc cancela o traço.
- Reta de Bresenham no núcleo (`core/Raster`).
- Testes da sessão (get/put, transparência, traço, cancelamento, undo, zoom) e da rasterização.

## [0.4.0] - 2026-10-13

### Adicionado
- `Document`, que reúne canvas, paleta, caminho e histórico próprio (`QUndoStack`), com cache de exibição atualizado só nas regiões alteradas.
- Janelas de canvas MDI (`CanvasWindow`/`CanvasView`), com ampliação sem suavização e cursor de célula do tamanho do personagem.
- Arquivo: Novo (com predefinições de tamanho do RPG Maker e cópia da paleta atual), Abrir (vários arquivos ou arrastar e soltar), Salvar, Salvar como, Reverter e Fechar.
- Confirmação ao fechar documentos modificados e ao sair da aplicação.
- Lista com os 4 arquivos recentes.
- Janela: Cascata, Empilhar, Lado a lado e Ajustar ao canvas.
- `AppSettings`, que dá acesso tipado ao `QSettings`; geometria da janela persistente.
- Posição do mouse no canvas exibida na barra de status.
- Testes do ciclo de vida do documento (modificação, desfazer, salvar, reverter).

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
