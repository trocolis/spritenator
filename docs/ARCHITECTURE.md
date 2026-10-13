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
| `core::IndexedImage` | Matriz de índices de cor (1 byte por pixel). |
| `core::Palette` | 16 ou 256 cores RGB. |
| `core::WriteRules` | Regras de escrita: cor transparente na origem e máscara de cores protegidas no destino. |
| `core::CharacterLayout` | Grade de células de personagem (tamanho, sub-grade, meia célula). |
| `gui::Document` | Canvas + paleta + caminho + `QUndoStack` próprio. |
| `gui::DrawBuffer` | Buffer da área de desenho, compartilhado entre documentos, com `QUndoStack` próprio. |
| `gui::EditorSession` | Estado central: documento ativo, cores, zoom, grades, ferramenta, tom e máscara. |

### Fluxo principal (herdado do Character Maker 1999)

```
 canvas (Document)                  área de desenho (DrawBuffer)
 ┌───────┬───────┬───────┐  botão   ┌─────────────┐
 │ célula│ célula│ célula│ direito  │             │  ferramentas
 │   0   │   1   │   2   │ ───────► │  célula 1   │ ◄──────────── usuário
 ├───────┼───────┼───────┤          │  ampliada   │
 │  ...  │  ...  │  ...  │ ◄─────── │             │
 └───────┴───────┴───────┘  botão   └─────────────┘
                            esquerdo (WriteRules: transparência + máscara)
```

## 3. Padrões de projeto

- **Command** (`QUndoCommand`, em `gui/commands`): toda alteração passa por um comando.
  - `RegionCommand` guarda apenas o retângulo afetado (antes/depois).
  - `PaletteCommand` funde ajustes seguidos da mesma cor (`mergeWith`), de modo que arrastar um controle deslizante gera um único passo.
- **Strategy** (`gui/tools`): a `DrawAreaView` só converte coordenadas e repassa os eventos à ferramenta atual. Para criar uma ferramenta nova, basta implementar `Tool` e registrá-la na `EditorSession`.
- **Observer** (sinais e slots): as views observam a `EditorSession`, o `Document` e o `DrawBuffer`, e nenhuma delas guarda cópia do estado.
- **Interface comum** (`ImageTarget`): `Document` e `DrawBuffer` expõem a mesma interface de edição, então os mesmos comandos de desfazer servem para ambos.

### Edição ao vivo

As ferramentas alteram o buffer a cada movimento do mouse, para dar retorno
imediato, mas registram **um único** passo de desfazer ao soltar o botão:

```
beginEdit()   -> guarda uma cópia (snapshot)
editLive(f)   -> aplica f e acumula a área alterada
restoreSnapshot() -> (figuras) volta ao início antes de redesenhar
commitEdit()  -> RegionCommand(antes, depois) com "já aplicado"
```

### Desfazer unificado

Um `QUndoGroup` reúne a pilha do buffer e a de cada documento. A pilha ativa
passa a ser a última que mudou. Assim, Ctrl+Z desfaz sempre a edição mais
recente, seja um traço na área de desenho ou uma célula gravada no canvas.

## 4. Pontos de extensão

| Quero... | Onde |
|---|---|
| Nova ferramenta | Implementar `gui::Tool`, registrar em `EditorSession` e adicionar o botão em `ToolPanel`. |
| Nova transformação | Função pura em `core/Transform` e entrada em `EditOperations::transform`. |
| Novo formato de paleta | `io/PaletteFile` (`parse`/`encode`). |
| Novo comando de menu | `ActionId` + `ActionCatalog` + conexão na `MainWindow`. |
| Trocar ícones | Substituir os PNGs em `resources/icons` (mesmos nomes). |
| Novo idioma | `translations/spritenator_<idioma>.ts` em `TS_FILES` (`src/CMakeLists.txt`), `update_translations`, traduzir; opcionalmente `resources/help/manual_<idioma>.html`. |
| Novo texto na interface | Sempre via `tr()` (ou `QCoreApplication::translate` com contexto literal fora de QObject); depois `update_translations`. |

## 5. Testes

Os testes ficam em `tests/<camada>`, um executável Qt Test por assunto,
registrados no CTest:

- **core**: geometria, paleta, imagem, rasterização, transformações, operações de paleta e animação;
- **io**: ida e volta de todos os formatos, entradas inválidas e backups;
- **gui**: documento, sessão (get/put, ferramentas, desfazer), operações de edição, configurações, ajuda e testes de **integração** da `MainWindow`, que disparam as ações reais dos menus.

Os testes de GUI rodam com `QT_QPA_PLATFORM=offscreen`, sem servidor gráfico.
