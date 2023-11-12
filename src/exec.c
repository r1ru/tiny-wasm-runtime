#include "exec.h"
#include "print.h"
#include "exception.h"
#include "memory.h"

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
    size_t num_vals = vals.len;
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
    VECTOR_NEW(vals, num_vals);
    
    // pop values
    VECTOR_FOR_EACH(val, vals) {
        pop_val(val, stack);
    }
}

void pop_vals_n(vals_t *vals, size_t n, stack_t *stack) {
    // init vector
    VECTOR_NEW(vals, n);
    
    // pop values
    VECTOR_FOR_EACH(val, vals) {
        pop_val(val, stack);
    }
}

void pop_label(label_t *label, stack_t *stack) {
    *label = stack->pool[stack->idx].label;
    stack->idx--;
    list_pop_tail(&stack->labels);
}

void try_pop_label(label_t *label, stack_t *stack) {
    // nop
    if(stack->pool[stack->idx].type != TYPE_LABEL)
        return;
    
    pop_label(label, stack);
}

void pop_frame(frame_t *frame, stack_t *stack) {
    *frame = stack->pool[stack->idx].frame;
    stack->idx--;
    list_pop_tail(&stack->frames);
    //printf("pop frame idx: %ld\n", stack->idx);
}

// pop while stack top is not a frame
void pop_while_not_frame(stack_t *stack) {
    while(stack->pool[stack->idx].type != TYPE_FRAME) {
        switch(stack->pool[stack->idx].type) {
            case TYPE_VAL:
                stack->idx--;
                break;
            case TYPE_FRAME: {
                frame_t tmp;
                pop_frame(&tmp, stack);
                break;
            }
            case TYPE_LABEL: {
                label_t tmp;
                pop_label(&tmp, stack);
                break;
            }
        }
    }
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
    uint32_t num_funcs = module->funcs.len;
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

    // allocate tables
    uint32_t num_tables = module->tables.len;
    moduleinst->tableaddrs = malloc(sizeof(tableaddr_t) * num_tables);
    for(uint32_t i = 0; i < num_tables; i++) {
        table_t *table = VECTOR_ELEM(&module->tables, i);
        tableinst_t *tableinst = VECTOR_ELEM(&S->tables, i);

        uint32_t n = table->type.limits.min;
        tableinst->type = table->type;
        VECTOR_NEW(&tableinst->elem, n);

        // init with ref.null
        VECTOR_FOR_EACH(elem, &tableinst->elem) {
            *elem = REF_NULL;
        }
        moduleinst->tableaddrs[i] = i;
    }

    // allocate mems
    uint32_t num_mems = module->mems.len;
    moduleinst->memaddrs = malloc(sizeof(memaddr_t) * num_mems);
    for(uint32_t i = 0; i < num_mems; i++) {
        mem_t *mem = VECTOR_ELEM(&module->mems, i);
        meminst_t *meminst = VECTOR_ELEM(&S->mems, i);

        meminst->type = mem->type;
        for(int j = 0; j < 16; j++) {
            meminst->base[j] = NULL;
        }
        moduleinst->memaddrs[i] = i;
    }

    // allocate datas
    uint32_t num_datas = module->datas.len;
    moduleinst->exports = module->exports.elem;
    moduleinst->dataaddrs = malloc(sizeof(dataaddr_t) * num_datas);
    for(uint32_t i = 0; i < num_datas; i++) {
        data_t *data = VECTOR_ELEM(&module->datas, i);
        datains_t *datainst = VECTOR_ELEM(&S->datas, i);

        VECTOR_COPY(&datainst->data, &data->init);

        moduleinst->dataaddrs[i] = i;
    }

    return moduleinst;
}

