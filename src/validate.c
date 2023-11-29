#include <stdbool.h>
#include "validate.h"
#include "print.h"
#include "exception.h"
#include "memory.h"

// used only in validation
#define STACK_SIZE 4096
typedef struct {
    bool            polymorphic;
    size_t          idx;
    valtype_t       pool[STACK_SIZE];
} type_stack;

static inline bool empty(type_stack *stack) {
    return stack->idx == -1;
}

static inline void push(type_stack *stack, valtype_t ty) {
    // todo: panic if stack is full?
    stack->pool[++stack->idx] = ty;
    //printf("push %x idx: %ld\n", ty, stack->idx);
}

// Type is not verified if expect is 0.
static inline error_t try_pop(type_stack *stack, valtype_t expect) {
    __try {
        if(empty(stack)) {
            __throwif(ERR_TYPE_MISMATCH, !stack->polymorphic);
            //printf("pop %x idx: %ld\n", expect, stack->idx);
        }
        else {
            valtype_t ty;
            ty = stack->pool[stack->idx--];
            if(expect)
                __throwif(ERR_TYPE_MISMATCH, ty != expect && ty != TYPE_ANY);
            //printf("pop %x idx: %ld\n", expect, stack->idx);
        }
    } 
    __catch:
        return err;
}

static inline error_t peek_stack_top(type_stack *stack, valtype_t *t) {
    __try {
        if(empty(stack)) {
            __throwif(ERR_TYPE_MISMATCH, !stack->polymorphic);
            *t = TYPE_ANY;
        }
        else {
            *t = stack->pool[stack->idx];
        }
    }
    __catch:
        return err;
}

error_t validate_blocktype(context_t *C, blocktype_t bt, functype_t *ty) {
    __try {
        VECTOR_INIT(&ty->rt1);

        switch(bt.valtype) {
            case 0x40:
                VECTOR_INIT(&ty->rt2);
                break;

            case TYPE_NUM_I32:
            case TYPE_NUM_I64:
            case TYPE_NUM_F32:
            case TYPE_NUM_F64:
            case TYPE_EXTENREF:
            case TYPE_FUNCREF:
                VECTOR_NEW(&ty->rt2, 1, 1);
                *VECTOR_ELEM(&ty->rt2, 0) = bt.valtype;
                break;

            default:
                // treat as typeidx
                functype_t *type = VECTOR_ELEM(&C->types, bt.typeidx);
                __throwif(ERR_FAILED, !type);
                VECTOR_COPY(&ty->rt1, &type->rt1);
                VECTOR_COPY(&ty->rt2, &type->rt2);
                break;
        }
    }
    __catch:
        return err;
}

error_t validate_instrs(context_t *C, instr_t *start, resulttype_t *rt1, resulttype_t *rt2);

