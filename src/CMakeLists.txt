add_library(tiny_wasm_runtime SHARED decode.c exec.c list.c  vector.c validate.c)
target_link_libraries(tiny_wasm_runtime m)