error_t exec_expr(expr_t * expr, store_t *S);
error_t instantiate(store_t **S, module_t *module) {
    __try {
        // allocate store
        store_t *store = *S = malloc(sizeof(store_t));
        
        // allocate stack
        new_stack(&store->stack);
        
        VECTOR_NEW(&store->funcs, module->funcs.len);
        VECTOR_NEW(&store->tables, module->tables.len);
        VECTOR_NEW(&store->mems, module->mems.len);
        VECTOR_NEW(&store->globals, module->globals.len);
        VECTOR_NEW(&store->elems, module->elems.len);
        VECTOR_NEW(&store->datas, module->datas.len);

        moduleinst_t *moduleinst = allocmodule(store, module);

        // alloc globals
        frame_t F = {.module = moduleinst, .locals = NULL};
        push_frame(F, store->stack);

        uint32_t num_globals = module->globals.len;
        moduleinst->globaladdrs = malloc(sizeof(globaladdr_t) * num_globals);
        for(uint32_t i = 0; i < num_globals; i++) {
            global_t *global = VECTOR_ELEM(&module->globals, i);
            globalinst_t *globalinst = VECTOR_ELEM(&store->globals, i);

            globalinst->gt = global->gt;

            exec_expr(&global->expr, store);
            pop_val(&globalinst->val, store->stack);
            
            moduleinst->globaladdrs[i] = i;
        }

        // alloc elems
        uint32_t num_elems = module->elems.len;
        moduleinst->elemaddrs = malloc(sizeof(elemaddr_t) * num_elems);
        for(uint32_t i = 0; i < num_elems; i++) {
            elem_t *elem = VECTOR_ELEM(&module->elems, i);
            eleminst_t *eleminst = VECTOR_ELEM(&store->elems, i);
            VECTOR_NEW(&eleminst->elem, elem->init.len);

            for(uint32_t j = 0; j < elem->init.len; j++) {
                expr_t *init = VECTOR_ELEM(&elem->init, j);
                exec_expr(init, store);
                val_t val;
                pop_val(&val, store->stack);
                *VECTOR_ELEM(&eleminst->elem, j) = val.ref;
            }

            moduleinst->elemaddrs[i] = i;
        }

        // init table if elemmode is active
        for(uint32_t i = 0; i < module->elems.len; i++) {
            elem_t *elem = VECTOR_ELEM(&module->elems, i);
            
            if(elem->mode.kind != 0)
                continue;
            
            tableidx_t tableidx = elem->mode.table;

            uint32_t n, s, d;
            // exec instruction sequence
            exec_expr(&elem->mode.offset, store);
            pop_i32(&d, store->stack);

            // exec i32.const 0; i32.const n;
            n = elem->init.len;
            s = 0;

            // exec table.init tableidx i
            while(n--) {
                tableaddr_t ta = F.module->tableaddrs[tableidx];
                tableinst_t *tab = VECTOR_ELEM(&store->tables, ta);

                if(s + n > elem->init.len || d + n > tab->elem.len) {
                    PANIC("trap");
                }

                // get init expr
                expr_t *init = VECTOR_ELEM(&elem->init, s);

                // eval init expr
                exec_expr(init, store);
                val_t val;
                pop_val(&val, store->stack);

                // exec table.set tableidx
                *VECTOR_ELEM(&tab->elem, d) = val.ref;

                // update d, s
                d++;
                s++;  
            }
        }

        // init memory if datamode is active
        for(uint32_t i = 0; i < module->datas.len; i++) {
            data_t *data = VECTOR_ELEM(&module->datas, i);

            if(data->mode.kind != DATA_MODE_ACTIVE)
                continue;
            
            __throwif(ERR_FAILED, data->mode.memory != 0);

            exec_expr(&data->mode.offset, store);
            // i32.const 0
            push_i32(0, store->stack);
            // i32.const n
            push_i32(data->init.len, store->stack);

            // memory.init i
            instr_t memory_init = {
                .op1 = OP_0XFC, .op2 = 8, .x = i, .next = NULL
            };
            expr_t expr = &memory_init;
            exec_expr(&expr, store);
        }

        pop_frame(&F, store->stack);

        // todo: support start section
    }
    __catch:
        return err;
}

