cmake_minimum_required( VERSION 3.22 )

project( runtest C )

set( PARSON_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/parson )

include_directories( ${PARSON_ROOT} )

add_executable(runtest runtest.c ${PARSON_ROOT}/parson.c )