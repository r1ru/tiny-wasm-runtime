#include "exec.h"

// stack
error_t new_stack(stack_t **d) {
    stack_t *stack = *d = malloc(sizeof(stack_t));
    
    *stack = (stack_t) {
        .idx        = -1,
        .num_vals   = 0,
        .num_labels = 0,
        .num_frames = 0,
        .pool       = malloc(STACK_SIZE)
    };

    LIST_INIT(&stack->frames);

    return ERR_SUCCESS;
}

error_t push_val(val_t val, stack_t *stack) {
    if(full(stack))
        return ERR_FAILED;
    
    stack->pool[++stack->idx] = (obj_t) {
        .type   = 0,
        .val    = val 
    };
    stack->num_vals++;

    return ERR_SUCCESS;
}

error_t push_vals(vals_t vals, stack_t *stack) {
    VECTOR_FOR_EACH(val, &vals, val_t) {
        push_val(*val, stack);
    }

    return ERR_SUCCESS;
}

static inline error_t push_i32(int32_t val, stack_t *stack) {
    val_t v = {.type = 0x7f, .num.int32 = val};

    push_val(v, stack);

    return ERR_SUCCESS;
}

error_t push_label(label_t label, stack_t *stack) {
    if(full(stack))
        return ERR_FAILED;
    
    stack->pool[++stack->idx] = (obj_t) {
        .type   = 1,
        .label  = label 
    };
    stack->num_labels++;

    return ERR_SUCCESS;
}

error_t push_frame(frame_t frame, stack_t *stack) {
    if(full(stack))
        return ERR_FAILED;
    
    obj_t *obj = &stack->pool[++stack->idx];

    *obj = (obj_t) {
        .type   = 2,
        .frame  = frame 
    };

    stack->num_frames++;
    list_push_back(&stack->frames, &obj->frame.link);

    return ERR_SUCCESS;
}

error_t pop_val(val_t *val, stack_t *stack) {
    if(empty(stack))
        return ERR_FAILED;
    
    obj_t obj = stack->pool[stack->idx];
    
    *val = obj.val;
    stack->idx--;
    stack->num_vals--;

    return ERR_SUCCESS;
}

// pop all values from the stack top
error_t pop_vals(vals_t *vals, stack_t *stack) {
    // count values
    size_t num_vals = 0;
    size_t i = stack->idx;
    while(stack->pool[i].type == 0) {
        num_vals++;
        i--;
    }
    
    // init vector
    VECTOR_INIT(vals, num_vals, val_t);
    
    // pop values
    VECTOR_FOR_EACH(val, vals, val_t) {
        pop_val(val, stack);
    }

    return ERR_SUCCESS;
}

static inline error_t pop_i32(int32_t *val, stack_t *stack) {
    val_t v;

    pop_val(&v, stack);
    *val = v.num.int32;
    return ERR_SUCCESS;
}

error_t pop_label(label_t *label, stack_t *stack) {
    if(empty(stack))
        return ERR_FAILED;
    
    obj_t obj = stack->pool[stack->idx];
    
    *label = obj.label;
    stack->idx--;
    stack->num_labels--;

    return ERR_SUCCESS;
}

error_t try_pop_label(label_t *label, stack_t *stack) {
    if(empty(stack))
        return ERR_FAILED;
    
    obj_t obj = stack->pool[stack->idx];

    if(obj.type != 1)
        return ERR_FAILED;
    
    *label = obj.label;
    stack->idx--;
    stack->num_labels--;

    return ERR_SUCCESS;
}

error_t pop_frame(frame_t *frame, stack_t *stack) {
    if(empty(stack))
        return ERR_FAILED;
    
    *frame = stack->pool[stack->idx].frame;
    stack->idx--;
    stack->num_frames--;

    return ERR_SUCCESS;
}

// There is no need to use append when instantiating, since everything we need (functions, imports, etc.) is known to us.
// see Note of https://webassembly.github.io/spec/core/exec/modules.html#instantiation

// todo: add externval(support imports)

// There must be enough space in S to allocate all functions.
funcaddr_t allocfunc(store_t *S, func_t *func, moduleinst_t *moduleinst) {
    static funcaddr_t next_addr = 0;

    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, next_addr);
    functype_t *functype = VECTOR_ELEM(&moduleinst->types, func->type);

    funcinst->type   = functype;
    funcinst->module = moduleinst;
    funcinst->code   = func;

    return next_addr++; 
}

moduleinst_t *allocmodule(store_t *S, module_t *module) {
    // allocate moduleinst
    moduleinst_t *moduleinst = malloc(sizeof(moduleinst_t));
   
    VECTOR_COPY(&moduleinst->types, &module->types, functype_t);    
    VECTOR_INIT(&moduleinst->fncaddrs, module->funcs.n, funcaddr_t);

    // allocate funcs
    // In this implementation, the index always matches the address.
    int idx = 0;
    VECTOR_FOR_EACH(func, &module->funcs, func_t) {
        // In this implementation, the index always matches the address.
        *VECTOR_ELEM(&moduleinst->fncaddrs, idx++) = allocfunc(S, func, moduleinst);
    }

    // allocate exports
    VECTOR_COPY(&moduleinst->exports, &module->exports, export_t);

    return moduleinst;
}

