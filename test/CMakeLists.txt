set(PARSON_ROOT ${PROJECT_SOURCE_DIR}/third_party/parson)

include_directories(
    runtest 
    PLIVATE ${PARSON_ROOT}
    PLIVATE ${PROJECT_SOURCE_DIR}/src
)

add_executable(runtest runtest.c ${PARSON_ROOT}/parson.c)
target_link_libraries(runtest tiny_wasm_runtime)

add_custom_target(
    tests ALL
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/int_literals.wast -o ${CMAKE_CURRENT_BINARY_DIR}/int_literals
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/i32.wast -o ${CMAKE_CURRENT_BINARY_DIR}/i32
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/i64.wast -o ${CMAKE_CURRENT_BINARY_DIR}/i64
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/f32.wast -o ${CMAKE_CURRENT_BINARY_DIR}/f32
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/f32_bitwise.wast -o ${CMAKE_CURRENT_BINARY_DIR}/f32_bitwise
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/f32_cmp.wast -o ${CMAKE_CURRENT_BINARY_DIR}/f32_cmp
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/f64.wast -o ${CMAKE_CURRENT_BINARY_DIR}/f64
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/f64_bitwise.wast -o ${CMAKE_CURRENT_BINARY_DIR}/f64_bitwise
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/f64_cmp.wast -o ${CMAKE_CURRENT_BINARY_DIR}/f64_cmp
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/float_misc.wast -o ${CMAKE_CURRENT_BINARY_DIR}/float_misc
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/fac.wast -o ${CMAKE_CURRENT_BINARY_DIR}/fac
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/conversions.wast -o ${CMAKE_CURRENT_BINARY_DIR}/conversions
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/float_literals.wast -o ${CMAKE_CURRENT_BINARY_DIR}/float_literals
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/forward.wast -o ${CMAKE_CURRENT_BINARY_DIR}/forward
)

add_test(
    NAME int_literals
    COMMAND runtest int_literals
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME i32
    COMMAND runtest i32
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME i64
    COMMAND runtest i64
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME f32
    COMMAND runtest f32
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME f32_bitwise
    COMMAND runtest f32_bitwise
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME f32_cmp
    COMMAND runtest f32_cmp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME f64
    COMMAND runtest f64
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME f64_bitwise
    COMMAND runtest f64_bitwise
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME f64_cmp
    COMMAND runtest f64_cmp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME float_misc
    COMMAND runtest float_misc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME fac
    COMMAND runtest fac
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME conversions
    COMMAND runtest conversions
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME float_literals
    COMMAND runtest float_literals
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME forward
    COMMAND runtest forward
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)