#include "exec.h"
#include "print.h"
#include "exception.h"

// todo: fix this?
#include <math.h>

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
    LIST_INIT(&stack->labels);
}

static inline bool full(stack_t *s) {
    return s->idx == NUM_STACK_ENT;
}

static inline bool empty(stack_t *s) {
    return s->idx == -1;
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

static inline void push_i64(int64_t val, stack_t *stack) {
    val_t v = {.num.i64 = val};
    push_val(v, stack);
}

static inline void push_f32(float val, stack_t *stack) {
    val_t v = {.num.f32 = val};
    push_val(v, stack);
}

static inline void push_f64(double val, stack_t *stack) {
    val_t v = {.num.f64 = val};
    push_val(v, stack);
}

// todo: fix this?
void push_vals(vals_t vals, stack_t *stack) {
    size_t num_vals = vals.n;
    for(int32_t i = (num_vals - 1); 0 <= i; i--) {
        push_val(vals.elem[i], stack);
    }
}

void push_label(label_t label, stack_t *stack) {
    if(full(stack))
        PANIC("stack is full");
    
    obj_t *obj = &stack->pool[++stack->idx];

    *obj = (obj_t) {
        .type   = TYPE_LABEL,
        .label  = label 
    };
    list_push_back(&stack->labels, &obj->label.link);
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

static inline void pop_i64(int64_t *val, stack_t *stack) {
    val_t v;
    pop_val(&v, stack);
    *val = v.num.i64;
}

static inline void pop_f32(float *val, stack_t *stack) {
    val_t v;
    pop_val(&v, stack);
    *val = v.num.f32;
}

static inline void pop_f64(double *val, stack_t *stack) {
    val_t v;
    pop_val(&v, stack);
    *val = v.num.f64;
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

void pop_vals_n(vals_t *vals, size_t n, stack_t *stack) {
    // init vector
    VECTOR_INIT(vals, n, val_t);
    
    // pop values
    VECTOR_FOR_EACH(val, vals, val_t) {
        pop_val(val, stack);
    }
}

// pop while stack top is not a frame
void pop_while_not_frame(stack_t *stack) {
    while(stack->pool[stack->idx].type != TYPE_FRAME) {
        stack->idx--;
    }
}

void pop_label(label_t *label, stack_t *stack) {
    *label = stack->pool[stack->idx].label;
    stack->idx--;
    list_pop_tail(&stack->labels);
    //printf("pop label idx: %ld\n", stack->idx);
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

        // alloc func
        funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, i);
        functype_t *functype = &moduleinst->types[func->type];

        funcinst->type   = functype;
        funcinst->module = moduleinst;
        funcinst->code   = func;
        moduleinst->funcaddrs[i] = i;
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

static void expand_F(functype_t *ty, blocktype_t bt, frame_t *F) {
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
            *ty = F->module->types[bt.typeidx];
            break;
    }
}

// execute a sequence of instructions
// ref: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
// ref: https://github.com/wasm3/wasm3/blob/main/source/m3_exec.h
// ref: https://github.com/wasm3/wasm3/blob/main/source/m3_math_utils.h
// ref: https://en.wikipedia.org/wiki/Circular_shift
static error_t invoke_func(store_t *S, funcaddr_t funcaddr);
error_t exec_instrs(instr_t * ent, store_t *S) {
    instr_t *ip = ent;

    // current frame
    frame_t *F = LIST_TAIL(&S->stack->frames, frame_t, link);

    __try {
        while(ip) {
            instr_t *next_ip = ip->next;
            static instr_t end = {.op1 = OP_END};

            int32_t rhs_i32, lhs_i32;
            int64_t rhs_i64, lhs_i64;
            float   rhs_f32, lhs_f32;
            double  rhs_f64, lhs_f64;

            // unary operator
            if(ip->op1 == OP_I32_EQZ || OP_I32_CLZ <= ip->op1 && ip->op1 <= OP_I32_POPCNT ||
               OP_I64_EXTEND_I32_S <= ip->op1 && ip->op1 <= OP_I64_EXTEND_I32_U ||
               OP_I32_EXTEND8_S <= ip->op1 && ip->op1 <= OP_I32_EXTEND16_S ||
               OP_F32_CONVERT_I32_S <= ip->op1 && ip->op1 <= OP_F32_CONVERT_I32_U ||
               OP_F64_CONVERT_I32_S <= ip->op1 && ip->op1 <= OP_F64_CONVERT_I32_U ||
               ip->op1 == OP_F32_REINTERPRET_I32) {
                pop_i32(&lhs_i32, S->stack);
            }
            if(ip->op1 == OP_I64_EQZ || OP_I64_CLZ <= ip->op1 && ip->op1 <= OP_I64_POPCNT ||
               OP_I64_EXTEND8_S <= ip->op1 && ip->op1 <= OP_I64_EXTEND32_S ||
               ip->op1 == OP_I32_WRAP_I64 || 
               OP_F32_CONVERT_I64_S <= ip->op1 && ip->op1 <= OP_F32_CONVERT_I64_U || 
               OP_F64_CONVERT_I64_S <= ip->op1 && ip->op1 <= OP_F64_CONVERT_I64_U ||
               ip->op1 == OP_F64_REINTERPRET_I64) {
                pop_i64(&lhs_i64, S->stack);
            }
            if(OP_F32_ABS <= ip->op1 && ip->op1 <= OP_F32_SQRT || 
               OP_I32_TRUNC_F32_S <= ip->op1 && ip->op1 <= OP_I32_TRUNC_F32_U ||
               OP_I64_TRUNC_F32_S <= ip->op1 && ip->op1 <= OP_I64_TRUNC_F32_U ||
               ip->op1 == OP_F64_PROMOTE_F32 || 
               ip->op1 == OP_I32_REINTERPRET_F32 ||
               ip->op1 == OP_TRUNC_SAT && (ip->op2 == 0 || ip->op2 == 1 || ip->op2 == 4 || ip->op2 == 5)) {
                pop_f32(&lhs_f32, S->stack);
            }
            if(OP_F64_ABS <= ip->op1 && ip->op1 <= OP_F64_SQRT ||
               OP_I32_TRUNC_F64_S <= ip->op1 && ip->op1 <= OP_I32_TRUNC_F64_U || 
               OP_I64_TRUNC_F64_S <= ip->op1 && ip->op1 <= OP_I64_TRUNC_F64_U || 
               ip->op1 == OP_F32_DEMOTE_F64 || 
               ip->op1 == OP_I64_REINTERPRET_F64 ||
               ip->op1 == OP_TRUNC_SAT && (ip->op2 == 2 || ip->op2 == 3 || ip->op2 == 6 || ip->op2 == 7)) {
                pop_f64(&lhs_f64, S->stack);
            }
            
            // binary operator
            if(OP_I32_EQ <= ip->op1 && ip->op1 <= OP_I32_GE_U || 
               OP_I32_ADD <= ip->op1 && ip->op1 <= OP_I32_ROTR) {
                pop_i32(&rhs_i32, S->stack);
                pop_i32(&lhs_i32, S->stack);
            }
            if(OP_I64_EQ <= ip->op1 && ip->op1 <= OP_I64_GE_U || 
               OP_I64_ADD <= ip->op1 && ip->op1 <= OP_I64_ROTR) {
                pop_i64(&rhs_i64, S->stack);
                pop_i64(&lhs_i64, S->stack);
            }
            if(OP_F32_EQ <= ip->op1 && ip->op1 <= OP_F32_GE ||
               OP_F32_ADD <= ip->op1 && ip->op1 <= OP_F32_COPYSIGN) {
                pop_f32(&rhs_f32, S->stack);
                pop_f32(&lhs_f32, S->stack);
            }
            if(OP_F64_EQ <= ip->op1 && ip->op1 <= OP_F64_GE ||
               OP_F64_ADD <= ip->op1 && ip->op1 <= OP_F64_COPYSIGN) {
                pop_f64(&rhs_f64, S->stack);
                pop_f64(&lhs_f64, S->stack);
            }

            switch(ip->op1) {
                case OP_BLOCK: {
                    functype_t ty;
                    expand_F(&ty, ip->bt, F);
                    label_t L = {
                        .arity  = ty.rt2.n,
                        .parent = ip,
                        .continuation = &end,
                    };
                    vals_t vals;
                    pop_vals_n(&vals, ty.rt1.n, S->stack);
                    push_label(L, S->stack);
                    push_vals(vals, S->stack);
                    next_ip = ip->in1;
                    break;
                }

                case OP_LOOP: {
                    functype_t ty;
                    expand_F(&ty, ip->bt, F);
                    label_t L = {
                        .arity = ty.rt1.n,
                        .parent = ip,
                        .continuation = ip,
                    };
                    vals_t vals;
                    pop_vals_n(&vals, ty.rt1.n, S->stack);
                    push_label(L, S->stack);
                    push_vals(vals, S->stack);
                    next_ip = ip->in1;
                    break; 
                }

                case OP_IF: {
                    int32_t c;
                    pop_i32(&c, S->stack);

                    functype_t ty;
                    expand_F(&ty, ip->bt, F);
                    label_t L = {
                        .arity = ty.rt2.n,
                        .parent = ip,
                        .continuation = &end,
                    };
                    vals_t vals;
                    pop_vals_n(&vals, ty.rt1.n, S->stack);
                    push_label(L, S->stack);
                    push_vals(vals, S->stack);

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
                    // exit instr* with label L
                    // pop all values from stack
                    vals_t vals;
                    pop_vals(&vals, S->stack);

                    // exit instr* with label L
                    label_t l;
                    pop_label(&l, S->stack);
                    push_vals(vals, S->stack);
                    next_ip = l.parent != NULL ? l.parent->next : NULL;
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
                    label_t *l = LIST_GET_ELEM(&S->stack->labels, label_t, link, ip->labelidx);
                    vals_t vals;
                    pop_vals_n(&vals, l->arity, S->stack);
                    label_t L;
                    for(int i = 0; i <= ip->labelidx; i++) {
                        vals_t tmp;
                        pop_vals(&tmp, S->stack);
                        pop_label(&L, S->stack);
                    }
                    push_vals(vals, S->stack);

                    // br instruction of block, if is treated as a "end" instruction
                    switch(L.parent->op1) {
                        case OP_BLOCK:
                        case OP_IF:
                            next_ip = L.parent->next;
                            break;
                        case OP_LOOP:
                            next_ip = L.continuation;
                            break;
                    }
                    break;
                }

                // nop
                case OP_RETURN: {
                    next_ip = NULL;
                    break;
                }

                case OP_CALL: {
                    // invoke func
                    __throwiferr(invoke_func(S, F->module->funcaddrs[ip->funcidx]));
                    break;
                }

                case OP_DROP: {
                    val_t val;
                    pop_val(&val, S->stack);
                    break;
                }

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
                
                case OP_I64_CONST:
                    push_i64(ip->c.i64, S->stack);
                    break;
                
                case OP_F32_CONST:
                    push_f32(ip->c.f32, S->stack);
                    break;
                
                case OP_F64_CONST:
                    push_f64(ip->c.f64, S->stack);
                    break;
                
                case OP_I32_EQZ:
                    push_i32(lhs_i32 == 0, S->stack);
                    break;
                                    
                case OP_I32_EQ:
                    push_i32(lhs_i32 == rhs_i32, S->stack);
                    break;
                
                case OP_I32_NE:
                    push_i32(lhs_i32 != rhs_i32, S->stack);
                    break;
                
                case OP_I32_LT_S:
                    push_i32(lhs_i32 < rhs_i32, S->stack);
                    break;
                
                case OP_I32_LT_U:
                    push_i32((uint32_t)lhs_i32 < (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I32_GT_S:
                    push_i32(lhs_i32 > rhs_i32, S->stack);
                    break;
                
                case OP_I32_GT_U:
                    push_i32((uint32_t)lhs_i32 > (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I32_LE_S:
                    push_i32(lhs_i32 <= rhs_i32, S->stack);
                    break;
                
                case OP_I32_LE_U:
                    push_i32((uint32_t)lhs_i32 <= (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I32_GE_S:
                    push_i32(lhs_i32 >= rhs_i32, S->stack);
                    break;
                
                case OP_I32_GE_U:
                    push_i32((uint32_t)lhs_i32 >= (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I64_EQZ:
                    push_i32(lhs_i64 == 0, S->stack);
                    break;
                                    
                case OP_I64_EQ:
                    push_i32(lhs_i64 == rhs_i64, S->stack);
                    break;
                
                case OP_I64_NE:
                    push_i32(lhs_i64 != rhs_i64, S->stack);
                    break;
                
                case OP_I64_LT_S:
                    push_i32(lhs_i64 < rhs_i64, S->stack);
                    break;
                
                case OP_I64_LT_U:
                    push_i32((uint64_t)lhs_i64 < (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_I64_GT_S:
                    push_i32(lhs_i64 > rhs_i64, S->stack);
                    break;
                
                case OP_I64_GT_U:
                    push_i32((uint64_t)lhs_i64 > (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_I64_LE_S:
                    push_i32(lhs_i64 <= rhs_i64, S->stack);
                    break;
                
                case OP_I64_LE_U:
                    push_i32((uint64_t)lhs_i64 <= (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_I64_GE_S:
                    push_i32(lhs_i64 >= rhs_i64, S->stack);
                    break;
                
                case OP_I64_GE_U:
                    push_i32((uint64_t)lhs_i64 >= (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_F32_EQ:
                    push_i32(lhs_f32 == rhs_f32, S->stack);
                    break;
                
                case OP_F32_NE:
                    push_i32(lhs_f32 != rhs_f32, S->stack);
                    break;
                
                case OP_F32_LT:
                    push_i32(lhs_f32 < rhs_f32, S->stack);
                    break;
                
                case OP_F32_GT:
                    push_i32(lhs_f32 > rhs_f32, S->stack);
                    break;
                
                case OP_F32_LE:
                    push_i32(lhs_f32 <= rhs_f32, S->stack);
                    break;
                
                case OP_F32_GE:
                    push_i32(lhs_f32 >= rhs_f32, S->stack);
                    break;
                
                case OP_F64_EQ:
                    push_i32(lhs_f64 == rhs_f64, S->stack);
                    break;
                
                case OP_F64_NE:
                    push_i32(lhs_f64 != rhs_f64, S->stack);
                    break;
                
                case OP_F64_LT:
                    push_i32(lhs_f64 < rhs_f64, S->stack);
                    break;
                
                case OP_F64_GT:
                    push_i32(lhs_f64 > rhs_f64, S->stack);
                    break;
                
                case OP_F64_LE:
                    push_i32(lhs_f64 <= rhs_f64, S->stack);
                    break;
                
                case OP_F64_GE:
                    push_i32(lhs_f64 >= rhs_f64, S->stack);
                    break;
                
                case OP_I32_CLZ:
                    if(lhs_i32 == 0)
                        push_i32(32, S->stack);
                    else
                        push_i32(__builtin_clz(lhs_i32), S->stack);
                    break;
                    
                case OP_I32_CTZ:
                    push_i32(__builtin_ctz(lhs_i32), S->stack);
                    break;
                
                case OP_I32_POPCNT:
                    push_i32(__builtin_popcount(lhs_i32), S->stack);
                    break;
                
                case OP_I32_ADD:
                    push_i32(lhs_i32 + rhs_i32, S->stack);
                    break;
                
                case OP_I32_SUB:
                    push_i32(lhs_i32 - rhs_i32, S->stack);
                    break;
                
                case OP_I32_MUL:
                    push_i32(lhs_i32 * rhs_i32, S->stack);
                    break;
                
                case OP_I32_DIV_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    __throwif(ERR_TRAP_INTERGET_OVERFLOW, lhs_i32 == INT32_MIN && rhs_i32 == -1);
                    push_i32(lhs_i32 / rhs_i32, S->stack);
                    break;
                
                case OP_I32_DIV_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    push_i32((uint32_t)lhs_i32 / (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I32_REM_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    if(lhs_i32 == INT32_MIN && rhs_i32 == -1) {
                        push_i32(0, S->stack);
                    }
                    else {
                        push_i32(lhs_i32 % rhs_i32, S->stack);
                    }
                    break;
                
                case OP_I32_REM_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    push_i32((uint32_t)lhs_i32 % (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I32_AND:
                    push_i32(lhs_i32 & rhs_i32, S->stack);
                    break;
                
                case OP_I32_OR:
                    push_i32(lhs_i32 | rhs_i32, S->stack);
                    break;
                
                case OP_I32_XOR:
                    push_i32(lhs_i32 ^ rhs_i32, S->stack);
                    break;
                
                case OP_I32_SHL:
                    push_i32(lhs_i32 << rhs_i32, S->stack);
                    break;
                
                case OP_I32_SHR_S:
                    push_i32(lhs_i32 >> rhs_i32, S->stack);
                    break;
                
                case OP_I32_SHR_U:
                    push_i32((uint32_t)lhs_i32 >> (uint32_t)rhs_i32, S->stack);
                    break;
                
                case OP_I32_ROTL: {
                    uint32_t n = rhs_i32 & 31;
                    push_i32(
                        ((uint32_t)lhs_i32 << n) | ((uint32_t)lhs_i32 >> ((-n) & 31)), 
                        S->stack
                    );
                    break;
                }

                case OP_I32_ROTR: {
                    uint32_t n = rhs_i32 & 31;
                    push_i32(
                        ((uint32_t)lhs_i32 >> n) | ((uint32_t)lhs_i32 << ((-n) & 31)), 
                        S->stack
                    );
                    break;
                }

                case OP_I64_CLZ:
                    if(lhs_i64 == 0)
                        push_i64(64, S->stack);
                    else
                        push_i64(__builtin_clzl(lhs_i64), S->stack);
                    break;
                    
                case OP_I64_CTZ:
                    push_i64(__builtin_ctzl(lhs_i64), S->stack);
                    break;
                
                case OP_I64_POPCNT:
                    push_i64(__builtin_popcountl(lhs_i64), S->stack);
                    break;
                
                case OP_I64_ADD:
                    push_i64(lhs_i64 + rhs_i64, S->stack);
                    break;
                
                case OP_I64_SUB:
                    push_i64(lhs_i64 - rhs_i64, S->stack);
                    break;
                
                case OP_I64_MUL:
                    push_i64(lhs_i64 * rhs_i64, S->stack);
                    break;
                
                case OP_I64_DIV_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    __throwif(ERR_TRAP_INTERGET_OVERFLOW, lhs_i64 == INT64_MIN && rhs_i64 == -1);
                    push_i64(lhs_i64 / rhs_i64, S->stack);
                    break;
                
                case OP_I64_DIV_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    push_i64((uint64_t)lhs_i64 / (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_I64_REM_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    if(lhs_i64 == INT64_MIN && rhs_i64 == -1) {
                        push_i64(0, S->stack);
                    }
                    else {
                        push_i64(lhs_i64 % rhs_i64, S->stack);
                    }
                    break;
                
                case OP_I64_REM_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    push_i64((uint64_t)lhs_i64 % (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_I64_AND:
                    push_i64(lhs_i64 & rhs_i64, S->stack);
                    break;
                
                case OP_I64_OR:
                    push_i64(lhs_i64 | rhs_i64, S->stack);
                    break;
                
                case OP_I64_XOR:
                    push_i64(lhs_i64 ^ rhs_i64, S->stack);
                    break;
                
                case OP_I64_SHL:
                    push_i64(lhs_i64 << rhs_i64, S->stack);
                    break;
                
                case OP_I64_SHR_S:
                    push_i64(lhs_i64 >> rhs_i64, S->stack);
                    break;
                
                case OP_I64_SHR_U:
                    push_i64((uint64_t)lhs_i64 >> (uint64_t)rhs_i64, S->stack);
                    break;
                
                case OP_I64_ROTL: {
                    uint64_t n = rhs_i64 & 63;
                    push_i64(
                        ((uint64_t)lhs_i64 << n) | ((uint64_t)lhs_i64 >> ((-n) & 63)), 
                        S->stack
                    );
                    break;
                }

                case OP_I64_ROTR: {
                    uint64_t n = rhs_i64 & 63;
                    push_i64(
                        ((uint64_t)lhs_i64 >> n) | ((uint64_t)lhs_i64 << ((-n) & 63)), 
                        S->stack
                    );
                    break;
                }

                case OP_F32_ABS:
                    push_f32(fabsf(lhs_f32), S->stack);
                    break;

                case OP_F32_NEG:
                    push_f32(-lhs_f32, S->stack);
                    break;
                
                case OP_F32_CEIL:
                    push_f32(ceilf(lhs_f32), S->stack);
                    break;

                case OP_F32_FLOOR:
                    push_f32(floorf(lhs_f32), S->stack);
                    break;

                case OP_F32_TRUNC:
                    push_f32(truncf(lhs_f32), S->stack);
                    break;
                
                case OP_F32_NEAREST:
                    push_f32(nearbyintf(lhs_f32), S->stack);
                    break;
                
                case OP_F32_SQRT:
                    push_f32(sqrtf(lhs_f32), S->stack);
                    break;
                
                case OP_F32_ADD:
                    push_f32(lhs_f32 + rhs_f32, S->stack);
                    break;
                
                case OP_F32_SUB:
                    push_f32(lhs_f32 - rhs_f32, S->stack);
                    break;
                
                case OP_F32_MUL:
                    push_f32(lhs_f32 * rhs_f32, S->stack);
                    break;
                
                case OP_F32_DIV:
                    push_f32(lhs_f32 / rhs_f32, S->stack);
                    break;
                
                case OP_F32_MIN:
                    if(isnan(lhs_f32) || isnan(rhs_f32))
                        push_f32(NAN, S->stack);
                    else if(lhs_f32 == 0 && rhs_f32 == 0)
                        push_f32(signbit(lhs_f32) ? lhs_f32 : rhs_f32, S->stack);
                    else 
                        push_f32(
                            lhs_f32 < rhs_f32 ? lhs_f32 : rhs_f32,
                            S->stack
                        );
                    break;
                
                case OP_F32_MAX:
                    if(isnan(lhs_f32) || isnan(rhs_f32))
                        push_f32(NAN, S->stack);
                    else if(lhs_f32 == 0 && rhs_f32 == 0)
                        push_f32(signbit(lhs_f32) ? rhs_f32 : lhs_f32, S->stack);
                    else
                        push_f32(
                            lhs_f32 > rhs_f32 ? lhs_f32 : rhs_f32,
                            S->stack
                        );
                    break;
                
                case OP_F32_COPYSIGN:
                    push_f32(copysignf(lhs_f32, rhs_f32), S->stack);
                    break;
                
                case OP_F64_ABS:
                    push_f64(fabs(lhs_f64), S->stack);
                    break;
                
                case OP_F64_NEG:
                    push_f64(-lhs_f64, S->stack);
                    break;
                
                case OP_F64_CEIL:
                    push_f64(ceil(lhs_f64), S->stack);
                    break;
                
                case OP_F64_FLOOR:
                    push_f64(floor(lhs_f64), S->stack);
                    break;
                
                case OP_F64_TRUNC:
                    push_f64(trunc(lhs_f64), S->stack);
                    break;
                
                case OP_F64_NEAREST:
                    push_f64(nearbyint(lhs_f64), S->stack);
                    break;
                
                case OP_F64_SQRT:
                    push_f64(sqrt(lhs_f64), S->stack);
                    break;
                
                case OP_F64_ADD:
                    push_f64(lhs_f64 + rhs_f64, S->stack);
                    break;
                
                case OP_F64_SUB:
                    push_f64(lhs_f64 - rhs_f64, S->stack);
                    break;
                
                case OP_F64_MUL:
                    push_f64(lhs_f64 * rhs_f64, S->stack);
                    break;
                
                case OP_F64_DIV:
                    push_f64(lhs_f64 / rhs_f64, S->stack);
                    break;
                
                case OP_F64_MIN:
                    if(isnan(lhs_f64) || isnan(rhs_f64))
                        push_f64(NAN, S->stack);
                    else if(lhs_f64 == 0 && rhs_f64 == 0)
                        push_f64(signbit(lhs_f64) ? lhs_f64 : rhs_f64, S->stack);
                    else 
                        push_f64(
                            lhs_f64 < rhs_f64 ? lhs_f64 : rhs_f64,
                            S->stack
                        );
                    break;
                
                case OP_F64_MAX:
                    if(isnan(lhs_f64) || isnan(rhs_f64))
                        push_f64(NAN, S->stack);
                    else if(lhs_f64 == 0 && rhs_f64 == 0)
                        push_f64(signbit(lhs_f64) ? rhs_f64 : lhs_f64, S->stack);
                    else
                        push_f64(
                            lhs_f64 > rhs_f64 ? lhs_f64 : rhs_f64,
                            S->stack
                        );
                    break;
                
                case OP_F64_COPYSIGN:
                    push_f64(copysign(lhs_f64, rhs_f64), S->stack);
                    break;
                
                case OP_I32_WRAP_I64:
                    push_i32(lhs_i64 & 0xffffffff, S->stack);
                    break;

                case OP_I32_TRUNC_F32_S:
                    push_i32((int32_t)lhs_f32, S->stack);
                    break;
                
                case OP_I32_TRUNC_F32_U:
                    push_i32((int32_t)(uint32_t)lhs_f32, S->stack);
                    break;
                
                case OP_I32_TRUNC_F64_S:
                    push_i32((int32_t)lhs_f64, S->stack);
                    break;
                
                case OP_I32_TRUNC_F64_U:
                    push_i32((int32_t)(uint32_t)lhs_f64, S->stack);
                    break;
                
                case OP_I64_EXTEND_I32_S:
                    push_i64((int64_t)(int32_t)lhs_i32, S->stack);
                    break;
                
                case OP_I64_EXTEND_I32_U:
                    push_i64((int64_t)(uint32_t)lhs_i32, S->stack);
                    break;

                case OP_I64_TRUNC_F32_S:
                    push_i64((int64_t)lhs_f32, S->stack);
                    break;
                
                case OP_I64_TRUNC_F32_U:
                    push_i64((int64_t)(uint64_t)lhs_f32, S->stack);
                    break;
                
                case OP_I64_TRUNC_F64_S:
                    push_i64((int64_t)lhs_f64, S->stack);
                    break;
                
                case OP_I64_TRUNC_F64_U:
                    push_i64((int64_t)(uint64_t)lhs_f64, S->stack);
                    break;
                
                case OP_F32_CONVERT_I32_S:
                    push_f32((float)lhs_i32, S->stack);
                    break;
                
                case OP_F32_CONVERT_I32_U:
                    push_f32((float)(uint32_t)lhs_i32, S->stack);
                    break;

                case OP_F32_CONVERT_I64_S:
                    push_f32((float)lhs_i64, S->stack);
                    break;
                
                case OP_F32_CONVERT_I64_U:
                    push_f32((float)(uint64_t)lhs_i64, S->stack);
                    break;
                
                case OP_F32_DEMOTE_F64:
                    push_f32((float)lhs_f64, S->stack);
                    break;
                
                case OP_F64_CONVERT_I32_S:
                    push_f64((double)lhs_i32, S->stack);
                    break;

                case OP_F64_CONVERT_I32_U:
                    push_f64((double)(uint32_t)lhs_i32, S->stack);
                    break;

                case OP_F64_CONVERT_I64_S:
                    push_f64((double)lhs_i64, S->stack);
                    break;

                case OP_F64_CONVERT_I64_U:
                    push_f64((double)(uint64_t)lhs_i64, S->stack);
                    break;
                
                case OP_F64_PROMOTE_F32:
                    push_f64((double)lhs_f32, S->stack);
                    break;
                
                // todo: fix this
                case OP_I32_REINTERPRET_F32: {
                    num_t num = {.f32 = lhs_f32};
                    push_i32(num.i32, S->stack);
                    break;
                }

                case OP_I64_REINTERPRET_F64: {
                    num_t num = {.f64 = lhs_f64};
                    push_i64(num.i64, S->stack);
                    break;
                }
                
                case OP_F32_REINTERPRET_I32: {
                    num_t num = {.i32 = lhs_i32};
                    push_f32(num.f32, S->stack);
                    break;
                }

                case OP_F64_REINTERPRET_I64: {
                    num_t num = {.i64 = lhs_i64};
                    push_f64(num.f64, S->stack);
                    break;
                }

                case OP_I32_EXTEND8_S:
                    push_i32((int32_t)(int8_t)lhs_i32, S->stack);
                    break;
                
                case OP_I32_EXTEND16_S: 
                    push_i32((int32_t)(int16_t)lhs_i32, S->stack);
                    break;
                
                case OP_I64_EXTEND8_S:
                    push_i64((int64_t)(int8_t)lhs_i64, S->stack);
                    break;
                
                case OP_I64_EXTEND16_S: 
                    push_i64((int64_t)(int16_t)lhs_i64, S->stack);
                    break;
                
                case OP_I64_EXTEND32_S: 
                    push_i64((int64_t)(int32_t)lhs_i64, S->stack);
                    break;

                case OP_TRUNC_SAT:
                    switch(ip->op2) {
                        case 0x00: {
                            int32_t val;
                            if(isnan(lhs_f32)) {
                                val = 0;
                            } else if(lhs_f32 <= -2147483904.0f) {
                                val = INT32_MIN;
                            } else if(lhs_f32 >= 2147483648.0f) {
                                val = INT32_MAX;
                            } else {
                                val = (int32_t)lhs_f32;
                            }
                            push_i32(val, S->stack);
                            break;
                        }
                        
                        case 0x01: {
                            int32_t val;
                            if(isnan(lhs_f32)) {
                                val = 0;
                            } else if(lhs_f32 <= -1.0f) {
                                val = 0UL;
                            } else if(lhs_f32 >= 4294967296.0f) {
                                val = UINT32_MAX;
                            } else {
                                val = (uint32_t)lhs_f32;
                            }
                            push_i32(val, S->stack);
                            break;
                        }
                        
                        case 0x02: {
                            int32_t val;
                            if(isnan(lhs_f64)) {
                                val = 0;
                            } else if(lhs_f64 <= -2147483649.0) {
                                val = INT32_MIN;
                            } else if(lhs_f64 >= 2147483648.0) {
                                val = INT32_MAX;
                            } else {
                                val = (int32_t)lhs_f64;
                            }
                            push_i32(val, S->stack);
                            break;
                        }

                        case 0x03: {
                            int32_t val;
                            if(isnan(lhs_f64)) {
                                val = 0;
                            } else if(lhs_f64 <= -1.0) {
                                val = 0UL;
                            } else if(lhs_f64 >= 4294967296.0) {
                                val = UINT32_MAX;
                            } else {
                                val = (uint32_t)lhs_f64;
                            }
                            push_i32(val, S->stack);
                            break;
                        }

                        case 0x04: {
                            int64_t val;
                            if(isnan(lhs_f32)) {
                                val = 0;
                            } else if(lhs_f32 <= -9223373136366403584.0f) {
                                val = INT64_MIN;
                            } else if(lhs_f32 >= 9223372036854775808.0f) {
                                val = INT64_MAX;
                            } else {
                                val = (int64_t)lhs_f32;
                            }
                            push_i64(val, S->stack);
                            break;
                        }

                        case 0x05: {
                            int64_t val;
                            if(isnan(lhs_f32)) {
                                val = 0;
                            } else if(lhs_f32 <= -1.0f) {
                                val = 0ULL;
                            } else if(lhs_f32 >= 18446744073709551616.0f) {
                                val = UINT64_MAX;
                            } else {
                                val = (uint64_t)lhs_f32;
                            }
                            push_i64(val, S->stack);
                            break;
                        }
                        
                        case 0x06: {
                            int64_t val;
                            if(isnan(lhs_f64)) {
                                val = 0;
                            } else if(lhs_f64 <= -9223372036854777856.0) {
                                val = INT64_MIN;
                            } else if(lhs_f64 >= 9223372036854775808.0) {
                                val = INT64_MAX;
                            } else {
                                val = (int64_t)lhs_f64;
                            }
                            push_i64(val, S->stack);
                            break;
                        }

                        case 0x07: {
                            int64_t val;
                            if(isnan(lhs_f64)) {
                                val = 0;
                            } else if(lhs_f64 <= -1.0f) {
                                val = 0ULL;
                            } else if(lhs_f64 >= 18446744073709551616.0) {
                                val = UINT64_MAX;
                            } else {
                                val = (uint64_t)lhs_f64;
                            }
                            push_i64(val, S->stack);
                            break;
                        }
                            
                    }
                    break;
                
                default:
                    PANIC("Exec: unsupported opcode: %x\n", ip->op1);
            }
            
            // update ip
            ip = next_ip;
        }
    }
    __catch:
        return err;
}

// ref: https://webassembly.github.io/spec/core/exec/instructions.html#function-calls
static error_t invoke_func(store_t *S, funcaddr_t funcaddr) {
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
    static instr_t end = {.op1 = OP_END, .next = NULL};
    label_t L = {.arity = functype->rt2.n, .parent = NULL, .continuation = &end};

    __try {
        // enter instr* with label L
        push_label(L, S->stack);

        __throwiferr(exec_instrs(funcinst->code->body ,S));

        // return from a function
        vals_t vals;
        pop_vals_n(&vals, frame.arity, S->stack);
        pop_while_not_frame(S->stack);
        pop_frame(&frame, S->stack);
        push_vals(vals, S->stack);
    }
    __catch:
        return err;
}

// The args is a reference to args_t. 
// This is because args is also used to return results.
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args) {
    __try {
        funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, funcaddr);
        __throwif(ERR_FAILED, !funcinst);

        functype_t *functype = funcinst->type;
        __throwif(ERR_FAILED, args->n != functype->rt1.n);

        size_t idx = 0;
        VECTOR_FOR_EACH(arg, args, arg_t) {
            __throwif(ERR_FAILED, arg->type != *VECTOR_ELEM(&functype->rt1, idx++));
        }

        // Omit the process of pushing the dummy frame onto the stack.
        // ref: https://github.com/WebAssembly/spec/issues/1690
        
        // push args
        VECTOR_FOR_EACH(arg, args, arg_t) {
            push_val(arg->val, S->stack);
        }

        // invoke func
        __throwiferr(invoke_func(S, funcaddr));

        // reuse args to return results since it is no longer used.
        //free(args->elem);
        VECTOR_INIT(args, functype->rt2.n, arg_t);
        idx = 0;
        VECTOR_FOR_EACH(ret, args, arg_t) {
            ret->type = *VECTOR_ELEM(&functype->rt2, idx++);
            pop_val(&ret->val, S->stack);
        }
    }
    __catch:
        return err;
}