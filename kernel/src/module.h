#pragma once

// ref: https://webassembly.github.io/spec/core/

#include <stdint.h>

typedef uint8_t valtype_t;
typedef uint32_t typeidx_t;
typedef uint32_t funcidx_t;
typedef uint32_t labelidx_t;
typedef uint32_t funcidx_t;
typedef uint32_t localidx_t;

typedef struct {
    uint32_t    n;
    valtype_t   *types;
} resulttype_t;

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
} value_t;

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
        value_t             c;        
    };
} instr_t;

typedef struct {
    uint32_t    n;
    valtype_t   type;
} locals_t;

typedef struct {
    struct {
        uint32_t    num_locals;
        locals_t    *locals;
    };
    instr_t     *expr;
} func_t;

typedef func_t code_t;

typedef struct {
    uint32_t    n;
    union {
        // typesec
        functype_t  *functypes;
        // funcsec
        typeidx_t   *typeidxes;
        // exportsec
        export_t    *exports;
        // codesec
        code_t      *codes;
    };
} section_t ;

typedef struct {
    section_t *known_sections[12];
} module_t;