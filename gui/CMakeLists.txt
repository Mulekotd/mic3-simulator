cmake_minimum_required(VERSION 3.24)
project(mic3_gui LANGUAGES C CXX)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)

find_package(Threads REQUIRED)
add_executable(mic3_gui src/main.cpp src/GuiApp.cpp)
target_compile_features(mic3_gui PRIVATE cxx_std_17)
target_include_directories(mic3_gui PRIVATE ../simulator/src)
target_link_libraries(mic3_gui PRIVATE raylib Threads::Threads)
