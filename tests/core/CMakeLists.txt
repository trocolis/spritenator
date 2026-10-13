# =============================================================================
# tests/core/CMakeLists.txt — testes do núcleo
# =============================================================================

spritenator_add_test(test_geometry
    SOURCES GeometryTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_palette
    SOURCES PaletteTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_indexed_image
    SOURCES IndexedImageTest.cpp
    LIBRARIES Spritenator::Core)
