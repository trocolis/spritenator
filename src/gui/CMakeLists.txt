# =============================================================================
# src/gui/CMakeLists.txt — biblioteca com toda a interface gráfica (Qt Widgets)
#
# A GUI é uma biblioteca estática separada do executável para que possa ser
# exercitada por testes sem depender do main().
# =============================================================================

add_library(spritenator_gui STATIC
    ActionCatalog.cpp
    ActionCatalog.h
    AppSettings.cpp
    AppSettings.h
    CanvasView.cpp
    CanvasView.h
    CanvasWindow.cpp
    CanvasWindow.h
    Document.cpp
    Document.h
    IconProvider.cpp
    IconProvider.h
    ImageRendering.cpp
    ImageRendering.h
    MainWindow.cpp
    MainWindow.h
    dialogs/NewImageDialog.cpp
    dialogs/NewImageDialog.h
)
add_library(Spritenator::Gui ALIAS spritenator_gui)

# Os includes são relativos a src/ (ex.: #include "gui/MainWindow.h").
target_include_directories(spritenator_gui PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(spritenator_gui
    PUBLIC
        Qt6::Widgets
        Spritenator::Core
        Spritenator::Io
        Spritenator::Version
)

spritenator_set_warnings(spritenator_gui)

# -----------------------------------------------------------------------------
# Recursos embutidos: ícones. Usamos GLOB com CONFIGURE_DEPENDS para que basta
# substituir/adicionar PNGs em resources/icons — sem editar este arquivo.
# -----------------------------------------------------------------------------
file(GLOB SPRITENATOR_ICON_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/icons/*.png")

qt_add_resources(spritenator_gui "spritenator_icons"
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/resources/icons"
    FILES ${SPRITENATOR_ICON_FILES}
)
