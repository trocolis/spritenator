// =============================================================================
// Theme.h
//
// Tema das janelas: o do sistema, claro ou escuro.
//
// Claro e escuro usam o estilo Fusion do Qt com paletas definidas aqui — o
// mesmo visual em Linux, Windows e macOS. "Sistema" devolve o estilo e as
// cores nativas que o programa tinha ao abrir. A troca vale na hora: o Qt
// avisa todos os widgets (QEvent::PaletteChange) e eles se redesenham.
// =============================================================================
#pragma once

#include <QString>

class QPalette;

namespace spritenator::gui {

/// Tema das janelas.
enum class Theme { System, Light, Dark };

/// Nome usado nas preferências ("system", "light", "dark").
[[nodiscard]] QString themeKey(Theme theme);
/// Inverso de themeKey (desconhecido = Sistema).
[[nodiscard]] Theme themeFromKey(const QString& key);

/// Paletas dos temas claro e escuro (expostas para testes).
[[nodiscard]] QPalette lightPalette();
[[nodiscard]] QPalette darkPalette();

/// Aplica o tema a toda a aplicação (exige uma QApplication).
void applyTheme(Theme theme);

/// Tema aplicado por último (Sistema se nenhum).
[[nodiscard]] Theme currentTheme();

} // namespace spritenator::gui
