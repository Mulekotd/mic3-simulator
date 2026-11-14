cmake_minimum_required(VERSION 3.12)
project(mic1-simulator LANGUAGES CXX)

option(MIC1_ENABLE_GUI "Compilar a janela Raylib" ON)
option(BUILD_TESTING "Compilar testes opcionais" OFF)

add_library(mic1_components INTERFACE)
target_compile_features(mic1_components INTERFACE cxx_std_17)
target_include_directories(mic1_components INTERFACE src)

add_library(mic1_core STATIC src/MicroMachine.cpp)
target_link_libraries(mic1_core PUBLIC mic1_components)

add_executable(mic1-simulator src/main.cpp src/CLI.cpp)
target_link_libraries(mic1-simulator PRIVATE mic1_core)

if(MIC1_ENABLE_GUI)
    find_package(raylib QUIET)
    if(NOT raylib_FOUND AND NOT TARGET raylib)
        include(FetchContent)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(raylib URL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz)
        FetchContent_MakeAvailable(raylib)
    endif()
    target_link_libraries(mic1-simulator PRIVATE raylib)
    target_compile_definitions(mic1-simulator PRIVATE MIC1_HAS_GUI=1)
endif()

include(CTest)
if(BUILD_TESTING)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/components.cpp)
        add_executable(mic1_tests tests/components.cpp)
        target_link_libraries(mic1_tests PRIVATE mic1_components)
        add_test(NAME components COMMAND mic1_tests ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.py)
            add_test(NAME simulator_cli
                     COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.py
                             $<TARGET_FILE:mic1-simulator>)
        endif()
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../tools/microcode/build_rom.py)
            add_test(NAME microcode_rom
                     COMMAND ${Python3_EXECUTABLE}
                             ${CMAKE_CURRENT_SOURCE_DIR}/../tools/microcode/build_rom.py --check)
            set_tests_properties(microcode_rom PROPERTIES
                                 ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/..")
        endif()
    endif()
endif()
