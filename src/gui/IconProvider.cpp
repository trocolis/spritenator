// =============================================================================
// IconProvider.cpp
// =============================================================================
#include "IconProvider.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

#include <algorithm>

namespace spritenator::gui {

namespace {

constexpr auto kIconPrefix = ":/icons/";
constexpr auto kDisabledSuffix = "_disabled";

/// Contorno dos ícones (cor "K" de tools/make_icons.py) e o substituto claro.
const QColor kOutline(27, 29, 46);
const QColor kDarkThemeOutline(150, 156, 174); // distinto do branco (tracejados)

/// Verdadeiro se o tema atual tem fundo escuro.
bool darkThemeActive()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

/// Ícone de pixel art: escolhe a imagem (normal/desabilitada), troca o
/// contorno no tema escuro e amplia por um fator inteiro.
class PixelIconEngine final : public QIconEngine {
public:
    PixelIconEngine(QImage normal, QImage disabled)
        : m_normal(std::move(normal))
        , m_disabled(std::move(disabled))
    {
    }

    QIconEngine* clone() const override { return new PixelIconEngine(m_normal, m_disabled); }

    QSize actualSize(const QSize& size, QIcon::Mode /*mode*/, QIcon::State /*state*/) override
    {
        const int factor = factorFor(size);
        return factor > 0 ? m_normal.size() * factor
                          : m_normal.size().scaled(size, Qt::KeepAspectRatio);
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(
        const QSize& size, QIcon::Mode mode, QIcon::State /*state*/, qreal scale) override
    {
        const QSize device = size * scale;
        const QImage& source
            = mode == QIcon::Disabled && !m_disabled.isNull() ? m_disabled : m_normal;
        QImage image = darkThemeActive() ? recolored(source) : source;
        const int factor = factorFor(device);
        if (factor > 0) {
            image = image.scaled(
                image.size() * factor, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        } else { // menor que o original: reduz suavemente
            image = image.scaled(device, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QPixmap result = QPixmap::fromImage(image);
        result.setDevicePixelRatio(scale);
        return result;
    }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal scale = painter->device()->devicePixelRatioF();
        const QPixmap pix = scaledPixmap(rect.size(), mode, state, scale);
        const QSize logical = pix.size() / scale;
        // Centralizado no retângulo pedido.
        const QPoint topLeft(rect.x() + (rect.width() - logical.width()) / 2,
            rect.y() + (rect.height() - logical.height()) / 2);
        painter->drawPixmap(QRect(topLeft, logical), pix);
    }

    bool isNull() override { return m_normal.isNull(); }

private:
    /// Maior fator inteiro que cabe no tamanho pedido (0 se nem 1x cabe).
    [[nodiscard]] int factorFor(const QSize& size) const
    {
        if (m_normal.isNull()) {
            return 0;
        }
        return std::min(size.width() / m_normal.width(), size.height() / m_normal.height());
    }

    /// Cópia com o contorno escuro trocado pelo tom claro (alfa mantido).
    [[nodiscard]] static QImage recolored(const QImage& source)
    {
        QImage image = source.convertToFormat(QImage::Format_ARGB32);
        const QRgb outline = kOutline.rgb() & 0xFFFFFFU;
        const QRgb replacement = kDarkThemeOutline.rgb() & 0xFFFFFFU;
        for (int y = 0; y < image.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                if ((line[x] & 0xFFFFFFU) == outline && qAlpha(line[x]) != 0) {
                    line[x] = (line[x] & 0xFF000000U) | replacement;
                }
            }
        }
        return image;
    }

    QImage m_normal;
    QImage m_disabled;
};

} // namespace

QString IconProvider::resourcePath(const QString& name)
{
    return QString::fromLatin1(kIconPrefix) + name + QStringLiteral(".png");
}

QColor IconProvider::outlineColor()
{
    return kOutline;
}

QColor IconProvider::darkThemeOutlineColor()
{
    return kDarkThemeOutline;
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
        // A variante desabilitada é opcional: só a usamos se existir.
        const QString disabledPath = resourcePath(name + QString::fromLatin1(kDisabledSuffix));
        result = QIcon(new PixelIconEngine(
            QImage(normalPath), QFile::exists(disabledPath) ? QImage(disabledPath) : QImage()));
    }

    cache.insert(name, result);
    return result;
}

} // namespace spritenator::gui
