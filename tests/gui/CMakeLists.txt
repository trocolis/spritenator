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

spritenator_add_test(test_app_settings
    SOURCES AppSettingsTest.cpp
    LIBRARIES Spritenator::Gui)

spritenator_add_test(test_translation
    SOURCES TranslationTest.cpp
    LIBRARIES Spritenator::Gui)
# Pasta dos .ts, para o teste verificar a integridade das traduções.
target_compile_definitions(test_translation PRIVATE
    SPRITENATOR_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/translations")

spritenator_add_test(test_rgba_editing
    SOURCES RgbaEditingTest.cpp
    LIBRARIES Spritenator::Gui)

spritenator_add_test(test_color_picker
    SOURCES ColorPickerTest.cpp
    LIBRARIES Spritenator::Gui)
