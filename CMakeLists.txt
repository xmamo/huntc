cmake_minimum_required(VERSION 3.13)

project(coogle VERSION 0.1.0 LANGUAGES C CXX)

add_executable(coogle main.cpp)

find_package(LLVM REQUIRED)
target_include_directories(coogle PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_directories(coogle PRIVATE ${LLVM_LIBRARY_DIRS})
target_link_libraries(coogle PRIVATE clang)
target_compile_options(coogle PRIVATE -pedantic-errors -Wall -Wextra)
