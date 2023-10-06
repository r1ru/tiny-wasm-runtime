set(PARSON_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/parson)

include_directories(runtest PRIVATE ${PARSON_ROOT})

add_executable(runtest runtest.c ${PARSON_ROOT}/parson.c)