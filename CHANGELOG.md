# Changelog

Todas as mudanças relevantes do projeto são documentadas aqui.

O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto adota o [Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [1.6.0] - 2026-10-13

### Adicionado
- **Seletor de cor circular** (`ColorWheel`), que aparece no lugar da paleta em imagens RGBA:
  - o anel escolhe o matiz;
  - o triângulo interno vai da cor pura ao branco e ao preto e gira com o matiz;
  - botão esquerdo define a primária e o direito a secundária, e a cor escolhida vira o mostrador ativo;
  - escolher uma cor para um destino totalmente transparente a torna opaca.
- Barra **A** (opacidade) no painel de cores, no modo RGBA.
- **Mostradores** de cor primária e secundária (`ColorSwatch`), com fundo xadrez para o alfa. Clicar num mostrador o torna **ativo**, e as barras editam o mostrador ativo (também no modo indexado, onde editam a entrada da paleta da cor ativa).
- Sincronização nos dois sentidos entre seletor, barras e mostradores.
- O manual explica os mostradores, o seletor e a barra A (nos dois idiomas).
- Testes da geometria do triângulo, dos cliques no seletor, da integração com a sessão e das barras.

## [1.5.0] - 2026-10-13

### Adicionado
- **Documentos RGBA** (32 bits, cores reais com transparência):
  - "RGBA" é a primeira opção, e o padrão, no diálogo "Nova imagem";
  - fundo xadrez atrás dos pixels transparentes no canvas, na área de desenho, na prévia e na animação;
  - cores primária e secundária RGBA; a secundária é transparente por padrão (botão direito apaga);
  - desenhar substitui o pixel pela cor exata, inclusive o alfa;
  - todas as ferramentas, transformações, sombra (mantém o alfa), texto, área de transferência (preserva o alfa) e animação funcionam em RGBA;
  - efeito de cor transparente: ignora pixels com alfa 0 ao gravar no canvas;
  - gravação somente em PNG (BMP não guarda transparência; o diálogo oferece apenas PNG);
  - "RGBA" aparece no título da janela do documento.
- Arquivos em cores reais (PNG/BMP sem paleta) abrem como RGBA, sem redução de cores.
- A área de desenho acompanha o modo do documento ativo e converte o conteúdo automaticamente (o histórico dela é reiniciado nessa troca). Gravar o buffer num documento de outro modo converte só a cópia gravada.
- `core::AnyImage`, `ColorRole` e `EditorSession::plot` por papel de cor.
- Testes de edição RGBA (buffer, ferramentas, transparência, conversões, operações e janela principal) e de leitura/gravação RGBA.

### Alterado
- O menu Paleta, a máscara e a grade da paleta ficam desabilitados/ocultos em documentos RGBA. Até a 1.6.0, as barras R, G e B editam a cor primária nesse modo.
- A área de transferência usa o formato privado `application/x-spritenator-image`, que guarda índices ou RGBA.

## [1.4.0] - 2026-10-13

### Adicionado
- Núcleo genérico pelo tipo de pixel, preparando as imagens RGBA:
  - `Image<Pixel>`, com `IndexedImage = Image<ColorIndex>` e `RgbaImage = Image<Rgba>`;
  - tipo `Rgba` (8 bits por canal, alfa 0 = transparente);
  - `WriteRulesFor<Pixel>`: em RGBA, o efeito transparente ignora pixels com alfa 0;
  - balde (`floodRegion`) e todas as transformações valem para os dois tipos, com instanciação explícita;
  - sombra RGBA, que mantém o alfa original.
- `core/ColorConversion`: indexada → RGBA (exata) e RGBA → indexada (cor mais próxima ou pontilhamento; pixels quase transparentes viram o índice 0).
- `core/Dithering`: Floyd–Steinberg e cor mais próxima genéricos, compartilhados por `PaletteOps` e pelas conversões.
- Testes de imagens RGBA e das conversões.

### Alterado
- `IndexedImage.h/.cpp` foram substituídos por `Image.h` (template, apenas cabeçalho). Nenhuma mudança visível na interface.

## [1.3.0] - 2026-10-13

### Adicionado
- Imagens de **2 cores (1 bit)**: opção no diálogo "Nova imagem", paleta padrão preto/branco, gravação de BMP de 1 bit e leitura de BMP/PNG de 2 cores.
- `core::bitsPerPixelOf` e o gravador de BMP genérico para 1, 4 e 8 bits por pixel.

### Corrigido
- O leitor de BMP do Qt invertia a ordem da paleta (e os índices) em imagens de 1 bit. O carregamento agora restaura a ordem de cores do arquivo, com teste de regressão.

## [1.2.1] - 2026-10-13

### Alterado
- Nova ordem do painel lateral, de cima para baixo: ferramentas e tons, ajuste das cores (amostras e R, G, B) e paleta (grade e botões), com um espaço separando os três painéis. O manual foi atualizado nos dois idiomas.

## [1.2.0] - 2026-10-13

### Adicionado
- Troca de idioma **sem reiniciar**: escolher um idioma em *Exibir > Idioma* atualiza na hora toda a interface:
  - menus, barra de ferramentas e Integrator (dicas de ferramentas, tons e transformações);
  - controles de cor, janelas de canvas ("SemTitulo1"), janela de animação e ajuda;
  - nomes das operações já registradas no histórico de desfazer.
- `UndoLabel`/`UndoOperation`: catálogo tipado dos nomes das operações de desfazer. Os comandos (`LabeledCommand`) guardam a operação e se retraduzem.
- Teste que troca o idioma com o programa aberto (inglês → português → inglês) e varre todos os widgets e ações visíveis, garantindo que nenhum texto do idioma anterior permanece.

### Alterado
- `ActionCatalog`: todas as ações passaram a ser descritas numa única tabela declarativa (texto, dica, ícone, atalho), reutilizada em `retranslate()`.
- `Translations::install()` virou `Translations::apply()`, que substitui os tradutores já instalados.
- Componentes persistentes definem seus textos em `retranslate()`, chamado na construção e em `changeEvent(QEvent::LanguageChange)`.

### Removido
- O aviso "o novo idioma será usado na próxima execução".

## [1.1.0] - 2026-10-13

### Adicionado
- Interface traduzida para **português do Brasil**: menus, dicas, diálogos, ferramentas, nomes das operações de desfazer e mensagens de erro (332 textos).
- Manual de ajuda em português (`manual_pt_BR.html`), escolhido automaticamente conforme o idioma.
- Traduções do próprio Qt carregadas para os diálogos padrão (botões, seletor de arquivos, nomes de teclas).
- Exibir > Idioma: padrão do sistema, inglês ou português (vale na próxima execução).
- Infraestrutura do Qt Linguist:
  - `qt_add_translations` com o alvo `update_translations` (lupdate);
  - os `.qm` são compilados e embutidos no executável.
- `gui/Translations`: carregamento dos tradutores e da localidade padrão.
- Teste `test_translation`:
  - tradução aplicada em todas as camadas;
  - `.ts` completo, com marcadores, aceleradores, quebras de linha e espaços preservados;
  - aceleradores únicos em cada menu;
  - manual em português.

### Alterado
- A versão mínima do Qt passou a ser 6.7 (API `qt_add_translations(TARGETS)`), e o módulo LinguistTools passou a ser necessário.
- Os textos da camada `io` agora usam `QCoreApplication::translate` com contexto explícito, para que o lupdate os encontre.

## [1.0.0] - 2026-10-13

Primeira versão estável. Reúne todas as funcionalidades do Character Maker 1999
planejadas para a recriação (exceto o controle de licença shareware, os
plugins Susie e os hooks específicos do Windows, propositalmente omitidos).

### Adicionado
- Empacotamento:
  - regras de instalação (executável, atalho `.desktop` e ícone no Linux);
  - CPack: `.tar.gz`/`.deb` no Linux, `.zip` no Windows e `.dmg` no macOS;
  - implantação das bibliotecas do Qt no Windows e no macOS (`qt_generate_deploy_app_script`).
- Ícone e informações de versão no executável do Windows (`spritenator.rc`).
- Integração contínua (GitHub Actions) em Linux, Windows e macOS, com avisos tratados como erros, testes e pacotes como artefatos.
- `docs/ARCHITECTURE.md` (camadas, padrões de projeto, fluxo de dados e pontos de extensão) e README completo.
- O script de ícones também gera `resources/app_icon.ico`.

### Corrigido
- Abrir uma imagem maior que 8192 px encerrava a aplicação; agora mostra uma mensagem de erro.
- Fechar a aplicação depois de gravar uma célula no canvas podia chamar código de uma janela já destruída (abort em builds de depuração).

### Verificado
- Compilação sem avisos com GCC e Clang (`-Werror`).
- 16 suítes de teste passando, inclusive com AddressSanitizer e UndefinedBehaviorSanitizer.

## [0.10.0] - 2026-10-13

### Adicionado
- Arquivo > Pasta de backup: antes de sobrescrever um arquivo, a versão anterior é copiada para a pasta escolhida, com data e hora no nome. Uma falha no backup pergunta se a gravação deve continuar.
- `io/Backup`: nomes de backup com carimbo de tempo, sem sobrescrever backups do mesmo segundo.
- Ajuda (F1): manual embutido com ícones e uma tabela de atalhos gerada a partir das ações reais, que por isso nunca fica desatualizada.
- As preferências de exibição (zoom, grades, efeito transparente, lado do Integrator e divisão da tela) agora são restauradas ao abrir a aplicação.
- Testes das configurações, do backup e do conteúdo da ajuda.

## [0.9.0] - 2026-10-13

### Adicionado
- Canvas > Tamanho do canvas (Ctrl+E): novo tamanho, com a opção de redimensionar o conteúdo. Sem ela, o conteúdo fica ancorado no canto e as áreas novas recebem a cor de fundo.
- Canvas > Limpar tudo (Ctrl+Shift+N): preenche o canvas com a cor de fundo.
- Janela de animação (F2):
  - lista de quadros e tempo de espera de 50 a 1000 ms;
  - zoom de 1x a 4x, reprodução em laço e pausa;
  - prévia ao vivo: edições no canvas aparecem na hora;
  - "Add Cell" adiciona a última célula trazida para a área de desenho;
  - "Walk Cycle" gera a sequência 1-2-3-2 do RPG Maker.
- `core/Animation`: modelo de sequência de quadros e gerador do ciclo de caminhada.
- Testes do modelo de animação, de Limpar tudo (com desfazer) e da janela de animação.

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
