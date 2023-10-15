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
}

static inline error_t try_pop(valtype_t expect, type_stack *stack) {
    __try {
        if(empty(stack)) {
            __throwif(ERR_FAILED, !stack->polymorphic);
            __throw(ERR_SUCCESS);
        }
        else {
            valtype_t ty;
            ty = stack->pool[stack->idx--];
            __throwif(ERR_FAILED, ty != expect);
        }
    } 
    __catch:
        return err;
}

error_t validate_blocktype(context_t *C, blocktype_t bt, functype_t *ty) {
    // blocktype is expected to be [] -> [t*]
    // todo: consider the case where blocktype is typeidx
    __try {
        switch(bt.valtype) {
            case 0x40:
                ty->rt2 = (resulttype_t){.n = 0, .elem = NULL};
                return ERR_SUCCESS;

            case TYPE_NUM_I32:
                ty->rt1 = (resulttype_t){.n = 0, .elem = NULL};
                VECTOR_INIT(&ty->rt2, 1, valtype_t);
                *VECTOR_ELEM(&ty->rt2, 0) = TYPE_NUM_I32;
                return ERR_SUCCESS;

            default:
                __throw(ERR_FAILED);
        }
    }
    __catch:
        return err;
}

error_t validate_expr(context_t *C, instr_t *start, resulttype_t *expect);

error_t validate_instr(context_t *C, instr_t *ip, type_stack *stack) {
    __try {
        switch(ip->op) {
            case OP_BLOCK:
            case OP_LOOP: {
                functype_t ty;
                __throwif(ERR_FAILED, IS_ERROR(validate_blocktype(C, ip->bt, &ty)));

                // push label
                labeltype_t l = {.ty = ty.rt2};
                list_push_back(&C->labels, &l.link);

                __throwif(ERR_FAILED, IS_ERROR(validate_expr(C, ip->in1, &ty.rt2)));

                VECTOR_FOR_EACH(t ,&ty.rt2, valtype_t) {
                    push(*t, stack);
                }

                // cleanup
                VECTOR_DESTORY(&ty.rt2);
                list_pop_tail(&C->labels);
                break;
            }

            case OP_IF: {
                functype_t ty;
                __throwif(ERR_FAILED, IS_ERROR(validate_blocktype(C, ip->bt, &ty)));

                // push label
                labeltype_t l = {.ty = ty.rt2};
                list_push_back(&C->labels, &l.link);

                __throwif(ERR_FAILED, IS_ERROR(validate_expr(C, ip->in1, &ty.rt2)));
                __throwif(ERR_FAILED, IS_ERROR(validate_expr(C, ip->in2, &ty.rt2)));

                __throwif(ERR_FAILED, IS_ERROR(try_pop(TYPE_NUM_I32, stack)));
                VECTOR_FOR_EACH(t ,&ty.rt2, valtype_t) {
                    push(*t, stack);
                }
                
                // cleanup
                VECTOR_DESTORY(&ty.rt2);
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
                    __throwif(ERR_FAILED, IS_ERROR(try_pop(*t, stack)));
                }
                // empty the stack
                stack->idx = -1;
                stack->polymorphic = true;
                break;
            }

            case OP_BR_IF: {
                labeltype_t *l = LIST_GET_ELEM(&C->labels, labeltype_t, link, ip->labelidx);
                __throwif(ERR_FAILED, !l);
                __throwif(ERR_FAILED, IS_ERROR(try_pop(TYPE_NUM_I32, stack)));
                VECTOR_FOR_EACH(t, &l->ty, valtype_t) {
                    __throwif(ERR_FAILED, IS_ERROR(try_pop(*t, stack)));
                }
                VECTOR_FOR_EACH(t, &l->ty, valtype_t) {
                    push(*t, stack);
                }
                break;
            }

            case OP_CALL: {
                functype_t *ty = VECTOR_ELEM(&C->funcs, ip->funcidx);
                __throwif(ERR_FAILED, !ty);

                VECTOR_FOR_EACH(t, &ty->rt1, valtype_t) {
                    __throwif(ERR_FAILED, IS_ERROR(try_pop(*t, stack)));
                }

                VECTOR_FOR_EACH(t, &ty->rt2, valtype_t) {
                    push(*t, stack);
                }
                break;
            }

            case OP_LOCAL_GET: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_FAILED, !t);
                push(*t, stack);
                break;
            }

            case OP_LOCAL_SET: {
                valtype_t *t = VECTOR_ELEM(&C->locals, ip->localidx);
                __throwif(ERR_FAILED, !t);
                __throwif(ERR_FAILED, IS_ERROR(try_pop(*t, stack)));
                break;
            }
            
            case OP_I32_CONST:
                push(TYPE_NUM_I32, stack);
                break;
            
            // binop
            case OP_I32_ADD:
            case OP_I32_GE_S:
                __throwif(ERR_FAILED, IS_ERROR(try_pop(TYPE_NUM_I32, stack)));
                __throwif(ERR_FAILED, IS_ERROR(try_pop(TYPE_NUM_I32, stack)));
                push(TYPE_NUM_I32, stack);
                break;
        }       
    } 
    __catch:
        return err;
}

// validate instructiin sequence
error_t validate_instrs(context_t *C, instr_t *start, type_stack *stack) {
    __try {
        instr_t *ip = start;
        instr_t *next_ip;
        while(ip) {
            next_ip = ip->next;
            __throwif(ERR_FAILED, IS_ERROR(validate_instr(C, ip, stack)));
            ip = next_ip;
        }
    }
    __catch:
        return err;
}

error_t validate_expr(context_t *C, instr_t *start, resulttype_t *expect) {
    __try {
        type_stack stack = {.idx = -1, .polymorphic = false};
        __throwif(ERR_FAILED, IS_ERROR(validate_instrs(C, start, &stack)));
        
        // compare witch expected type
        VECTOR_FOR_EACH(e, expect, valtype_t) {
            __throwif(ERR_FAILED,IS_ERROR(try_pop(*e, &stack)));
        }
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

        labeltype_t l ={.ty = expect->rt2};
        list_push_back(&ctx.labels, &l.link);
        ctx.ret = &expect->rt2;

        // validate expr
        __throwif(ERR_FAILED, IS_ERROR(validate_expr(&ctx, func->body, &expect->rt2)));

        VECTOR_COPY(&actual->rt1, &expect->rt1, valtype_t);
        
        VECTOR_COPY(&actual->rt2, &expect->rt2, valtype_t);
    }
    __catch:
        VECTOR_DESTORY(&ctx.locals);
        return err;
}

error_t validate_module(module_t *mod) {
    __try {
        // create context
        context_t C;

        VECTOR_COPY(&C.types, &mod->types, functype_t);
        VECTOR_INIT(&C.funcs, mod->funcs.n, functype_t);
        LIST_INIT(&C.labels);

        size_t idx = 0;
        VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
            __throwif(ERR_FAILED, validate_func(&C, func, VECTOR_ELEM(&C.funcs, idx++)));
        }
    }
    __catch:
        return err;
}