// =============================================================================
// IconProvider.cpp
// =============================================================================
#include "IconProvider.h"

#include <QFile>
#include <QHash>
#include <QPixmap>

namespace spritenator::gui {

namespace {

/// Prefixo dos ícones no sistema de recursos do Qt (definido no CMake).
constexpr auto kIconPrefix = ":/icons/";

/// Sufixo dos arquivos com a variante "desabilitada" do ícone.
constexpr auto kDisabledSuffix = "_disabled";

/// Fatores de ampliação pré-gerados. Os ícones são pixel art minúsculos
/// (16x16/20x20); se o Qt os ampliasse sozinho (telas HiDPI, barras maiores)
/// usaria filtragem suave e eles ficariam borrados. Gerando as versões
/// ampliadas por "vizinho mais próximo" os pixels continuam nítidos.
constexpr int kScaleFactors[] = { 1, 2, 3, 4 };

/// Adiciona ao ícone o arquivo informado em todas as escalas pré-definidas.
void addCrispPixmaps(QIcon& icon, const QString& path, QIcon::Mode mode)
{
    const QPixmap base(path);
    if (base.isNull()) {
        return;
    }
    for (const int factor : kScaleFactors) {
        icon.addPixmap(
            base.scaled(base.size() * factor, Qt::IgnoreAspectRatio, Qt::FastTransformation), mode);
    }
}

} // namespace

QString IconProvider::resourcePath(const QString& name)
{
    return QString::fromLatin1(kIconPrefix) + name + QStringLiteral(".png");
}

QIcon IconProvider::icon(const QString& name)
{
    // Cache simples: QIcon é implicitamente compartilhado (cópia barata), e
    // evitamos reabrir os mesmos arquivos a cada menu/botão criado.
    static QHash<QString, QIcon> cache;
    if (const auto it = cache.constFind(name); it != cache.constEnd()) {
        return it.value();
    }

    QIcon result;
    const QString normalPath = resourcePath(name);
    if (QFile::exists(normalPath)) {
        addCrispPixmaps(result, normalPath, QIcon::Normal);

        // A variante desabilitada é opcional: só a usamos se existir.
        const QString disabledPath = resourcePath(name + QString::fromLatin1(kDisabledSuffix));
        if (QFile::exists(disabledPath)) {
            addCrispPixmaps(result, disabledPath, QIcon::Disabled);
        }
    }

    cache.insert(name, result);
    return result;
}

} // namespace spritenator::gui
