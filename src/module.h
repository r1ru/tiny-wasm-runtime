#pragma once

// module.h defines the representation of abstract module defined in WASM Spec
// ref: https://webassembly.github.io/spec/core/syntax/index.html

#include <stdint.h>

typedef uint8_t     valtype_t;
typedef uint32_t    typeidx_t;
typedef uint32_t    funcidx_t;
typedef uint32_t    labelidx_t;
typedef uint32_t    funcidx_t;
typedef uint32_t    localidx_t;

// useful macros
#define VECTOR(type)                                \
struct {                                            \
    uint32_t    n;                                  \
    type        *elem;                              \
}

#define VECTOR_INIT(vec, len, type)                 \
    ({                                              \
        (vec)->n = (len);                           \
        (vec)->elem = malloc(sizeof(type) * (len)); \
    })

#define VECTOR_FOR_EACH(iter, vec, type)            \
    for(type *iter = &(vec)->elem[0];               \
        iter != &(vec)->elem[(vec)->n];             \
        iter++                                      \
    )

#define VECTOR_ELEM(vec, n)                         \
    (&(vec)->elem[n])

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
    OP_BLOCK        = 0x02,
    OP_LOOP         = 0x03,
    OP_IF           = 0x04,
    OP_ELSE         = 0x05,
    OP_END          = 0x0b,
    OP_BR           = 0x0c,
    OP_BR_IF        = 0x0d,
    OP_CALL         = 0x10,
    OP_LOCAL_GET    = 0x20,
    OP_LOCAL_SET    = 0x21,
    OP_I32_CONST    = 0x41,
    OP_I32_EQZ      = 0x45,
    OP_I32_LT_S     = 0x48,
    OP_I32_GE_S     = 0x4e,
    OP_I32_ADD      = 0x6a,
    OP_I32_REM_S    = 0x6f, 
};

typedef union {
    // todo: support s33
    valtype_t valtype;
} blocktype_t;

typedef union {
    int32_t i32;
} const_t;

typedef struct instr {
    struct instr            *next;
    uint8_t                 op;

    union {
        // control instructions
        struct {
            blocktype_t     bt;
            struct instr    *in1;
            struct instr    *in2;
        };
        labelidx_t          labelidx;
        funcidx_t           funcidx;
        // variable instructions
        localidx_t          localidx;
        // const instrcutions
        const_t             c;        
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