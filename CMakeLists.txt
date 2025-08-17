cmake_minimum_required(VERSION 3.20)
project(handmade)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCE_FILES
        "src/*.cpp")

add_executable(handmade WIN32 ${SOURCE_FILES})

if (LINUX)
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32/lib)

    add_compile_options(-g)
    target_link_libraries(handmade
            libuser32.a
            libgdi32.a
            -static-libgcc
            -static-libstdc++)
endif ()

if (WIN32)
    add_compile_options(-Zi)
    target_link_libraries(handmade user32.lib gdi32.lib)
endif ()

# Copy compile_commands.json to the root directory.
# This is useful for tools like clangd and VSCode.
add_custom_command(TARGET handmade POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json)
