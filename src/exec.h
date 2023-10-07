#pragma once

#include "module.h"
#include "error.h"

// ref: https://webassembly.github.io/spec/core/exec/index.html
typedef export_t    exportinst_t;
typedef uint32_t    funcaddr_t;

// useful macro
#define VECTOR_COPY(dst, src, type)                             \
    do {                                                        \
        VECTOR_INIT((dst), (src)->n, type);                     \
        int idx = 0;                                            \
        VECTOR_FOR_EACH(e, (dst), type){                        \
            *e = *VECTOR_ELEM((src), idx++);                    \
        }                                                       \
    }while(0)

// In C, accessing outside the range of an array is not an exception.
// Therefore, VECTOR is used to have the number of elements. 
typedef struct {
    VECTOR(functype_t)      types;
    VECTOR(funcaddr_t)      fncaddrs;
    VECTOR(exportinst_t)    exports;
} moduleinst_t;

typedef struct {
    functype_t      *type;
    moduleinst_t    *module;
    func_t          *code;
} funcinst_t;

typedef struct {
    VECTOR(funcinst_t)  funcs;
} store_t;

typedef union {
    int32_t         int32;
} num_t;

typedef union {
    num_t           num;
} val_t;

typedef struct {
    VECTOR(val_t)   locals;
    moduleinst_t    *module;
} frame_t;

funcaddr_t  allocfunc(store_t *S, func_t *func, moduleinst_t *mod);
moduleinst_t *allocmodule(store_t *S, module_t *module);
error_t instantiate(store_t **store, module_t *module);
