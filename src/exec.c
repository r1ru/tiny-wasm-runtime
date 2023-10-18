#include "exec.h"
#include "print.h"

// stack
void new_stack(stack_t **d) {
    stack_t *stack = *d = malloc(sizeof(stack_t));
    
    if(!stack)
        PANIC("out of memory");
    
    *stack = (stack_t) {
        .idx        = -1,
        .pool       = malloc(STACK_SIZE)
    };

    LIST_INIT(&stack->frames);
}

static inline bool full(stack_t *s) {
    return s->idx == NUM_STACK_ENT;
}

void push_val(val_t val, stack_t *stack) {
    if(full(stack))
        PANIC("stack is full");
    
    stack->pool[++stack->idx] = (obj_t) {
        .type   = TYPE_VAL,
        .val    = val 
    };
    //printf("push val: %x idx: %ld\n", val.num.i32, stack->idx);
}

static inline void push_i32(int32_t val, stack_t *stack) {
    val_t v = {.num.i32 = val};
    push_val(v, stack);
}

void push_vals(vals_t vals, stack_t *stack) {
    VECTOR_FOR_EACH(val, &vals, val_t) {
        push_val(*val, stack);
    }
}

void push_label(label_t label, stack_t *stack) {
    if(full(stack))
        PANIC("stack is full");
    
    stack->pool[++stack->idx] = (obj_t) {
        .type   = TYPE_LABEL,
        .label  = label 
    };
    //printf("push label idx: %ld\n", stack->idx);
}

void push_frame(frame_t frame, stack_t *stack) {
    if(full(stack))
        PANIC("stack is full");
    
    obj_t *obj = &stack->pool[++stack->idx];

    *obj = (obj_t) {
        .type   = TYPE_FRAME,
        .frame  = frame 
    };
    list_push_back(&stack->frames, &obj->frame.link);
    //printf("push frame idx: %ld\n", stack->idx);
}

void pop_val(val_t *val, stack_t *stack) {    
    *val = stack->pool[stack->idx].val;
    stack->idx--;
    //printf("pop val: %x idx: %ld\n", val->num.i32, stack->idx);
}

static inline void pop_i32(int32_t *val, stack_t *stack) {
    val_t v;
    pop_val(&v, stack);
    *val = v.num.i32;
}

// pop all values from the stack top
void pop_vals(vals_t *vals, stack_t *stack) {
    // count values
    size_t num_vals = 0;
    size_t i = stack->idx;
    while(stack->pool[i].type == TYPE_VAL) {
        num_vals++;
        i--;
    }
    
    // init vector
    VECTOR_INIT(vals, num_vals, val_t);
    
    // pop values
    VECTOR_FOR_EACH(val, vals, val_t) {
        pop_val(val, stack);
    }
}

void pop_label(label_t *label, stack_t *stack) {
    *label = stack->pool[stack->idx].label;
    stack->idx--;
    //printf("pop label idx: %ld\n", stack->idx);
}

error_t try_pop_label(label_t *label, stack_t *stack) { 
    obj_t obj = stack->pool[stack->idx];

    if(obj.type != TYPE_LABEL)
        return ERR_FAILED;
    
    *label = obj.label;
    stack->idx--;
    //printf("pop label idx: %ld\n", stack->idx);
    return ERR_SUCCESS;
}

void pop_frame(frame_t *frame, stack_t *stack) {
    *frame = stack->pool[stack->idx].frame;
    stack->idx--;
    list_pop_tail(&stack->frames);
    //printf("pop frame idx: %ld\n", stack->idx);
}

// There is no need to use append when instantiating, since everything we need (functions, imports, etc.) is known to us.
// see Note of https://webassembly.github.io/spec/core/exec/modules.html#instantiation

// todo: add externval(support imports)

// There must be enough space in S to allocate all functions.
funcaddr_t allocfunc(store_t *S, func_t *func, moduleinst_t *moduleinst) {
    static funcaddr_t next_addr = 0;

    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, next_addr);
    functype_t *functype = &moduleinst->types[func->type];

    funcinst->type   = functype;
    funcinst->module = moduleinst;
    funcinst->code   = func;

    return next_addr++; 
}

