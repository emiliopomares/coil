# Applies the ADR-0002 warning set, as errors, to <target>.
function(coil_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor
            -Wold-style-cast -Woverloaded-virtual -Werror)
    endif()
endfunction()
