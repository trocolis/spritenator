// =============================================================================
// IoError.h
//
// Exceção lançada por todas as operações de leitura/gravação de arquivos. A
// mensagem é um QString já pronto para ser exibido ao usuário.
// =============================================================================
#pragma once

#include <QString>

#include <stdexcept>

namespace spritenator::io {

/// Erro de entrada/saída (arquivo inexistente, formato inválido, etc.).
class IoError final : public std::runtime_error {
public:
    explicit IoError(const QString& message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    /// Mensagem legível, destinada ao usuário.
    [[nodiscard]] const QString& message() const noexcept { return m_message; }

private:
    QString m_message;
};

} // namespace spritenator::io
