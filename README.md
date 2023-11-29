# tiny-wasm-runtime
Unofficial reference WASM interpreter written in C. 
The purpose of this project is to promote understanding of [the specification](https://webassembly.github.io/spec/core/) by giving an implementation according to it. 
The priority is on clarity rather than efficiency and speed.

# Building
Clone this repository and run the following command. 
Make sure that [wabt](https://github.com/WebAssembly/wabt) is installed on the system.
```bash
$ cmake -S . -B build
```

# Testing
The interpreter is tested with [this testsuite](https://github.com/RI5255/testsuite), which differs somewhat from [the official one](https://github.com/WebAssembly/spec/tree/main/test/core). 
This is because the official testsuite contains some test cases depending on the spec interpreter implementation.
The interpreter can currently pass all core tests except those related to utf8 and WAT.
To run the test, execute the following command.
```bash
$ ctest --test-dir build
```