# COIL_SANITIZE: semicolon-separated list of sanitizers applied to every target (e.g. "address;undefined").
set(COIL_SANITIZE "" CACHE STRING "Sanitizers to enable (Clang/GCC only), e.g. address;undefined")

if(COIL_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "COIL_SANITIZE is only supported with Clang and GCC")
    endif()
    list(JOIN COIL_SANITIZE "," coil_sanitizers)
    add_compile_options(-fsanitize=${coil_sanitizers} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${coil_sanitizers})
endif()