error_t validate_instr(context_t *C, instr_t *ip, type_stack *stack) {
    __try {
        //printf("[+] ip = %x\n", ip->op1);
        switch(ip->op1) {
            case OP_UNREACHABLE:
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            
            case OP_NOP:
                break;
            
            case OP_BLOCK:
            case OP_LOOP: {
                functype_t ty;
                __throwiferr(validate_blocktype(C, ip->bt, &ty));

                // push label
                labeltype_t l;
                
                if(ip->op1 == OP_BLOCK)
                    l.ty = ty.rt2;
                else
                    l.ty = ty.rt1;
                
                list_push_back(&C->labels, &l.link);

                __throwiferr(validate_instrs(C, ip->in1, &ty.rt1, &ty.rt2));

                // valid with type [t1*] -> [t2*]
                VECTOR_FOR_EACH_REVERSE(t, &ty.rt1) {
                    __throwiferr(try_pop(stack, *t));
                }
                VECTOR_FOR_EACH(t,& ty.rt2) {
                    push(stack, *t);
                }

                // cleanup
                // todo: add VECTOR_DESTROY
                list_pop_tail(&C->labels);
                break;
            }

            case OP_IF: {
                functype_t ty;
                __throwif(ERR_FAILED, IS_ERROR(validate_blocktype(C, ip->bt, &ty)));

                // push label
                labeltype_t l = {.ty = ty.rt2};
                list_push_back(&C->labels, &l.link);
                
                __throwiferr(validate_instrs(C, ip->in1, &ty.rt1, &ty.rt2));
                __throwiferr(validate_instrs(C, ip->in2, &ty.rt1, &ty.rt2));
                __throwiferr(try_pop(stack, TYPE_NUM_I32));

                // valid with type [t1*] -> [t2*]
                VECTOR_FOR_EACH(t,& ty.rt1) {
                    __throwiferr(try_pop(stack, *t));
                }
                VECTOR_FOR_EACH(t ,&ty.rt2) {
                    push(stack, *t);
                }
                
                // cleanup
                list_pop_tail(&C->labels);
                break;
            }

            case OP_ELSE:
            case OP_END:
                // nop
                break;
            
            case OP_BR: {
                labeltype_t *l = LIST_GET_ELEM(&C->labels, labeltype_t, link, ip->labelidx);
                __throwif(ERR_UNKNOWN_LABEL, !l);
                VECTOR_FOR_EACH_REVERSE(t, &l->ty) {
                    __throwiferr(try_pop(stack, *t));
                }
                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_BR_IF: {
                labeltype_t *l = LIST_GET_ELEM(&C->labels, labeltype_t, link, ip->labelidx);
                __throwif(ERR_UNKNOWN_LABEL, !l);
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                VECTOR_FOR_EACH_REVERSE(t, &l->ty) {
                    __throwiferr(try_pop(stack, *t));
                }
                VECTOR_FOR_EACH(t, &l->ty) {
                    push(stack, *t);
                }
                break;
            }

            case OP_BR_TABLE: {
                labeltype_t *default_label = LIST_GET_ELEM(&C->labels, labeltype_t, link, ip->default_label);
                __throwif(ERR_UNKNOWN_LABEL, !default_label);
                

                VECTOR_FOR_EACH(i, &ip->labels) {
                    labeltype_t *l = LIST_GET_ELEM(&C->labels, labeltype_t, link, *i);
                    __throwif(ERR_UNKNOWN_LABEL, !l);

                    __throwif(ERR_TYPE_MISMATCH, default_label->ty.len != l->ty.len);

                    __throwiferr(try_pop(stack, TYPE_NUM_I32));
                    VECTOR_FOR_EACH_REVERSE(t, &l->ty) {
                        __throwiferr(try_pop(stack, *t));
                    }

                    // empty the stack
                    stack->idx = -1;
                    stack->polymorphic = true;
                }

                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                VECTOR_FOR_EACH_REVERSE(t, &default_label->ty) {
                    __throwiferr(try_pop(stack, *t));
                }

                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_RETURN: {
                resulttype_t *ty = C->ret;
                __throwif(ERR_FAILED, !ty);
                VECTOR_FOR_EACH_REVERSE(t, ty) {
                    __throwiferr(try_pop(stack, *t));
                }
                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_CALL: {
                functype_t *ty = VECTOR_ELEM(&C->funcs, ip->funcidx);
                __throwif(ERR_UNKNOWN_FUNC, !ty);
                VECTOR_FOR_EACH_REVERSE(t, &ty->rt1) {
                    __throwiferr(try_pop(stack, *t));
                }

                VECTOR_FOR_EACH(t, &ty->rt2) {
                    push(stack, *t);
                }
                break;
            }
            
            case OP_CALL_INDIRECT: {
                tabletype_t *tt = VECTOR_ELEM(&C->tables, ip->x);
                __throwif(ERR_UNKNOWN_TABLE, !tt);

                reftype_t t = tt->reftype;
                __throwif(ERR_FAILED, t != TYPE_FUNCREF);

                functype_t *ft = VECTOR_ELEM(&C->types, ip->y);
                __throwif(ERR_UNKNOWN_TYPE, !ft);
                
                // valid with type [t1* i32] -> [t2*]
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                VECTOR_FOR_EACH_REVERSE(t, &ft->rt1) {
                    __throwiferr(try_pop(stack, *t));
                }
                VECTOR_FOR_EACH(t, &ft->rt2) {
                    push(stack, *t);
                }
                break;
            }

            case OP_DROP:
                __throwiferr(try_pop(stack, 0));
                break;
            
            case OP_SELECT: {
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                valtype_t t;
                __throwiferr(peek_stack_top(stack, &t));
                // reftype is not allowed
                __throwif(ERR_TYPE_MISMATCH, t == TYPE_EXTENREF || t == TYPE_FUNCREF);
                __throwiferr(try_pop(stack, t));
                __throwiferr(try_pop(stack, t));
                push(stack, t);
                break;
            }

            case OP_SELECT_T: {
                __throwif(ERR_INVALID_RESULT_ARITY, ip->types.len != 1);
                valtype_t t = ip->types.elem[0];
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                __throwiferr(try_pop(stack, t));
                __throwiferr(try_pop(stack, t));
                push(stack, t);
                break;
            }

            case OP_LOCAL_GET: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_UNKNOWN_LOCAL, !t);
                push(stack, *t);
                break;
            }

            case OP_LOCAL_SET: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_UNKNOWN_LOCAL, !t);
                __throwiferr(try_pop(stack, *t));
                break;
            }

            case OP_LOCAL_TEE: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_UNKNOWN_LOCAL, !t);
                __throwiferr(try_pop(stack, *t));
                push(stack, *t);
                break;
            }

            case OP_GLOBAL_GET: {
                globaltype_t *gt = VECTOR_ELEM(&C->globals, ip->globalidx);
                __throwif(ERR_UNKNOWN_GLOBAL, !gt);
                push(stack, gt->type);
                break;
            }

            case OP_GLOBAL_SET: {
                globaltype_t *gt = VECTOR_ELEM(&C->globals, ip->globalidx);
                __throwif(ERR_UNKNOWN_GLOBAL, !gt);
                __throwif(ERR_GLOBAL_IS_IMMUTABLE, !gt->mut);
                __throwiferr(try_pop(stack, gt->type));
                break;
            }

            case OP_TABLE_GET: {
                tabletype_t *t = VECTOR_ELEM(&C->tables, ip->x);
                __throwif(ERR_UNKNOWN_TABLE, !t);
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, t->reftype);
                break;
            }

            case OP_TABLE_SET: {
                tabletype_t *t = VECTOR_ELEM(&C->tables, ip->x);
                __throwif(ERR_UNKNOWN_TABLE, !t);
                __throwiferr(try_pop(stack, t->reftype));
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                break;
            }

            case OP_I32_LOAD:
            case OP_I64_LOAD:
            case OP_F32_LOAD:
            case OP_F64_LOAD:
            case OP_I32_LOAD8_S:
            case OP_I32_LOAD8_U:
            case OP_I32_LOAD16_S:
            case OP_I32_LOAD16_U:
            case OP_I64_LOAD8_S:
            case OP_I64_LOAD8_U:
            case OP_I64_LOAD16_S:
            case OP_I64_LOAD16_U:
            case OP_I64_LOAD32_S:
            case OP_I64_LOAD32_U: {
                memtype_t *mem = VECTOR_ELEM(&C->mems, 0);
                __throwif(ERR_UNKNOWN_MEMORY, !mem);
                int32_t n;
                switch(ip->op1) {
                    case OP_I32_LOAD:
                    case OP_F32_LOAD:
                    case OP_I64_LOAD32_S:
                    case OP_I64_LOAD32_U:
                        n = 4;
                        break;
                    case OP_I64_LOAD:
                    case OP_F64_LOAD:
                        n = 8;
                        break;
                    case OP_I32_LOAD8_S:
                    case OP_I32_LOAD8_U:
                    case OP_I64_LOAD8_S:
                    case OP_I64_LOAD8_U:
                        n = 1;
                        break;
                    case OP_I32_LOAD16_S:
                    case OP_I32_LOAD16_U:
                    case OP_I64_LOAD16_S:
                    case OP_I64_LOAD16_U:
                        n = 2;
                        break;
                }
                __throwif(ERR_ALIGNMENT_MUST_NOT_BE_LARGER_THAN_NATURAL, (1 << ip->m.align) > n);
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                valtype_t t;
                switch(ip->op1) {
                    case OP_I32_LOAD:
                    case OP_I32_LOAD8_S:
                    case OP_I32_LOAD8_U:
                    case OP_I32_LOAD16_S:
                    case OP_I32_LOAD16_U:
                        t = TYPE_NUM_I32;
                        break;
                    case OP_I64_LOAD:
                    case OP_I64_LOAD8_S:
                    case OP_I64_LOAD8_U:
                    case OP_I64_LOAD16_S:
                    case OP_I64_LOAD16_U:
                    case OP_I64_LOAD32_S:
                    case OP_I64_LOAD32_U:
                        t = TYPE_NUM_I64;
                        break;
                    case OP_F32_LOAD:
                        t = TYPE_NUM_F32;
                        break;
                    case OP_F64_LOAD:
                        t = TYPE_NUM_F64;
                        break;
                }
                push(stack, t);
                break;
            }
            case OP_I32_STORE:
            case OP_I64_STORE:
            case OP_F32_STORE:
            case OP_F64_STORE:
            case OP_I32_STORE8:
            case OP_I32_STORE16:
            case OP_I64_STORE8:
            case OP_I64_STORE16:
            case OP_I64_STORE32: {
                memtype_t *mem = VECTOR_ELEM(&C->mems, 0);
                __throwif(ERR_UNKNOWN_MEMORY, !mem);
                int32_t n;
                switch(ip->op1) {
                    case OP_I32_STORE:
                    case OP_F32_STORE:
                    case OP_I64_STORE32:
                        n = 4;
                        break;
                    case OP_I64_STORE:
                    case OP_F64_STORE:
                        n = 8;
                        break;
                    case OP_I32_STORE8:
                    case OP_I64_STORE8:
                        n = 1;
                        break;
                    case OP_I32_STORE16:
                    case OP_I64_STORE16:
                        n = 2;
                        break;
                }
                __throwif(ERR_ALIGNMENT_MUST_NOT_BE_LARGER_THAN_NATURAL, (1 << ip->m.align) > n);
                valtype_t t;
                switch(ip->op1) {
                    case OP_I32_STORE:
                    case OP_I32_STORE8:
                    case OP_I32_STORE16:
                        t = TYPE_NUM_I32;
                        break;
                    case OP_F32_STORE:
                        t = TYPE_NUM_F32;
                        break;
                    case OP_F64_STORE:
                        t = TYPE_NUM_F64;
                        break;
                    case OP_I64_STORE:
                    case OP_I64_STORE8:
                    case OP_I64_STORE16:
                    case OP_I64_STORE32:
                        t = TYPE_NUM_I64;
                        break;
                }
                __throwiferr(try_pop(stack, t));
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                break;
            }

            case OP_MEMORY_SIZE: {
                memtype_t *mem = VECTOR_ELEM(&C->mems, 0);
                __throwif(ERR_UNKNOWN_MEMORY, !mem);
                // valid with [] -> [i32]
                push(stack, TYPE_NUM_I32);
                break;
            }

            case OP_MEMORY_GROW: {
                memtype_t *mem = VECTOR_ELEM(&C->mems, 0);
                __throwif(ERR_UNKNOWN_MEMORY, !mem);
                // valid with [i32] -> [i32]
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, TYPE_NUM_I32);
                break;
            }

            case OP_I32_CONST:
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_I64_CONST:
                push(stack, TYPE_NUM_I64);
                break;
            
            case OP_F32_CONST:
                push(stack, TYPE_NUM_F32);
                break;
            
            case OP_F64_CONST:
                push(stack, TYPE_NUM_F64);
                break;
             
            // testop and unop
            case OP_I32_EQZ:
            case OP_I32_CLZ:
            case OP_I32_CTZ:
            case OP_I32_POPCNT:
            case OP_I32_EXTEND8_S:
            case OP_I32_EXTEND16_S:
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_I64_EQZ:
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_I64_CLZ:
            case OP_I64_CTZ:
            case OP_I64_POPCNT:
            case OP_I64_EXTEND8_S:
            case OP_I64_EXTEND16_S:
            case OP_I64_EXTEND32_S:
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_I64);
                break;
            
            // relop and binop
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
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, TYPE_NUM_I32);
                break;
            
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
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_F32_EQ:
            case OP_F32_NE:
            case OP_F32_LT:
            case OP_F32_GT:
            case OP_F32_LE:
            case OP_F32_GE:
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_F64_EQ:
            case OP_F64_NE:
            case OP_F64_LT:
            case OP_F64_GT:
            case OP_F64_LE:
            case OP_F64_GE:
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                push(stack, TYPE_NUM_I32);
                break;
            
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
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_I64);
                break;
            
            case OP_F32_ABS:
            case OP_F32_NEG:
            case OP_F32_CEIL:
            case OP_F32_FLOOR:
            case OP_F32_TRUNC:
            case OP_F32_NEAREST:
            case OP_F32_SQRT:
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                push(stack, TYPE_NUM_F32);
                break;
            
            case OP_F32_ADD:
            case OP_F32_SUB:
            case OP_F32_MUL:
            case OP_F32_DIV:
            case OP_F32_MIN:
            case OP_F32_MAX:
            case OP_F32_COPYSIGN:
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                push(stack, TYPE_NUM_F32);
                break;
            
            case OP_F64_ABS:
            case OP_F64_NEG:
            case OP_F64_CEIL:
            case OP_F64_FLOOR:
            case OP_F64_TRUNC:
            case OP_F64_NEAREST:
            case OP_F64_SQRT:
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                push(stack, TYPE_NUM_F64);
                break;
            
            case OP_F64_ADD:
            case OP_F64_SUB:
            case OP_F64_MUL:
            case OP_F64_DIV:
            case OP_F64_MIN:
            case OP_F64_MAX:
            case OP_F64_COPYSIGN:
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                push(stack, TYPE_NUM_F64);
                break;
            
            case OP_I32_WRAP_I64:
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_I32_TRUNC_F32_S:
            case OP_I32_TRUNC_F32_U:
            case OP_I32_REINTERPRET_F32:
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_I32_TRUNC_F64_S:
            case OP_I32_TRUNC_F64_U:
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_I64_EXTEND_I32_S:
            case OP_I64_EXTEND_I32_U:
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, TYPE_NUM_I64);
                break;

            case OP_I64_TRUNC_F32_S:
            case OP_I64_TRUNC_F32_U:
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                push(stack, TYPE_NUM_I64);
                break;
            
            case OP_I64_TRUNC_F64_S:
            case OP_I64_TRUNC_F64_U:
            case OP_I64_REINTERPRET_F64:
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                push(stack, TYPE_NUM_I64);
                break;
            
            case OP_F32_CONVERT_I32_S:
            case OP_F32_CONVERT_I32_U:
            case OP_F32_REINTERPRET_I32:
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, TYPE_NUM_F32);
                break;
            
            case OP_F32_CONVERT_I64_S:
            case OP_F32_CONVERT_I64_U:
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_F32);
                break;

            case OP_F32_DEMOTE_F64:
                __throwiferr(try_pop(stack, TYPE_NUM_F64));
                push(stack, TYPE_NUM_F32);
                break;
            
            case OP_F64_CONVERT_I32_S:
            case OP_F64_CONVERT_I32_U:
                __throwiferr(try_pop(stack, TYPE_NUM_I32));
                push(stack, TYPE_NUM_F64);
                break;
            
            case OP_F64_CONVERT_I64_S:
            case OP_F64_CONVERT_I64_U:
            case OP_F64_REINTERPRET_I64:
                __throwiferr(try_pop(stack, TYPE_NUM_I64));
                push(stack, TYPE_NUM_F64);
                break;
            
            case OP_F64_PROMOTE_F32:
                __throwiferr(try_pop(stack, TYPE_NUM_F32));
                push(stack, TYPE_NUM_F64);
                break;
            
            case OP_REF_NULL:
                push(stack, ip->t);
                break;
            
            case OP_REF_IS_NULL:
                __throwiferr(try_pop(stack, TYPE_ANY));
                push(stack, TYPE_NUM_I32);
                break;
            
            case OP_REF_FUNC: {
                functype_t *ty = VECTOR_ELEM(&C->funcs, ip->x);
                __throwif(ERR_UNKNOWN_FUNC, !ty);

                bool is_contained = *VECTOR_ELEM(&C->refs, ip->x);
                if(is_contained) {
                    // valid with type [] -> [funcref]
                    push(stack, TYPE_FUNCREF);
                }
                else
                    __throw(ERR_UNDECLARED_FUNCTIION_REFERENCE);
                break;
            }
            
            case OP_0XFC:
                switch(ip->op2) {
                    case 0x00:
                    case 0x01:
                        __throwiferr(try_pop(stack, TYPE_NUM_F32));
                        push(stack, TYPE_NUM_I32);
                        break;

                    case 0x02:
                    case 0x03:
                        __throwiferr(try_pop(stack, TYPE_NUM_F64));
                        push(stack, TYPE_NUM_I32);
                        break;
                    
                    case 0x04:
                    case 0x05:
                        __throwiferr(try_pop(stack, TYPE_NUM_F32));
                        push(stack, TYPE_NUM_I64);
                        break;
                    
                    case 0x06:
                    case 0x07:
                        __throwiferr(try_pop(stack, TYPE_NUM_F64));
                        push(stack, TYPE_NUM_I64);
                        break;
                    
                    // memory.init
                    case 0x08: {
                        memtype_t *mem = VECTOR_ELEM(&C->mems, 0);
                        __throwif(ERR_UNKNOWN_MEMORY, !mem);
                        ok_t *data = VECTOR_ELEM(&C->datas, ip->x);
                        __throwif(ERR_UNKNOWN_DATA_SEGMENT, !data);
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        break;
                    }

                    // data.drop
                    case 0x09: {
                        ok_t *data = VECTOR_ELEM(&C->datas, ip->x);
                        __throwif(ERR_UNKNOWN_DATA_SEGMENT, !data);
                        break;
                    }

                    // memory.copy
                    case 0x0A:
                    // memory.fill
                    case 0x0B: {
                        memtype_t *mem = VECTOR_ELEM(&C->mems, 0);
                        __throwif(ERR_UNKNOWN_MEMORY, !mem);
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        break;
                    }

                    // table.init
                    case 0x0C: {
                        tabletype_t *t1 = VECTOR_ELEM(&C->tables, ip->x);
                        __throwif(ERR_UNKNOWN_TABLE, !t1);
                        reftype_t *t2 = VECTOR_ELEM(&C->elems, ip->y);
                        __throwif(ERR_UNKNOWN_ELEM_SEGMENT, !t2);
                        __throwif(ERR_TYPE_MISMATCH, t1->reftype != *t2);
                        // validt with type [i32 i32 i32] -> []
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        break;
                    }

                    // elem.drop
                    case 0x0D: {
                        reftype_t *elem = VECTOR_ELEM(&C->elems, ip->x);
                        __throwif(ERR_UNKNOWN_ELEM_SEGMENT, !elem);
                        break;
                    }

                    // table.copy
                    case 0x0E: {
                        tabletype_t *t1 = VECTOR_ELEM(&C->tables, ip->x);
                        __throwif(ERR_UNKNOWN_TABLE, !t1);
                        tabletype_t *t2 = VECTOR_ELEM(&C->tables, ip->y);
                        __throwif(ERR_UNKNOWN_TABLE, !t2);
                        __throwif(ERR_TYPE_MISMATCH, t1->reftype != t2->reftype);
                        // validt with type [i32 i32 i32] -> []
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        break;
                    }

                    // table.grow
                    case 0x0F: {
                        tabletype_t *tt = VECTOR_ELEM(&C->tables, ip->x);
                        __throwif(ERR_UNKNOWN_TABLE, !tt);
                        // valid with type [t i32] -> [i32]
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, tt->reftype));
                        push(stack, TYPE_NUM_I32);
                        break;
                    }

                    // table.size
                    case 0x10: {
                        tabletype_t *tt = VECTOR_ELEM(&C->tables, ip->x);
                        __throwif(ERR_UNKNOWN_TABLE, !tt);
                        // valid with type [] -> [i32]
                        push(stack, TYPE_NUM_I32);
                        break;
                    }

                    // table.fill
                    case 0x11: {
                         tabletype_t *t = VECTOR_ELEM(&C->tables, ip->x);
                        __throwif(ERR_UNKNOWN_TABLE, !t);
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        __throwiferr(try_pop(stack, t->reftype));
                        __throwiferr(try_pop(stack, TYPE_NUM_I32));
                        break;
                    }

                    default:
                        PANIC("Validation: unsupported opcode: 0x7c %x", ip->op2);
                }
                break;
            
            default:
                PANIC("Validation: unsupported opcode: %x\n", ip->op1);
        }       
    } 
    __catch:
        return err;
}

