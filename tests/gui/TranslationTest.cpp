// =============================================================================
// TranslationTest.cpp — tradução da interface para português do Brasil.
//
// Verifica que a tradução embutida é carregada e aplicada, que o arquivo .ts
// está completo e consistente (marcadores %1/%n, aceleradores "&", espaços) e
// que não há aceleradores repetidos dentro de um mesmo menu traduzido.
// =============================================================================
#include "gui/ActionCatalog.h"
#include "gui/CanvasEditor.h"
#include "gui/Document.h"
#include "gui/EditorSession.h"
#include "gui/HelpWindow.h"
#include "gui/MainWindow.h"
#include "gui/Translations.h"
#include "gui/commands/UndoLabel.h"
#include "gui/tools/Tool.h"
#include "io/ImageFile.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

#include <memory>

using namespace spritenator;
using spritenator::gui::ActionId;
using spritenator::gui::Translations;

namespace {

/// Marcadores de argumento (%1..%99 e %n), ordenados, para comparação.
QStringList placeholdersOf(const QString& text)
{
    static const QRegularExpression pattern(QStringLiteral("%(\\d+|n)"));
    QStringList found;
    for (auto it = pattern.globalMatch(text); it.hasNext();) {
        found.append(it.next().captured(0));
    }
    found.sort();
    return found;
}

/// Letra aceleradora de um texto de menu ("&Abrir" -> 'a'); nula se não houver.
QChar acceleratorOf(const QString& text)
{
    for (qsizetype index = 0; index + 1 < text.size(); ++index) {
        if (text.at(index) == QLatin1Char('&')) {
            if (text.at(index + 1) == QLatin1Char('&')) {
                ++index; // "&&" é um "&" literal
                continue;
            }
            return text.at(index + 1).toLower();
        }
    }
    return { };
}

/// Uma mensagem do arquivo .ts.
struct TsMessage {
    QString context;
    QString source;
    QStringList translations; ///< 1 item, ou várias formas de plural
    bool unfinished = false;
};

/// Lê todas as mensagens do .ts com QXmlStreamReader.
QList<TsMessage> readTsFile(const QString& path)
{
    QList<TsMessage> messages;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return messages;
    }
    QXmlStreamReader xml(&file);
    QString context;
    TsMessage current;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            if (xml.isEndElement() && xml.name() == QLatin1String("message")) {
                messages.append(current);
            }
            continue;
        }
        const auto name = xml.name();
        if (name == QLatin1String("name")) {
            context = xml.readElementText();
        } else if (name == QLatin1String("message")) {
            current = TsMessage { context, { }, { }, false };
        } else if (name == QLatin1String("source")) {
            current.source = xml.readElementText();
        } else if (name == QLatin1String("translation")) {
            current.unfinished
                = xml.attributes().value(QLatin1String("type")) == QLatin1String("unfinished");
            // Texto simples OU várias <numerusform> (plurais), uma por forma.
            QString text;
            QStringList forms;
            while (!(xml.isEndElement() && xml.name() == QLatin1String("translation"))) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1String("numerusform")) {
                    forms.append(xml.readElementText());
                } else if (xml.isCharacters()) {
                    text += xml.text();
                }
            }
            if (!forms.isEmpty()) {
                current.translations = forms;
            } else if (!text.isEmpty()) {
                current.translations.append(text);
            }
        }
    }
    return messages;
}

/// Todos os textos visíveis de widgets e ações abertos no momento.
QStringList visibleTexts()
{
    QStringList texts;
    for (QWidget* widget : QApplication::allWidgets()) {
        texts << widget->windowTitle() << widget->toolTip();
        if (const auto* label = qobject_cast<QLabel*>(widget)) {
            texts << label->text();
        }
        if (const auto* button = qobject_cast<QAbstractButton*>(widget)) {
            texts << button->text();
        }
        if (const auto* box = qobject_cast<QGroupBox*>(widget)) {
            texts << box->title();
        }
        for (const QAction* action : widget->actions()) {
            texts << action->text() << action->toolTip() << action->statusTip();
        }
    }
    texts.removeAll(QString());
    return texts;
}

/// Textos de um idioma que NÃO existem no outro (ex.: fontes em inglês cuja
/// tradução é diferente). "translated" escolhe o lado: true = português.
QSet<QString> exclusiveTexts(bool translated)
{
    QSet<QString> result;
    const QList<TsMessage> messages
        = readTsFile(QStringLiteral(SPRITENATOR_TRANSLATIONS_DIR "/spritenator_pt_BR.ts"));
    for (const TsMessage& message : messages) {
        for (const QString& translation : message.translations) {
            // Textos com marcadores não aparecem literalmente na interface.
            if (translation != message.source && !message.source.contains(QLatin1Char('%'))) {
                result.insert(translated ? translation : message.source);
            }
        }
    }
    // Textos idênticos nos dois idiomas não indicam nada.
    for (const TsMessage& message : messages) {
        for (const QString& translation : message.translations) {
            if (translated) {
                result.remove(message.source);
            } else {
                result.remove(translation);
            }
        }
    }
    return result;
}

} // namespace

class TranslationTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SpritenatorTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TranslationTest"));
        QVERIFY(Translations::apply(QLocale(QStringLiteral("pt_BR"))));
    }

    void portugueseIsAvailable()
    {
        QVERIFY(Translations::availableLanguages().contains(QStringLiteral("pt_BR")));
        QCOMPARE(Translations::localeFor(QString()), QLocale::system());
        QCOMPARE(Translations::localeFor(QStringLiteral("en")).language(), QLocale::English);
    }

    /// Textos de todas as camadas aparecem traduzidos.
    void stringsAreTranslated()
    {
        QObject owner;
        const gui::ActionCatalog actions(&owner);
        QCOMPARE(actions.action(ActionId::FileNew)->text(), QStringLiteral("&Novo..."));
        QCOMPARE(actions.action(ActionId::EditShadow)->text(), QStringLiteral("Preparar s&ombra"));
        QCOMPARE(io::ImageFile::saveFilters().first(), QStringLiteral("Imagem PNG (*.png)"));
        QCOMPARE(gui::UndoLabel { gui::UndoOperation::Paste }.text(), QStringLiteral("Colar"));
        QCOMPARE((gui::UndoLabel { gui::UndoOperation::EditColor, 7 }.text()),
            QStringLiteral("Editar cor 7"));
        QCOMPARE(QCoreApplication::translate("spritenator::gui::PaletteEditDialog",
                     "%n unused color(s) removed.", nullptr, 3),
            QStringLiteral("3 cores não usadas removidas."));
    }

    /// As traduções do próprio Qt também são carregadas (botões padrão dos
    /// diálogos). Se o pacote de traduções do Qt não estiver instalado no
    /// sistema, o teste é ignorado em vez de falhar.
    void qtStandardButtonsAreTranslated()
    {
        const QString save = QCoreApplication::translate("QPlatformTheme", "Save");
        if (save == QLatin1String("Save")) {
            QSKIP("qtbase_pt_BR.qm não está disponível neste sistema");
        }
        QCOMPARE(save, QStringLiteral("Salvar"));
    }

    /// O arquivo .ts está completo e cada tradução preserva marcadores,
    /// acelerador, quebras de linha e espaços nas bordas do original.
    void tsFileIsCompleteAndConsistent()
    {
        const QList<TsMessage> messages
            = readTsFile(QStringLiteral(SPRITENATOR_TRANSLATIONS_DIR "/spritenator_pt_BR.ts"));
        QVERIFY(messages.size() > 300);
        for (const TsMessage& message : messages) {
            const QByteArray where
                = (message.context + QStringLiteral(": ") + message.source).toUtf8();
            QVERIFY2(!message.unfinished, where.constData());
            QVERIFY2(!message.translations.isEmpty(), where.constData());
            for (const QString& translation : message.translations) {
                QStringList expected = placeholdersOf(message.source);
                QStringList actual = placeholdersOf(translation);
                // Formas de plural podem omitir o %n (ex.: "uma cor").
                if (!translation.contains(QStringLiteral("%n"))) {
                    expected.removeAll(QStringLiteral("%n"));
                }
                QVERIFY2(expected == actual, where.constData());
                QVERIFY2(
                    acceleratorOf(message.source).isNull() == acceleratorOf(translation).isNull(),
                    where.constData());
                QVERIFY2(
                    message.source.count(QLatin1Char('\n')) == translation.count(QLatin1Char('\n')),
                    where.constData());
                QVERIFY2(message.source.startsWith(QLatin1Char(' '))
                        == translation.startsWith(QLatin1Char(' ')),
                    where.constData());
            }
        }
    }

    /// Dentro de cada menu traduzido, nenhuma letra aceleradora se repete
    /// (senão Alt+letra ficaria ambíguo). Vale também para a barra de menus.
    void menuAcceleratorsAreUnique()
    {
        gui::MainWindow window;
        QList<QMenu*> menus;
        QSet<QChar> topLevel;
        for (QAction* menuAction : window.menuBar()->actions()) {
            const QChar letter = acceleratorOf(menuAction->text());
            QVERIFY2(!topLevel.contains(letter), qPrintable(menuAction->text()));
            topLevel.insert(letter);
            menus.append(menuAction->menu());
        }
        while (!menus.isEmpty()) {
            QMenu* menu = menus.takeFirst();
            QSet<QChar> used;
            for (QAction* item : menu->actions()) {
                if (item->isSeparator() || !item->isVisible()) {
                    continue;
                }
                if (item->menu() != nullptr) {
                    menus.append(item->menu()); // submenus (ex.: Idioma)
                }
                const QChar letter = acceleratorOf(item->text());
                if (letter.isNull()) {
                    continue;
                }
                QVERIFY2(!used.contains(letter),
                    qPrintable(menu->title() + QStringLiteral(" > ") + item->text()));
                used.insert(letter);
            }
        }
    }

    /// Com a interface em português, a ajuda usa o manual traduzido.
    void helpUsesPortugueseManual()
    {
        QObject owner;
        const gui::ActionCatalog actions(&owner);
        const QString html = gui::HelpWindow::buildHtml(&actions);
        QVERIFY(html.contains(QStringLiteral("Atalhos de teclado")));
        QVERIFY(html.contains(QStringLiteral("<th>Atalho</th>")));
        QVERIFY(html.contains(QStringLiteral("Preparar sombra")));
    }

    /// Troca de idioma com o programa aberto: janela principal, documento,
    /// animação, ajuda e histórico de desfazer passam a usar o novo idioma —
    /// e nenhum texto do idioma anterior permanece visível.
    void languageSwitchesWithoutRestart()
    {
        QVERIFY(!Translations::apply(QLocale(QStringLiteral("en"))));
        QCoreApplication::processEvents();

        gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.actions()->action(ActionId::HelpContents)->trigger();

        // Um passo de desfazer criado em inglês ("Pen") num documento aberto.
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("pen.png"));
        io::ImageFile::save(path,
            { core::IndexedImage({ 8, 8 }), core::Palette::makeDefault(core::ColorDepth::Bits8) });
        QVERIFY(window.openFile(path));
        gui::EditorSession* session = window.session();
        session->setToolKind(gui::ToolKind::Pen);
        session->tool().press(*session, { 1, 1 }, Qt::LeftButton);
        session->tool().release(*session, { 1, 1 });
        QCOMPARE(window.actions()->action(ActionId::EditUndo)->text(), QStringLiteral("&Undo Pen"));
        QCOMPARE(window.menuBar()->actions().first()->text(), QStringLiteral("&File"));
        // Editor de um documento sem nome ("Untitled<N>").
        auto canvas = std::make_unique<gui::CanvasEditor>(
            new gui::Document(
                core::IndexedImage({ 8, 8 }), core::Palette::makeDefault(core::ColorDepth::Bits8)),
            session);
        canvas->show();
        QVERIFY(canvas->windowTitle().startsWith(QStringLiteral("Untitled")));

        // --- Inglês -> português ---
        QVERIFY(Translations::apply(QLocale(QStringLiteral("pt_BR"))));
        QCoreApplication::processEvents(); // LanguageChange é postado às janelas
        QCOMPARE(window.menuBar()->actions().first()->text(), QStringLiteral("&Arquivo"));
        QCOMPARE(window.actions()->action(ActionId::FileNew)->text(), QStringLiteral("&Novo..."));
        QCOMPARE(window.actions()->action(ActionId::EditUndo)->text(),
            QStringLiteral("&Desfazer Lápis"));
        auto* timeline = window.findChild<QDockWidget*>(QStringLiteral("TimelineDock"));
        QVERIFY(timeline != nullptr);
        QCOMPARE(timeline->windowTitle(), QStringLiteral("Linha do tempo"));
        QVERIFY(canvas->windowTitle().startsWith(QStringLiteral("SemTitulo")));

        const QSet<QString> english = exclusiveTexts(false);
        for (const QString& text : visibleTexts()) {
            QVERIFY2(
                !english.contains(text), qPrintable(QStringLiteral("Ainda em inglês: ") + text));
        }

        // --- Português -> inglês ---
        QVERIFY(!Translations::apply(QLocale(QStringLiteral("en"))));
        QCoreApplication::processEvents();
        QCOMPARE(window.menuBar()->actions().first()->text(), QStringLiteral("&File"));
        QCOMPARE(window.actions()->action(ActionId::EditUndo)->text(), QStringLiteral("&Undo Pen"));
        const QSet<QString> portuguese = exclusiveTexts(true);
        for (const QString& text : visibleTexts()) {
            QVERIFY2(!portuguese.contains(text),
                qPrintable(QStringLiteral("Ainda em português: ") + text));
        }

        // Restaura o português para os demais testes desta suíte.
        QVERIFY(Translations::apply(QLocale(QStringLiteral("pt_BR"))));
        QCoreApplication::processEvents();
    }
};

QTEST_MAIN(TranslationTest)
#include "TranslationTest.moc"
