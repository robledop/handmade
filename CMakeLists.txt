cmake_minimum_required(VERSION 3.20)
project(handmade)
set(CMAKE_CXX_STANDARD 20)

if (LINUX)
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32/lib)
endif ()

file(GLOB_RECURSE SOURCE_FILES
        "src/*.cpp")

if (WIN32)
    add_definitions(-Zi)
endif ()

add_executable(handmade WIN32 ${SOURCE_FILES})

if (LINUX)
    target_link_libraries(handmade libuser32.a libgdi32.a)
endif ()

if (WIN32)
    target_link_libraries(handmade user32.lib gdi32.lib)
endif ()