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

static inline void push(valtype_t ty, type_stack *stack) {
    // todo: panic if stack is full?
    stack->pool[++stack->idx] = ty;
    //printf("push %x\n", ty);
}

// Type is not verified if expect is 0.
static inline error_t try_pop(valtype_t expect, type_stack *stack) {
    __try {
        if(empty(stack)) {
            __throwif(ERR_TYPE_MISMATCH, !stack->polymorphic);
            //printf("pop %x\n", expect);
            __throw(ERR_SUCCESS);
        }
        else {
            valtype_t ty;
            ty = stack->pool[stack->idx--];
            if(expect)
                __throwif(ERR_TYPE_MISMATCH, ty != expect);
            //printf("pop %x\n", expect);
        }
    } 
    __catch:
        return err;
}

error_t validate_blocktype(context_t *C, blocktype_t bt, functype_t *ty) {
    // blocktype is expected to be [] -> [t*]
    // todo: consider the case where blocktype is typeidx
    __try {
        ty->rt1 = (resulttype_t){.n = 0, .elem = NULL};

        switch(bt.valtype) {
            case 0x40:
                ty->rt2 = (resulttype_t){.n = 0, .elem = NULL};
                break;

            case TYPE_NUM_I32:
            case TYPE_NUM_I64:
            case TYPE_NUM_F32:
            case TYPE_NUM_F64:
                VECTOR_INIT(&ty->rt2, 1, valtype_t);
                *VECTOR_ELEM(&ty->rt2, 0) = bt.valtype;
                break;

            default:
                // treat as typeidx
                functype_t *type = VECTOR_ELEM(&C->types, bt.typeidx);
                __throwif(ERR_FAILED, !type);
                VECTOR_COPY(&ty->rt1, &type->rt1, valtype_t);
                VECTOR_COPY(&ty->rt2, &type->rt2, valtype_t);
                break;
        }
    }
    __catch:
        return err;
}

error_t validate_instrs(context_t *C, instr_t *start, resulttype_t *rt1, resulttype_t *rt2);

