add_executable(cmd cmd.c decode.c exec.c list.c validate.c)
add_library(tiny_wasm_runtime SHARED decode.c exec.c list.c validate.c)