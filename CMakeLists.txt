cmake_minimum_required(VERSION 3.20)
project(handmade)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if (LINUX)
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.20
            GIT_PROGRESS TRUE
    )

    FetchContent_Declare(
            SDL3_IMAGE
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
            GIT_TAG release-3.2.4
            GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(SDL3 SDL3_IMAGE)

    if (SDL)
        add_executable(handmade src/sdl_handmade.cpp)

        set(SDL_LIBRARIES ${SDL_LIBRARIES} SDL3main SDL3-static SDL3_image-static)
        target_include_directories("${PROJECT_NAME}" PRIVATE include)
        target_link_libraries("${PROJECT_NAME}" PRIVATE ${SDL_LIBRARIES})
        include_directories(${SDL3_SOURCE_DIR}/include ${SDL3_SOURCE_DIR}/include/SDL3 ${SDL3_IMAGE_SOURCE_DIR}/include)
    else ()
        set(CMAKE_SYSTEM_NAME Windows)
        set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
        set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
        set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
        set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32/lib)
        add_executable(handmade WIN32 src/win32_handmade.cpp)

        target_link_libraries(handmade
                libuser32.a
                libgdi32.a
                -static-libgcc
                -static-libstdc++)
    endif ()

    add_compile_options(-g)
endif ()

if (WIN32)
    add_executable(handmade WIN32 src/win32_handmade.cpp)
    add_compile_options(-Zi)
    target_link_libraries(handmade user32.lib gdi32.lib)
endif ()

# Copy compile_commands.json to the root directory.
# This is useful for tools like clangd and VSCode.
add_custom_command(TARGET handmade POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json)