error_t validate_instr(context_t *C, instr_t *ip, type_stack *stack) {
    __try {
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
                VECTOR_FOR_EACH(t,& ty.rt1, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }
                VECTOR_FOR_EACH(t,& ty.rt2, valtype_t) {
                    push(*t, stack);
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
                __throwiferr(try_pop(TYPE_NUM_I32, stack));

                // valid with type [t1*] -> [t2*]
                VECTOR_FOR_EACH(t,& ty.rt1, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }
                VECTOR_FOR_EACH(t ,&ty.rt2, valtype_t) {
                    push(*t, stack);
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
                __throwif(ERR_FAILED, !l);
                VECTOR_FOR_EACH(t, &l->ty, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }
                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_BR_IF: {
                labeltype_t *l = LIST_GET_ELEM(&C->labels, labeltype_t, link, ip->labelidx);
                __throwif(ERR_FAILED, !l);
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                VECTOR_FOR_EACH(t, &l->ty, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }
                VECTOR_FOR_EACH(t, &l->ty, valtype_t) {
                    push(*t, stack);
                }
                break;
            }

            case OP_BR_TABLE: {
                labeltype_t *default_label = LIST_GET_ELEM(&C->labels, labeltype_t, link, ip->default_label);
                __throwif(ERR_FAILED, !default_label);

                VECTOR_FOR_EACH(i, &ip->labels, labelidx_t) {
                    labeltype_t *l = LIST_GET_ELEM(&C->labels, labeltype_t, link, *i);
                    __throwif(ERR_FAILED, !l);

                    size_t j = 0;
                    VECTOR_FOR_EACH(t, &default_label->ty, valtype_t) {
                        __throwif(ERR_FAILED, *t != *VECTOR_ELEM(&l->ty, j++));
                    }
                }

                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                VECTOR_FOR_EACH(t, &default_label->ty, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }

                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_RETURN: {
                resulttype_t *ty = C->ret;
                __throwif(ERR_FAILED, !ty);
                VECTOR_FOR_EACH(t, ty, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }
                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_CALL: {
                functype_t *ty = VECTOR_ELEM(&C->funcs, ip->funcidx);
                __throwif(ERR_FAILED, !ty);

                VECTOR_FOR_EACH(t, &ty->rt1, valtype_t) {
                    __throwiferr(try_pop(*t, stack));
                }

                VECTOR_FOR_EACH(t, &ty->rt2, valtype_t) {
                    push(*t, stack);
                }
                break;
            }

            case OP_DROP:
                __throwiferr(try_pop(0, stack));
                break;
            
            case OP_LOCAL_GET: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_FAILED, !t);
                push(*t, stack);
                break;
            }

            case OP_LOCAL_SET: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_FAILED, !t);
                __throwiferr(try_pop(*t, stack));
                break;
            }

            case OP_LOCAL_TEE: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_FAILED, !t);
                __throwiferr(try_pop(*t, stack));
                push(*t, stack);
                break;
            }

            case OP_GLOBAL_GET: {
                globaltype_t *gt = VECTOR_ELEM(&C->globals, ip->globalidx);
                __throwif(ERR_FAILED, !gt);
                push(gt->type, stack);
                break;
            }

             case OP_GLOBAL_SET: {
                globaltype_t *gt = VECTOR_ELEM(&C->globals, ip->globalidx);
                __throwif(ERR_FAILED, !gt);
                __throwif(ERR_FAILED, !gt->mut);
                __throwiferr(try_pop(gt->type, stack));
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
                mem_t *mem = VECTOR_ELEM(&C->mems, 0);
                __throwif(ERR_FAILED, !mem);
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
                __throwif(ERR_FAILED, (1 << ip->m.align) > n);
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
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
                push(t, stack);
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
                mem_t *mem = VECTOR_ELEM(&C->mems, 0);
                __throwif(ERR_FAILED, !mem);
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
                __throwif(ERR_FAILED, (1 << ip->m.align) > n);
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
                __throwiferr(try_pop(t, stack));
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                break;
            }

            case OP_I32_CONST:
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_I64_CONST:
                push(TYPE_NUM_I64, stack);
                break;
            
            case OP_F32_CONST:
                push(TYPE_NUM_F32, stack);
                break;
            
            case OP_F64_CONST:
                push(TYPE_NUM_F64, stack);
                break;
             
            // testop and unop
            case OP_I32_EQZ:
            case OP_I32_CLZ:
            case OP_I32_CTZ:
            case OP_I32_POPCNT:
            case OP_I32_EXTEND8_S:
            case OP_I32_EXTEND16_S:
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_I64_EQZ:
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_I64_CLZ:
            case OP_I64_CTZ:
            case OP_I64_POPCNT:
            case OP_I64_EXTEND8_S:
            case OP_I64_EXTEND16_S:
            case OP_I64_EXTEND32_S:
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_I64, stack);
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
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                push(TYPE_NUM_I32, stack);
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
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_F32_EQ:
            case OP_F32_NE:
            case OP_F32_LT:
            case OP_F32_GT:
            case OP_F32_LE:
            case OP_F32_GE:
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_F64_EQ:
            case OP_F64_NE:
            case OP_F64_LT:
            case OP_F64_GT:
            case OP_F64_LE:
            case OP_F64_GE:
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                push(TYPE_NUM_I32, stack);
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
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_I64, stack);
                break;
            
            case OP_F32_ABS:
            case OP_F32_NEG:
            case OP_F32_CEIL:
            case OP_F32_FLOOR:
            case OP_F32_TRUNC:
            case OP_F32_NEAREST:
            case OP_F32_SQRT:
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                push(TYPE_NUM_F32, stack);
                break;
            
            case OP_F32_ADD:
            case OP_F32_SUB:
            case OP_F32_MUL:
            case OP_F32_DIV:
            case OP_F32_MIN:
            case OP_F32_MAX:
            case OP_F32_COPYSIGN:
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                push(TYPE_NUM_F32, stack);
                break;
            
            case OP_F64_ABS:
            case OP_F64_NEG:
            case OP_F64_CEIL:
            case OP_F64_FLOOR:
            case OP_F64_TRUNC:
            case OP_F64_NEAREST:
            case OP_F64_SQRT:
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                push(TYPE_NUM_F64, stack);
                break;
            
            case OP_F64_ADD:
            case OP_F64_SUB:
            case OP_F64_MUL:
            case OP_F64_DIV:
            case OP_F64_MIN:
            case OP_F64_MAX:
            case OP_F64_COPYSIGN:
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                push(TYPE_NUM_F64, stack);
                break;
            
            case OP_I32_WRAP_I64:
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_I32_TRUNC_F32_S:
            case OP_I32_TRUNC_F32_U:
            case OP_I32_REINTERPRET_F32:
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_I32_TRUNC_F64_S:
            case OP_I32_TRUNC_F64_U:
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                push(TYPE_NUM_I32, stack);
                break;
            
            case OP_I64_EXTEND_I32_S:
            case OP_I64_EXTEND_I32_U:
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                push(TYPE_NUM_I64, stack);
                break;

            case OP_I64_TRUNC_F32_S:
            case OP_I64_TRUNC_F32_U:
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                push(TYPE_NUM_I64, stack);
                break;
            
            case OP_I64_TRUNC_F64_S:
            case OP_I64_TRUNC_F64_U:
            case OP_I64_REINTERPRET_F64:
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                push(TYPE_NUM_I64, stack);
                break;
            
            case OP_F32_CONVERT_I32_S:
            case OP_F32_CONVERT_I32_U:
            case OP_F32_REINTERPRET_I32:
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                push(TYPE_NUM_F32, stack);
                break;
            
            case OP_F32_CONVERT_I64_S:
            case OP_F32_CONVERT_I64_U:
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_F32, stack);
                break;

            case OP_F32_DEMOTE_F64:
                __throwiferr(try_pop(TYPE_NUM_F64, stack));
                push(TYPE_NUM_F32, stack);
                break;
            
            case OP_F64_CONVERT_I32_S:
            case OP_F64_CONVERT_I32_U:
                __throwiferr(try_pop(TYPE_NUM_I32, stack));
                push(TYPE_NUM_F64, stack);
                break;
            
            case OP_F64_CONVERT_I64_S:
            case OP_F64_CONVERT_I64_U:
            case OP_F64_REINTERPRET_I64:
                __throwiferr(try_pop(TYPE_NUM_I64, stack));
                push(TYPE_NUM_F64, stack);
                break;
            
            case OP_F64_PROMOTE_F32:
                __throwiferr(try_pop(TYPE_NUM_F32, stack));
                push(TYPE_NUM_F64, stack);
                break;
            
            case OP_TRUNC_SAT:
                switch(ip->op2) {
                    case 0x00:
                    case 0x01:
                        __throwiferr(try_pop(TYPE_NUM_F32, stack));
                        push(TYPE_NUM_I32, stack);
                        break;

                    case 0x02:
                    case 0x03:
                        __throwiferr(try_pop(TYPE_NUM_F64, stack));
                        push(TYPE_NUM_I32, stack);
                        break;
                    
                    case 0x04:
                    case 0x05:
                        __throwiferr(try_pop(TYPE_NUM_F32, stack));
                        push(TYPE_NUM_I64, stack);
                        break;
                    
                    case 0x06:
                    case 0x07:
                        __throwiferr(try_pop(TYPE_NUM_F64, stack));
                        push(TYPE_NUM_I64, stack);
                        break;
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
        VECTOR_FOR_EACH(t, rt1, valtype_t) {
            push(*t, &stack);
        }

        instr_t *ip = start;
        instr_t *next_ip;
        while(ip) {
            next_ip = ip->next;
            __throwiferr(validate_instr(C, ip, &stack));
            ip = next_ip;
        }

        // compare witch expected type
        VECTOR_FOR_EACH(t, rt2, valtype_t) {
            __throwiferr(try_pop(*t, &stack));
        }

        // check if stack is empty
        __throwif(ERR_FAILED, !empty(&stack));
    }
    __catch:
        return err;
}

