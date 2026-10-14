// =============================================================================
// Theme.cpp
// =============================================================================
#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace spritenator::gui {

namespace {

/// Estilo e paleta nativos, guardados na primeira troca de tema.
struct NativeLook {
    QString styleName;
    QPalette palette;
};

NativeLook& nativeLook()
{
    static NativeLook look { QApplication::style()->name(), QApplication::palette() };
    return look;
}

Theme& storedTheme()
{
    static Theme theme = Theme::System;
    return theme;
}

constexpr auto kFusion = "Fusion";

/// Avisa a plataforma do esquema de cores (ex.: barra de título escura no
/// Windows). Disponível a partir do Qt 6.8; antes disso, só a paleta muda.
void setColorScheme([[maybe_unused]] Qt::ColorScheme scheme)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(scheme);
#endif
}

} // namespace

QString themeKey(Theme theme)
{
    switch (theme) {
    case Theme::Light: return QStringLiteral("light");
    case Theme::Dark: return QStringLiteral("dark");
    case Theme::System: break;
    }
    return QStringLiteral("system");
}

Theme themeFromKey(const QString& key)
{
    if (key == QLatin1String("light")) {
        return Theme::Light;
    }
    if (key == QLatin1String("dark")) {
        return Theme::Dark;
    }
    return Theme::System;
}

QPalette lightPalette()
{
    QStyle* fusion = QStyleFactory::create(QString::fromLatin1(kFusion));
    QPalette palette = fusion != nullptr ? fusion->standardPalette() : QPalette();
    delete fusion;
    return palette;
}

QPalette darkPalette()
{
    // Cinzas neutros e o azul dos ícones como destaque.
    const QColor window(43, 45, 49);
    const QColor base(31, 32, 35);
    const QColor button(53, 55, 60);
    const QColor text(230, 230, 232);
    const QColor disabledText(122, 125, 133);
    const QColor highlight(52, 116, 214);

    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::ToolTipBase, button);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, disabledText);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor(255, 110, 100));
    palette.setColor(QPalette::Link, QColor(110, 165, 255));
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    // Tons de relevo (bordas, divisores e o fundo do editor de imagem).
    palette.setColor(QPalette::Light, QColor(74, 76, 82));
    palette.setColor(QPalette::Midlight, QColor(62, 64, 70));
    palette.setColor(QPalette::Mid, QColor(38, 39, 43));
    palette.setColor(QPalette::Dark, QColor(24, 25, 28));
    palette.setColor(QPalette::Shadow, QColor(10, 10, 12));
    for (const auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText }) {
        palette.setColor(QPalette::Disabled, role, disabledText);
    }
    palette.setColor(QPalette::Disabled, QPalette::Base, window);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(70, 72, 78));
    return palette;
}

void applyTheme(Theme theme)
{
    const NativeLook& native = nativeLook(); // guarda o visual nativo antes de mudar
    switch (theme) {
    case Theme::System:
        QApplication::setStyle(native.styleName);
        QApplication::setPalette(native.palette);
        setColorScheme(Qt::ColorScheme::Unknown); // volta a seguir o sistema
        break;
    case Theme::Light:
        QApplication::setStyle(QString::fromLatin1(kFusion));
        QApplication::setPalette(lightPalette());
        setColorScheme(Qt::ColorScheme::Light);
        break;
    case Theme::Dark:
        QApplication::setStyle(QString::fromLatin1(kFusion));
        QApplication::setPalette(darkPalette());
        setColorScheme(Qt::ColorScheme::Dark);
        break;
    }
    storedTheme() = theme;
}

Theme currentTheme()
{
    return storedTheme();
}

} // namespace spritenator::gui
