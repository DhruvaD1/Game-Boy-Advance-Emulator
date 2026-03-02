cmake_minimum_required(VERSION 3.20)
project(gba_emu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SDL2 - try system first, fallback to extracted headers
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    # Use extracted headers
    set(SDL2_INCLUDE_DIRS "/tmp/sdl2-dev/usr/include/SDL2" "/tmp/sdl2-dev/usr/include/x86_64-linux-gnu")
    set(SDL2_LIBRARIES "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0")
    message(STATUS "Using extracted SDL2 headers from ${SDL2_INCLUDE_DIRS}")
endif()

# Gather sources
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(gba_emu ${SOURCES})

target_include_directories(gba_emu PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(gba_emu PRIVATE ${SDL2_LIBRARIES})

# Release: optimize hard
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(gba_emu PRIVATE -O2 -march=native -flto)
    target_link_options(gba_emu PRIVATE -flto)
endif()

# Warnings
target_compile_options(gba_emu PRIVATE -Wall -Wextra -Wno-unused-parameter)
