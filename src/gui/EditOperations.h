// =============================================================================
// EditOperations.h
//
// Operações de edição aplicadas à área de desenho (região de trabalho = a
// seleção, ou o buffer inteiro se não houver seleção):
//   - área de transferência: recortar, copiar, colar, limpar, selecionar tudo;
//   - transformações (os 12 botões sob a área de desenho);
//   - preparar sombra e desenhar texto.
//
// Cada operação gera exatamente UM passo de desfazer no buffer.
// =============================================================================
#pragma once

#include "core/IndexedImage.h"

#include <QFont>
#include <QObject>
#include <QString>

#include <functional>

namespace spritenator::gui {

class EditorSession;

/// Transformações disponíveis nos botões do Integrator.
enum class TransformKind {
    ShiftUp,
    ShiftRight,
    ShiftDown,
    ShiftLeft,
    FlipVertical,
    FlipHorizontal,
    MirrorVertical,
    MirrorHorizontal,
    RotateClockwise,
    RotateCounterClockwise,
};

/// Operações de edição sobre o buffer da área de desenho.
class EditOperations final : public QObject {
    Q_OBJECT

public:
    /// Tipo MIME privado: preserva os ÍNDICES de cor ao copiar e colar dentro
    /// do Spritenator (a imagem comum também é publicada para outros apps).
    static constexpr auto kIndexedMimeType = "application/x-spritenator-indexed";

    explicit EditOperations(EditorSession* session, QObject* parent = nullptr);

    // --- Área de transferência ---
    void copy();
    void cut();
    /// Cola no canto da seleção (ou em 0,0); o conteúdo colado fica selecionado.
    void paste();
    /// Preenche a região de trabalho com a cor de fundo.
    void clear();
    void selectAll();
    /// Verdadeiro se a área de transferência tiver algo colável.
    [[nodiscard]] static bool canPaste();

    // --- Transformações ---
    void transform(TransformKind kind);
    /// Rotação livre (graus, sentido horário).
    void rotate(double degrees);
    /// Redimensiona o conteúdo da região de trabalho para "newSize".
    void stretch(core::Size newSize);

    // --- Outras ---
    /// Todo pixel diferente da cor de fundo vira a cor de desenho.
    void prepareShadow();
    /// Desenha o texto com a cor de desenho, sem suavização, a partir do canto
    /// da região de trabalho.
    void drawText(const QString& text, const QFont& font);

    /// Rasteriza texto num bitmap monocromático (exposto para a prévia do
    /// diálogo e para testes): true = pixel do texto.
    [[nodiscard]] static core::IndexedImage renderText(const QString& text, const QFont& font);

private:
    /// Aplica "function" à região de trabalho, registrando um passo de desfazer.
    void applyToWorkArea(const QString& undoText,
        const std::function<core::IndexedImage(const core::IndexedImage&)>& function);

    EditorSession* m_session;
};

} // namespace spritenator::gui
