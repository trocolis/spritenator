// =============================================================================
// SelectionTransform.h
//
// Transformação "flutuante" da seleção do documento ativo: os pixels
// selecionados são erguidos, movidos/escalados/girados/espelhados com prévia
// ao vivo e só viram UM passo de desfazer ao aplicar (commit). Cancelar
// devolve tudo como estava, inclusive a seleção.
//
// O lugar de onde os pixels saem recebe o pixel de fundo do modo: a cor de
// fundo (indexada) ou transparente (RGBA). Usada pelas setas do teclado, pelos
// atalhos R/E, pelos botões de transformação (com seleção) e pelas alças do
// editor de imagem.
// =============================================================================
#pragma once

#include "commands/UndoLabel.h"
#include "core/AnyImage.h"
#include "core/FreeTransform.h"
#include "core/Selection.h"

#include <QObject>
#include <QPointer>

namespace spritenator::gui {

class Document;
class EditorSession;

/// Transformação livre em andamento (no máximo uma por sessão).
class SelectionTransform final : public QObject {
    Q_OBJECT

public:
    explicit SelectionTransform(EditorSession* session);

    /// Verdadeiro se há uma transformação em andamento (ainda não aplicada).
    [[nodiscard]] bool isActive() const;
    /// Documento sendo transformado (nulo se inativa).
    [[nodiscard]] Document* document() const;

    /// Começa a transformar a seleção do documento ativo. Se já houver uma
    /// transformação nesse documento, continua nela. Devolve false se não há
    /// documento ou seleção.
    bool begin();

    /// Parâmetros atuais, relativos à seleção original.
    [[nodiscard]] const core::TransformParams& params() const { return m_params; }
    /// Aplica novos parâmetros (prévia ao vivo no documento).
    void setParams(const core::TransformParams& params);
    /// Retângulo envolvente da seleção original.
    [[nodiscard]] core::Rect originalBounds() const { return m_original.bounds(); }
    /// Transformação afim atual (coordenadas do canvas).
    [[nodiscard]] core::Affine affine() const;

    // --- Operações compostas sobre os parâmetros atuais ---
    void translate(double dx, double dy);
    /// 90 graus em torno do centro (horário ou anti-horário).
    void rotate90(bool clockwise);
    /// Espelha na horizontal (esquerda <-> direita) ou na vertical, no
    /// sentido da TELA, mesmo se a seleção já estiver girada.
    void flip(bool horizontal);
    void rotateBy(double degrees);
    /// Escala para o tamanho informado (a partir do original, sem girar).
    void scaleTo(core::Size size);

    /// Aplica: vira um passo de desfazer (com o nome informado). A seleção
    /// fica no lugar novo.
    void commit(UndoOperation operation = UndoOperation::TransformSelection);
    /// Descarta a transformação (pixels e seleção voltam ao original).
    void cancel();

signals:
    /// Começou, mudou ou terminou (para o editor redesenhar as alças).
    void changed();

private:
    /// Redesenha a prévia no documento com os parâmetros atuais.
    void render();

    EditorSession* m_session;
    QPointer<Document> m_document;
    core::Selection m_original; ///< seleção ao começar
    core::AnyImage m_content; ///< pixels de m_original.bounds() ao começar
    core::TransformParams m_params;
};

} // namespace spritenator::gui
