# =============================================================================
# Spritenator - editor de sprites multiplataforma inspirado no "Character Maker 1999"
#
# Arquivo CMake raiz. Define a versão do projeto (fonte única de verdade para
# o versionamento X.Y.Z), as opções de build e inclui os subdiretórios.
# =============================================================================
cmake_minimum_required(VERSION 3.21)

# A versão declarada aqui é propagada para o código via Version.h (gerado) e
# para o empacotamento (CPack). Sempre que uma etapa é concluída, este número
# é incrementado seguindo o versionamento semântico (MAJOR.MINOR.PATCH).
project(Spritenator
    VERSION 3.0.0
    DESCRIPTION "Editor de sprites (cores indexadas e RGBA) inspirado no Character Maker 1999"
    LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Padrão da linguagem: C++20, sem extensões específicas de compilador, para
# garantir portabilidade entre GCC, Clang e MSVC.
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Exporta compile_commands.json para ferramentas como clangd e clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build padrão "Debug" quando nenhum tipo é informado (geradores de config única).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Tipo de build" FORCE)
endif()

# -----------------------------------------------------------------------------
# Opções do projeto
# -----------------------------------------------------------------------------
option(SPRITENATOR_BUILD_TESTS "Compila os testes automatizados" ON)
option(SPRITENATOR_WARNINGS_AS_ERRORS "Trata avisos do compilador como erros" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# -----------------------------------------------------------------------------
# Dependências: apenas o Qt 6 (Core, Gui, Widgets e as ferramentas de
# tradução LinguistTools). O módulo "core" do projeto propositalmente NÃO
# depende do Qt. A versão 6.7 é a mínima para a API qt_add_translations(TARGETS).
# -----------------------------------------------------------------------------
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
qt_standard_project_setup()

add_subdirectory(src)

if(SPRITENATOR_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Empacotamento (CPack): "cpack --config build/CPackConfig.cmake" gera
#   - Linux: .tar.gz (e .deb, se o dpkg estiver disponível)
#   - Windows: .zip
#   - macOS: .dmg
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "spritenator")
set(CPACK_PACKAGE_VENDOR "Spritenator")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Spritenator")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/\\.git/;/original/CharacterMaker1999")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ")
    find_program(SPRITENATOR_DPKG dpkg)
    if(SPRITENATOR_DPKG)
        list(APPEND CPACK_GENERATOR "DEB")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Spritenator")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    endif()
endif()

include(CPack)
