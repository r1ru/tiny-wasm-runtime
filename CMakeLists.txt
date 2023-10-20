cmake_minimum_required(VERSION 3.22)

project(tiny-wasm-runtime C)

enable_testing()

add_subdirectory(src)
add_subdirectory(test)