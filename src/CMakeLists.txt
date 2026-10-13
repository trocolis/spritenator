# =============================================================================
# src/CMakeLists.txt
#
# Organização em camadas (cada uma só depende das camadas abaixo dela):
#
#   app   -> executável "spritenator" (apenas o main)
#   gui   -> janelas, diálogos e widgets Qt
#   io    -> leitura/gravação de imagens e paletas (usa QImage do Qt Gui)
#   core  -> modelo de dados e algoritmos em C++ puro (sem Qt)
# =============================================================================

# -----------------------------------------------------------------------------
# Cabeçalho de versão gerado a partir de project(VERSION ...). Fica disponível
# como "spritenator/Version.h" para qualquer alvo que ligue spritenator_version.
# -----------------------------------------------------------------------------
set(SPRITENATOR_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in"
    "${SPRITENATOR_GENERATED_DIR}/spritenator/Version.h"
    @ONLY)

add_library(spritenator_version INTERFACE)
add_library(Spritenator::Version ALIAS spritenator_version)
target_include_directories(spritenator_version INTERFACE "${SPRITENATOR_GENERATED_DIR}")

add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(app)
