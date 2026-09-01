function(aegis_enable_compiler_warnings target_name)
    get_target_property(target_type "${target_name}" TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(warning_scope INTERFACE)
    else()
        set(warning_scope PRIVATE)
    endif()

    if(MSVC)
        set(warning_flags /W4 /permissive-)
        if(AEGIS_WARNINGS_AS_ERRORS)
            list(APPEND warning_flags /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
        set(
            warning_flags
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
        )
        if(AEGIS_WARNINGS_AS_ERRORS)
            list(APPEND warning_flags -Werror)
        endif()
    else()
        message(WARNING "No warning profile is defined for ${CMAKE_CXX_COMPILER_ID}")
    endif()

    if(warning_flags)
        target_compile_options("${target_name}" ${warning_scope} ${warning_flags})
    endif()
endfunction()
