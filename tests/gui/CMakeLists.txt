# =============================================================================
# tests/gui/CMakeLists.txt — testes da camada de interface
# =============================================================================

spritenator_add_test(test_action_catalog
    SOURCES ActionCatalogTest.cpp
    LIBRARIES Spritenator::Gui)
