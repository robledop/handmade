cmake_minimum_required(VERSION 3.20)
project(handmade)

# mingw-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32/lib)

set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} ")

set(SOURCE_FILES
        src/win32_handmade.cpp
)

#set(LOGIC_SOURCE_FILES
#        debug_stuff.c
#        logic.c
#        tile.c)

IF (APPLE)
    #SET(GUI_TYPE MACOSX_BUNDLE)
    FIND_LIBRARY(SDL2_LIBRARY SDL2 sdl)
    INCLUDE_DIRECTORIES(sdl/SDL2.framework/Headers)
    MARK_AS_ADVANCED(SDL2_LIBRARY)
    SET(EXTRA_LIBS ${SDL2_LIBRARY})
ENDIF (APPLE)

#add_definitions(-Zi)
#add_definitions(-std=c11)
#add_definitions(-g)
#add_definitions(-Wall)
#add_definitions(-O0)
#add_definitions(-msse4.2)
#add_definitions(-Wno-unused-variable)

add_executable(handmade WIN32 ${SOURCE_FILES})
if (LINUX)
    target_link_libraries(handmade libuser32.a libgdi32.a)
endif ()
if (WIN32)
    target_link_libraries(handmade user32.lib gdi32.lib)
endif ()
#add_library(herologic MODULE ${LOGIC_SOURCE_FILES})
#target_link_libraries(hero ${EXTRA_LIBS})
#target_link_libraries(herologic ${EXTRA_LIBS})
