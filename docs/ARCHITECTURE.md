# Arquitetura do Spritenator

Este documento descreve como o código está organizado e por quê. Ele é voltado
a quem vai manter ou estender o projeto.

## 1. Camadas

```
┌────────────────────────────────────────────────────────────┐
│ app   main.cpp — cria QApplication e MainWindow            │
├────────────────────────────────────────────────────────────┤
│ gui   Qt Widgets: janelas, diálogos, widgets, ferramentas, │
│       comandos de desfazer, estado do editor               │
├────────────────────────────────────────────────────────────┤
│ io    arquivos: imagens (BMP/PNG), paletas, backups        │
├────────────────────────────────────────────────────────────┤
│ core  C++20 puro: imagem indexada, paleta, rasterização,   │
│       transformações, tons, operações de paleta, animação  │
└────────────────────────────────────────────────────────────┘
```

Cada camada depende apenas das camadas abaixo dela.

- O **core não depende do Qt**. Todos os algoritmos (elipse, preenchimento, pontilhamento, rotação...) são funções puras, fáceis de testar e de reutilizar.
- A camada **io** usa o `QImage` apenas para decodificar PNG/BMP. O codificador de BMP de 4 bits e os formatos de paleta são próprios.
- A **gui** nunca manipula pixels diretamente. Ela chama o core e registra o resultado como um comando de desfazer.

## 2. Modelo de dados

| Classe | Papel |
|---|---|
| `core::Image<Pixel>` | Matriz 2D genérica de pixels. `IndexedImage = Image<ColorIndex>` (1 byte, índice da paleta) e `RgbaImage = Image<Rgba>` (4 bytes, cor real com alfa). Algoritmos e transformações são escritos uma vez só para os dois tipos. |
| `core::Palette` | 16 ou 256 cores RGB. |
| `core::WriteRulesFor<Pixel>` | Regras de escrita. Indexada: cor transparente na origem e máscara de cores protegidas no destino. RGBA: ignora pixels de origem com alfa 0. |
| `core::AnyImage` | `std::variant<IndexedImage, RgbaImage>` + `ColorMode`: o tipo usado pelo documento e pelos comandos de desfazer. O compilador obriga a tratar os dois modos (`std::visit`). |
| `core::ColorConversion` | Conversões indexada → RGBA (exata) e RGBA → indexada (cor mais próxima ou Floyd–Steinberg; quase transparente → índice 0). |
| `core::GridLayout` | Espaçamentos da grade principal (padrão 1×1) e da sub-grade ("tamanho do personagem", padrão 24×32); célula da sub-grade sob um ponto. |
| `gui::Document` | Canvas + paleta + caminho + `QUndoStack` próprio + seleção + edição ao vivo. |
| `gui::CanvasEditor` | Editor de um documento (uma aba): exibe a imagem inteira com zoom próprio, grades, seleção e destaque; repassa o mouse à ferramenta atual. É dono do `Document`. |
| `gui::ColorRole` | Papel da cor (primária/secundária). As ferramentas pedem um papel e a `EditorSession::plot` decide o pixel conforme o modo. |
| `gui::EditorSession` | Estado central: documento ativo, cores, grades, ferramenta, tom, máscara e a célula do último clique. O zoom fica em cada `CanvasEditor`. |

### Fluxo principal

Até a 1.x o Spritenator reproduzia o fluxo do Character Maker 1999 (trazer uma
célula do canvas para uma área de desenho ampliada e gravá-la de volta). A
partir da 2.0 a imagem inteira é editada diretamente:

```
 mouse ──► CanvasEditor ──(ponto da imagem)──► Tool (Strategy) ──► Document
   │        zoom, rolagem,                      press/move/release   beginEdit/editLive/
   │        lupa, Esc                                                commitEdit (undo)
   └──(sem botão)──► Tool::footprint ∩ EditorSession::canPlot ──► destaque
```

- O `CanvasEditor` converte posições do viewport em pixels da imagem (`imagePointAt`) e trata sozinho o que é só de exibição: zoom (roda, lupa), rolagem (Ctrl+roda, botão do meio) e a pintura de fundo xadrez, grades, seleção e destaque.
- `Tool::footprint` diz quais pixels a ferramenta afetaria num ponto (lápis grosso: o "+"; balde: a região, até 262144 pixels). O editor filtra o resultado por `EditorSession::canPlot` (imagem, tom e máscara) e o desenha em destaque.
- O clique registra, na sessão, a célula da sub-grade (`setLastClickedPoint`), usada pela janela de animação.