// validate instruction sequence
error_t validate_instrs(context_t *C, instr_t *start, resulttype_t *rt1, resulttype_t *rt2) {
    __try {
        type_stack stack = {.idx = -1, .polymorphic = false};
        VECTOR_FOR_EACH(t, rt1) {
            push(&stack, *t);
        }

        instr_t *ip = start;
        while(ip) {
            __throwiferr(validate_instr(C, ip, &stack));
            ip = ip->next;
        }

        // compare witch expected type
        VECTOR_FOR_EACH_REVERSE(t, rt2) {
            __throwiferr(try_pop(&stack, *t));
        }

        // check if stack is empty
        __throwif(ERR_TYPE_MISMATCH, !empty(&stack));
    }
    __catch:
        return err;
}

error_t validate_expr(context_t *C, expr_t *expr, resulttype_t *rt2) {
    __try {
        type_stack stack = {.idx = -1, .polymorphic = false};

        instr_t *ip = *expr;
        while(ip) {
            __throwiferr(validate_instr(C, ip, &stack));
            ip = ip->next;
        }
        // compare witch expected type
        VECTOR_FOR_EACH_REVERSE(t, rt2) {
            __throwiferr(try_pop(&stack, *t));
        }

        // check if stack is empty
        __throwif(ERR_TYPE_MISMATCH, !empty(&stack));
    }
    __catch:
        return err;
}

