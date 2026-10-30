# es_set_warnings(<target>)
# Applies the project-wide warning set to a target. Warnings are kept at a
# reasonable level while the legacy code base is being modernised; enable
# ES_WARNINGS_AS_ERRORS once a module is clean.

function(es_set_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
            -Wno-sign-compare
        )
        if(ES_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
