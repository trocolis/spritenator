// =============================================================================
// EditorSession.h
//
// Estado central do editor — a "fonte única de verdade" compartilhada por
// todas as views (editores de imagem, paleta, seletor de cor, menus):
//
//   - documento ativo (a imagem editada diretamente pelas ferramentas);
//   - cores primária/secundária (índices ou RGBA, conforme o modo de cor);
//   - grades (visibilidade e espaçamentos), tom, máscara de cores;
//   - ferramenta atual e regras de escrita;
//   - grupo de desfazer (uma pilha por documento; a ativa é a do documento
//     ativo).
//
// O zoom NÃO fica aqui: cada editor de imagem (CanvasEditor) tem o seu.
// As views observam os sinais desta classe; nenhuma guarda cópia do estado.
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/Brush.h"
#include "core/GridLayout.h"
#include "core/Image.h"
#include "core/Palette.h"
#include "core/Tone.h"
#include "tools/Tool.h"

#include <QObject>
#include <QPointer>

#include <map>
#include <memory>
#include <optional>

class QUndoGroup;
class QUndoStack;

namespace spritenator::gui {

class Document;
class SelectionTransform;

/// Papel de uma cor: primária (desenho, botão esquerdo) ou secundária
/// (fundo, botão direito). As ferramentas pedem um papel, e a sessão decide o
/// pixel conforme o modo de cor (índice da paleta ou RGBA).
enum class ColorRole { Primary, Secondary };

/// Papel correspondente ao botão do mouse (esquerdo = primária).
[[nodiscard]] constexpr ColorRole colorRoleFor(Qt::MouseButton button)
{
    return button == Qt::RightButton ? ColorRole::Secondary : ColorRole::Primary;
}

/// Estado compartilhado do editor.
class EditorSession final : public QObject {
    Q_OBJECT

public:
    explicit EditorSession(QObject* parent = nullptr);
    ~EditorSession() override;

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    // --- Documento ativo ---
    /// Documento editado pelas ferramentas e operações (nulo se nenhum).
    [[nodiscard]] Document* activeDocument() const;
    void setActiveDocument(Document* document);

    /// Paleta em uso: a do documento ativo ou a padrão, se não houver.
    [[nodiscard]] const core::Palette& palette() const;

    /// Modo de cor em uso: o do documento ativo (indexado, se não houver).
    [[nodiscard]] core::ColorMode colorMode() const;

    /// Grupo com as pilhas de desfazer de cada documento.
    [[nodiscard]] QUndoGroup* undoGroup() const { return m_undoGroup; }

    // --- Cores do modo indexado (índices da paleta) ---
    [[nodiscard]] core::ColorIndex drawColor() const { return m_drawColor; }
    [[nodiscard]] core::ColorIndex backgroundColor() const { return m_backgroundColor; }
    void setDrawColor(int index);
    void setBackgroundColor(int index);

    // --- Cores do modo RGBA ---
    [[nodiscard]] const core::Rgba& primaryRgba() const { return m_primaryRgba; }
    [[nodiscard]] const core::Rgba& secondaryRgba() const { return m_secondaryRgba; }
    void setPrimaryRgba(const core::Rgba& color);
    void setSecondaryRgba(const core::Rgba& color);
    /// Cor RGBA do papel informado.
    [[nodiscard]] const core::Rgba& rgbaFor(ColorRole role) const;
    void setRgbaFor(ColorRole role, const core::Rgba& color);

    // --- Mostrador ativo ---
    /// Qual cor (primária ou secundária) as barras R, G, B, A editam.
    [[nodiscard]] ColorRole activeColorRole() const { return m_activeColorRole; }
    void setActiveColorRole(ColorRole role);

    // --- Grades ---
    [[nodiscard]] bool mainGridVisible() const { return m_mainGridVisible; }
    [[nodiscard]] bool subGridVisible() const { return m_subGridVisible; }
    void setMainGridVisible(bool visible);
    void setSubGridVisible(bool visible);

