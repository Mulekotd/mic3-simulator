cmake_minimum_required(VERSION 3.12)
project(mic3_simulator LANGUAGES CXX)

add_library(mic3_components INTERFACE)
target_compile_features(mic3_components INTERFACE cxx_std_17)
target_include_directories(mic3_components INTERFACE src)

add_library(mic3_core STATIC src/MicroMachine.cpp)
target_link_libraries(mic3_core PUBLIC mic3_components)

add_executable(mic3_simulator src/main.cpp src/CLI.cpp src/io/ProtocolSession.cpp src/io/ProtocolServer.cpp)
target_link_libraries(mic3_simulator PRIVATE mic3_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(mic3_protocol_tests tests/protocol.cpp)
    target_link_libraries(mic3_protocol_tests PRIVATE mic3_components)
    add_test(NAME protocol COMMAND mic3_protocol_tests)

    add_executable(mic3_tests tests/components.cpp)
    target_link_libraries(mic3_tests PRIVATE mic3_components)
    add_test(NAME components COMMAND mic3_tests ${CMAKE_CURRENT_BINARY_DIR})

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_session.py)
            add_test(NAME protocol_session
                     COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_session.py
                             $<TARGET_FILE:mic3_simulator>)
        endif()
        add_test(NAME protocol_header
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_protocol_cpp.py --check)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.py)
            add_test(NAME simulator_cli
                     COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.py
                             $<TARGET_FILE:mic3_simulator>)
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
