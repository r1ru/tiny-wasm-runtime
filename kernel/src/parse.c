#include "parse.h"
#include "print.h"
#include "memory.h"

typedef struct section * (*parser_t) (struct buffer *buf);

static parser_t parsers[11] = {
    [1]     = parse_typesec,
    [3]     = parse_funcsec,
    [10]    = parse_codesec,
};

struct section *parse_typesec(struct buffer *buf) {
    uint32_t n = read_u64_leb128(buf);

    struct section *typesec = malloc(sizeof(struct section));

    VECTOR_INIT(typesec->functypes, n, functype_t);

    VECTOR_FOR_EACH(e1, typesec->functypes, functype_t) {
        // expected to be 0x60
        uint8_t magic = read_byte(buf);

        // parse argment types
        n = read_u64_leb128(buf);
        VECTOR_INIT(e1->rt1, n, uint8_t);

        VECTOR_FOR_EACH(e2, e1->rt1, uint8_t) {
            *e2 = read_byte(buf);
        }

        // pase return types
        n = read_u64_leb128(buf);
        VECTOR_INIT(e1->rt2, n, uint8_t);

        VECTOR_FOR_EACH(e2, e1->rt2, uint8_t) {
            *e2 = read_byte(buf);
        }
    };

    return typesec;
}

struct section *parse_funcsec(struct buffer *buf) {
    uint32_t n = read_u64_leb128(buf);

    struct section *funcsec = malloc(sizeof(struct section));

    VECTOR_INIT(funcsec->typeidxes, n, uint32_t);

    VECTOR_FOR_EACH(elem, funcsec->typeidxes, uint32_t) {
        *elem = read_u64_leb128(buf);
    };

    return funcsec;
}

instr_t *parse_instr(struct buffer *buf) {
    instr_t *instr = malloc(sizeof(instr_t));
    instr->next = NULL;

    instr->op = read_byte(buf);

    // todo: fix this
    switch(instr->op) {

    }

    return instr;
}

struct section *parse_codesec(struct buffer *buf) {
    struct section *codesec = malloc(sizeof(struct section));

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

struct module *parse_module(struct buffer *buf) {
    uint32_t magic      = read_u32(buf);
    uint32_t version    = read_u32(buf);

    INFO("magic = %#x, version = %#x", magic, version);

    struct module *mod = malloc(sizeof(struct module));

    while(!eof(buf)) {
        uint8_t id = read_byte(buf);
        uint32_t size = read_u64_leb128(buf);
        
        struct buffer *sec = read_buffer(buf, size);

        INFO("section id = %#x, size = %#x", id, size);

        if(id <= 11 && parsers[id])
            mod->known_sections[id] = parsers[id](sec);
    }

    return 0;
}
