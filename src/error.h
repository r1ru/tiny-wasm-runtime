#pragma once

typedef int error_t;

#define IS_ERROR(err) ((err) < 0)

#define ERR_CODE(c) (-c)

#define ERR_SUCCESS                         0
#define ERR_FAILED                          ERR_CODE(1)

// validation error 
#define ERR_TYPE_MISMATCH                   ERR_CODE(2)

// runtime error
#define ERR_TRAP_INTERGER_DIVIDE_BY_ZERO    ERR_CODE(3)
#define ERR_TRAP_INTERGET_OVERFLOW          ERR_CODE(4)