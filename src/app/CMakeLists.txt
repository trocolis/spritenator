# =============================================================================
# src/app/CMakeLists.txt — executável final
# =============================================================================

qt_add_executable(spritenator WIN32 MACOSX_BUNDLE
    main.cpp
)

target_link_libraries(spritenator PRIVATE Spritenator::Gui)
spritenator_set_warnings(spritenator)

set_target_properties(spritenator PROPERTIES
    OUTPUT_NAME "spritenator"
    MACOSX_BUNDLE_BUNDLE_NAME "Spritenator"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

install(TARGETS spritenator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
