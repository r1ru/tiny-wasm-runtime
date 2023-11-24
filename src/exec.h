#pragma once

#include "module.h"
#include "error.h"
#include "vector.h"
#include "list.h"
#include <stdbool.h>

// ref: https://webassembly.github.io/spec/core/exec/index.html
typedef uint32_t    funcaddr_t;
typedef uint32_t    tableaddr_t;
typedef uint32_t    memaddr_t;
typedef uint32_t    globaladdr_t;
typedef uint32_t    elemaddr_t;
typedef uint32_t    dataaddr_t;

#define EXTERN_FUNC     0
#define EXTERN_TABLE    1
#define EXTERN_MEM      2
#define EXTERN_GLOBAL   3
typedef struct {
    uint8_t             kind;
    union {
        funcaddr_t      func;
        tableaddr_t     table;
        memaddr_t       mem;
        globaladdr_t    global;
    };
} externval_t;

typedef VECTOR(externval_t) externvals_t;

typedef struct {
    uint8_t             *name;
    externval_t         value;
} exportinst_t;

typedef struct {
    functype_t              *types;
    funcaddr_t              *funcaddrs;
    tableaddr_t             *tableaddrs;
    memaddr_t               *memaddrs;
    globaladdr_t            *globaladdrs;
    elemaddr_t              *elemaddrs;
    dataaddr_t              *dataaddrs;
    VECTOR(exportinst_t)    exports;
} moduleinst_t;

struct instance_t;
typedef struct {
    functype_t          *type;
    moduleinst_t        *module;
    func_t              *code;
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
#define NUM_PAGE_MAX    (65536)

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
} datainst_t;

typedef struct {
    VECTOR(funcinst_t)      funcs;
    VECTOR(tableinst_t)     tables;
    VECTOR(meminst_t)       mems;
    VECTOR(globalinst_t)    globals;
    VECTOR(eleminst_t)      elems;
    VECTOR(datainst_t)      datas;
    stack_t                 *stack;
} store_t;

typedef struct {
    valtype_t   type;
    val_t       val;
} arg_t;

typedef VECTOR(arg_t) args_t;

void new_stack(stack_t **d);
error_t push_val(stack_t *stack, val_t val);
error_t push_label(stack_t *stack, label_t label);
error_t push_frame(stack_t *stack, frame_t frame);
void pop_val(stack_t *stack, val_t *val);
void pop_vals(stack_t *stack, vals_t *vals);
void pop_label(stack_t *stack, label_t *label);
void pop_frame(stack_t *stack, frame_t *frame);

store_t *new_store(void);
error_t instantiate(store_t *S, module_t *module, externvals_t *externvals, moduleinst_t **inst);
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args);