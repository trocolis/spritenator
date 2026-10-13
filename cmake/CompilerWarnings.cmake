# =============================================================================
# CompilerWarnings.cmake
#
# Fornece a função spritenator_set_warnings(<alvo>), que aplica um conjunto
# rigoroso de avisos ao alvo informado. Manter os avisos centralizados evita
# divergência de flags entre bibliotecas, executável e testes.
# =============================================================================

function(spritenator_set_warnings target)
    if(MSVC)
        # /W4: nível alto de avisos; /permissive-: conformidade estrita ao padrão.
        set(warnings /W4 /permissive- /utf-8)
        if(SPRITENATOR_WARNINGS_AS_ERRORS)
            list(APPEND warnings /WX)
        endif()
    else()
        # Conjunto comum a GCC e Clang.
        set(warnings
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough)
        if(SPRITENATOR_WARNINGS_AS_ERRORS)
            list(APPEND warnings -Werror)
        endif()
    endif()

    target_compile_options(${target} PRIVATE ${warnings})
endfunction()
