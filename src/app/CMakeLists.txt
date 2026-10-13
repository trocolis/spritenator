# =============================================================================
# src/app/CMakeLists.txt — executável final e regras de instalação
# =============================================================================

set(SPRITENATOR_APP_SOURCES main.cpp)

# Windows: ícone e informações de versão do .exe (arquivo .rc gerado).
if(WIN32)
    configure_file(spritenator.rc.in "${CMAKE_CURRENT_BINARY_DIR}/spritenator.rc" @ONLY)
    list(APPEND SPRITENATOR_APP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/spritenator.rc")
endif()

# macOS: ícone do pacote (.app), copiado para Contents/Resources.
if(APPLE)
    set(SPRITENATOR_ICNS "${PROJECT_SOURCE_DIR}/resources/app_icon.icns")
    set_source_files_properties("${SPRITENATOR_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND SPRITENATOR_APP_SOURCES "${SPRITENATOR_ICNS}")
endif()

qt_add_executable(spritenator WIN32 MACOSX_BUNDLE ${SPRITENATOR_APP_SOURCES})

target_link_libraries(spritenator PRIVATE Spritenator::Gui)
spritenator_set_warnings(spritenator)

set_target_properties(spritenator PROPERTIES
    OUTPUT_NAME "spritenator"
    MACOSX_BUNDLE_BUNDLE_NAME "Spritenator"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.spritenator"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "app_icon.icns"
)

# -----------------------------------------------------------------------------
# Instalação
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS spritenator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Linux: atalho no menu de aplicativos e ícone em vários tamanhos (hicolor).
if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/resources/linux/spritenator.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    foreach(size 16 32 48 64 128 256)
        install(FILES "${PROJECT_SOURCE_DIR}/resources/linux/icons/${size}.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME spritenator.png)
    endforeach()
endif()

# Windows/macOS: copia as bibliotecas e plugins do Qt para junto do
# executável (windeployqt/macdeployqt), gerando um pacote autossuficiente.
# Disponível a partir do Qt 6.3; em outras plataformas não faz nada.
if(COMMAND qt_generate_deploy_app_script AND (WIN32 OR APPLE))
    qt_generate_deploy_app_script(
        TARGET spritenator
        OUTPUT_SCRIPT spritenator_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${spritenator_deploy_script})
endif()
