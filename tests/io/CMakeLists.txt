# =============================================================================
# tests/io/CMakeLists.txt — testes de leitura/gravação
# =============================================================================

spritenator_add_test(test_image_file
    SOURCES ImageFileTest.cpp
    LIBRARIES Spritenator::Io)

spritenator_add_test(test_palette_file
    SOURCES PaletteFileTest.cpp
    LIBRARIES Spritenator::Io)

spritenator_add_test(test_backup
    SOURCES BackupTest.cpp
    LIBRARIES Spritenator::Io)

spritenator_add_test(test_animation_file
    SOURCES AnimationFileTest.cpp
    LIBRARIES Spritenator::Io)
