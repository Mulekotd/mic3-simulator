cmake_minimum_required(VERSION 3.12)
project(mic3_simulator LANGUAGES CXX)

add_library(mic3_components INTERFACE)
target_compile_features(mic3_components INTERFACE cxx_std_17)
target_include_directories(mic3_components INTERFACE src)

add_executable(mic3_simulator src/main.cpp)
target_link_libraries(mic3_simulator PRIVATE mic3_components)