error_t validate_expr(context_t *C, instr_t *start, resulttype_t *rt2) {
    __try {
        type_stack stack = {.idx = -1, .polymorphic = false};

        instr_t *ip = start;
        instr_t *next_ip;
        while(ip) {
            next_ip = ip->next;
            __throwiferr(validate_instr(C, ip, &stack));
            ip = next_ip;
        }
        
        // compare witch expected type
        VECTOR_FOR_EACH(t, rt2, valtype_t) {
            __throwiferr(try_pop(*t, &stack));
        }

        // check if stack is empty
        __throwif(ERR_FAILED, !empty(&stack));
    }
    __catch:
        return err;
}

error_t validate_func(context_t *C, func_t *func, functype_t *actual) {
    context_t ctx = *C;
    __try {
        functype_t *expect = VECTOR_ELEM(&C->types, func->type);
        __throwif(ERR_FAILED, !expect);

        // create context C'
        VECTOR_CONCAT(&ctx.locals, &expect->rt1, &func->locals, valtype_t);

        LIST_INIT(&ctx.labels);
        labeltype_t l ={.ty = expect->rt2};
        list_push_back(&ctx.labels, &l.link);
        ctx.ret = &expect->rt2;

        // validate expr
        __throwiferr(validate_expr(&ctx, func->body, &expect->rt2));

        VECTOR_COPY(&actual->rt1, &expect->rt1, valtype_t);
        VECTOR_COPY(&actual->rt2, &expect->rt2, valtype_t);
    }
    __catch:
        VECTOR_DESTORY(&ctx.locals);
        return err;
}

