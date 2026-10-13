// =============================================================================
// ColorWheel.cpp
// =============================================================================
#include "ColorWheel.h"

#include <QConicalGradient>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace spritenator::gui {

namespace wheel {

SaturationValue toSaturationValue(Barycentric point)
{
    // Pesos negativos = ponto fora do triângulo: zera e renormaliza.
    double hue = std::max(0.0, point.hueWeight);
    double white = std::max(0.0, point.whiteWeight);
    double black = std::max(0.0, point.blackWeight);
    const double sum = hue + white + black;
    if (sum <= 0) {
        return { 0, 0 };
    }
    hue /= sum;
    white /= sum;
    black /= sum;
    // P = V * (S * Cor + (1 - S) * Branco) + (1 - V) * Preto
    //  => peso da cor = V*S, peso do branco = V*(1-S), peso do preto = 1-V.
    const double value = std::clamp(1.0 - black, 0.0, 1.0);
    const double saturation = value > 1e-9 ? std::clamp(hue / value, 0.0, 1.0) : 0.0;
    return { saturation, value };
}

Barycentric toBarycentric(SaturationValue color)
{
    return { color.value * color.saturation, color.value * (1.0 - color.saturation),
        1.0 - color.value };
}

Barycentric barycentricOf(QPointF point, QPointF a, QPointF b, QPointF c)
{
    // Resolução padrão por áreas (produto vetorial em 2D).
    const double denominator
        = (b.y() - c.y()) * (a.x() - c.x()) + (c.x() - b.x()) * (a.y() - c.y());
    if (std::abs(denominator) < 1e-12) {
        return { 0, 0, 1 };
    }
    const double weightA
        = ((b.y() - c.y()) * (point.x() - c.x()) + (c.x() - b.x()) * (point.y() - c.y()))
        / denominator;
    const double weightB
        = ((c.y() - a.y()) * (point.x() - c.x()) + (a.x() - c.x()) * (point.y() - c.y()))
        / denominator;
    return { weightA, weightB, 1.0 - weightA - weightB };
}

} // namespace wheel

namespace {

/// Espessura do anel em relação ao raio externo.
constexpr double kRingThickness = 0.18;
/// Graus -> radianos.
constexpr double kDegreesToRadians = std::numbers::pi / 180.0;

/// Ângulo de tela (radianos, y para baixo) do matiz: 0° (vermelho) à
/// direita e crescendo no sentido anti-horário, como no QConicalGradient.
double angleOfHue(double hue)
{
    return -hue * kDegreesToRadians;
}

} // namespace

ColorWheel::ColorWheel(QWidget* parent)
    : QWidget(parent)
{
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setCursor(Qt::CrossCursor);
    retranslate();
}

void ColorWheel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void ColorWheel::retranslate()
{
    setToolTip(tr("Ring: hue. Triangle: saturation and brightness.\n"
                  "Left button: primary color. Right button: secondary color."));
}

QSize ColorWheel::sizeHint() const
{
    return { 200, 200 };
}

QSize ColorWheel::minimumSizeHint() const
{
    return { 120, 120 };
}

// -----------------------------------------------------------------------------
// Cor
// -----------------------------------------------------------------------------
void ColorWheel::setColor(const QColor& color)
{
    const QColor hsv = color.toHsv();
    const int previousHue = m_hue;
    // Cinzas (incluindo preto/branco) não têm matiz: mantém o atual para que o
    // triângulo não "pule" de lado ao passar por eles.
    if (hsv.hsvHue() >= 0) {
        m_hue = hsv.hsvHue();
    }
    m_saturation = static_cast<double>(hsv.hsvSaturationF());
    m_value = static_cast<double>(hsv.valueF());
    if (m_hue != previousHue) {
        rebuildTriangleImage();
    }
    update();
}

QColor ColorWheel::color() const
{
    return QColor::fromHsvF(static_cast<float>(m_hue) / 360.0F, static_cast<float>(m_saturation),
        static_cast<float>(m_value));
}

// -----------------------------------------------------------------------------
// Geometria
// -----------------------------------------------------------------------------
QPointF ColorWheel::center() const
{
    return { width() / 2.0, height() / 2.0 };
}

double ColorWheel::outerRadius() const
{
    return std::min(width(), height()) / 2.0 - 2.0;
}

double ColorWheel::innerRadius() const
{
    return outerRadius() * (1.0 - kRingThickness);
}

void ColorWheel::triangleVertices(
    QPointF& hueVertex, QPointF& whiteVertex, QPointF& blackVertex) const
{
    // Triângulo equilátero inscrito no anel; o vértice da cor pura aponta para
    // o matiz, e os outros dois ficam a ±120°.
    const double radius = innerRadius() - 2.0;
    const QPointF origin = center();
    const auto vertexAt = [&](double angle) {
        return QPointF(
            origin.x() + radius * std::cos(angle), origin.y() + radius * std::sin(angle));
    };
    const double hueAngle = angleOfHue(m_hue);
    hueVertex = vertexAt(hueAngle);
    whiteVertex = vertexAt(hueAngle - 120.0 * kDegreesToRadians);
    blackVertex = vertexAt(hueAngle + 120.0 * kDegreesToRadians);
}

