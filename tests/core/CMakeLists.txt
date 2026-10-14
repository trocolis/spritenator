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

spritenator_add_test(test_raster
    SOURCES RasterTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_transform
    SOURCES TransformTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_palette_ops
    SOURCES PaletteOpsTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_animation
    SOURCES AnimationTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_rgba_image
    SOURCES RgbaImageTest.cpp
    LIBRARIES Spritenator::Core)

spritenator_add_test(test_selection
    SOURCES SelectionTest.cpp
    LIBRARIES Spritenator::Core)
