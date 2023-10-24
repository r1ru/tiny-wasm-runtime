#include "decode.h"
#include "memory.h"
#include "print.h"
#include "exception.h"

error_t new_buffer(buffer_t **d, uint8_t *head, size_t size) {
    __try {
        buffer_t *buf = malloc(sizeof(buffer_t));
        __throwif(ERR_FAILED, !buf);

        *buf = (buffer_t) {
            .p   = head,
            .end = head + size
        };
        *d = buf;
    }
    __catch:
        return err;
}

error_t read_buffer(buffer_t **d, size_t size, buffer_t *buf) {
    __try {
        __throwif(ERR_FAILED, buf->p + size > buf->end);
        __throwiferr(new_buffer(d, buf->p, size));
        buf->p += size;
    }
    __catch:
        return err;
}

error_t read_byte(uint8_t *d, buffer_t *buf) {
    __try {
        __throwif(ERR_FAILED, buf->p + 1 > buf->end);
        *d = *buf->p++;
    }
    __catch:
        return err;
}

// read vec(byte)
error_t read_bytes(uint8_t **d, buffer_t *buf) {
    __try {
        uint32_t n;
        __throwiferr(read_u32_leb128(&n, buf));
        
        uint8_t *str = malloc(sizeof(uint8_t) * (n + 1));
        __throwif(ERR_FAILED, !str);

        for(uint32_t i = 0; i < n; i++) {
            __throwiferr(read_byte(&str[i], buf));
        }
        str[n] = '\0';
        *d = str;
    }
    __catch:
        return err;
}

error_t read_u32(uint32_t *d, buffer_t *buf) {
    __try {
        __throwif(ERR_FAILED, buf->p + 4 > buf->end);
         *d = *(uint32_t *)buf->p;
        buf->p += 4;
    }
    __catch:
        return err;
}

error_t read_i32(int32_t *d, buffer_t *buf) {
     __try {
        __throwif(ERR_FAILED, buf->p + 4 > buf->end);
         *d = *(int32_t *)buf->p;
        buf->p += 4;
    }
    __catch:
        return err;
}

// Little Endian Base 128
error_t read_u32_leb128(uint32_t *d, buffer_t *buf) {
    __try {
        uint32_t result = 0, shift = 0;
        while(1) {
            uint8_t byte;
            __throwiferr(read_byte(&byte, buf));

            result |= (byte & 0b1111111) << shift;
            shift += 7;
            if((0b10000000 & byte) == 0)
                break;
        }
        *d = result;
    }
    __catch:
        return err;
}

error_t read_u64_leb128(uint64_t *d, buffer_t *buf) {
    __try {
        uint64_t result = 0, shift = 0;
        while(1) {
            uint8_t byte;
            __throwiferr(read_byte(&byte, buf));

            result |= (byte & 0b1111111) << shift;
            shift += 7;
            if((0b10000000 & byte) == 0)
                break;
        }
        *d = result;
    }
    __catch:
        return err;
}

error_t read_i32_leb128(int32_t *d, buffer_t *buf) {
    __try {
        int32_t result = 0, shift = 0;
        while(1) {
            uint8_t byte;
            __throwiferr(read_byte(&byte, buf));

            result |= (byte & 0b1111111) << shift;
            shift += 7;
            if((0b10000000 & byte) == 0) {
                if((byte & 0b1000000) != 0)
                    result |= ~0 << shift;
                else
                    result;
                break;
            }
        }
        *d = result;
    }
    __catch:
        return err;
}

error_t read_i64_leb128(int64_t *d, buffer_t *buf) {
    __try {
        int64_t result = 0, shift = 0;
        while(1) {
            uint8_t byte;
            __throwiferr(read_byte(&byte, buf));

            result |= (byte & 0b1111111) << shift;
            shift += 7;
            if((0b10000000 & byte) == 0) {
                if((byte & 0b1000000) != 0)
                    result |= ~0 << shift;
                else
                    result;
                break;
            }
        }
        *d = result;
    }
    __catch:
        return err;
}

error_t read_f32(float *d, buffer_t *buf) {
    __try {
        __throwif(ERR_FAILED, buf->p + 4 > buf->end);
         *d = *(float *)buf->p;
        buf->p += 4;
    }
    __catch:
        return err;
}

error_t read_f64(double *d, buffer_t *buf) {
    __try {
        __throwif(ERR_FAILED, buf->p + 8 > buf->end);
         *d = *(double *)buf->p;
        buf->p += 8;
    }
    __catch:
        return err;
}

