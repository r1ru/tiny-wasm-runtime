#pragma once

// ref: https://webassembly.github.io/spec/core/valid/index.html

#include "module.h"
#include "error.h"
#include "list.h"

// used in select instruction
#define TYPE_ANY    0

typedef struct {
    list_elem_t     link;
    resulttype_t    ty;
} labeltype_t;

typedef struct {
    VECTOR(functype_t)      types;
    VECTOR(functype_t)      funcs;
    VECTOR(tabletype_t)     tables;
    VECTOR(mem_t)           mems;
    VECTOR(globaltype_t)    globals;
    VECTOR(reftype_t)       elems;
    VECTOR(valtype_t)       locals;
    list_t                  labels;
    resulttype_t            *ret;
    VECTOR(funcidx_t)       refs;
} context_t;

error_t validate_module(module_t *mod);