    [[nodiscard]] const core::GridLayout& gridLayout() const { return m_gridLayout; }
    void setGridLayout(const core::GridLayout& layout);

    // --- Regras de escrita ---
    /// "Efeito de cor transparente": ao COLAR, pixels da cor de fundo
    /// (indexada) ou totalmente transparentes (RGBA) não são colados.
    [[nodiscard]] bool transparentEffect() const { return m_transparentEffect; }
    void setTransparentEffect(bool enabled);

    // --- Máscara de cores protegidas (só indexadas) ---
    /// Cores cujos pixels NÃO podem ser alterados pelo desenho nem por colar.
    [[nodiscard]] const core::ColorSet& mask() const { return m_mask; }
    void setMask(const core::ColorSet& mask);
    void toggleMasked(int index);
    void clearMask();
    void invertMask();
    /// Modo de edição da máscara: cliques na paleta marcam/desmarcam cores.
    [[nodiscard]] bool maskMode() const { return m_maskMode; }
    void setMaskMode(bool enabled);
    /// Máscara automática: protege tudo exceto a cor de fundo; ao desligar,
    /// a máscara anterior é restaurada.
    [[nodiscard]] bool autoMask() const { return m_autoMask; }
    void setAutoMask(bool enabled);

    /// Regras usadas pelas ferramentas de desenho (máscara de cores).
    [[nodiscard]] core::WriteRules drawingRules() const;
    /// Regras de colar em imagens indexadas (máscara + efeito transparente).
    [[nodiscard]] core::WriteRules pasteRules() const;
    /// Regras de colar em imagens RGBA (efeito transparente: ignora alfa 0).
    [[nodiscard]] core::RgbaWriteRules rgbaPasteRules() const;

    // --- Tom (retícula) ---
    [[nodiscard]] core::Tone tone() const { return m_tone; }
    void setTone(core::Tone tone);
    /// Deslocamento atual do padrão (0..15).
    [[nodiscard]] int toneShift() const { return m_toneShift; }
    /// Avança para o próximo deslocamento (botão "tone shift").
    void cycleToneShift();

    /// Pinta um pixel com a cor do papel informado, respeitando o tom, as
    /// regras de desenho e a seleção do documento ativo (havendo seleção, só
    /// os pixels dentro dela mudam). Ponto central por onde TODAS as ferramentas
    /// desenham, em qualquer modo de cor. Devolve true se o pixel mudou.
    bool plot(core::AnyImage& image, core::Point point, ColorRole role) const;

    /// Verifica se uma ferramenta PODERIA pintar o pixel (dentro da imagem e
    /// da seleção, coberto pelo tom e não protegido pela máscara). Usado no destaque dos
    /// pixels sob o mouse.
    [[nodiscard]] bool canPlot(const core::AnyImage& image, core::Point point) const;

    /// Imagem nova do modo de "image", toda com o pixel de fundo do modo: o
    /// índice da cor de fundo (indexada) ou transparente (RGBA).
    [[nodiscard]] core::AnyImage blankLike(const core::AnyImage& image, core::Size size) const;

    // --- Ferramentas ---
    [[nodiscard]] ToolKind toolKind() const { return m_toolKind; }
    void setToolKind(ToolKind kind);
    /// Ferramenta atual (nunca nula).
    [[nodiscard]] Tool& tool() const;
    /// Interrompe a operação em andamento da ferramenta atual.
    void cancelTool();

    // --- Transformação livre da seleção ---
    /// Transformação flutuante (mover, escalar, girar, espelhar) da seleção.
    [[nodiscard]] SelectionTransform* selectionTransform() const { return m_transform; }
    /// Aplica a transformação em andamento, se houver (antes de outras edições).
    void commitTransform();

    // --- Modificadores do mouse ---
    /// Teclas Shift/Ctrl/Alt do evento de mouse em tratamento (o editor as
    /// informa antes de chamar a ferramenta; as de seleção as usam para
    /// somar/subtrair e para restringir a forma).
    [[nodiscard]] Qt::KeyboardModifiers inputModifiers() const { return m_inputModifiers; }
    void setInputModifiers(Qt::KeyboardModifiers modifiers) { m_inputModifiers = modifiers; }