error_t instantiate(store_t **S, module_t *module) {
    // allocate store
    store_t *store = *S = malloc(sizeof(store_t));
    
    // init ip
    store->ip = NULL; 

    // allocate stack
    new_stack(&store->stack);

    // allocate funcs
    VECTOR_INIT(&store->funcs, module->funcs.n, funcinst_t);

    moduleinst_t *moduleinst = allocmodule(store, module);

    // todo: support start section

    return ERR_SUCCESS;
}

// execute a sequence of instructions
static error_t exec_instrs(store_t *S) {
    // current ip
    instr_t *ip = S->ip;

    // current frame
    frame_t *F = LIST_TAIL(&S->stack->frames, frame_t, link);

    while(ip) {
        instr_t *next_ip = ip->next;

        switch(ip->op) {
            // ref: https://webassembly.github.io/spec/core/exec/instructions.html#returning-from-a-function
            // ref: https://webassembly.github.io/spec/core/exec/instructions.html#exiting-xref-syntax-instructions-syntax-instr-mathit-instr-ast-with-label-l
            case OP_END: {
                // pop all values from stack
                vals_t vals;
                pop_vals(&vals, S->stack);

                // Divide the cases according to whether there is a label or frame on the stack top.
                label_t L;
                error_t err;
                err = try_pop_label(&L, S->stack);

                switch(err) {
                    case ERR_SUCCESS:
                        // exit instr* with label L
                        push_vals(vals, S->stack);
                        next_ip = L.continuation;
                        break;
                    
                    default: {
                        // return from function
                        frame_t frame;
                        pop_frame(&frame, S->stack);
                        push_vals(vals, S->stack);
                        next_ip = frame.ret;
                        break;
                    }
                }            
                break;
            }

            case OP_LOCAL_GET: {
                localidx_t x = ip->localidx;
                val_t val = *VECTOR_ELEM(&F->locals, x);
                push_val(val, S->stack);
                break;
            }
            case OP_LOCAL_SET: {
                localidx_t x = ip->localidx;
                val_t val;
                pop_val(&val, S->stack);
                *VECTOR_ELEM(&F->locals, x) = val;
                break;
            }
            case OP_I32_ADD: {
                int32_t rhs, lhs;
                pop_i32(&rhs, S->stack);
                pop_i32(&lhs, S->stack);
                push_i32(lhs + rhs, S->stack);
                break;
            }
        }
        
        // update ip
        ip = S->ip = next_ip;
    }

    return ERR_SUCCESS;
}

// ref: https://webassembly.github.io/spec/core/exec/instructions.html#function-calls
error_t invoke_func(store_t *S, funcaddr_t funcaddr) {
    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, funcaddr);
    functype_t *functype = funcinst->type;

    // create new frame
    frame_t frame;
    frame.module = funcinst->module;
    VECTOR_INIT(&frame.locals, funcinst->code->locals.n, val_t);

    // pop args
    uint32_t num_args = functype->rt1.n;

    VECTOR_FOR_EACH(local, &frame.locals, val_t) {
        pop_val(local, S->stack);
        if(--num_args == 0) break;
    }

    // push activation frame
    frame.arity  = functype->rt2.n;
    // S->ip is expected to be a call instruction.
    frame.ret    = S->ip ? S->ip->next : NULL;
    push_frame(frame, S->stack);

    // create label L
    static instr_t end = {.op = OP_END};
    label_t L = {.arity = functype->rt2.n, .continuation = &end};

    // enter instr* with label L
    push_label(L, S->stack);

    // set ip and execute
    S->ip = funcinst->code->body;
    exec_instrs(S);

    return ERR_SUCCESS;
}

// The args is a reference to args_t. 
// This is because args is also used to return results.
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args) {
    if(funcaddr < 0 || (S->funcs.n - 1) < funcaddr) {
        return ERR_FAILED;
    }

    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, funcaddr);
    functype_t *functype = funcinst->type;

    if(args->n != functype->rt1.n)
        return ERR_FAILED;
   
    int idx = 0;
    VECTOR_FOR_EACH(val, args, val_t) {
        if(val->type != *VECTOR_ELEM(&functype->rt1, idx++))
            return ERR_FAILED;
    }

    // push dummy frame
    frame_t frame = {
        .arity  = 0,
        .locals =  {.n = 0, .elem = NULL},
        .module = NULL
    };
    push_frame(frame, S->stack);
    
    // push args
    VECTOR_FOR_EACH(val, args, val_t) {
        push_val(*val, S->stack);
    }

    // invoke func
    invoke_func(S, funcaddr);

    // reuse args to return results since it is no longer used.
    //free(args->elem);
    VECTOR_INIT(args, functype->rt2.n, val_t);
    VECTOR_FOR_EACH(ret, args, val_t) {
        pop_val(ret, S->stack);
    }

    return ERR_SUCCESS;
}