error_t validate_func(context_t *C, func_t *func) {
    __try {
        functype_t *expect = VECTOR_ELEM(&C->types, func->type);

        // create context C'
        VECTOR_CONCAT(&C->locals, &expect->rt1, &func->locals);

        labeltype_t l ={.ty = expect->rt2};
        list_push_back(&C->labels, &l.link);
        C->ret = &expect->rt2;

        // validate expr
        __throwiferr(validate_expr(C, &func->body, &expect->rt2));
    }
    __catch:
        return err;
}

error_t validate_tabletype(tabletype_t *tt) {
    __try {
        uint32_t k = UINT32_MAX;
        __throwif(ERR_FAILED, tt->limits.min > k);
        if(tt->limits.has_max) {
            __throwif(ERR_FAILED, tt->limits.max > k);
            __throwif(ERR_SIZE_MINIMUM_MUST_NOT_BE_GREATER_THAN_MAXIMUM, tt->limits.max < tt->limits.min);
        }
    }
    __catch:
        return err;
}

error_t validate_table(table_t *table) {
    return validate_tabletype(&table->type);
}

error_t validate_memtype(memtype_t *memtype) {
    __try {
        uint32_t k = 1<<16;
        __throwif(ERR_MEMORY_SIZE_MUST_BE_AT_MOST_65536_PAGES, memtype->min > k);
        if(memtype->has_max) {
            __throwif(ERR_MEMORY_SIZE_MUST_BE_AT_MOST_65536_PAGES, memtype->max > k);
            __throwif(ERR_SIZE_MINIMUM_MUST_NOT_BE_GREATER_THAN_MAXIMUM, memtype->max < memtype->min);
        }
    }
    __catch:
        return err;
}

