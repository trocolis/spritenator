// =============================================================================
// ToolOptionsTest.cpp — ferramentas do painel lateral (sem lupa e sem lápis
// grosso), opções do lápis (ponta e tamanho, [ e ]), quebra de linha das
// seções (FlowLayout) e ordem das seções do painel.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/Document.h"
#include "gui/EditOperations.h"
#include "gui/EditorSession.h"
#include "gui/IntegratorPanel.h"
#include "gui/MainWindow.h"
#include "gui/widgets/ColorEditor.h"
#include "gui/widgets/FlowLayout.h"
#include "gui/widgets/PenOptions.h"
#include "gui/widgets/ToolPanel.h"
#include "gui/widgets/WandOptions.h"

#include <QAction>
#include <QCoreApplication>
#include <QSettings>
#include <QSpinBox>
#include <QTest>
#include <QToolButton>

#include <memory>
#include <set>

using namespace spritenator;
using gui::ActionId;
using gui::EditorSession;
using gui::ToolKind;

class ToolOptionsTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("ToolOptionsTest"));
        QSettings().clear();
    }

    /// 14 ferramentas: sem a lupa e sem o lápis grosso.
    void panelHasFourteenTools()
    {
        EditorSession session;
        const gui::ToolPanel panel(&session);
        QCOMPARE(panel.findChildren<QToolButton*>().size(), 14);
        QVERIFY(panel.button(ToolKind::Pen) != nullptr);
        QVERIFY(panel.button(ToolKind::MagicWand) != nullptr);
    }

    /// As opções só aparecem com a ferramenta delas; a barra e a sessão
    /// ficam sincronizadas nos dois sentidos.
    void penOptionsFollowToolAndSession()
    {
        EditorSession session;
        QWidget parent;
        auto* pen = new gui::PenOptions(&session, &parent);
        auto* wand = new gui::WandOptions(&session, &parent);
        session.setToolKind(ToolKind::Pen);
        QVERIFY(!pen->isHidden());
        QVERIFY(wand->isHidden());
        session.setToolKind(ToolKind::MagicWand);
        QVERIFY(pen->isHidden());
        QVERIFY(!wand->isHidden());

        pen->roundButton()->click();
        QCOMPARE(session.brushShape(), core::BrushShape::Round);
        pen->sizeBox()->setValue(7);
        QCOMPARE(session.brushSize(), 7);
        session.setBrushShape(core::BrushShape::Square);
        session.setBrushSize(200); // limitado a 64
        QVERIFY(pen->squareButton()->isChecked());
        QCOMPARE(pen->sizeBox()->value(), 64);
    }

    /// O lápis pinta a ponta inteira em cada ponto do traço.
    void penPaintsWholeTip()
    {
        EditorSession session;
        gui::Document document(
            core::IndexedImage({ 16, 16 }, 0), core::Palette::makeDefault(core::ColorDepth::Bits8));
        session.setActiveDocument(&document);
        session.setToolKind(ToolKind::Pen);
        session.setDrawColor(9);
        session.setBrushSize(4);
        session.tool().press(session, { 5, 5 }, Qt::LeftButton);
        session.tool().release(session, { 5, 5 });
        int painted = 0;
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                painted += document.indexedCanvas().pixel(x, y) == 9 ? 1 : 0;
            }
        }
        QCOMPARE(painted, 16); // 4 x 4, de (4, 4) a (7, 7)
        QCOMPARE(int(document.indexedCanvas().pixel(4, 4)), 9);
        QCOMPARE(int(document.indexedCanvas().pixel(7, 7)), 9);

        // Um traço com ponta redonda de 3 px é contínuo e tem 3 px de altura.
        session.setBrushShape(core::BrushShape::Round);
        session.setBrushSize(3);
        session.setDrawColor(2);
        session.tool().press(session, { 2, 12 }, Qt::LeftButton);
        session.tool().move(session, { 12, 12 });
        session.tool().release(session, { 12, 12 });
        for (int x = 2; x <= 12; ++x) {
            QCOMPARE(int(document.indexedCanvas().pixel(x, 11)), 2);
            QCOMPARE(int(document.indexedCanvas().pixel(x, 13)), 2);
        }
        QCOMPARE(int(document.indexedCanvas().pixel(1, 11)), 0); // canto do "+"
        session.setActiveDocument(nullptr);
    }

    /// [ e ] mudam o tamanho da ponta (atalhos configuráveis).
    void bracketsChangePenSize()
    {
        gui::MainWindow window;
        EditorSession* session = window.session();
        QCOMPARE(window.actions()->action(ActionId::PenSizeIncrease)->shortcut(),
            QKeySequence(Qt::Key_BracketRight));
        session->setBrushSize(1);
        window.actions()->action(ActionId::PenSizeDecrease)->trigger();
        QCOMPARE(session->brushSize(), 1);
        window.actions()->action(ActionId::PenSizeIncrease)->trigger();
        window.actions()->action(ActionId::PenSizeIncrease)->trigger();
        QCOMPARE(session->brushSize(), 3);
    }

    /// FlowLayout: tudo numa linha com espaço; quebra quando estreita.
    void flowLayoutWraps()
    {
        QWidget container;
        auto* layout = new gui::FlowLayout(&container);
        std::vector<QToolButton*> buttons;
        for (int index = 0; index < 10; ++index) {
            auto* button = new QToolButton(&container);
            button->setFixedSize(20, 20);
            layout->addWidget(button);
            buttons.push_back(button);
        }
        const auto rows = [&](int width) {
            layout->setGeometry(QRect(0, 0, width, layout->heightForWidth(width)));
            std::set<int> tops;
            for (const QToolButton* button : buttons) {
                tops.insert(button->geometry().top());
            }
            return tops.size();
        };
        QCOMPARE(rows(400), std::size_t { 1 });
        QCOMPARE(rows(90), std::size_t { 3 }); // 4 por linha (4 x 20 + 3 de espaço)
        QVERIFY(layout->heightForWidth(90) > layout->heightForWidth(400));
        QCOMPARE(rows(10), std::size_t { 10 }); // estreito demais: um por linha
    }

    /// Ordem do painel lateral: ferramentas, opções, tons, transformações e
    /// cores, de cima para baixo.
    void sidePanelOrder()
    {
        QObject owner;
        gui::ActionCatalog actions(&owner);
        EditorSession session;
        gui::EditOperations operations(&session);
        gui::IntegratorPanel panel(&session, &actions, &operations);
        session.setToolKind(ToolKind::Pen);
        panel.resize(600, 900);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        const int tools = panel.findChild<gui::ToolPanel*>()->geometry().top();
        const int options = panel.findChild<gui::PenOptions*>()->geometry().top();
        const int tones = panel.findChild<gui::TonePanel*>()->geometry().top();
        const int colors = panel.findChild<gui::ColorEditor*>()->geometry().top();
        QVERIFY(tools < options);
        QVERIFY(options < tones);
        QVERIFY(tones < colors);
        // Largo: as 14 ferramentas cabem numa linha só.
        std::set<int> tops;
        for (const QToolButton* button :
            panel.findChild<gui::ToolPanel*>()->findChildren<QToolButton*>()) {
            tops.insert(button->geometry().top());
        }
        QCOMPARE(tops.size(), std::size_t { 1 });
    }
};

QTEST_MAIN(ToolOptionsTest)
#include "ToolOptionsTest.moc"
