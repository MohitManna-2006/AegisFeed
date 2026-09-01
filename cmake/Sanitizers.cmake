option(AEGIS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(AEGIS_ENABLE_SANITIZERS AND
   NOT CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    message(
        FATAL_ERROR
        "AEGIS_ENABLE_SANITIZERS requires AppleClang, Clang, or GCC; "
        "${CMAKE_CXX_COMPILER_ID} is not supported"
    )
endif()

function(aegis_enable_sanitizers target_name)
    if(NOT AEGIS_ENABLE_SANITIZERS)
        return()
    endif()

    set(
        sanitizer_flags
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
    )

    target_compile_options("${target_name}" PRIVATE ${sanitizer_flags})
    target_link_options("${target_name}" PRIVATE ${sanitizer_flags})
endfunction()