error_t validate_mem(mem_t *mem) {
    return validate_memtype(&mem->type);
}

static bool is_constant_expr(context_t *C, expr_t *expr) {
    for(instr_t *i = *expr; i->op1 != OP_END; i = i ->next) {
        if(0x41 <= i->op1 && i->op1 <= 0x44)
            continue;
        else if(i->op1 == OP_REF_FUNC)
            continue;
        else if(i->op1 == OP_REF_NULL)
            continue;
        else if(i->op1 == OP_GLOBAL_GET) {
            globaltype_t *gt = VECTOR_ELEM(&C->globals, i->globalidx);
             // Validate that the global variable is immutable only if it exists.
            if(!gt)
                continue;
            if(gt->mut != 0)
                return false;
            continue;
        }
        else
            return false;
    }
    return true;
}

error_t validate_global(context_t *ctx, global_t *global) {
    resulttype_t rt2 = {.len = 1, .elem = &global->gt.type};
    __try {
        __throwif(ERR_CONSTANT_EXPRESSION_REQUIRED, !is_constant_expr(ctx, &global->expr));
        __throwiferr(validate_expr(ctx, &global->expr, &rt2));
    }
    __catch:
        return err;
}

error_t validate_elemmode(context_t *ctx, elemmode_t *mode, reftype_t expect) {
    __try {
        switch(mode->kind) {
            // active
            case 0x00:
                tabletype_t *tt = VECTOR_ELEM(&ctx->tables, mode->table);
                __throwif(ERR_UNKNOWN_TABLE, !tt);

                // expr must be constant
                __throwif(ERR_CONSTANT_EXPRESSION_REQUIRED, !is_constant_expr(ctx, &mode->offset));

                // expr must be valid with result type [i32]
                valtype_t type_i32 = TYPE_NUM_I32;
                resulttype_t rt2 = {.len = 1, .elem = &type_i32};
                __throwiferr(validate_expr(ctx, &mode->offset, &rt2));

                __throwif(ERR_TYPE_MISMATCH, tt->reftype != expect);
                
                break;
            
            // passive and declarative
            case 0x01:
            case 0x02:
                break;
                    
            default:
                PANIC("unsupported elemmode %x", mode->kind);
        }
    }
    __catch:
        return err;
}