moduleinst_t *allocmodule(store_t *S, module_t *module) {
    // allocate moduleinst
    moduleinst_t *moduleinst = malloc(sizeof(moduleinst_t));
    
    moduleinst->types = module->types.elem;

    // allocate funcs
    // In this implementation, the index always matches the address.
    uint32_t num_funcs = module->funcs.n;
    moduleinst->funcaddrs = malloc(sizeof(funcaddr_t) * num_funcs);
    for(uint32_t i = 0; i < num_funcs; i++) {
        func_t *func = VECTOR_ELEM(&module->funcs, i);
        moduleinst->funcaddrs[i] = allocfunc(S, func, moduleinst);
    }

    moduleinst->exports = module->exports.elem;

    return moduleinst;
}

error_t instantiate(store_t **S, module_t *module) {
    // allocate store
    store_t *store = *S = malloc(sizeof(store_t));
    
    // allocate stack
    new_stack(&store->stack);

    // allocate funcs
    VECTOR_INIT(&store->funcs, module->funcs.n, funcinst_t);

    moduleinst_t *moduleinst = allocmodule(store, module);

    // todo: support start section

    return ERR_SUCCESS;
}

static int32_t i32_eqz(int32_t c) {
    return c == 0;
}

// ref: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
static int32_t i32_clz(int32_t c) {
    if(c == 0)
        return 32;
    else 
        return __builtin_clz(c);
}

static int32_t i32_ctz(int32_t c) {
    return __builtin_ctz(c);
}

static int32_t i32_popcnt(int32_t c) {
    return __builtin_popcount(c);
}

static int32_t i32_extend8_s(int32_t c) {
    return (int32_t)(int8_t)c;
}

static int32_t i32_extend16_s(int32_t c) {
    return (int32_t)(int16_t)c;
}

typedef int32_t (*unop_t) (int32_t);

static unop_t i32_unop[] = {
    [OP_I32_EQZ]        = i32_eqz,
    [OP_I32_CLZ]        = i32_clz,
    [OP_I32_CTZ]        = i32_ctz,
    [OP_I32_POPCNT]     = i32_popcnt,
    [OP_I32_EXTEND8_S]  = i32_extend8_s,
    [OP_I32_EXTEND16_S] = i32_extend16_s,
};

static int32_t i32_eq(int32_t lhs, int32_t rhs) {
    return lhs == rhs;
}

static int32_t i32_ne(int32_t lhs, int32_t rhs) {
    return lhs != rhs;
}

static int32_t i32_lt_s(int32_t lhs, int32_t rhs) {
    return lhs < rhs;
}

static int32_t i32_lt_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u < rhs_u;
}

static int32_t i32_gt_s(int32_t lhs, int32_t rhs) {
    return lhs > rhs;
}

static int32_t i32_gt_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u > rhs_u;
}

static int32_t i32_le_s(int32_t lhs, int32_t rhs) {
    return lhs <= rhs;
}

static int32_t i32_le_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u <= rhs_u;
}

static int32_t i32_ge_s(int32_t lhs, int32_t rhs) {
    return lhs >= rhs;
}

static int32_t i32_ge_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u >= rhs_u;
}

static int32_t i32_add(int32_t lhs, int32_t rhs) {
    return lhs + rhs;
}

static int32_t i32_sub(int32_t lhs, int32_t rhs) {
    return lhs - rhs;
}

static int32_t i32_mul(int32_t lhs, int32_t rhs) {
    return lhs * rhs;
}

// todo: panic if rhs == 0?
static int32_t i32_div_s(int32_t lhs, int32_t rhs) {
    return lhs / rhs;
}

static int32_t i32_div_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u / rhs_u;
}

// ref: https://github.com/wasm3/wasm3/blob/main/source/m3_exec.h
// ref: https://github.com/wasm3/wasm3/blob/main/source/m3_math_utils.h
static int32_t i32_rem_s(int32_t lhs, int32_t rhs) {
    // todo: trap if rhs == 0
    if(lhs == INT32_MIN && rhs == -1)
        return 0;
    else
        return lhs % rhs;
}

