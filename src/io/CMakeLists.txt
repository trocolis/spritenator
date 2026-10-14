# =============================================================================
# src/io/CMakeLists.txt — leitura/gravação de imagens e paletas
#
# Depende do Qt Gui apenas para decodificar BMP/PNG (QImage); a gravação de
# BMP indexado e os formatos de paleta são implementados aqui mesmo.
# =============================================================================

add_library(spritenator_io STATIC
    Backup.cpp
    Backup.h
    BmpWriter.cpp
    BmpWriter.h
    GifFile.cpp
    GifFile.h
    ImageFile.cpp
    ImageFile.h
    IoError.h
    PaletteFile.cpp
    PaletteFile.h
    ProjectFile.cpp
    ProjectFile.h
)
add_library(Spritenator::Io ALIAS spritenator_io)

target_include_directories(spritenator_io PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(spritenator_io
    PUBLIC
        Qt6::Gui
        Spritenator::Core
)

spritenator_set_warnings(spritenator_io)
