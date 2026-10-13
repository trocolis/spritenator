// =============================================================================
// AnimationTest.cpp — testes do modelo de animação.
// =============================================================================
#include "core/Animation.h"

#include <QTest>

using namespace spritenator::core;

class AnimationTest final : public QObject {
    Q_OBJECT

private slots:
    void waitIsClamped()
    {
        Animation animation;
        QCOMPARE(animation.waitMs(), Animation::kDefaultWaitMs);
        animation.setWaitMs(10);
        QCOMPARE(animation.waitMs(), Animation::kMinWaitMs);
        animation.setWaitMs(5000);
        QCOMPARE(animation.waitMs(), Animation::kMaxWaitMs);
    }

    void framesLoopAround()
    {
        Animation animation;
        QCOMPARE(animation.nextPosition(0), std::size_t { 0 }); // vazia
        animation.addFrame({ 0, 0, 24, 32 });
        animation.addFrame({ 24, 0, 24, 32 });
        QCOMPARE(animation.nextPosition(0), std::size_t { 1 });
        QCOMPARE(animation.nextPosition(1), std::size_t { 0 });
        animation.removeFrame(0);
        animation.removeFrame(99); // posição inválida: ignorada
        QCOMPARE(animation.size(), std::size_t { 1 });
        QCOMPARE(animation.frames().front().x, 24);
    }

    /// Ciclo de caminhada: 1-2-3-2 dentro do bloco de 3 células da direção.
    void walkCycleUsesDirectionBlock()
    {
        // Célula do meio do SEGUNDO personagem da folha (colunas 3-5).
        const std::vector<Rect> frames = walkCycleFrames({ 96, 64, 24, 32 });
        QCOMPARE(frames.size(), std::size_t { 4 });
        QCOMPARE(frames.at(0), (Rect { 72, 64, 24, 32 }));
        QCOMPARE(frames.at(1), (Rect { 96, 64, 24, 32 }));
        QCOMPARE(frames.at(2), (Rect { 120, 64, 24, 32 }));
        QCOMPARE(frames.at(3), frames.at(1));
        QVERIFY(walkCycleFrames({ }).empty());
    }
};

QTEST_APPLESS_MAIN(AnimationTest)
#include "AnimationTest.moc"