## 3. Padrões de projeto

- **Command** (`QUndoCommand`, em `gui/commands`): toda alteração passa por um comando.
  - `RegionCommand` guarda apenas o retângulo afetado (antes/depois).
  - `PaletteCommand` funde ajustes seguidos da mesma cor (`mergeWith`), de modo que arrastar um controle deslizante gera um único passo.
- **Strategy** (`gui/tools`): o `CanvasEditor` só converte coordenadas e repassa os eventos à ferramenta atual, que edita o documento ativo da sessão. Para criar uma ferramenta nova, basta implementar `Tool` (e, se preciso, `footprint`) e registrá-la na `EditorSession`.
- **Observer** (sinais e slots): as views observam a `EditorSession` e o `Document`, e nenhuma delas guarda cópia do estado.
- **Interface mínima** (`ImageTarget`): os comandos de desfazer dependem só da interface de edição de imagem que o `Document` implementa.

### Edição ao vivo

As ferramentas alteram o documento a cada movimento do mouse, para dar retorno
imediato, mas registram **um único** passo de desfazer ao soltar o botão:

```
beginEdit()   -> guarda uma cópia (snapshot)
editLive(f)   -> aplica f e acumula a área alterada
restoreSnapshot() -> (figuras) volta ao início antes de redesenhar
commitEdit()  -> RegionCommand(antes, depois) com "já aplicado"
```

### Desfazer por documento

Um `QUndoGroup` reúne a pilha de cada documento; a pilha ativa é a do
documento da aba ativa. Trocar de aba no meio de um traço cancela o traço
(`Document::cancelEdit`), para que nada fique pela metade.

### Troca de idioma em tempo de execução

`Translations::apply()` substitui os tradutores instalados. O Qt então envia
`QEvent::LanguageChange` a cada janela, e o evento se propaga aos filhos.

- Cada componente que permanece aberto (janela principal, painel lateral, janela de animação, ajuda e editores de imagem) reaplica seus textos em `retranslate()`.
- Diálogos modais não precisam disso: enquanto um deles está aberto, não há como trocar o idioma.
- O histórico de desfazer também é retraduzido, porque cada comando guarda um `UndoLabel` (a operação), e não o texto já traduzido.

## 4. Pontos de extensão

| Quero... | Onde |
|---|---|
| Nova ferramenta | Implementar `gui::Tool`, registrar em `EditorSession` e adicionar o botão em `ToolPanel`. |
| Nova transformação | Função pura em `core/Transform` e entrada em `EditOperations::transform`. |
| Novo formato de paleta | `io/PaletteFile` (`parse`/`encode`). |
| Novo comando de menu | `ActionId` + `ActionCatalog` + conexão na `MainWindow`. |
| Trocar ícones | Substituir os PNGs em `resources/icons` (mesmos nomes). |
| Novo idioma | `translations/spritenator_<idioma>.ts` em `TS_FILES` (`src/CMakeLists.txt`), `update_translations`, traduzir; opcionalmente `resources/help/manual_<idioma>.html`. |
| Novo texto na interface | Sempre via `tr()` (ou `QCoreApplication::translate` com contexto literal fora de QObject). Em widgets que ficam abertos, defina-o em `retranslate()`. Depois rode `update_translations`. |
| Nova operação desfazível | Acrescente-a a `UndoOperation` e a `kSources` (`commands/UndoLabel.cpp`); o `static_assert` impede esquecer o nome. |

## 5. Testes

Os testes ficam em `tests/<camada>`, um executável Qt Test por assunto,
registrados no CTest:

- **core**: geometria, paleta, imagem, rasterização, transformações, operações de paleta e animação;
- **io**: ida e volta de todos os formatos, entradas inválidas e backups;
- **gui**: documento, sessão (ferramentas, pegadas, regras, desfazer), editor de imagem (zoom, roda do mouse, lupa, edição com o mouse, destaque, grades), operações de edição, configurações, ajuda e testes de **integração** da `MainWindow`, que disparam as ações reais dos menus.

Os testes de GUI rodam com `QT_QPA_PLATFORM=offscreen`, sem servidor gráfico.