static int32_t i32_rem_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u % rhs_u;
}

static int32_t i32_and(int32_t lhs, int32_t rhs) {
    return lhs & rhs;
}

static int32_t i32_or(int32_t lhs, int32_t rhs) {
    return lhs | rhs;
}

static int32_t i32_xor(int32_t lhs, int32_t rhs) {
    return lhs ^ rhs;
}

static int32_t i32_shl(int32_t lhs, int32_t rhs) {
    return lhs << rhs;
}

static int32_t i32_shr_s(int32_t lhs, int32_t rhs) {
    return lhs >> rhs;
}

static int32_t i32_shr_u(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    return lhs_u >> rhs_u;
}

// ref: https://en.wikipedia.org/wiki/Circular_shift
static int32_t i32_rotl(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    rhs_u &= 31;
    return (lhs_u << rhs_u) | (lhs_u >> ((-rhs_u) & 31));
}

static int32_t i32_rotr(int32_t lhs, int32_t rhs) {
    uint32_t lhs_u = lhs, rhs_u = rhs;
    rhs_u &= 31;
    return (lhs_u >> rhs_u) | (lhs_u << ((-rhs_u) & 31));
}

typedef int32_t (*binop_t) (int32_t, int32_t);

static binop_t i32_binop[] = {
    [OP_I32_EQ]     = i32_eq,
    [OP_I32_NE]     = i32_ne,
    [OP_I32_LT_S]   = i32_lt_s,
    [OP_I32_LT_U]   = i32_lt_u,
    [OP_I32_GT_S]   = i32_gt_s,
    [OP_I32_GT_U]   = i32_gt_u,
    [OP_I32_LE_S]   = i32_le_s,
    [OP_I32_LE_U]   = i32_le_u,
    [OP_I32_GE_S]   = i32_ge_s,
    [OP_I32_GE_U]   = i32_ge_u,
    [OP_I32_ADD]    = i32_add,
    [OP_I32_SUB]    = i32_sub,
    [OP_I32_MUL]    = i32_mul,
    [OP_I32_DIV_S]  = i32_div_s,
    [OP_I32_DIV_U]  = i32_div_u,
    [OP_I32_REM_S]  = i32_rem_s,
    [OP_I32_REM_U]  = i32_rem_u,
    [OP_I32_AND]    = i32_and,
    [OP_I32_OR]     = i32_or,
    [OP_I32_XOR]    = i32_xor,
    [OP_I32_SHL]    = i32_shl,
    [OP_I32_SHR_S]  = i32_shr_s,
    [OP_I32_SHR_U]  = i32_shr_u,
    [OP_I32_ROTL]   = i32_rotl,
    [OP_I32_ROTR]   = i32_rotr,
};

