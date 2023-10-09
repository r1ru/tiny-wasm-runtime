#pragma once

#include "module.h"
#include "error.h"
#include "vector.h"
#include "list.h"
#include <stdbool.h>

// ref: https://webassembly.github.io/spec/core/exec/index.html
typedef export_t    exportinst_t;
typedef uint32_t    funcaddr_t;

typedef struct {
    functype_t      *types;
    funcaddr_t      *funcaddrs;
    exportinst_t    *exports;
} moduleinst_t;

typedef struct {
    functype_t      *type;
    moduleinst_t    *module;
    func_t          *code;
} funcinst_t;

typedef union {
    int32_t         int32;
} num_t;

typedef struct {
    valtype_t       type;
    union {
        num_t       num;
    };
} val_t;

typedef struct {
    uint32_t        arity;
    instr_t         *continuation;
} label_t;

typedef struct {
    list_elem_t     link;
    // Used when a function is called by a call instruction.
    instr_t         *ret;

    uint32_t        arity;
    val_t           *locals;
    moduleinst_t    *module;
} frame_t;

// stack
#define TYPE_VAL    0
#define TYPE_LABEL  1
#define TYPE_FRAME  2

typedef struct {
    uint32_t        type; // identifier
    union {
        val_t       val;
        label_t     label;
        frame_t     frame;
    };
} obj_t;

typedef struct {
    size_t          num_vals;
    size_t          num_labels;
    size_t          num_frames;
    list_t          frames;
    size_t          idx;
    obj_t           *pool;
} stack_t;

typedef struct {
    stack_t         *stack;
    instr_t         *ip;

    VECTOR(funcinst_t)  funcs;
} store_t;

#define STACK_SIZE              (4096)
#define NUM_STACK_ENT           (STACK_SIZE / sizeof(obj_t))

static inline bool full(stack_t *s) {
    return s->idx == NUM_STACK_ENT;
}

void new_stack(stack_t **d);
void push_val(val_t val, stack_t *stack);
void push_label(label_t label, stack_t *stack);
void push_frame(frame_t frame, stack_t *stack);
void pop_val(val_t *val, stack_t *stack);
typedef VECTOR(val_t) vals_t;
void pop_vals(vals_t *vals, stack_t *stack);
void pop_label(label_t *label, stack_t *stack);
void pop_frame(frame_t *frame, stack_t *stack);

static inline void push_i32(int32_t val, stack_t *stack) {
    val_t v = {.type = 0x7f, .num.int32 = val};
    push_val(v, stack);
}

static inline void pop_i32(int32_t *val, stack_t *stack) {
    val_t v;
    pop_val(&v, stack);
    *val = v.num.int32;
}

funcaddr_t  allocfunc(store_t *S, func_t *func, moduleinst_t *mod);
moduleinst_t *allocmodule(store_t *S, module_t *module);
error_t instantiate(store_t **store, module_t *module);
void invoke_func(store_t *S, funcaddr_t funcaddr);
typedef VECTOR(val_t) args_t;
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args);