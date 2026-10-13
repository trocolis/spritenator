# =============================================================================
# Spritenator - recriação multiplataforma do "Character Maker 1999"
#
# Arquivo CMake raiz. Define a versão do projeto (fonte única de verdade para
# o versionamento X.Y.Z), as opções de build e inclui os subdiretórios.
# =============================================================================
cmake_minimum_required(VERSION 3.21)

# A versão declarada aqui é propagada para o código via Version.h (gerado) e
# para o empacotamento (CPack). Sempre que uma etapa é concluída, este número
# é incrementado seguindo o versionamento semântico (MAJOR.MINOR.PATCH).
project(Spritenator
    VERSION 0.7.0
    DESCRIPTION "Editor de sprites com cores indexadas inspirado no Character Maker 1999"
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
# Dependências: apenas o Qt 6 (Core, Gui e Widgets). O módulo "core" do
# projeto propositalmente NÃO depende do Qt.
# -----------------------------------------------------------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

add_subdirectory(src)

if(SPRITENATOR_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
