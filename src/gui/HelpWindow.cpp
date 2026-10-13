// =============================================================================
// HelpWindow.cpp
// =============================================================================
#include "HelpWindow.h"

#include "ActionCatalog.h"

#include <spritenator/Version.h>

#include <QAction>
#include <QFile>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace spritenator::gui {

namespace {

/// Caminho do manual no sistema de recursos (ver src/gui/CMakeLists.txt).
constexpr auto kManualResource = ":/help/manual.html";

/// Marcadores substituídos no HTML do manual.
constexpr auto kVersionMarker = "{{VERSION}}";
constexpr auto kShortcutsMarker = "{{SHORTCUTS}}";

/// Texto da ação sem o "&" dos aceleradores de menu.
QString plainText(const QAction* action)
{
    QString text = action->text();
    text.remove(QLatin1Char('&'));
    return text.toHtmlEscaped();
}

} // namespace

HelpWindow::HelpWindow(const ActionCatalog* actions, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_browser(new QTextBrowser(this))
{
    setWindowTitle(tr("Spritenator Help"));
    m_browser->setOpenExternalLinks(true);
    m_browser->setHtml(buildHtml(actions));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_browser);
    resize(720, 640);
}

QString HelpWindow::buildHtml(const ActionCatalog* actions)
{
    QFile file(QString::fromLatin1(kManualResource));
    QString html = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll())
                                                  : tr("<p>The manual could not be loaded.</p>");

    // Tabela de atalhos montada a partir das ações reais.
    QString rows;
    for (std::size_t index = 0; index < kActionCount; ++index) {
        const QAction* action = actions->action(static_cast<ActionId>(index));
        if (action->shortcut().isEmpty()) {
            continue;
        }
        rows += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td></tr>")
                    .arg(action->shortcut().toString(QKeySequence::NativeText).toHtmlEscaped(),
                        plainText(action), action->statusTip().toHtmlEscaped());
    }
    const QString table = QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>"
                                         "<tr><th>%1</th><th>%2</th><th>%3</th></tr>%4</table>")
                              .arg(tr("Shortcut"), tr("Command"), tr("Description"), rows);

    const auto& versionText = version::kString;
    html.replace(QString::fromLatin1(kVersionMarker),
        QString::fromLatin1(versionText.data(), static_cast<qsizetype>(versionText.size())));
    html.replace(QString::fromLatin1(kShortcutsMarker), table);
    return html;
}

} // namespace spritenator::gui
