#include "decode.h"
#include "memory.h"

// useful macros
typedef error_t (*decoder_t) (module_t *mod, buffer_t *buf);

static decoder_t decoders[11] = {
    [1]     = decode_typesec,
    [3]     = decode_funcsec,
    [7]     = decode_exportsec,
    [10]    = decode_codesec,
};

error_t decode_typesec(module_t *mod, buffer_t*buf) {
    uint32_t n1;
    read_u32_leb128(&n1, buf);

    VECTOR_INIT(&mod->types, n1, functype_t);

    VECTOR_FOR_EACH(functype, &mod->types, functype_t) {
        // expected to be 0x60
        uint8_t magic;
        read_byte(&magic, buf);

        // decode parameter types
        uint32_t n2;
        read_u32_leb128(&n2, buf);
        VECTOR_INIT(&functype->rt1, n2, valtype_t);
        VECTOR_FOR_EACH(valtype, &functype->rt1, valtype_t) {
            read_byte(valtype, buf);
        }

        // decode return types
        read_u32_leb128(&n2, buf);
        VECTOR_INIT(&functype->rt2, n2, valtype_t);
        VECTOR_FOR_EACH(valtype, &functype->rt2, valtype_t) {
            read_byte(valtype, buf);
        }
    }

    return ERR_SUCCESS;
}

error_t decode_funcsec(module_t *mod, buffer_t *buf) {
    uint32_t n;
    read_u32_leb128(&n, buf);

    VECTOR_INIT(&mod->funcs, n, func_t);

    VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
        read_u32_leb128(&func->type, buf);
    }

    return ERR_SUCCESS;
}

error_t decode_exportsec(module_t *mod, buffer_t *buf) {
    uint32_t n;
    read_u32_leb128(&n, buf);

    VECTOR_INIT(&mod->exports, n, export_t);

    VECTOR_FOR_EACH(export, &mod->exports, export_t) {
        read_bytes(&export->name, buf);
        read_byte(&export->exportdesc.kind, buf);
        read_u32_leb128(&export->exportdesc.funcidx, buf);
    }

    return ERR_SUCCESS;
}

// todo: delete this?
error_t decode_instr(instr_t **instr, buffer_t *buf) {
    instr_t *i = *instr = malloc(sizeof(instr_t));
    i->next = NULL;

    read_byte(&i->op, buf);
    
    switch(i->op) {
        case OP_BLOCK:
        case OP_LOOP:
        case OP_IF: {
            // todo: support s33
            read_byte(&i->bt.valtype, buf);

            decode_instr(&i->in1, buf);
            instr_t *j = i->in1;
            while(j->op != OP_END && j->op != OP_ELSE) {
                decode_instr(&j->next, buf);
                j = j->next;
            }

            if(i->op == OP_ELSE) {
                decode_instr(&i->in2, buf);
                j = i->in2;
                while(j->op != OP_END) {
                    decode_instr(&j->next, buf);
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

error_t decode_codesec(module_t *mod, buffer_t *buf) {
    uint32_t n1;
    read_u32_leb128(&n1, buf);

    // assert(n1 == mod->funcs.n)

    VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
        // size is unused
        uint32_t size;
        read_u32_leb128(&size, buf);

        VECTOR(locals_t) localses;
        uint32_t n2;
        read_u32_leb128(&n2, buf);
        VECTOR_INIT(&localses, n2, locals_t);
        
        // count local variables
        functype_t *functype = VECTOR_ELEM(&mod->types, func->type);
        uint32_t num_locals = functype->rt1.n;

        VECTOR_FOR_EACH(locals, &localses, locals_t) {
            read_u32_leb128(&locals->n, buf);
            num_locals += locals->n;
            read_byte(&locals->type, buf);
        }

        // create vec(valtype)
        VECTOR_INIT(&func->locals, num_locals, valtype_t);

        uint32_t idx = 0;
        for(uint32_t i = 0; i < functype->rt1.n; i++) {
            func->locals.elem[idx++] = functype->rt1.elem[i];
        }

        VECTOR_FOR_EACH(locals, &localses, locals_t) {
            for(uint32_t i = 0; i < locals->n; i++) {
                func->locals.elem[idx++] = locals->type;
            }
        }
        // todo: free localses.elem?
        
        // decode body
        decode_instr(&func->body, buf);
        instr_t *instr = func->body;
        while(instr->op != OP_END) {
            decode_instr(&instr->next, buf);
            instr = instr->next;
        }
    }

    return ERR_SUCCESS;
}

error_t decode_module(module_t **mod, uint8_t *image, size_t image_size) {
    buffer_t *buf;
    new_buffer(&buf, image, image_size);

    uint32_t magic, version;
    
    read_u32(&magic, buf);
    read_u32(&version, buf);

    // init
    module_t *m = *mod = malloc(sizeof(module_t));
    VECTOR_INIT(&m->types, 0, functype_t);
    VECTOR_INIT(&m->funcs, 0, func_t);
    VECTOR_INIT(&m->exports, 0, export_t);

    while(!eof(buf)) {
        uint8_t id;
        uint32_t size;
        read_byte(&id, buf);
        read_u32_leb128(&size, buf);
        
        buffer_t *sec;
        read_buffer(&sec, size, buf);

        if(id <= 11 && decoders[id])
            decoders[id](m, sec);
    }

    return ERR_SUCCESS;
}