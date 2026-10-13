// =============================================================================
// EditorSession.h
//
// Estado central do editor — a "fonte única de verdade" compartilhada por
// todas as views (área de desenho, paleta, canvas, menus):
//
//   - buffer da área de desenho (DrawBuffer) e documento ativo;
//   - cores de desenho/fundo, zoom, grades e grade de personagens;
//   - ferramenta atual e regras de escrita (transparência, máscara);
//   - roteamento do desfazer/refazer entre buffer e documentos.
//
// As views observam os sinais desta classe; nenhuma guarda cópia do estado.
// =============================================================================
#pragma once

#include "core/AnyImage.h"
#include "core/CharacterLayout.h"
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
class DrawBuffer;

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
    /// Fatores de zoom disponíveis (iguais aos do original).
    static constexpr int kZoomLevels[] = { 1, 2, 4, 6, 8, 12 };

    explicit EditorSession(QObject* parent = nullptr);
    ~EditorSession() override;

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    // --- Buffer e documento ---
    [[nodiscard]] DrawBuffer* buffer() const { return m_buffer; }
    [[nodiscard]] Document* activeDocument() const;
    void setActiveDocument(Document* document);

    /// Paleta em uso: a do documento ativo ou a padrão, se não houver.
    [[nodiscard]] const core::Palette& palette() const;

    /// Modo de cor em uso: o do documento ativo (indexado, se não houver).
    [[nodiscard]] core::ColorMode colorMode() const;

    // --- Desfazer/refazer ---
    /// Grupo com a pilha do buffer e as pilhas de cada documento. A pilha
    /// ativa é sempre a da edição mais recente ("desfazer a última coisa").
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

    // --- Visualização ---
    [[nodiscard]] int zoom() const { return m_zoom; }
    void setZoom(int factor);
    /// Próximo/anterior nível de zoom (usado pela ferramenta lupa).
    void zoomIn();
    void zoomOut();

    [[nodiscard]] bool mainGridVisible() const { return m_mainGridVisible; }
    [[nodiscard]] bool subGridVisible() const { return m_subGridVisible; }
    void setMainGridVisible(bool visible);
    void setSubGridVisible(bool visible);

    [[nodiscard]] const core::CharacterLayout& characterLayout() const { return m_layout; }
    /// Altera a grade; se o tamanho da célula mudar, o buffer é redimensionado.
    void setCharacterLayout(const core::CharacterLayout& layout);

    // --- Regras de escrita ---
    [[nodiscard]] bool transparentEffect() const { return m_transparentEffect; }
    void setTransparentEffect(bool enabled);

    // --- Máscara de cores protegidas ---
    /// Cores cujos pixels NÃO podem ser alterados pelo desenho nem pela
    /// gravação no canvas.
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
    /// Regras usadas ao gravar o buffer no canvas (transparência + máscara).
    [[nodiscard]] core::WriteRules transferRules() const;
    /// Regras RGBA ao gravar no canvas: com o efeito transparente ligado,
    /// pixels totalmente transparentes (alfa 0) não são gravados.
    [[nodiscard]] core::RgbaWriteRules rgbaTransferRules() const;

    // --- Tom (retícula) ---
    [[nodiscard]] core::Tone tone() const { return m_tone; }
    void setTone(core::Tone tone);
    /// Deslocamento atual do padrão (0..15).
    [[nodiscard]] int toneShift() const { return m_toneShift; }
    /// Avança para o próximo deslocamento (botão "tone shift").
    void cycleToneShift();

    /// Pinta um pixel com a cor do papel informado, respeitando o tom e as
    /// regras de desenho. Ponto central por onde TODAS as ferramentas
    /// desenham, em qualquer modo de cor. Devolve true se o pixel mudou.
    bool plot(core::AnyImage& image, core::Point point, ColorRole role) const;

    /// Pixel "de fundo" do modo da imagem: o índice da cor de fundo
    /// (indexada) ou transparente (RGBA). Usado para limpar e preencher
    /// áreas novas.
    [[nodiscard]] core::AnyImage blankLike(const core::AnyImage& image, core::Size size) const;

    // --- Ferramentas ---
    [[nodiscard]] ToolKind toolKind() const { return m_toolKind; }
    void setToolKind(ToolKind kind);
    /// Ferramenta atual (nunca nula).
    [[nodiscard]] Tool& tool() const;
    /// Interrompe a operação em andamento da ferramenta atual.
    void cancelTool();

    // --- Transferência canvas <-> área de desenho ---
    /// Traz uma célula do canvas para o buffer (botão direito no canvas).
    void getFromCanvas(Document* document, const core::Rect& cell);
    /// Grava o buffer no canvas na posição da célula (botão esquerdo).
    void putToCanvas(Document* document, const core::Rect& cell);
    /// Traz o canvas inteiro do documento ativo para o buffer (F3).
    void transferAll();

    /// Última célula trazida do canvas para a área de desenho (usada pela
    /// janela de animação para montar a lista de quadros).
    [[nodiscard]] const std::optional<core::Rect>& lastCell() const { return m_lastCell; }

signals:
    void activeDocumentChanged(gui::Document* document);
    void paletteChanged();
    void colorsChanged();
    void zoomChanged(int factor);
    void gridsChanged();
    void characterLayoutChanged(const core::CharacterLayout& layout);
    void transparentEffectChanged(bool enabled);
    void toolChanged(gui::ToolKind kind);
    void toneChanged();
    void maskChanged();
    void maskModeChanged(bool enabled);
    void autoMaskChanged(bool enabled);
    void lastCellChanged(const core::Rect& cell);
    /// O modo de cor em uso mudou (troca de documento ou "Reverter").
    void colorModeChanged(core::ColorMode mode);

private:
    /// Ajusta as cores ao tamanho da paleta e repassa a paleta ao buffer.
    void syncPalette();
    /// Converte o buffer para o modo do documento ativo e avisa as views.
    void syncColorMode();

    /// Inclui a pilha no grupo e a torna ativa sempre que for alterada.
    void trackUndoStack(QUndoStack* stack);

    DrawBuffer* m_buffer = nullptr;
    QPointer<Document> m_activeDocument;
    QUndoGroup* m_undoGroup = nullptr;
    core::Palette m_defaultPalette;

    core::ColorIndex m_drawColor = 15;
    core::ColorIndex m_backgroundColor = 0;
    core::Rgba m_primaryRgba { 0, 0, 0, 255 }; ///< preto opaco
    core::Rgba m_secondaryRgba { }; ///< transparente: o botão direito apaga
    core::ColorMode m_colorMode = core::ColorMode::Indexed;
    int m_zoom = 8;
    bool m_mainGridVisible = true;
    bool m_subGridVisible = false;
    core::CharacterLayout m_layout;
    bool m_transparentEffect = false;

    std::optional<core::Rect> m_lastCell;

    core::ColorSet m_mask;
    core::ColorSet m_maskBeforeAuto; ///< restaurada ao desligar a máscara automática
    bool m_maskMode = false;
    bool m_autoMask = false;

    core::Tone m_tone = core::Tone::A;
    int m_toneShift = 0;

    ToolKind m_toolKind = ToolKind::PenThin;
    std::map<ToolKind, std::unique_ptr<Tool>> m_tools;
};

} // namespace spritenator::gui
