# =============================================================================
# tests/gui/CMakeLists.txt — testes da camada de interface
# =============================================================================

spritenator_add_test(test_action_catalog
    SOURCES ActionCatalogTest.cpp
    LIBRARIES Spritenator::Gui)

spritenator_add_test(test_document
    SOURCES DocumentTest.cpp
    LIBRARIES Spritenator::Gui)

spritenator_add_test(test_editor_session
    SOURCES EditorSessionTest.cpp
    LIBRARIES Spritenator::Gui)

spritenator_add_test(test_edit_operations
    SOURCES EditOperationsTest.cpp
    LIBRARIES Spritenator::Gui)

spritenator_add_test(test_main_window
    SOURCES MainWindowTest.cpp
    LIBRARIES Spritenator::Gui)