    // --- Ponta do lápis ---
    [[nodiscard]] core::BrushShape brushShape() const { return m_brushShape; }
    void setBrushShape(core::BrushShape shape);
    /// Tamanho da ponta, em pixels (1 a 64).
    [[nodiscard]] int brushSize() const { return m_brushSize; }
    void setBrushSize(int size);

    // --- Varinha mágica ---
    /// Tolerância de cor: 0 (só a mesma cor) a 100 (qualquer cor).
    [[nodiscard]] int wandTolerance() const { return m_wandTolerance; }
    void setWandTolerance(int tolerance);
    /// Só a região conectada ao ponto clicado (senão, a imagem toda).
    [[nodiscard]] bool wandContiguous() const { return m_wandContiguous; }
    void setWandContiguous(bool contiguous);

    // --- Célula para a animação ---
    /// Célula da sub-grade sob o último clique na imagem (usada pela janela
    /// de animação para montar a lista de quadros).
    [[nodiscard]] const std::optional<core::Rect>& lastCell() const { return m_lastCell; }
    /// Registra o ponto clicado (convertido para a célula da sub-grade).
    void setLastClickedPoint(core::Point point);

signals:
    void activeDocumentChanged(gui::Document* document);
    void paletteChanged();
    void colorsChanged();
    void gridsChanged();
    void gridLayoutChanged(const core::GridLayout& layout);
    void transparentEffectChanged(bool enabled);
    void toolChanged(gui::ToolKind kind);
    void toneChanged();
    void maskChanged();
    void maskModeChanged(bool enabled);
    void autoMaskChanged(bool enabled);
    void lastCellChanged(const core::Rect& cell);
    /// Tolerância ou modo "contígua" da varinha mudaram.
    void wandOptionsChanged();
    /// Formato ou tamanho da ponta do lápis mudaram.
    void brushChanged();
    /// O modo de cor em uso mudou (troca de documento ou "Reverter").
    void colorModeChanged(core::ColorMode mode);
    /// O mostrador ativo mudou.
    void activeColorRoleChanged(gui::ColorRole role);

private:
    /// Ajusta as cores ao tamanho da paleta e avisa as views.
    void syncPalette();
    /// Avisa as views se o modo de cor em uso mudou.
    void syncColorMode();

    QPointer<Document> m_activeDocument;
    QUndoGroup* m_undoGroup = nullptr;
    SelectionTransform* m_transform = nullptr;
    core::Palette m_defaultPalette;

    core::ColorIndex m_drawColor = 15;
    core::ColorIndex m_backgroundColor = 0;
    core::Rgba m_primaryRgba { 0, 0, 0, 255 }; ///< preto opaco
    core::Rgba m_secondaryRgba { }; ///< transparente: o botão direito apaga
    core::ColorMode m_colorMode = core::ColorMode::Indexed;
    ColorRole m_activeColorRole = ColorRole::Primary;

    bool m_mainGridVisible = true;
    bool m_subGridVisible = false;
    core::GridLayout m_gridLayout;
    bool m_transparentEffect = false;

    std::optional<core::Rect> m_lastCell;
    Qt::KeyboardModifiers m_inputModifiers;
    core::BrushShape m_brushShape = core::BrushShape::Square;
    int m_brushSize = 1;
    int m_wandTolerance = 15;
    bool m_wandContiguous = true;

    core::ColorSet m_mask;
    core::ColorSet m_maskBeforeAuto; ///< restaurada ao desligar a máscara automática
    bool m_maskMode = false;
    bool m_autoMask = false;

    core::Tone m_tone = core::Tone::A;
    int m_toneShift = 0;

    ToolKind m_toolKind = ToolKind::Pen;
    std::map<ToolKind, std::unique_ptr<Tool>> m_tools;
};

} // namespace spritenator::gui
