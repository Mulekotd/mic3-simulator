cmake_minimum_required(VERSION 3.12)
project(mic1_assembler LANGUAGES CXX)

add_executable(assembler
    src/main.cpp
    src/Assembler.cpp
    src/ProgramWriter.cpp)
target_compile_features(assembler PRIVATE cxx_std_17)