error_t validate_elem(context_t *C, elem_t *elem) {
    __try {
        VECTOR_FOR_EACH(e, &elem->init) {
            // e must be constant
            __throwif(ERR_CONSTANT_EXPRESSION_REQUIRED, !is_constant_expr(C, e));

            // e must be valid with result type [t]
            resulttype_t rt2 = {.len = 1, .elem = &elem->type};
            __throwiferr(validate_expr(C, e, &rt2));
        }

        // elemmode must be valid with reference type t
        __throwiferr(validate_elemmode(C, &elem->mode, elem->type));
    }
    __catch:
        return err;
}

error_t validate_data(context_t *C, data_t *data) {
    __try {
        switch(data->mode.kind) {
            case DATA_MODE_ACTIVE:
                memtype_t *m = VECTOR_ELEM(&C->mems, data->mode.memory);
                __throwif(ERR_UNKNOWN_MEMORY, !m);

                // expr must be constant
                __throwif(ERR_CONSTANT_EXPRESSION_REQUIRED, !is_constant_expr(C, &data->mode.offset));

                // expr must be valid with result type [i32]
                valtype_t type_i32 = TYPE_NUM_I32;
                resulttype_t rt2 = {.len = 1, .elem = &type_i32};
                __throwiferr(validate_expr(C, &data->mode.offset, &rt2));

                
                break;
            case DATA_MODE_PASSIVE:
                break;
        }
    }
    __catch:
        return err;
}