// todo: support s33
error_t read_bt(blocktype_t *bt, buffer_t *buf) {
    __try {
        __throwif(ERR_FAILED, buf->p + 1 > buf->end);

        switch(buf->p[0]) {
            case 0x40:
            case TYPE_NUM_I32:
            case TYPE_NUM_I64:
            case TYPE_NUM_F32:
            case TYPE_NUM_F64:
                __throwiferr(read_byte(&bt->valtype, buf));
                break;
            
            default:
                __throwiferr(read_u32_leb128(&bt->typeidx, buf));
                break;
        }
    }
    __catch:
        return err;
}

// useful macros
typedef error_t (*decoder_t) (module_t *mod, buffer_t *buf);

static decoder_t decoders[11] = {
    [1]     = decode_typesec,
    [3]     = decode_funcsec,
    [7]     = decode_exportsec,
    [10]    = decode_codesec,
};

error_t decode_typesec(module_t *mod, buffer_t*buf) {
    __try {
        uint32_t n1;
        __throwiferr(read_u32_leb128(&n1, buf));

        VECTOR_INIT(&mod->types, n1, functype_t);

        VECTOR_FOR_EACH(functype, &mod->types, functype_t) {
            // expected to be 0x60
            uint8_t magic;
            __throwiferr(read_byte(&magic, buf));
            __throwif(ERR_FAILED, magic != 0x60);

            // decode parameter types
            uint32_t n2;
            __throwiferr(read_u32_leb128(&n2, buf));
            VECTOR_INIT(&functype->rt1, n2, valtype_t);
            VECTOR_FOR_EACH(valtype, &functype->rt1, valtype_t) {
                __throwiferr(read_byte(valtype, buf));
            }

            // decode return types
            __throwiferr(read_u32_leb128(&n2, buf));
            VECTOR_INIT(&functype->rt2, n2, valtype_t);
            VECTOR_FOR_EACH(valtype, &functype->rt2, valtype_t) {
                __throwiferr(read_byte(valtype, buf));
            }
        }
    }
    __catch:
        return err;
}

error_t decode_funcsec(module_t *mod, buffer_t *buf) {
    __try {
        uint32_t n;
        __throwiferr(read_u32_leb128(&n, buf));

        VECTOR_INIT(&mod->funcs, n, func_t);

        VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
            __throwiferr(read_u32_leb128(&func->type, buf));
        }
    }
    __catch:
        return err;
}

error_t decode_exportsec(module_t *mod, buffer_t *buf) {
    __try {
        uint32_t n;
        __throwiferr(read_u32_leb128(&n, buf));
        VECTOR_INIT(&mod->exports, n, export_t);

        VECTOR_FOR_EACH(export, &mod->exports, export_t) {
            __throwiferr(read_bytes(&export->name, buf));
            __throwiferr(read_byte(&export->exportdesc.kind, buf));
            __throwiferr(read_u32_leb128(&export->exportdesc.funcidx, buf));
        }
    }
    __catch:
        return err;
}