static void expand_F(functype_t *ty, blocktype_t bt, frame_t *F) {
    ty->rt1 = (resulttype_t){.len = 0, .elem = NULL};

    switch(bt.valtype) {
        case 0x40:
            ty->rt2 = (resulttype_t){.len = 0, .elem = NULL};
            break;

        case TYPE_NUM_I32:
        case TYPE_NUM_I64:
        case TYPE_NUM_F32:
        case TYPE_NUM_F64:
        case TYPE_EXTENREF:
            VECTOR_NEW(&ty->rt2, 1);
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

#define TRUNC(A, TYPE, RMIN, RMAX)                              \
    ({                                                          \
        if(isnan(A)) {                                          \
            __throw(ERR_TRAP_INVALID_CONVERSION_TO_INTERGER);   \
        }                                                       \
        if(A <= RMIN || A >= RMAX) {                            \
            __throw(ERR_TRAP_INTERGET_OVERFLOW);                \
        }                                                       \
        (TYPE)A;                                                \
    })

#define I32_TRUNC_F32(A)    TRUNC(A, int32_t, -2147483904.0f, 2147483648.0f)
#define U32_TRUNC_F32(A)    TRUNC(A, uint32_t,         -1.0f, 4294967296.0f)
#define I32_TRUNC_F64(A)    TRUNC(A, int32_t, -2147483649.0 , 2147483648.0 )
#define U32_TRUNC_F64(A)    TRUNC(A, uint32_t,         -1.0 , 4294967296.0 )

#define I64_TRUNC_F32(A)    TRUNC(A, int64_t, -9223373136366403584.0f,  9223372036854775808.0f)
#define U64_TRUNC_F32(A)    TRUNC(A, uint64_t,                  -1.0f, 18446744073709551616.0f)
#define I64_TRUNC_F64(A)    TRUNC(A, int64_t, -9223372036854777856.0 ,  9223372036854775808.0 )
#define U64_TRUNC_F64(A)    TRUNC(A, uint64_t,                  -1.0 , 18446744073709551616.0 )

#define TRUNC_SAT(A, TYPE, RMIN, RMAX, IMIN, IMAX)          \
    ({                                                      \
        TYPE __val;                                         \
        if (isnan(A)) {                                     \
            __val = 0;                                      \
        } else if (A <= RMIN) {                             \
            __val = IMIN;                                   \
        } else if (A >= RMAX) {                             \
            __val = IMAX;                                   \
        } else {                                            \
            __val = (TYPE)A;                                \
        }                                                   \
        __val;                                              \
    })

#define I32_TRUNC_SAT_F32(A)    TRUNC_SAT(A, int32_t, -2147483904.0f, 2147483648.0f,   INT32_MIN,  INT32_MAX)
#define U32_TRUNC_SAT_F32(A)    TRUNC_SAT(A, uint32_t,         -1.0f, 4294967296.0f,         0UL, UINT32_MAX)
#define I32_TRUNC_SAT_F64(A)    TRUNC_SAT(A, int32_t, -2147483649.0 , 2147483648.0,    INT32_MIN,  INT32_MAX)
#define U32_TRUNC_SAT_F64(A)    TRUNC_SAT(A, uint32_t,         -1.0 , 4294967296.0,          0UL, UINT32_MAX)
#define I64_TRUNC_SAT_F32(A)    TRUNC_SAT(A, int64_t, -9223373136366403584.0f,  9223372036854775808.0f, INT64_MIN,  INT64_MAX)
#define U64_TRUNC_SAT_F32(A)    TRUNC_SAT(A, uint64_t,                  -1.0f, 18446744073709551616.0f,      0ULL, UINT64_MAX)
#define I64_TRUNC_SAT_F64(A)    TRUNC_SAT(A, int64_t, -9223372036854777856.0 ,  9223372036854775808.0,  INT64_MIN,  INT64_MAX)
#define U64_TRUNC_SAT_F64(A)    TRUNC_SAT(A, uint64_t,                  -1.0 , 18446744073709551616.0,       0ULL, UINT64_MAX)

error_t exec_expr(expr_t * expr, store_t *S) {
    instr_t *ip = *expr;

    // current frame
    frame_t *F = LIST_TAIL(&S->stack->frames, frame_t, link);

    __try {
        while(ip) {
            //printf("[+] ip = %x\n", ip->op1);
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
               ip->op1 == OP_0XFC && (ip->op2 == 0 || ip->op2 == 1 || ip->op2 == 4 || ip->op2 == 5)) {
                pop_f32(&lhs_f32, S->stack);
            }
            if(OP_F64_ABS <= ip->op1 && ip->op1 <= OP_F64_SQRT ||
               OP_I32_TRUNC_F64_S <= ip->op1 && ip->op1 <= OP_I32_TRUNC_F64_U || 
               OP_I64_TRUNC_F64_S <= ip->op1 && ip->op1 <= OP_I64_TRUNC_F64_U || 
               ip->op1 == OP_F32_DEMOTE_F64 || 
               ip->op1 == OP_I64_REINTERPRET_F64 ||
               ip->op1 == OP_0XFC && (ip->op2 == 2 || ip->op2 == 3 || ip->op2 == 6 || ip->op2 == 7)) {
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
                case OP_UNREACHABLE:
                    __throw(ERR_TRAP_UNREACHABLE);
                    break;
                
                case OP_NOP:
                    break;
                
                case OP_BLOCK: {
                    functype_t ty;
                    expand_F(&ty, ip->bt, F);
                    label_t L = {
                        .arity  = ty.rt2.len,
                        .parent = ip,
                        .continuation = &end,
                    };
                    vals_t vals;
                    pop_vals_n(&vals, ty.rt1.len, S->stack);
                    push_label(L, S->stack);
                    push_vals(vals, S->stack);
                    next_ip = ip->in1;
                    break;
                }

                case OP_LOOP: {
                    functype_t ty;
                    expand_F(&ty, ip->bt, F);
                    label_t L = {
                        .arity = ty.rt1.len,
                        .parent = ip,
                        .continuation = ip,
                    };
                    vals_t vals;
                    pop_vals_n(&vals, ty.rt1.len, S->stack);
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
                        .arity = ty.rt2.len,
                        .parent = ip,
                        .continuation = &end,
                    };
                    vals_t vals;
                    pop_vals_n(&vals, ty.rt1.len, S->stack);
                    push_label(L, S->stack);
                    push_vals(vals, S->stack);

                    if(c) {
                        next_ip = ip->in1;
                    } 
                    else if(ip->in2) {
                        next_ip = ip->in2;
                    }
                    else {
                        // exec end instruction
                        static instr_t end = {.op1 = OP_END};
                        next_ip = &end;
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
                    label_t l = {.parent = NULL};
                    try_pop_label(&l, S->stack);
                    push_vals(vals, S->stack);
                    next_ip = l.parent != NULL ? l.parent->next : NULL;
                    break;
                }

                case OP_BR_IF: {
                    labelidx_t idx;
                    int32_t c;
                    
                    pop_i32(&c, S->stack);

                    if(c == 0) {
                        break;
                    }
                    idx = ip->labelidx;
                    goto __br;

                case OP_BR_TABLE:
                    pop_i32(&c, S->stack);
                    if(c < ip->labels.len) {
                        idx = ip->labels.elem[c];
                    }
                    else {
                        idx = ip->default_label;
                    }
                    goto __br;

                case OP_BR:
                    idx = ip->labelidx;
                __br:
                    label_t *l = LIST_GET_ELEM(&S->stack->labels, label_t, link, idx);
                    vals_t vals;
                    pop_vals_n(&vals, l->arity, S->stack);
                    label_t L;
                    for(int i = 0; i <= idx; i++) {
                        vals_t tmp;
                        pop_vals(&tmp, S->stack);
                        pop_label(&L, S->stack);
                    }
                    push_vals(vals, S->stack);

                    // br to "outermost" label (return from function)
                    if(!L.parent) {
                        next_ip = NULL;
                    }
                    else {
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

                case OP_CALL_INDIRECT: {
                    tableaddr_t ta = F->module->tableaddrs[ip->x];
                    tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);

                    functype_t *ft_expect = &F->module->types[ip->y];

                    int32_t i;
                    pop_i32(&i, S->stack);
                    __throwif(ERR_TRAP_UNDEFINED_ELEMENT, i >= tab->elem.len);
                    ref_t r = *VECTOR_ELEM(&tab->elem, i);
                    __throwif(ERR_TRAP_UNINITIALIZED_ELEMENT, r == REF_NULL);

                    funcinst_t *f = VECTOR_ELEM(&S->funcs, r);
                    functype_t *ft_actual = f->type;

                    __throwif(
                        ERR_TRAP_INDIRECT_CALL_TYPE_MISMATCH, 
                        ft_expect->rt1.len != ft_actual->rt1.len || ft_expect->rt2.len != ft_actual->rt2.len
                    );

                    for(uint32_t i = 0; i < ft_expect->rt1.len; i++) {
                        valtype_t e = *VECTOR_ELEM(&ft_expect->rt1, i);
                        valtype_t a = *VECTOR_ELEM(&ft_actual->rt1, i);
                        __throwif(ERR_TRAP_INDIRECT_CALL_TYPE_MISMATCH, e != a);
                    }

                    for(uint32_t i = 0; i < ft_expect->rt2.len; i++) {
                        valtype_t e = *VECTOR_ELEM(&ft_expect->rt2, i);
                        valtype_t a = *VECTOR_ELEM(&ft_actual->rt2, i);
                        __throwif(ERR_TRAP_INDIRECT_CALL_TYPE_MISMATCH, e != a);
                    }

                    invoke_func(S, r);
                    break;
                }

                case OP_DROP: {
                    val_t val;
                    pop_val(&val, S->stack);
                    break;
                }

                case OP_SELECT:
                case OP_SELECT_T: {
                    val_t v1, v2;
                    int32_t c;
                    pop_i32(&c, S->stack);
                    pop_val(&v2, S->stack);
                    pop_val(&v1, S->stack);
                    if(c != 0)
                        push_val(v1, S->stack);
                    else
                        push_val(v2, S->stack);
                    break;
                }

                case OP_LOCAL_GET: {
                    localidx_t x = ip->localidx;
                    val_t val = F->locals[x];
                    push_val(val, S->stack);
                    break;
                }

                case OP_LOCAL_TEE: {
                    val_t val;
                    pop_val(&val, S->stack);
                    push_val(val, S->stack);
                    push_val(val, S->stack);
                }

                case OP_LOCAL_SET: {
                    localidx_t x = ip->localidx;
                    val_t val;
                    pop_val(&val, S->stack);
                    F->locals[x] = val;
                    break;
                }

                case OP_GLOBAL_GET: {
                    globaladdr_t a = F->module->globaladdrs[ip->globalidx];
                    globalinst_t *glob = VECTOR_ELEM(&S->globals, a);
                    push_val(glob->val, S->stack);
                    break;
                }

                case OP_GLOBAL_SET: {
                    val_t val;
                    globaladdr_t a = F->module->globaladdrs[ip->globalidx];
                    globalinst_t *glob = VECTOR_ELEM(&S->globals, a);
                    pop_val(&val, S->stack);
                    glob->val = val;
                    break;
                }

                case OP_TABLE_GET: {
                    tableaddr_t a = F->module->tableaddrs[ip->x];
                    tableinst_t *tab = VECTOR_ELEM(&S->tables, a);
                    int32_t i;
                    pop_i32(&i, S->stack);
                    __throwif(ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS, !(i < tab->elem.len));
                    val_t val;
                    val.ref = *VECTOR_ELEM(&tab->elem, i);
                    push_val(val, S->stack);
                    break;
                }

                case OP_TABLE_SET: {
                    tableaddr_t a = F->module->tableaddrs[ip->x];
                    tableinst_t *tab = VECTOR_ELEM(&S->tables, a);
                    val_t val;
                    pop_val(&val, S->stack);
                    int32_t i;
                    pop_i32(&i, S->stack);
                    __throwif(ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS, !(i < tab->elem.len));
                    *VECTOR_ELEM(&tab->elem, i) = val.ref;
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
                    memaddr_t a = F->module->memaddrs[0];
                    meminst_t *mem = VECTOR_ELEM(&S->mems, a);

                    int32_t i;
                    pop_i32(&i, S->stack);
                    uint64_t ea = (uint32_t)i;
                    ea += ip->m.offset;

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

                    if(ea + n > WASM_MEM_SIZE) {
                        __throw(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS);
                    }

                    int32_t idx = ea >> 12;
                    int32_t offs = ea - 0x1000 * idx;

                    // todo: fix this?
                    if(!mem->base[idx]) {
                        mem->base[idx] = calloc(4096, 1);
                    }

                    uint8_t *base = mem->base[idx];
                    val_t val = {.num.i64 = 0};

                    switch(ip->op1) {
                        case OP_I32_LOAD:
                            val.num.i32 = *(int32_t *)(base + offs);
                            break;
                        case OP_I32_LOAD8_S:
                            val.num.i32 = (int32_t)*(int8_t *)(base + offs);
                            break;
                        case OP_I32_LOAD8_U:
                            val.num.i32 = (int32_t)*(uint8_t *)(base + offs);
                            break;
                        case OP_I32_LOAD16_S:
                            val.num.i32 = (int32_t)*(int16_t *)(base + offs);
                            break;
                        case OP_I32_LOAD16_U:
                            val.num.i32 = (int32_t)*(uint16_t *)(base + offs);
                            break;
                        case OP_I64_LOAD:
                            val.num.i64 = *(int64_t *)(base + offs);
                            break;
                        case OP_I64_LOAD8_S:
                            val.num.i64 = (int64_t)*(int8_t *)(base + offs);
                            break;
                        case OP_I64_LOAD8_U:
                            val.num.i64 = (int64_t)*(uint8_t *)(base + offs);
                            break;
                        case OP_I64_LOAD16_S:
                            val.num.i64 = (int64_t)*(int16_t *)(base + offs);
                            break;
                        case OP_I64_LOAD16_U:
                            val.num.i64 = (int64_t)*(uint16_t *)(base + offs);
                            break;
                        case OP_I64_LOAD32_S:
                            val.num.i64 = (int64_t)*(int32_t *)(base + offs);
                            break;
                        case OP_I64_LOAD32_U:
                            val.num.i64 = (int64_t)*(uint32_t *)(base + offs);
                            break;
                        case OP_F32_LOAD:
                            val.num.f32 = *(float *)(base + offs);
                            break;
                        case OP_F64_LOAD:
                            val.num.f64 = *(double *)(base + offs);
                            break;
                    }
                    push_val(val, S->stack);
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
                    memaddr_t a = F->module->memaddrs[0];
                    meminst_t *mem = VECTOR_ELEM(&S->mems, a);

                    val_t c;
                    int32_t i;
                    pop_val(&c, S->stack);
                    pop_i32(&i, S->stack);

                    uint64_t ea = (uint32_t)i;
                    ea += ip->m.offset;

                    int32_t n;
                    switch(ip->op1) {
                        case OP_I32_STORE:
                        case OP_F32_STORE:
                        case OP_I64_STORE32:
                            n= 4;
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

                    if(ea + n > WASM_MEM_SIZE) {
                        __throw(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS);
                    }

                    int32_t idx = ea >> 12;
                    int32_t offs = ea - 0x1000 * idx;

                    if(!mem->base[idx]) {
                        mem->base[idx] = calloc(4096, 1);
                    }

                    uint8_t *base = mem->base[idx];

                    switch(ip->op1) {
                        case OP_I32_STORE:
                            *(int32_t *)(base + offs) = c.num.i32;
                            break;
                        case OP_I64_STORE:
                            *(int64_t *)(base + offs) = c.num.i64;
                            break;
                        case OP_F32_STORE:
                            *(float *)(base + offs) = c.num.f32;
                            break;
                        case OP_F64_STORE:
                            *(double *)(base + offs) = c.num.f64;
                            break;
                        case OP_I32_STORE8:
                            *(uint8_t *)(base + offs) = c.num.i32 & 0xff;
                            break;
                        case OP_I32_STORE16:
                            *(uint16_t *)(base + offs) = c.num.i32 & 0xffff;
                            break;
                        case OP_I64_STORE8:
                            *(uint8_t *)(base + offs) = c.num.i64 & 0xff;
                            break;
                        case OP_I64_STORE16:
                            *(uint16_t *)(base + offs) = c.num.i64 & 0xffff;
                            break;
                        case OP_I64_STORE32:
                            *(uint32_t *)(base + offs) = c.num.i64 & 0xffffffff;
                            break;
                    }
                    break;
                }                
                
                case OP_MEMORY_GROW: {
                    // support only memory.grow 0 for now
                    int32_t n;
                    pop_i32(&n, S->stack);
                    push_i32(1, S->stack);
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

                case OP_I32_TRUNC_F32_S: {
                    push_i32(I32_TRUNC_F32(lhs_f32), S->stack);
                    break;
                }
                
                case OP_I32_TRUNC_F32_U:
                    push_i32(U32_TRUNC_F32(lhs_f32), S->stack);
                    break;
                
                case OP_I32_TRUNC_F64_S:
                    push_i32(I32_TRUNC_F64(lhs_f64), S->stack);
                    break;
                
                case OP_I32_TRUNC_F64_U:
                    push_i32(U32_TRUNC_F64(lhs_f64), S->stack);
                    break;
                
                case OP_I64_EXTEND_I32_S:
                    push_i64((int64_t)(int32_t)lhs_i32, S->stack);
                    break;
                
                case OP_I64_EXTEND_I32_U:
                    push_i64((int64_t)(uint32_t)lhs_i32, S->stack);
                    break;

                case OP_I64_TRUNC_F32_S:
                    push_i64(I64_TRUNC_F32(lhs_f32), S->stack);
                    break;
                
                case OP_I64_TRUNC_F32_U:
                    push_i64(U64_TRUNC_F32(lhs_f32), S->stack);
                    break;
                
                case OP_I64_TRUNC_F64_S:
                    push_i64(I64_TRUNC_F64(lhs_f64), S->stack);
                    break;
                
                case OP_I64_TRUNC_F64_U:
                    push_i64(U64_TRUNC_F64(lhs_f64), S->stack);
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
                
                case OP_REF_NULL:
                    push_val((val_t){.ref = REF_NULL}, S->stack);
                    break;
                
                case OP_REF_IS_NULL: {
                    val_t val;
                    pop_val(&val, S->stack);
                    push_i32(val.ref == REF_NULL, S->stack);
                    break;
                }
                
                case OP_0XFC:
                    switch(ip->op2) {
                        case 0x00:
                            push_i32(I32_TRUNC_SAT_F32(lhs_f32), S->stack);
                            break;

                        case 0x01:
                            push_i32(U32_TRUNC_SAT_F32(lhs_f32), S->stack);
                            break;
                        
                        case 0x02:
                            push_i32(I32_TRUNC_SAT_F64(lhs_f64), S->stack);
                            break;

                        case 0x03:
                            push_i32(U32_TRUNC_SAT_F64(lhs_f64), S->stack);
                            break;
                        
                        case 0x04:
                            push_i64(I64_TRUNC_SAT_F32(lhs_f32), S->stack);
                            break;
                        
                        case 0x05:
                            push_i64(U64_TRUNC_SAT_F32(lhs_f32), S->stack);
                            break;

                        case 0x06:
                            push_i64(I64_TRUNC_SAT_F64(lhs_f64), S->stack);
                            break;

                        case 0x07:
                            push_i64(U64_TRUNC_SAT_F64(lhs_f64), S->stack);
                            break;
                        
                        // mememory.init
                        case 0x08: {
                            memaddr_t ma = F->module->memaddrs[0];
                            meminst_t *mem = VECTOR_ELEM(&S->mems, ma);
                            dataaddr_t da = F->module->dataaddrs[ip->x];
                            datains_t *data = VECTOR_ELEM(&S->datas, da);

                            int32_t n, s, d;
                            pop_i32(&n, S->stack);
                            pop_i32(&s, S->stack);
                            pop_i32(&d, S->stack);

                            // todo: fix this
                            __throwif(ERR_FAILED, s + n > data->data.len || d + n > WASM_MEM_SIZE);
                            

                            while(n--) {
                                byte_t b = *VECTOR_ELEM(&data->data, s);
                                
                                push_i32(d, S->stack);
                                push_i32((int32_t)b, S->stack);

                                instr_t i32_store8 = {
                                    .op1 = OP_I32_STORE8, .next = NULL, 
                                    .m = (memarg_t){.offset = 0, .align = 0}
                                };
                                expr_t expr = &i32_store8;
                                exec_expr(&expr, S);
                                s++;
                                d++;
                            }
                            break;
                        }
                        // memory.copy
                        case 0x0A: {
                            memaddr_t ma = F->module->memaddrs[0];
                            meminst_t *mem = VECTOR_ELEM(&S->mems, ma);
                            int32_t n, s, d;
                            pop_i32(&n, S->stack);
                            pop_i32(&s, S->stack);
                            pop_i32(&d, S->stack);
                            printf("n = %d s = %d d = %d\n", n, s, d);
                            uint64_t ea1 = (uint32_t)s, ea2 = (uint32_t)d;
                            ea1 += (uint32_t)n;
                            ea2 += (uint32_t)n;
                            __throwif(
                                ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS, 
                                ea1 > WASM_MEM_SIZE || ea2 > WASM_MEM_SIZE
                            );
                            instr_t i32_store8 = {
                                .op1 = OP_I32_STORE8, .next = NULL, 
                                .m = (memarg_t){.offset = 0, .align = 0}
                            };
                            instr_t i32_load8_u = {
                                .op1 = OP_I32_LOAD8_U, .next = &i32_store8, 
                                .m = (memarg_t){.offset = 0, .align = 0}
                            }; 
                            expr_t expr = &i32_load8_u;
                            
                            while(1) {
                                if(n == 0)
                                    break;
                                
                                if(d <= s) {
                                    push_i32(d, S->stack);
                                    push_i32(s, S->stack);
                                    exec_expr(&expr, S);
                                    d++;
                                    s++;
                                }
                                else {
                                    push_i32(d + n - 1, S->stack);
                                    push_i32(s + n - 1, S->stack);
                                    exec_expr(&expr, S);
                                }
                                n--;
                            }
                            break;
                        }

                        // memory.fill
                        case 0x0B: {
                            memaddr_t ma = F->module->memaddrs[0];
                            meminst_t *mem = VECTOR_ELEM(&S->mems, ma);
                            int32_t n, val, d;
                            pop_i32(&n, S->stack);
                            pop_i32(&val, S->stack);
                            pop_i32(&d, S->stack);
                            uint64_t ea = (uint32_t)d;
                            ea += (uint32_t)n;

                            __throwif(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS, ea > WASM_MEM_SIZE);

                            while(n--) {
                                push_i32(d, S->stack);
                                push_i32(val, S->stack);

                                instr_t i32_store8 = {
                                    .op1 = OP_I32_STORE8, .next = NULL, 
                                    .m = (memarg_t){.offset = 0, .align = 0}
                                };
                                expr_t expr = &i32_store8;
                                exec_expr(&expr, S);
                                d++;
                            }
                            break;
                        }

                        // table.grow
                        case 0x0F: {
                            tableaddr_t ta = F->module->tableaddrs[ip->x];
                            tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);
                            int32_t sz = tab->elem.len;
                            int32_t n;
                            val_t val;
                            pop_i32(&n, S->stack);
                            pop_val(&val, S->stack);

                            if(tab->type.limits.max && n + tab->elem.len > tab->type.limits.max) {
                                push_i32(-1, S->stack);
                                break;
                            }

                            if(!IS_ERROR(VECTOR_GROW(&tab->elem, n))) {
                                // init
                                for(int i = sz; i < tab->elem.len; i++) {
                                    *VECTOR_ELEM(&tab->elem, i) = val.ref;
                                }
                                push_i32(sz, S->stack);
                            }
                            else {
                                push_i32(-1, S->stack);
                            }
                            break;
                        }

                        // table.size
                        case 0x10: {
                            tableaddr_t ta = F->module->tableaddrs[ip->x];
                            tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);
                            push_i32(tab->elem.len, S->stack);
                            break;
                        }

                        // table.fill
                        case 0x11: {
                            tableaddr_t ta = F->module->tableaddrs[ip->x];
                            tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);
                            int32_t n, i;
                            val_t val;

                            pop_i32(&n, S->stack);
                            pop_val(&val, S->stack);
                            pop_i32(&i, S->stack);

                            while(1) {
                                __throwif(ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS, i + n > tab->elem.len);

                                if(n == 0)
                                    break;
                                
                                // table.set
                                *VECTOR_ELEM(&tab->elem, i) = val.ref;
                                i++;
                                n--;
                            }
                            break;
                        }
                        
                        default:
                            PANIC("Exec: unsupported opcode: 0xfc %x", ip->op2);           
                    }
                    break;
                
                case OP_REF_FUNC: {
                    funcaddr_t a = F->module->funcaddrs[ip->x];
                    push_val((val_t){.ref = a}, S->stack);
                    break;
                }

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
    uint32_t num_locals = functype->rt1.len + funcinst->code->locals.len;
    frame.module = funcinst->module;
    frame.locals = malloc(sizeof(val_t) * num_locals);

    // pop args
    for(int32_t i = (functype->rt1.len - 1); 0 <= i; i--) {
        pop_val(&frame.locals[i], S->stack);
    }

    // push activation frame
    frame.arity  = functype->rt2.len;
    push_frame(frame, S->stack);

    // create label L
    static instr_t end = {.op1 = OP_END, .next = NULL};
    label_t L = {.arity = functype->rt2.len, .parent = NULL, .continuation = &end};

    __try {
        // enter instr* with label L
        push_label(L, S->stack);

        __throwiferr(exec_expr(&funcinst->code->body ,S));

        // return from a function("return" instruction)
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
        __throwif(ERR_FAILED, args->len != functype->rt1.len);

        size_t idx = 0;
        VECTOR_FOR_EACH(arg, args) {
            __throwif(ERR_FAILED, arg->type != *VECTOR_ELEM(&functype->rt1, idx++));
        }

        // Omit the process of pushing the dummy frame onto the stack.
        // ref: https://github.com/WebAssembly/spec/issues/1690
        
        // push args
        VECTOR_FOR_EACH(arg, args) {
            push_val(arg->val, S->stack);
        }

        // invoke func
        __throwiferr(invoke_func(S, funcaddr));

        // reuse args to return results since it is no longer used.
        //free(args->elem);
        VECTOR_NEW(args, functype->rt2.len);
        idx = 0;
        VECTOR_FOR_EACH_REVERSE(ret, args) {
            ret->type = *VECTOR_ELEM(&functype->rt2, idx++);
            pop_val(&ret->val, S->stack);
        }
    }
    __catch:
        return err;
}