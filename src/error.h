#pragma once

typedef int error_t;

#define IS_ERROR(err) ((err) < 0)

#define ERR_CODE(c) (-c)

#define ERR_SUCCESS                                             0
#define ERR_FAILED                                              ERR_CODE(1)

// decode error
#define ERR_UNEXPECTED_END                                      ERR_CODE(2)
#define ERR_LENGTH_OUT_OF_BOUNDS                                ERR_CODE(3)
#define ERR_MALFORMED_SECTION_ID                                ERR_CODE(4)
#define ERR_FUNCTION_AND_CODE_SECTION_HAVE_INCOSISTENT_LENGTH   ERR_CODE(5)
#define ERR_DATA_COUNT_AND_DATA_SECTION_HAVE_INCOSISTENT_LENGTH ERR_CODE(6)

// validation error 
#define ERR_TYPE_MISMATCH                                       ERR_CODE(7)
#define ERR_UNKNOWN_LOCAL                                       ERR_CODE(8)
#define ERR_UNKNOWN_LABEL                                       ERR_CODE(9)
#define ERR_UNKNOWN_FUNC                                        ERR_CODE(10)
#define ERR_UNKNOWN_TABLE                                       ERR_CODE(11)
#define ERR_UNKNOWN_TYPE                                        ERR_CODE(12)
#define ERR_UNKNOWN_GLOBAL                                      ERR_CODE(13)
#define ERR_UNKNOWN_MEMORY                                      ERR_CODE(14)
#define ERR_UNKNOWN_DATA_SEGMENT                                ERR_CODE(15)
#define ERR_UNKNOWN_ELEM_SEGMENT                                ERR_CODE(16)
#define ERR_INVALID_RESULT_ARITY                                ERR_CODE(17)
#define ERR_ALIGNMENT_MUST_NOT_BE_LARGER_THAN_NATURAL           ERR_CODE(18)
#define ERR_UNDECLARED_FUNCTIION_REFERENCE                      ERR_CODE(19)
#define ERR_MULTIPLE_MEMORIES                                   ERR_CODE(20)
#define ERR_GLOBAL_IS_IMMUTABLE                                 ERR_CODE(21)
#define ERR_CONSTANT_EXPRESSION_REQUIRED                        ERR_CODE(22)

// link error
#define ERR_UNKNOWN_IMPORT                                      ERR_CODE(23)

// runtime error
#define ERR_TRAP_INTERGER_DIVIDE_BY_ZERO                        ERR_CODE(24)
#define ERR_TRAP_INTERGET_OVERFLOW                              ERR_CODE(25)
#define ERR_TRAP_INVALID_CONVERSION_TO_INTERGER                 ERR_CODE(26)
#define ERR_TRAP_UNDEFINED_ELEMENT                              ERR_CODE(27)
#define ERR_TRAP_UNREACHABLE                                    ERR_CODE(28)
#define ERR_TRAP_INDIRECT_CALL_TYPE_MISMATCH                    ERR_CODE(29)
#define ERR_TRAP_UNINITIALIZED_ELEMENT                          ERR_CODE(30)
#define ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS                    ERR_CODE(31)
#define ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS                     ERR_CODE(32)
#define ERR_TRAP_CALL_STACK_EXHAUSTED                           ERR_CODE(33)
#define ERR_INCOMPATIBLE_IMPORT_TYPE                            ERR_CODE(34)