static void mark_funcidx_in_expr(context_t *C, expr_t *expr) {
    instr_t *ip = *expr;
    while(ip) {
        if(ip->op1 == OP_REF_FUNC && ip->x < C->refs.len) {
            *VECTOR_ELEM(&C->refs, ip->x) = true;
        }
        ip = ip->next;
    }
}

error_t validate_module(module_t *mod) {
    __try {
        // create context C
        context_t C;
        VECTOR_COPY(&C.types, &mod->types);
        VECTOR_NEW(&C.funcs, 0, mod->num_func_imports + mod->funcs.len);
        VECTOR_NEW(&C.tables, 0, mod->num_table_imports + mod->tables.len);
        VECTOR_NEW(&C.mems, 0, mod->num_mem_imports + mod->mems.len);
        VECTOR_NEW(&C.globals, 0, mod->num_global_imports + mod->globals.len);
        VECTOR_NEW(&C.elems, 0, mod->elems.len);
        VECTOR_NEW(&C.datas, 0, mod->datas.len);
        VECTOR_INIT(&C.locals);
        LIST_INIT(&C.labels);
        C.ret = NULL;
        
        VECTOR_NEW(&C.refs, mod->num_func_imports + mod->funcs.len, mod->num_func_imports + mod->funcs.len);
        VECTOR_FOR_EACH(global, &mod->globals) {
            mark_funcidx_in_expr(&C, &global->expr);
        }
        VECTOR_FOR_EACH(elem, &mod->elems) {
            VECTOR_FOR_EACH(init, &elem->init) {
                mark_funcidx_in_expr(&C, init);
            }
        }
        VECTOR_FOR_EACH(import, &mod->imports) {
            if(import->d.kind == FUNC_IMPORTDESC && import->d.func < C.refs.len) {
                *VECTOR_ELEM(&C.refs, import->d.func) = true;
            }
        }
        VECTOR_FOR_EACH(export, &mod->exports) {
            if(export->exportdesc.kind == FUNC_EXPORTDESC && export->exportdesc.idx < C.refs.len) {
                *VECTOR_ELEM(&C.refs, export->exportdesc.idx) = true;
            }
        }

        // validate imports
        VECTOR_FOR_EACH(import, &mod->imports) {
            switch(import->d.kind) {
                case FUNC_IMPORTDESC: {
                    functype_t *ft = VECTOR_ELEM(&C.types, import->d.func);
                    __throwif(ERR_UNKNOWN_TYPE, !ft);
                    VECTOR_APPEND(&C.funcs, *ft);
                    break;
                }
                case TABLE_IMPORTDESC: {
                    __throwiferr(validate_tabletype(&import->d.table));
                    VECTOR_APPEND(&C.tables, import->d.table);
                    break;
                }
                case MEM_IMPORTDESC: {
                    __throwiferr(validate_memtype(&import->d.mem));
                    VECTOR_APPEND(&C.mems, import->d.mem);
                    break;
                }
                case GLOBAL_IMPORTDESC: {
                    VECTOR_APPEND(&C.globals, import->d.globaltype);
                    break;
                }
            }
        }

        VECTOR_FOR_EACH(func, &mod->funcs) {
            functype_t *functype = VECTOR_ELEM(&C.types, func->type);
            __throwif(ERR_UNKNOWN_TYPE, !functype);
            VECTOR_APPEND(&C.funcs, *functype);
        }
        
        // todo: validate exports

        // validate start function if exists
        if(mod->has_start) {
            functype_t *ft = VECTOR_ELEM(&C.funcs, mod->start);
            __throwif(ERR_UNKNOWN_FUNC, !ft);
            // ft must be [] -> []
            __throwif(ERR_START_FUNCTION, ft->rt1.len != 0 || ft->rt2.len != 0);
        }

        // validate tables
        VECTOR_FOR_EACH(table, &mod->tables) {
            __throwiferr(validate_table(table));
            VECTOR_APPEND(&C.tables, table->type);
        }

        // validate mems
        VECTOR_FOR_EACH(mem, &mod->mems) {
            __throwiferr(validate_mem(mem));
            VECTOR_APPEND(&C.mems, mem->type);
        }

        VECTOR(globaltype_t) globals;
        VECTOR_COPY(&globals, &C.globals);

        // validate globals
        VECTOR_FOR_EACH(global, &mod->globals) {
            __throwiferr(validate_global(&C, global));
            VECTOR_APPEND(&globals, global->gt);
        }

        // validate elems
        VECTOR_FOR_EACH(elem, &mod->elems) {
            __throwiferr(validate_elem(&C, elem));
            VECTOR_APPEND(&C.elems, elem->type);
        }

        // validate datas
        VECTOR_FOR_EACH(data, &mod->datas) {
           __throwiferr(validate_data(&C, data));
           VECTOR_APPEND(&C.datas, 1);
        }

        VECTOR_COPY(&C.globals, &globals);

        // under the context C
        // validate funcs
        VECTOR_FOR_EACH(func, &mod->funcs) {   
            __throwiferr(validate_func(&C, func));
        } 

        // C.mems must be larger than 1
        __throwif(ERR_MULTIPLE_MEMORIES, C.mems.len > 1);
        
    }
    __catch:
        return err;
}