# =============================================================================
# tests/CMakeLists.txt — testes automatizados (Qt Test + CTest)
#
# Cada arquivo de teste vira um executável independente registrado no CTest.
# Execute todos com:  ctest --test-dir build --output-on-failure
# =============================================================================

# spritenator_add_test(<nome> SOURCES <arquivos...> LIBRARIES <alvos...>)
function(spritenator_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBRARIES})
    spritenator_set_warnings(${name})

    add_test(NAME ${name} COMMAND ${name})

    # Plataforma "offscreen": permite rodar testes de GUI sem servidor gráfico
    # (útil em integração contínua).
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_subdirectory(core)
add_subdirectory(gui)
