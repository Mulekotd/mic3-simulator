cmake_minimum_required(VERSION 3.12)
project(mic3_assembler LANGUAGES CXX)

add_executable(assembler src/main.cpp)
target_compile_features(assembler PRIVATE cxx_std_17)
