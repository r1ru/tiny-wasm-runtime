#pragma once

// module.h defines the representation of abstract module defined in WASM Spec
// ref: https://webassembly.github.io/spec/core/syntax/index.html

#include <stdint.h>
#include "vector.h"

typedef uint8_t     valtype_t;
typedef uint32_t    typeidx_t;
typedef uint32_t    funcidx_t;
typedef uint32_t    labelidx_t;
typedef uint32_t    funcidx_t;
typedef uint32_t    localidx_t;
typedef uint32_t    tableidx_t;

#define TYPE_NUM_I32 0x7f

typedef VECTOR(valtype_t) resulttype_t;

typedef struct {
    resulttype_t rt1;
    resulttype_t rt2;
} functype_t;

typedef struct {
    uint8_t     kind;
    union {
        funcidx_t funcidx;
        // todo: add here
    };
} exportdesc_t;

typedef struct {
    uint8_t         *name;
    exportdesc_t    exportdesc;
} export_t;

enum op {
    OP_BLOCK            = 0x02,
    OP_LOOP             = 0x03,
    OP_IF               = 0x04,
    OP_ELSE             = 0x05,
    OP_END              = 0x0B,
    OP_BR               = 0x0C,
    OP_BR_IF            = 0x0D,
    OP_BR_TABLE         = 0x0E,
    OP_RETURN           = 0x0F,
    OP_CALL             = 0x10,
    OP_CALL_INDIRECT    = 0x11,
    OP_DROP             = 0x1A,
    OP_SELECT           = 0x1B,
    OP_LOCAL_GET        = 0x20,
    OP_LOCAL_SET        = 0x21,
    OP_LOCAL_TEE        = 0x22,
    OP_GLOBAL_GET       = 0x23,
    OP_GLOBAL_SET       = 0x24,
    OP_I32_LOAD         = 0x28,
    OP_I32_STORE        = 0x36,
    OP_MEMORY_GROW      = 0x40,
    OP_I32_CONST        = 0x41,
    OP_I64_CONST        = 0x42,
    OP_F32_CONST        = 0x43,
    OP_I32_EQZ          = 0x45,
    OP_I32_EQ           = 0x46,
    OP_I32_NE           = 0x47,
    OP_I32_LT_S         = 0x48,
    OP_I32_LT_U         = 0x49,
    OP_I32_GT_S         = 0x4A,
    OP_I32_GT_U         = 0x4B,
    OP_I32_LE_S         = 0x4C,
    OP_I32_LE_U         = 0x4D,
    OP_I32_GE_S         = 0x4E,
    OP_I32_GE_U         = 0x4F,
    OP_I32_CLZ          = 0x67,
    OP_I32_CTZ          = 0x68,
    OP_I32_POPCNT       = 0x69,
    OP_I32_ADD          = 0x6A,
    OP_I32_SUB          = 0x6B,
    OP_I32_MUL          = 0x6C,
    OP_I32_DIV_S        = 0x6D,
    OP_I32_DIV_U        = 0x6E,
    OP_I32_REM_S        = 0x6f,
    OP_I32_REM_U        = 0x70,
    OP_I32_AND          = 0x71,
    OP_I32_OR           = 0x72,
    OP_I32_XOR          = 0x73,
    OP_I32_SHL          = 0x74,
    OP_I32_SHR_S        = 0x75,
    OP_I32_SHR_U        = 0x76,
    OP_I32_ROTL         = 0x77,
    OP_I32_ROTR         = 0x78,
    OP_I32_EXTEND8_S    = 0xC0,
    OP_I32_EXTEND16_S   = 0xC1,
};

typedef union {
    // todo: support s33
    valtype_t   valtype;
} blocktype_t;

typedef union {
    int32_t     i32;
    int64_t     i64;
    float       f32;
} const_t;

typedef struct {
    uint32_t    align;
    uint32_t    offset;
} memarg_t;

typedef struct instr {
    struct instr                *next;
    uint8_t                     op;

    union {
        // control instructions
        struct {
            blocktype_t         bt;
            struct instr        *in1;
            struct instr        *in2;
        };
        // br_table
        struct {
            VECTOR(labelidx_t)  labels;
            labelidx_t          default_label;
        };
        // call_indirect
        struct {
            typeidx_t           y;
            tableidx_t          x;
        };
        memarg_t                m;
        labelidx_t              globalidx;
        labelidx_t              labelidx;
        funcidx_t               funcidx;
        // variable instructions
        localidx_t              localidx;
        // const instrcutions
        const_t                 c;        
    };
} instr_t;

// Type used only when decoding
typedef struct {
    uint32_t    n;
    valtype_t   type;
} locals_t;

typedef instr_t * expr_t;

typedef struct {
    typeidx_t           type;
    VECTOR(valtype_t)   locals;
    expr_t              body;
} func_t;

typedef struct {
    VECTOR(functype_t)  types;
    VECTOR(func_t)      funcs;
    VECTOR(export_t)    exports;
} module_t;