// execute a sequence of instructions
static void exec_instrs(instr_t * ent, store_t *S) {
    instr_t *ip = ent;

    // current frame
    frame_t *F = LIST_TAIL(&S->stack->frames, frame_t, link);

    while(ip) {
        instr_t *next_ip = ip->next;

        switch(ip->op) {
            // todo: consider the case where blocktype is typeidx.
            case OP_BLOCK: {
                label_t L = {
                    .arity = ip->bt.valtype == 0x40 ? 0 : 1,
                    .continuation = ip->next,
                };
                push_label(L, S->stack);
                next_ip = ip->in1;
                break;
            }

            case OP_LOOP: {
               label_t L = {
                    .arity = 0,
                    .continuation = ip,
                };
                push_label(L, S->stack);
                next_ip = ip->in1;
                break; 
            }

            case OP_IF: {
                int32_t c;
                pop_i32(&c, S->stack);

                // enter instr* with label L 
                label_t L = {.arity = 1, .continuation = ip->next};
                push_label(L, S->stack);

                if(c) {
                    next_ip = ip->in1;
                } 
                else {
                    next_ip = ip->in2;
                }
                break;
            }

            // ref: https://webassembly.github.io/spec/core/exec/instructions.html#returning-from-a-function
            // ref: https://webassembly.github.io/spec/core/exec/instructions.html#exiting-xref-syntax-instructions-syntax-instr-mathit-instr-ast-with-label-l
            // The else instruction is treated as an exit from the instruction sequence with a label
            case OP_ELSE:
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
                        break;
                    }
                }            
                break;
            }

            case OP_BR_IF: {
                int32_t c;
                pop_i32(&c, S->stack);

                if(c == 0) {
                    break;
                }
            }
            
            case OP_BR: {
                vals_t vals;
                pop_vals(&vals, S->stack);
                label_t L;
                for(int i = 0; i <= ip->labelidx; i++) {
                    vals_t tmp;
                    pop_vals(&tmp, S->stack);
                    pop_label(&L, S->stack);
                }
                push_vals(vals, S->stack);
                next_ip = L.continuation;
                break;
            }

            case OP_CALL:
                invoke_func(S, F->module->funcaddrs[ip->funcidx]);
                break;

            case OP_LOCAL_GET: {
                localidx_t x = ip->localidx;
                val_t val = F->locals[x];
                push_val(val, S->stack);
                break;
            }

            case OP_LOCAL_SET: {
                localidx_t x = ip->localidx;
                val_t val;
                pop_val(&val, S->stack);
                F->locals[x] = val;
                break;
            }
            
            case OP_I32_CONST:
                push_i32(ip->c.i32, S->stack);
                break;
            
            case OP_I32_EQZ:
            case OP_I32_CLZ:
            case OP_I32_CTZ:
            case OP_I32_POPCNT:
            case OP_I32_EXTEND8_S:
            case OP_I32_EXTEND16_S: {
                int32_t c;
                pop_i32(&c, S->stack);
                push_i32(i32_unop[ip->op](c), S->stack);
                break;
            }
            
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
            case OP_I32_ROTR: {
                int32_t rhs, lhs;
                pop_i32(&rhs, S->stack);
                pop_i32(&lhs, S->stack);
                push_i32(i32_binop[ip->op](lhs, rhs), S->stack);
                break;
            }

            default:
                PANIC("unsupported opcode: %x\n", ip->op);
        }
        
        // update ip
        ip = next_ip;
    }
}

// ref: https://webassembly.github.io/spec/core/exec/instructions.html#function-calls
void invoke_func(store_t *S, funcaddr_t funcaddr) {
    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, funcaddr);
    functype_t *functype = funcinst->type;

    // create new frame
    frame_t frame;
    uint32_t num_locals = functype->rt1.n + funcinst->code->locals.n;
    frame.module = funcinst->module;
    frame.locals = malloc(sizeof(val_t) * num_locals);

    // pop args
    for(int32_t i = (functype->rt1.n - 1); 0 <= i; i--) {
        pop_val(&frame.locals[i], S->stack);
    }

    // push activation frame
    frame.arity  = functype->rt2.n;
    push_frame(frame, S->stack);

    // create label L
    static instr_t end = {.op = OP_END, .next = NULL};
    label_t L = {.arity = functype->rt2.n, .continuation = &end};

    // enter instr* with label L
    push_label(L, S->stack);

    // execute
    exec_instrs(funcinst->code->body ,S);
}

// The args is a reference to args_t. 
// This is because args is also used to return results.
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args) {
    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, funcaddr);
    if(!funcinst)
        return ERR_FAILED;

    functype_t *functype = funcinst->type;

    if(args->n != functype->rt1.n)
        return ERR_FAILED;
   
    size_t idx = 0;
    VECTOR_FOR_EACH(arg, args, arg_t) {
        if(arg->type != *VECTOR_ELEM(&functype->rt1, idx++))
            return ERR_FAILED;
    }

    // Omit the process of pushing the dummy frame onto the stack.
    // ref: https://github.com/WebAssembly/spec/issues/1690
    
    // push args
    VECTOR_FOR_EACH(arg, args, arg_t) {
        push_val(arg->val, S->stack);
    }

    // invoke func
    invoke_func(S, funcaddr);

    // reuse args to return results since it is no longer used.
    //free(args->elem);
    VECTOR_INIT(args, functype->rt2.n, arg_t);
    VECTOR_FOR_EACH(ret, args, arg_t) {
        pop_val(&ret->val, S->stack);
    }

    return ERR_SUCCESS;
}