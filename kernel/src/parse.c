#include "parse.h"
#include "print.h"
#include "memory.h"

typedef section_t * (*parser_t) (buffer_t *buf);

static parser_t parsers[11] = {
    [1]     = parse_typesec,
    [3]     = parse_funcsec,
    [10]    = parse_codesec,
};

section_t *parse_typesec(buffer_t *buf) {
    section_t *typesec = malloc(sizeof(section_t));

    uint32_t n = read_u64_leb128(buf);
    VECTOR_INIT(typesec->functypes, n, functype_t);

    VECTOR_FOR_EACH(functype, typesec->functypes, functype_t) {
        // expected to be 0x60
        uint8_t magic = read_byte(buf);

        // parse argment types
        n = read_u64_leb128(buf);
        VECTOR_INIT(functype->rt1, n, uint8_t);

        VECTOR_FOR_EACH(valtype, functype->rt1, uint8_t) {
            *valtype = read_byte(buf);
        }

        // pase return types
        n = read_u64_leb128(buf);
        VECTOR_INIT(functype->rt2, n, uint8_t);

        VECTOR_FOR_EACH(valtype, functype->rt2, uint8_t) {
            *valtype = read_byte(buf);
        }
    };

    return typesec;
}

section_t *parse_funcsec(buffer_t *buf) {
    section_t *funcsec = malloc(sizeof(section_t));

    uint32_t n = read_u64_leb128(buf);

    VECTOR_INIT(funcsec->typeidxes, n, uint32_t);
    VECTOR_FOR_EACH(typeidx, funcsec->typeidxes, uint32_t) {
        *typeidx = read_u64_leb128(buf);
    };

    return funcsec;
}

instr_t *parse_instr(buffer_t *buf) {
    instr_t *instr = malloc(sizeof(instr_t));
    instr->next = NULL;

    instr->op = read_byte(buf);
    
    switch(instr->op) {
        case OP_END:
            break;
        
        case OP_LOCAL_GET:
            instr->localidx = readi64_LEB128(buf);
            break;
        
        case OP_LOCAL_SET:
            instr->localidx = readi64_LEB128(buf);
            break;
        
        case OP_I32_CONST:
            instr->c.i32 = readi64_LEB128(buf);
            break;
    }

    return instr;
}

section_t *parse_codesec(buffer_t *buf) {
    section_t *codesec = malloc(sizeof(section_t));

    uint32_t n = read_u64_leb128(buf);
    VECTOR_INIT(codesec->codes, n, code_t);

    VECTOR_FOR_EACH(code, codesec->codes, code_t) {
        code->size = read_u64_leb128(buf);

        // parse func
        func_t *func = &code->func;

        // parse locals
        n = read_u64_leb128(buf);
        VECTOR_INIT(func->locals, n, locals_t);
        
        VECTOR_FOR_EACH(locals, func->locals, locals_t) {
            locals->n       = read_u64_leb128(buf);
            locals->type    = read_byte(buf);
        };

        // parse expr
        func->expr = parse_instr(buf);
        instr_t *instr = func->expr;
        
        while(instr->op != OP_END) {
            instr = instr->next = parse_instr(buf);
        }
    }
    
    return codesec;
}

module_t *parse_module(buffer_t *buf) {
    uint32_t magic      = read_u32(buf);
    uint32_t version    = read_u32(buf);

    INFO("magic = %#x, version = %#x", magic, version);

    module_t *mod = malloc(sizeof(module_t));

    while(!eof(buf)) {
        uint8_t id = read_byte(buf);
        uint32_t size = read_u64_leb128(buf);
        
        buffer_t *sec = read_buffer(buf, size);

        INFO("section id = %#x, size = %#x", id, size);

        if(id <= 11 && parsers[id])
            mod->known_sections[id] = parsers[id](sec);
    }

    return mod;
}
