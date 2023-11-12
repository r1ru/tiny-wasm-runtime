#pragma once

#include "module.h"
#include "error.h"
#include "vector.h"
#include "list.h"
#include <stdbool.h>

// ref: https://webassembly.github.io/spec/core/exec/index.html
typedef export_t    exportinst_t;
typedef uint32_t    funcaddr_t;
typedef uint32_t    tableaddr_t;
typedef uint32_t    memaddr_t;
typedef uint32_t    globaladdr_t;
typedef uint32_t    elemaddr_t;
typedef uint32_t    dataaddr_t;

typedef struct {
    functype_t      *types;
    funcaddr_t      *funcaddrs;
    tableaddr_t     *tableaddrs;
    memaddr_t       *memaddrs;
    globaladdr_t    *globaladdrs;
    elemaddr_t      *elemaddrs;
    dataaddr_t       *dataaddrs;
    exportinst_t    *exports;
} moduleinst_t;

typedef struct {
    functype_t      *type;
    moduleinst_t    *module;
    func_t          *code;
} funcinst_t;

#define REF_NULL    -1
typedef uint32_t    ref_t;
typedef struct {
    tabletype_t     type;
    VECTOR(ref_t)   elem;
} tableinst_t;

typedef union {
    int32_t         i32;
    int64_t         i64;
    float           f32;
    double          f64;
} num_t;

typedef union {
    num_t       num;
    ref_t       ref;
} val_t;

typedef VECTOR(val_t) vals_t;

typedef struct {
    list_elem_t     link;
    uint32_t        arity;
    instr_t         *parent;
    instr_t         *continuation;
} label_t;

typedef struct {
    list_elem_t     link;
    uint32_t        arity;
    val_t           *locals;
    moduleinst_t    *module;
} frame_t;

// stack
typedef struct {
    uint32_t        type; // identifier
    union {
        val_t       val;
        label_t     label;
        frame_t     frame;
    };
} obj_t;

#define TYPE_VAL        0
#define TYPE_LABEL      1
#define TYPE_FRAME      2

#define STACK_SIZE      (4096 * 16)
#define NUM_STACK_ENT   (STACK_SIZE / sizeof(obj_t) - 1)

typedef struct {
    list_t          frames;
    list_t          labels;
    size_t          idx;
    obj_t           *pool;
} stack_t;

#define PAGE_SIZE       (4096)
#define WASM_PAGE_SIZE  (PAGE_SIZE * 16)
typedef struct {
    memtype_t       type;
    size_t          num_pages;
    uint8_t ***     table2[4];
} meminst_t;

typedef struct {
    globaltype_t        gt;
    val_t               val;
} globalinst_t;

typedef  struct {
    reftype_t           type;
    VECTOR(ref_t)       elem;
} eleminst_t;

typedef struct {
    VECTOR(byte_t)      data;
} datains_t;

typedef struct {
    stack_t                 *stack;
    VECTOR(funcinst_t)      funcs;
    VECTOR(tableinst_t)     tables;
    VECTOR(meminst_t)       mems;
    VECTOR(globalinst_t)    globals;
    VECTOR(eleminst_t)      elems;
    VECTOR(datains_t)       datas;
} store_t;

typedef struct {
    valtype_t   type;
    val_t       val;
} arg_t;

typedef VECTOR(arg_t) args_t;

void new_stack(stack_t **d);
void push_val(val_t val, stack_t *stack);
void push_label(label_t label, stack_t *stack);
void push_frame(frame_t frame, stack_t *stack);
void pop_val(val_t *val, stack_t *stack);
void pop_vals(vals_t *vals, stack_t *stack);
void pop_label(label_t *label, stack_t *stack);
void pop_frame(frame_t *frame, stack_t *stack);

moduleinst_t *allocmodule(store_t *S, module_t *module);
error_t instantiate(store_t **store, module_t *module);
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args);