QPointF ColorWheel::ringPointForHue(int hue) const
{
    const double radius = (outerRadius() + innerRadius()) / 2.0;
    const double angle = angleOfHue(hue);
    return { center().x() + radius * std::cos(angle), center().y() + radius * std::sin(angle) };
}

void ColorWheel::rebuildTriangleImage()
{
    if (width() <= 0 || height() <= 0) {
        return;
    }
    QPointF a;
    QPointF b;
    QPointF c;
    triangleVertices(a, b, c);
    m_triangleImage = QImage(size(), QImage::Format_ARGB32);
    m_triangleImage.fill(Qt::transparent);

    // Só os pixels dentro do retângulo envolvente do triângulo são avaliados.
    const int left = static_cast<int>(std::floor(std::min({ a.x(), b.x(), c.x() })));
    const int right = static_cast<int>(std::ceil(std::max({ a.x(), b.x(), c.x() })));
    const int top = static_cast<int>(std::floor(std::min({ a.y(), b.y(), c.y() })));
    const int bottom = static_cast<int>(std::ceil(std::max({ a.y(), b.y(), c.y() })));
    const float hue = static_cast<float>(m_hue) / 360.0F;
    for (int y = std::max(0, top); y <= std::min(height() - 1, bottom); ++y) {
        auto* line = reinterpret_cast<QRgb*>(m_triangleImage.scanLine(y));
        for (int x = std::max(0, left); x <= std::min(width() - 1, right); ++x) {
            const wheel::Barycentric weights
                = wheel::barycentricOf(QPointF(x + 0.5, y + 0.5), a, b, c);
            // Pequena tolerância negativa suaviza a borda do triângulo.
            if (weights.hueWeight < -0.01 || weights.whiteWeight < -0.01
                || weights.blackWeight < -0.01) {
                continue;
            }
            const wheel::SaturationValue sv = wheel::toSaturationValue(weights);
            line[x] = QColor::fromHsvF(
                hue, static_cast<float>(sv.saturation), static_cast<float>(sv.value))
                          .rgba();
        }
    }
}

void ColorWheel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildTriangleImage();
}

// -----------------------------------------------------------------------------
// Pintura
// -----------------------------------------------------------------------------
void ColorWheel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF origin = center();

    // Anel de matizes: gradiente cônico recortado entre os dois raios.
    QConicalGradient gradient(origin, 0);
    for (int step = 0; step <= 6; ++step) {
        gradient.setColorAt(
            step / 6.0, QColor::fromHsvF(static_cast<float>(step % 6) / 6.0F, 1, 1));
    }
    QPainterPath ring;
    ring.addEllipse(origin, outerRadius(), outerRadius());
    ring.addEllipse(origin, innerRadius(), innerRadius());
    painter.fillPath(ring, gradient);

    // Triângulo saturação/valor (imagem em cache).
    if (m_triangleImage.isNull()) {
        rebuildTriangleImage();
    }
    painter.drawImage(0, 0, m_triangleImage);

    // Marcadores: círculos com contorno duplo, visíveis sobre qualquer cor.
    const auto drawMarker = [&](const QPointF& position) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 2));
        painter.drawEllipse(position, 5, 5);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(position, 5, 5);
    };
    drawMarker(ringPointForHue(m_hue));

    QPointF a;
    QPointF b;
    QPointF c;
    triangleVertices(a, b, c);
    const wheel::Barycentric weights = wheel::toBarycentric({ m_saturation, m_value });
    drawMarker(a * weights.hueWeight + b * weights.whiteWeight + c * weights.blackWeight);
}

// -----------------------------------------------------------------------------
// Mouse
// -----------------------------------------------------------------------------
void ColorWheel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
        return;
    }
    const QPointF position = event->position();
    const double distance = std::hypot(position.x() - center().x(), position.y() - center().y());
    if (distance >= innerRadius() && distance <= outerRadius() + 2) {
        m_drag = DragTarget::Ring;
    } else if (distance < innerRadius()) {
        m_drag = DragTarget::Triangle;
    } else {
        return; // fora do anel: ignora
    }
    m_button = event->button();
    pickAt(position);
}

void ColorWheel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drag != DragTarget::None) {
        pickAt(event->position());
    }
}

void ColorWheel::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_drag = DragTarget::None;
    m_button = Qt::NoButton;
}

void ColorWheel::pickAt(const QPointF& position)
{
    if (m_drag == DragTarget::Ring) {
        // Ângulo do ponto -> matiz (mesma convenção de angleOfHue).
        const double angle
            = std::atan2(-(position.y() - center().y()), position.x() - center().x());
        double degrees = angle / kDegreesToRadians;
        if (degrees < 0) {
            degrees += 360.0;
        }
        m_hue = static_cast<int>(std::lround(degrees)) % 360;
        rebuildTriangleImage();
    } else {
        // Dentro (ou arrastando para fora) do triângulo: projeta e converte.
        QPointF a;
        QPointF b;
        QPointF c;
        triangleVertices(a, b, c);
        const wheel::SaturationValue sv
            = wheel::toSaturationValue(wheel::barycentricOf(position, a, b, c));
        m_saturation = sv.saturation;
        m_value = sv.value;
    }
    update();
    emit colorPicked(color(), m_button);
}

} // namespace spritenator::gui
