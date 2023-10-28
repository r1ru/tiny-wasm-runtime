#pragma once

// ref: https://webassembly.github.io/spec/core/valid/index.html

#include "module.h"
#include "error.h"
#include "list.h"

typedef struct {
    list_elem_t     link;
    resulttype_t    ty;
} labeltype_t;

typedef struct {
    VECTOR(functype_t)      types;
    VECTOR(functype_t)      funcs;
    VECTOR(mem_t)           mems;
    VECTOR(valtype_t)       locals;
    list_t                  labels;
    resulttype_t            *ret;
} context_t;

error_t validate_module(module_t *mod);