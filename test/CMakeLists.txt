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
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/comments.wast -o ${CMAKE_CURRENT_BINARY_DIR}/comments
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/type.wast -o ${CMAKE_CURRENT_BINARY_DIR}/type
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/inline-module.wast -o ${CMAKE_CURRENT_BINARY_DIR}/inline-module
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/int_literals.wast -o ${CMAKE_CURRENT_BINARY_DIR}/int_literals
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/i32.wast -o ${CMAKE_CURRENT_BINARY_DIR}/i32
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/i64.wast -o ${CMAKE_CURRENT_BINARY_DIR}/i64
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/int_exprs.wast -o ${CMAKE_CURRENT_BINARY_DIR}/int_exprs
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
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/const.wast -o ${CMAKE_CURRENT_BINARY_DIR}/const
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/local_get.wast -o ${CMAKE_CURRENT_BINARY_DIR}/local_get
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/local_set.wast -o ${CMAKE_CURRENT_BINARY_DIR}/local_set
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/labels.wast -o ${CMAKE_CURRENT_BINARY_DIR}/labels
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/switch.wast -o ${CMAKE_CURRENT_BINARY_DIR}/switch
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/store.wast -o ${CMAKE_CURRENT_BINARY_DIR}/store
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/block.wast -o ${CMAKE_CURRENT_BINARY_DIR}/block
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/br.wast -o ${CMAKE_CURRENT_BINARY_DIR}/br
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/br_if.wast -o ${CMAKE_CURRENT_BINARY_DIR}/br_if
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/br_table.wast -o ${CMAKE_CURRENT_BINARY_DIR}/br_table
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/call.wast -o ${CMAKE_CURRENT_BINARY_DIR}/call
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/call_indirect.wast -o ${CMAKE_CURRENT_BINARY_DIR}/call_indirect
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/return.wast -o ${CMAKE_CURRENT_BINARY_DIR}/return
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/if.wast -o ${CMAKE_CURRENT_BINARY_DIR}/if
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/loop.wast -o ${CMAKE_CURRENT_BINARY_DIR}/loop
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/load.wast -o ${CMAKE_CURRENT_BINARY_DIR}/load
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/local_tee.wast -o ${CMAKE_CURRENT_BINARY_DIR}/local_tee
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/func.wast -o ${CMAKE_CURRENT_BINARY_DIR}/func
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/endianness.wast -o ${CMAKE_CURRENT_BINARY_DIR}/endianness
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/align.wast -o ${CMAKE_CURRENT_BINARY_DIR}/align
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/left-to-right.wast -o ${CMAKE_CURRENT_BINARY_DIR}/left-to-right
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/stack.wast -o ${CMAKE_CURRENT_BINARY_DIR}/stack
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/unreachable.wast -o ${CMAKE_CURRENT_BINARY_DIR}/unreachable
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/unreached-valid.wast -o ${CMAKE_CURRENT_BINARY_DIR}/unreached-valid
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/unreached-invalid.wast -o ${CMAKE_CURRENT_BINARY_DIR}/unreached-invalid
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/unwind.wast -o ${CMAKE_CURRENT_BINARY_DIR}/unwind
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/ref_null.wast -o ${CMAKE_CURRENT_BINARY_DIR}/ref_null
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/traps.wast -o ${CMAKE_CURRENT_BINARY_DIR}/traps
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/table-sub.wast -o ${CMAKE_CURRENT_BINARY_DIR}/table-sub
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/table_set.wast -o ${CMAKE_CURRENT_BINARY_DIR}/table_set
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/table_get.wast -o ${CMAKE_CURRENT_BINARY_DIR}/table_get
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/ref_is_null.wast -o ${CMAKE_CURRENT_BINARY_DIR}/ref_is_null
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/table_fill.wast -o ${CMAKE_CURRENT_BINARY_DIR}/table_fill
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/table_grow.wast -o ${CMAKE_CURRENT_BINARY_DIR}/table_grow
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/table_size.wast -o ${CMAKE_CURRENT_BINARY_DIR}/table_size
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/address.wast -o ${CMAKE_CURRENT_BINARY_DIR}/address
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/float_exprs.wast -o ${CMAKE_CURRENT_BINARY_DIR}/float_exprs
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/float_memory.wast -o ${CMAKE_CURRENT_BINARY_DIR}/float_memory
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_redundancy.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_redundancy
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_fill.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_fill
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_copy.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_copy
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_init.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_init
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_grow.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_grow
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_size.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_size
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/memory_trap.wast -o ${CMAKE_CURRENT_BINARY_DIR}/memory_trap
    COMMAND wast2json ${CMAKE_CURRENT_SOURCE_DIR}/testsuite/nop.wast -o ${CMAKE_CURRENT_BINARY_DIR}/nop
)

add_test(
    NAME comments
    COMMAND runtest comments
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME type
    COMMAND runtest type
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME inline-module
    COMMAND runtest inline-module
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
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
    NAME int_exprs
    COMMAND runtest int_exprs
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

add_test(
    NAME const
    COMMAND runtest const
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME local_get
    COMMAND runtest local_get
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME local_set
    COMMAND runtest local_set
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME labels
    COMMAND runtest labels
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME switch
    COMMAND runtest switch
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME store
    COMMAND runtest store
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME block
    COMMAND runtest block
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME br
    COMMAND runtest br
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME br_if
    COMMAND runtest br_if
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME br_table
    COMMAND runtest br_table
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME call
    COMMAND runtest call
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME call_indirect
    COMMAND runtest call_indirect
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME return
    COMMAND runtest return
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME if
    COMMAND runtest if
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME load
    COMMAND runtest load
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME local_tee
    COMMAND runtest local_tee
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME func
    COMMAND runtest func
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME endianness
    COMMAND runtest endianness
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME align
    COMMAND runtest align
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME left-to-right
    COMMAND runtest left-to-right
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME unreachable
    COMMAND runtest unreachable
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(
    NAME unreached-valid
    COMMAND runtest unreached-valid
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME unreached-invalid
    COMMAND runtest unreached-invalid
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME unwind
    COMMAND runtest unwind
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME ref_null
    COMMAND runtest ref_null
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME traps
    COMMAND runtest traps
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME table-sub
    COMMAND runtest table-sub
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME table_set
    COMMAND runtest table_set
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME table_get
    COMMAND runtest table_get
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME ref_is_null
    COMMAND runtest ref_is_null
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME table_fill
    COMMAND runtest table_fill
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME table_grow
    COMMAND runtest table_grow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME table_size
    COMMAND runtest table_size
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME address
    COMMAND runtest address
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME float_exprs
    COMMAND runtest float_exprs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME float_memory
    COMMAND runtest float_memory
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_redundancy
    COMMAND runtest memory_redundancy
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_fill
    COMMAND runtest memory_fill
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_copy
    COMMAND runtest memory_copy
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_init
    COMMAND runtest memory_init
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_grow
    COMMAND runtest memory_grow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_size
    COMMAND runtest memory_size
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME memory_trap
    COMMAND runtest memory_trap
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME nop
    COMMAND runtest nop
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)