// todo: delete this?
error_t decode_instr(instr_t **instr, buffer_t *buf) {
    __try {
        instr_t *i = malloc(sizeof(instr_t));
        __throwif(ERR_FAILED, !i);

        i->next = NULL;

        __throwiferr(read_byte(&i->op1, buf));
        
        switch(i->op1) {
            case OP_BLOCK:
            case OP_LOOP:
            case OP_IF: {
                // todo: support s33
                __throwiferr(read_bt(&i->bt, buf));

                __throwiferr(decode_instr(&i->in1, buf));
                instr_t *j = i->in1;
                while(j->op1 != OP_END && j->op1 != OP_ELSE) {
                    __throwiferr(decode_instr(&j->next, buf));
                    j = j->next;
                }

                if(j->op1 == OP_ELSE) {
                    __throwiferr(decode_instr(&i->in2, buf));
                    j = i->in2;
                    while(j->op1 != OP_END) {
                        __throwiferr(decode_instr(&j->next, buf));
                        j = j->next;
                    }
                }
                else {
                    i->in2 = NULL;
                }

                break;
            }

            case OP_ELSE:
            case OP_END:
                break;
            
            case OP_BR:
            case OP_BR_IF:
                __throwiferr(read_u32_leb128(&i->labelidx, buf));
                break;
            
            case OP_BR_TABLE: {
                uint32_t n;
                __throwiferr(read_u32_leb128(&n, buf));
                VECTOR_INIT(&i->labels, n, labelidx_t);
                VECTOR_FOR_EACH(l, &i->labels, labelidx_t) {
                    __throwiferr(read_u32_leb128(l, buf));
                }
                __throwiferr(read_u32_leb128(&i->default_label, buf));
                break;
            }

            case OP_RETURN:
                break;
           
            case OP_CALL:
                __throwiferr(read_u32_leb128(&i->funcidx, buf));
                break;
            
            case OP_CALL_INDIRECT:
                __throwiferr(read_u32_leb128(&i->y, buf));
                __throwiferr(read_u32_leb128(&i->x, buf));
                break;
            
            case OP_DROP:
            case OP_SELECT:
                break;
            
            case OP_LOCAL_GET:
            case OP_LOCAL_SET:
            case OP_LOCAL_TEE:
                __throwiferr(read_u32_leb128(&i->localidx, buf)); 
                break;
            
            case OP_GLOBAL_GET:
            case OP_GLOBAL_SET:
                __throwiferr(read_u32_leb128(&i->globalidx, buf));
                break;
            
            case OP_I32_LOAD:
            case OP_I32_STORE:
                __throwiferr(read_u32_leb128(&i->m.align, buf));
                __throwiferr(read_u32_leb128(&i->m.offset, buf));
                break;
            
            case OP_MEMORY_GROW: {
                uint32_t zero;
                __throwiferr(read_u32_leb128(&zero, buf));
                __throwif(ERR_FAILED, zero != 0);
                break;
            }

            case OP_I32_CONST:
                __throwiferr(read_i32_leb128(&i->c.i32, buf));
                break;

            case OP_I64_CONST:
                __throwiferr(read_i64_leb128(&i->c.i64, buf));
                break;
            
            case OP_F32_CONST:
                __throwiferr(read_f32(&i->c.f32, buf));
                break;
            
            case OP_F64_CONST:
                __throwiferr(read_f64(&i->c.f64, buf));
                break;
            
            case OP_I32_EQZ:
            case OP_I32_EQ:
            case OP_I32_NE:
            case OP_I32_LT_S:
            case OP_I32_LT_U:
            case OP_I32_GT_S:
            case OP_I32_GT_U:
            case OP_I32_LE_S:
            case OP_I32_LE_U:
            case OP_I32_GE_S:
            case OP_I32_GE_U:
            case OP_I64_EQZ:
            case OP_I64_EQ:
            case OP_I64_NE:
            case OP_I64_LT_S:
            case OP_I64_LT_U:
            case OP_I64_GT_S:
            case OP_I64_GT_U:
            case OP_I64_LE_S:
            case OP_I64_LE_U:
            case OP_I64_GE_S:
            case OP_I64_GE_U:
            case OP_F32_EQ:
            case OP_F32_NE:
            case OP_F32_LT:
            case OP_F32_GT:
            case OP_F32_LE:
            case OP_F32_GE:
            case OP_F64_EQ:
            case OP_F64_NE:
            case OP_F64_LT:
            case OP_F64_GT:
            case OP_F64_LE:
            case OP_F64_GE:

            case OP_I32_CLZ:
            case OP_I32_CTZ:
            case OP_I32_POPCNT:
            case OP_I32_ADD:
            case OP_I32_SUB:
            case OP_I32_MUL:
            case OP_I32_DIV_S:
            case OP_I32_DIV_U:
            case OP_I32_REM_S:
            case OP_I32_REM_U:
            case OP_I32_AND:
            case OP_I32_OR:
            case OP_I32_XOR:
            case OP_I32_SHL:
            case OP_I32_SHR_S:
            case OP_I32_SHR_U:
            case OP_I32_ROTL:
            case OP_I32_ROTR:
            case OP_I64_CLZ:
            case OP_I64_CTZ:
            case OP_I64_POPCNT:
            case OP_I64_ADD:
            case OP_I64_SUB:
            case OP_I64_MUL:
            case OP_I64_DIV_S:
            case OP_I64_DIV_U:
            case OP_I64_REM_S:
            case OP_I64_REM_U:
            case OP_I64_AND:
            case OP_I64_OR:
            case OP_I64_XOR:
            case OP_I64_SHL:
            case OP_I64_SHR_S:
            case OP_I64_SHR_U:
            case OP_I64_ROTL:
            case OP_I64_ROTR:

            case OP_F32_ABS:
            case OP_F32_NEG:
            case OP_F32_CEIL:
            case OP_F32_FLOOR:
            case OP_F32_TRUNC:
            case OP_F32_NEAREST:
            case OP_F32_SQRT:
            case OP_F32_ADD:
            case OP_F32_SUB:
            case OP_F32_MUL:
            case OP_F32_DIV:
            case OP_F32_MIN:
            case OP_F32_MAX:
            case OP_F32_COPYSIGN:

            case OP_F64_ABS:
            case OP_F64_NEG:
            case OP_F64_CEIL:
            case OP_F64_FLOOR:
            case OP_F64_TRUNC:
            case OP_F64_NEAREST:
            case OP_F64_SQRT:
            case OP_F64_ADD:
            case OP_F64_SUB:
            case OP_F64_MUL:
            case OP_F64_DIV:
            case OP_F64_MIN:
            case OP_F64_MAX:
            case OP_F64_COPYSIGN:

            case OP_I32_WRAP_I64:
            case OP_I32_TRUNC_F32_S:
            case OP_I32_TRUNC_F32_U:
            case OP_I32_TRUNC_F64_S:
            case OP_I32_TRUNC_F64_U:
            case OP_I64_EXTEND_I32_S:
            case OP_I64_EXTEND_I32_U:
            case OP_I64_TRUNC_F32_S:
            case OP_I64_TRUNC_F32_U:
            case OP_I64_TRUNC_F64_S:
            case OP_I64_TRUNC_F64_U:
            case OP_F32_CONVERT_I32_S:
            case OP_F32_CONVERT_I32_U:
            case OP_F32_CONVERT_I64_S:
            case OP_F32_CONVERT_I64_U:
            case OP_F32_DEMOTE_F64:
            case OP_F64_CONVERT_I32_S:
            case OP_F64_CONVERT_I32_U:
            case OP_F64_CONVERT_I64_S:
            case OP_F64_CONVERT_I64_U:
            case OP_F64_PROMOTE_F32:
            case OP_I32_REINTERPRET_F32:
            case OP_I64_REINTERPRET_F64:
            case OP_F32_REINTERPRET_I32:
            case OP_F64_REINTERPRET_I64:

            case OP_I32_EXTEND8_S:
            case OP_I32_EXTEND16_S:
            case OP_I64_EXTEND8_S:
            case OP_I64_EXTEND16_S:
            case OP_I64_EXTEND32_S:
                break;
            
            case OP_TRUNC_SAT:
                __throwiferr(read_byte(&i->op2, buf));
                break;
            
            default:
                PANIC("Decode: unsupported opcode: %x", i->op1);
        }
        
        *instr = i;
    }
    __catch:
        return err;
}

