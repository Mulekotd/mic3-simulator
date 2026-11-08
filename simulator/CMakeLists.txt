cmake_minimum_required(VERSION 3.12)
project(mic3_simulator LANGUAGES CXX)

add_library(mic3_components INTERFACE)
target_compile_features(mic3_components INTERFACE cxx_std_17)
target_include_directories(mic3_components INTERFACE src)

add_executable(mic3_simulator src/main.cpp)
target_link_libraries(mic3_simulator PRIVATE mic3_components)

include(CTest)
if(BUILD_TESTING)
    add_executable(mic3_tests tests/components.cpp)
    target_link_libraries(mic3_tests PRIVATE mic3_components)
    add_test(NAME components COMMAND mic3_tests ${CMAKE_CURRENT_BINARY_DIR})

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/test_program.bin "ABCD")
    add_test(NAME simulator_cli
             COMMAND mic3_simulator ${CMAKE_CURRENT_BINARY_DIR}/test_program.bin --cycles 3)
    set_tests_properties(simulator_cli PROPERTIES
        PASS_REGULAR_EXPRESSION "loaded_words=1 cycles=3 PC=3 MPC=3 RAM_words=4096")
endif()
