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

#include "core/CharacterLayout.h"
#include "core/IndexedImage.h"
#include "core/Palette.h"
#include "core/Tone.h"
#include "tools/Tool.h"

#include <QObject>
#include <QPointer>

#include <map>
#include <memory>

class QUndoGroup;
class QUndoStack;

namespace spritenator::gui {

class Document;
class DrawBuffer;

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

    // --- Desfazer/refazer ---
    /// Grupo com a pilha do buffer e as pilhas de cada documento. A pilha
    /// ativa é sempre a da edição mais recente ("desfazer a última coisa").
    [[nodiscard]] QUndoGroup* undoGroup() const { return m_undoGroup; }

    // --- Cores ---
    [[nodiscard]] core::ColorIndex drawColor() const { return m_drawColor; }
    [[nodiscard]] core::ColorIndex backgroundColor() const { return m_backgroundColor; }
    void setDrawColor(int index);
    void setBackgroundColor(int index);

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

    /// Regras usadas pelas ferramentas de desenho (máscara de cores).
    [[nodiscard]] core::WriteRules drawingRules() const;
    /// Regras usadas ao gravar o buffer no canvas (transparência + máscara).
    [[nodiscard]] core::WriteRules transferRules() const;

    // --- Tom (retícula) ---
    [[nodiscard]] core::Tone tone() const { return m_tone; }
    void setTone(core::Tone tone);
    /// Deslocamento atual do padrão (0..15).
    [[nodiscard]] int toneShift() const { return m_toneShift; }
    /// Avança para o próximo deslocamento (botão "tone shift").
    void cycleToneShift();

    /// Pinta um pixel respeitando o tom e as regras de desenho. Ponto central
    /// por onde TODAS as ferramentas desenham. Devolve true se o pixel mudou.
    bool plot(core::IndexedImage& image, core::Point point, core::ColorIndex color) const;

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

private:
    /// Ajusta as cores ao tamanho da paleta e repassa a paleta ao buffer.
    void syncPalette();

    /// Inclui a pilha no grupo e a torna ativa sempre que for alterada.
    void trackUndoStack(QUndoStack* stack);

    DrawBuffer* m_buffer = nullptr;
    QPointer<Document> m_activeDocument;
    QUndoGroup* m_undoGroup = nullptr;
    core::Palette m_defaultPalette;

    core::ColorIndex m_drawColor = 15;
    core::ColorIndex m_backgroundColor = 0;
    int m_zoom = 8;
    bool m_mainGridVisible = true;
    bool m_subGridVisible = false;
    core::CharacterLayout m_layout;
    bool m_transparentEffect = false;

    core::Tone m_tone = core::Tone::A;
    int m_toneShift = 0;

    ToolKind m_toolKind = ToolKind::PenThin;
    std::map<ToolKind, std::unique_ptr<Tool>> m_tools;
};

} // namespace spritenator::gui
