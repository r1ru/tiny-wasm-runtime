#include "parse.h"
#include "print.h"
#include "memory.h"

typedef error_t (*parser_t) (module_t *mod, buffer_t *buf);

static parser_t parsers[11] = {
    [1]     = parse_typesec,
    [3]     = parse_funcsec,
    [7]     = parse_exportsec,
    [10]    = parse_codesec,
};

error_t parse_typesec(module_t *mod, buffer_t*buf) {    
    section_t *typesec = mod->known_sections[1] = malloc(sizeof(section_t));

    uint32_t n1;
    read_u32_leb128(&n1, buf);
    typesec->n = n1;
    typesec->functypes = malloc(sizeof(functype_t) * n1);

    for(uint32_t i = 0; i < n1; i++) {
        // expected to be 0x60
        uint8_t magic;
        read_byte(&magic, buf);

        // parse argment types
        functype_t *functype = &typesec->functypes[i];

        uint32_t n2;
        read_u32_leb128(&n2, buf);
        functype->rt1.n = n2;
        functype->rt1.types = malloc(sizeof(valtype_t) * n2);
        for(uint32_t j = 0; j < n2; j++) {
            read_byte(&functype->rt1.types[j], buf);
        }

        // parse return types
        read_u32_leb128(&n2, buf);
        functype->rt2.n = n2;
        functype->rt2.types = malloc(sizeof(valtype_t) * n2);
        for(size_t j = 0; j < n2; j++) {
            read_byte(&functype->rt2.types[j], buf);
        }
    }

    return ERR_SUCCESS;
}

error_t parse_funcsec(module_t *mod, buffer_t *buf) {
    section_t *funcsec = mod->known_sections[3] = malloc(sizeof(section_t));

    uint32_t n;
    read_u32_leb128(&n, buf);
    funcsec->n = n;
    funcsec->typeidxes = malloc(sizeof(typeidx_t) * n);

    for(uint32_t i = 0; i < n; i++) {
        read_u32_leb128(&funcsec->typeidxes[i], buf);
    }

    return ERR_SUCCESS;
}

error_t parse_exportsec(module_t *mod, buffer_t *buf) {
    section_t *exportsec = mod->known_sections[7] = malloc(sizeof(section_t));

    uint32_t n;
    read_u32_leb128(&n, buf);
    exportsec->n = n;
    exportsec->exports = malloc(sizeof(export_t) * n);

    for(uint32_t i = 0; i < n; i++) {
        export_t *export = &exportsec->exports[i];
        read_bytes(&export->name, buf);
        read_byte(&export->exportdesc.kind, buf);
        read_u32_leb128(&export->exportdesc.funcidx, buf);
    }

    return ERR_SUCCESS;
}

error_t parse_instr(instr_t **instr, buffer_t *buf) {
    instr_t *i = *instr = malloc(sizeof(instr_t));
    i->next = NULL;

    read_byte(&i->op, buf);
    
    switch(i->op) {
        case OP_BLOCK:
        case OP_LOOP:
        case OP_IF: {
            // todo: support s33
            read_byte(&i->bt.valtype, buf);

            parse_instr(&i->in1, buf);
            instr_t *j = i->in1;
            while(j->op != OP_END && j->op != OP_ELSE) {
                parse_instr(&j->next, buf);
                j = j->next;
            }

            if(i->op == OP_ELSE) {
                parse_instr(&i->in2, buf);
                j = i->in2;
                while(j->op != OP_END) {
                    parse_instr(&j->next, buf);
                    j = j->next;
                }
            }
            break;
        }

        case OP_END:
            break;
        
        case OP_BR:
        case OP_BR_IF:
            read_u32_leb128(&i->labelidx, buf);
            break;
        
        case OP_CALL:
            read_u32_leb128(&i->funcidx, buf);
            break;
        
        case OP_LOCAL_GET:
        case OP_LOCAL_SET:
            read_u32_leb128(&i->localidx, buf);
            break;

        case OP_I32_CONST:
            read_i32_leb128(&i->c.i32, buf);
            break;
        
        case OP_I32_EQZ:
        case OP_I32_LT_S:
        case OP_I32_GE_S:  
        case OP_I32_ADD:
        case OP_I32_REM_S:
            break;
    }

    return ERR_SUCCESS;
}

error_t parse_codesec(module_t *mod, buffer_t *buf) {
    section_t *codesec = mod->known_sections[10] = malloc(sizeof(section_t));

    uint32_t n1;
    read_u32_leb128(&n1, buf);
    codesec->n = n1;
    codesec->codes = malloc(sizeof(code_t) * n1);

    for(uint32_t i = 0; i < n1; i++) {
        // size is unused
        uint32_t size;
        read_u32_leb128(&size, buf);

        code_t *code = &codesec->codes[i];

        // parse locals
        uint32_t n2;
        read_u32_leb128(&n2, buf);
        code->num_localses = n2;
        code->localses = malloc(sizeof(locals_t) * n2);

        for(uint32_t j = 0; j < n2; j++) {
            locals_t *locals = &code->localses[j];
            read_u32_leb128(&locals->n, buf);
            read_byte(&locals->type, buf);
        }

        // parse expr
        parse_instr(&code->expr, buf);
        instr_t *instr = code->expr;
        while(instr->op != OP_END) {
            parse_instr(&instr->next, buf);
            instr = instr->next;
        }
    }

    return ERR_SUCCESS;
}

error_t parse_module(module_t **mod, uint8_t *image, size_t image_size) {
    buffer_t *buf;
    new_buffer(&buf, image, image_size);

    uint32_t magic, version;
    
    read_u32(&magic, buf);
    read_u32(&version, buf);

    INFO("magic = %#x, version = %#x", magic, version);

    module_t *m = *mod = malloc(sizeof(module_t));

    while(!eof(buf)) {
        uint8_t id;
        uint32_t size;
        read_byte(&id, buf);
        read_u32_leb128(&size, buf);
        
        buffer_t *sec;
        read_buffer(&sec, size, buf);

        INFO("section id = %#x, size = %#x", id, size);

        if(id <= 11 && parsers[id])
            parsers[id](m, sec);
        else 
            m->known_sections[id] = NULL;
    }

    return ERR_SUCCESS;
}