static error_t validate_limits(limits_t *limits, uint32_t k) {
    __try {
        __throwif(ERR_FAILED, limits->min > k);
        if(limits->max) {
            __throwif(ERR_FAILED, limits->max > k);
            __throwif(ERR_FAILED, limits->max < limits->min);
        }
    }
    __catch:
}

error_t validate_mem(context_t *ctx, mem_t *src, mem_t *dst) {
    __try {
        __throwiferr(validate_limits(&src->type, 1<<16));
        // append
        *dst = *src;
    }
    __catch:    
        return err;
}

error_t validate_global(context_t *ctx, global_t *g, globaltype_t *dst) {
    resulttype_t rt2 = {.n = 1, .elem = &g->gt.type};
    __try {
        __throwiferr(validate_expr(ctx, g->expr, &rt2));
        // append 
        *dst = g->gt;
    }
    __catch:
        return err;
}

error_t validate_module(module_t *mod) {
    __try {
        // create context C
        context_t C;
        VECTOR_COPY(&C.types, &mod->types, functype_t);
        VECTOR_INIT(&C.funcs, mod->funcs.n, functype_t);
        VECTOR_INIT(&C.mems, mod->mems.n, mem_t);
        VECTOR_INIT(&C.globals, mod->globals.n, globaltype_t);
        VECTOR_INIT(&C.locals, 0, valtype_t);
        LIST_INIT(&C.labels);
        C.ret = NULL;

        // C.mems must be larger than 1
        __throwif(ERR_FAILED, C.mems.n > 1);

        // validate mems
        size_t idx = 0;
        VECTOR_FOR_EACH(m, &mod->mems, mem_t) {
            __throwiferr(validate_mem(&C, m, VECTOR_ELEM(&C.mems, idx++)));
        }

        // validate globals
        idx = 0;
        VECTOR_FOR_EACH(g, &mod->globals, global_t) {
            __throwiferr(validate_global(&C, g, VECTOR_ELEM(&C.globals, idx++)));
        }

        // validte funcs
        idx = 0;
        VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
            __throwiferr(validate_func(&C, func, VECTOR_ELEM(&C.funcs, idx++)));
        }
    }
    __catch:
        return err;
}