# =============================================================================
# src/core/CMakeLists.txt — núcleo em C++ puro (sem dependência do Qt)
#
# Contém o modelo de dados (imagem indexada, paleta, geometria) e, nas etapas
# seguintes, os algoritmos de desenho e transformação. Por não depender de
# nenhuma biblioteca externa, é fácil de testar e reaproveitar.
# =============================================================================

add_library(spritenator_core STATIC
    CharacterLayout.h
    Color.h
    Geometry.h
    IndexedImage.cpp
    IndexedImage.h
    Palette.cpp
    Palette.h
)
add_library(Spritenator::Core ALIAS spritenator_core)

# Includes relativos a src/ (ex.: #include "core/Palette.h").
target_include_directories(spritenator_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(spritenator_core PUBLIC cxx_std_20)

spritenator_set_warnings(spritenator_core)
