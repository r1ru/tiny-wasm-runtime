#pragma once

// ref: https://webassembly.github.io/spec/core/

#include <stdint.h>
#include "vector.h"

typedef struct {
    vector_t rt1;
    vector_t rt2;
} functype_t;

typedef struct {
    uint8_t     kind;
    uint32_t    idx;
} exportdesc_t;

typedef struct {
    const char      *name;
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
    uint8_t valtype;
} blocktype_t;

typedef union {
    int32_t i32;
} value_t;

typedef struct instr {
    struct instr        *next;
    uint8_t             op;

    union {
        // control instructions
        struct {
            blocktype_t     bt;
            struct instr    *in1;
            struct instr    *in2;
        };
        uint32_t        l;
        
        // variable instructions
        uint32_t        idx;

        // const instrcutions
        value_t         c;        
    };
    
} instr_t;

typedef struct {
    uint32_t    n;
    uint8_t     type;
} locals_t;

typedef struct {
    vector_t    locals;
    instr_t     *expr;
} func_t;

typedef struct {
    uint32_t    size;
    func_t      func;
} code_t;

typedef struct {
    union {
        // typesec
        vector_t functypes;
        // funcsec
        vector_t typeidxes;
        // exportsec
        vector_t exports;
        // codesec
        vector_t codes;
    };
} section_t ;

typedef struct {
    section_t *known_sections[12];
} module_t;