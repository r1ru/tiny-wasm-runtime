#pragma once

typedef int error_t;

#define IS_ERROR(err) ((err) < 0)

#define ERR_CODE(c) (-c)

#define ERR_SUCCESS                                     0
#define ERR_FAILED                                      ERR_CODE(1)

// validation error 
#define ERR_TYPE_MISMATCH                               ERR_CODE(2)
#define ERR_UNKNOWN_LOCAL                               ERR_CODE(3)
#define ERR_UNKNOWN_LABEL                               ERR_CODE(4)
#define ERR_UNKNOWN_FUNC                                ERR_CODE(5)
#define ERR_UNKNOWN_TABLE                               ERR_CODE(6)
#define ERR_UNKNOWN_TYPE                                ERR_CODE(7)
#define ERR_ALIGNMENT_MUST_NOT_BE_LARGER_THAN_NATURAL   ERR_CODE(8)

// runtime error
#define ERR_TRAP_INTERGER_DIVIDE_BY_ZERO                ERR_CODE(9)
#define ERR_TRAP_INTERGET_OVERFLOW                      ERR_CODE(10)
#define ERR_TRAP_INVALID_CONVERSION_TO_INTERGER         ERR_CODE(11)
#define ERR_TRAP_UNDEFINED_ELEMENT                      ERR_CODE(12)
#define ERR_TRAP_UNREACHABLE                            ERR_CODE(13)
#define ERR_TRAP_INDIRECT_CALL_TYPE_MISMATCH            ERR_CODE(14)
#define ERR_TRAP_UNINITIALIZED_ELEMENT                  ERR_CODE(15)
#define ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS            ERR_CODE(16)