error_t decode_codesec(module_t *mod, buffer_t *buf) {
    __try {
        uint32_t n1;
        __throwif(ERR_FAILED, IS_ERROR(read_u32_leb128(&n1, buf)));

        // assert(n1 == mod->funcs.n)

        VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
            // size is unused
            uint32_t size;
            __throwiferr(read_u32_leb128(&size, buf));

            VECTOR(locals_t) localses;
            uint32_t n2;

            __throwiferr(read_u32_leb128(&n2, buf));
            VECTOR_INIT(&localses, n2, locals_t);

            // count local variables
            uint32_t num_locals = 0;
            VECTOR_FOR_EACH(locals, &localses, locals_t) {
                __throwiferr(read_u32_leb128(&locals->n, buf));
                __throwiferr(read_byte(&locals->type, buf));
                num_locals += locals->n;
            }

            // create vec(valtype)
            VECTOR_INIT(&func->locals, num_locals, valtype_t);
            VECTOR_FOR_EACH(locals, &localses, locals_t) {
                for(uint32_t i = 0; i < locals->n; i++) {
                    func->locals.elem[i] = locals->type;
                }
            }
            
            // decode body
            __throwiferr(decode_instr(&func->body, buf));
            instr_t *instr = func->body;
            while(instr->op1 != OP_END) {
                __throwiferr(decode_instr(&instr->next, buf));
                instr = instr->next;
            }
        }
    }
    __catch:
        return err;
}

error_t decode_module(module_t **mod, uint8_t *image, size_t image_size) {
    __try {    
        buffer_t *buf;
        __throwiferr(new_buffer(&buf, image, image_size));

        uint32_t magic, version;
        __throwiferr(read_u32(&magic, buf));
        __throwiferr(read_u32(&version, buf));

        // init
        module_t *m = *mod = malloc(sizeof(module_t));
        VECTOR_INIT(&m->types, 0, functype_t);
        VECTOR_INIT(&m->funcs, 0, func_t);
        VECTOR_INIT(&m->exports, 0, export_t);

        while(!eof(buf)) {
            uint8_t id;
            uint32_t size;
            __throwiferr(read_byte(&id, buf));
            __throwiferr(read_u32_leb128(&size, buf));
            
            buffer_t *sec;
            __throwiferr(read_buffer(&sec, size, buf));

            if(id <= 11 && decoders[id])
                __throwiferr(decoders[id](m, sec));
        }
    }
    __catch:
        return err;
}