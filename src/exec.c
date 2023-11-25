#include "exec.h"
#include "print.h"
#include "exception.h"
#include "memory.h"

// todo: fix this?
#include <math.h>
#include <string.h>

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

error_t push_val(stack_t *stack, val_t val) {
    __try {
        if(full(stack)) {
            __throw(ERR_TRAP_CALL_STACK_EXHAUSTED);
        }
        stack->pool[++stack->idx] = (obj_t) {
            .type   = TYPE_VAL,
            .val    = val 
        };
        //printf("push val: %x idx: %ld\n", val.num.i32, stack->idx);
    }
    __catch:
        return err;
}

static inline error_t push_i32(stack_t *stack, int32_t val) {
    __try {
        val_t v = {.num.i32 = val};
        __throwiferr(push_val(stack, v));
    }
    __catch:
        return err;
}

static inline error_t push_i64(stack_t *stack, int64_t val) {
    __try {
        val_t v = {.num.i64 = val};
        __throwiferr(push_val(stack, v));
    }
    __catch:
        return err;
    
}

static inline error_t push_f32(stack_t *stack, float val) {
    __try {
        val_t v = {.num.f32 = val};
        __throwiferr(push_val(stack, v));
    }
    __catch:
        return err;
}

static inline error_t push_f64(stack_t *stack, double val) {
    __try {
        val_t v = {.num.f64 = val};
        __throwiferr(push_val(stack, v));
    }
    __catch:
        return err;
}

// todo: fix this?
error_t push_vals(stack_t *stack, vals_t vals) {
    __try {
        size_t num_vals = vals.len;
        for(int32_t i = (num_vals - 1); 0 <= i; i--) {
           __throwiferr(push_val(stack, vals.elem[i]));
        }
    }
    __catch:
        return err;
}

error_t push_label(stack_t *stack, label_t label) {
    __try {
        if(full(stack)) {
            __throw(ERR_TRAP_CALL_STACK_EXHAUSTED);
        }
        obj_t *obj = &stack->pool[++stack->idx];

        *obj = (obj_t) {
            .type   = TYPE_LABEL,
            .label  = label 
        };
        list_push_back(&stack->labels, &obj->label.link);
        //printf("push label idx: %ld\n", stack->idx);
    }
    __catch:
        return err;
}

error_t push_frame(stack_t *stack, frame_t frame) {
    __try {
        if(full(stack)) {
            __throw(ERR_TRAP_CALL_STACK_EXHAUSTED);
        }
        obj_t *obj = &stack->pool[++stack->idx];

        *obj = (obj_t) {
            .type   = TYPE_FRAME,
            .frame  = frame 
        };
        list_push_back(&stack->frames, &obj->frame.link);
        //printf("push frame idx: %ld\n", stack->idx);
    }
    __catch:
        return err;
}

void pop_val(stack_t *stack, val_t *val) {    
    *val = stack->pool[stack->idx].val;
    stack->idx--;
    //printf("pop val: %x idx: %ld\n", val->num.i32, stack->idx);
}

static inline void pop_i32(stack_t *stack, int32_t *val) {
    val_t v;
    pop_val(stack, &v);
    *val = v.num.i32;
}

static inline void pop_i64(stack_t *stack, int64_t *val) {
    val_t v;
    pop_val(stack, &v);
    *val = v.num.i64;
}

static inline void pop_f32(stack_t *stack, float *val) {
    val_t v;
    pop_val(stack, &v);
    *val = v.num.f32;
}

static inline void pop_f64(stack_t *stack, double *val) {
    val_t v;
    pop_val(stack, &v);
    *val = v.num.f64;
}

// pop all values from the stack top
void pop_vals(stack_t *stack, vals_t *vals) {
    // count values
    size_t num_vals = 0;
    size_t i = stack->idx;
    while(stack->pool[i].type == TYPE_VAL) {
        num_vals++;
        i--;
    }
    
    // init vector
    VECTOR_NEW(vals, num_vals, num_vals);
    
    // pop values
    VECTOR_FOR_EACH(val, vals) {
        pop_val(stack, val);
    }
}

void pop_vals_n(stack_t *stack, size_t n, vals_t *vals) {
    // init vector
    VECTOR_NEW(vals, n, n);
    
    // pop values
    VECTOR_FOR_EACH(val, vals) {
        pop_val(stack, val);
    }
}

void pop_label(stack_t *stack, label_t *label) {
    *label = stack->pool[stack->idx].label;
    stack->idx--;
    list_pop_tail(&stack->labels);
}

void try_pop_label(stack_t *stack, label_t *label) {
    // nop
    if(stack->pool[stack->idx].type != TYPE_LABEL)
        return;
    
    pop_label(stack, label);
}

void pop_frame(stack_t *stack, frame_t *frame) {
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
            case TYPE_LABEL: {
                label_t tmp;
                pop_label(stack, &tmp);
                break;
            }
        }
    }
}

// memory
// 33bit address space
typedef uint64_t    eaddr_t;
typedef uint64_t    paddr_t;
static paddr_t eaddr_to_paddr(meminst_t *meminst, eaddr_t eaddr) {
    uint32_t vpn2 = (eaddr) >> 30 & 0x3;
    uint32_t vpn1 = (eaddr) >> 21 & 0x1ff;
    uint32_t vpn0 = (eaddr) >> 12 & 0x1ff;

    if(meminst->table2[vpn2] == NULL) {
        meminst->table2[vpn2] = calloc(1, 4096);
    }
    
    uint8_t ***table1 = meminst->table2[vpn2];

    if(table1[vpn1] == NULL) {
        table1[vpn1] = calloc(1, 4096);
    }

    uint8_t **table0 = table1[vpn1];

    if(table0[vpn0] == NULL) {
        table0[vpn0] = aligned_alloc(4096, 4096);
        memset(table0[vpn0], 0, 4096);
    }

    return (uint64_t)table0[vpn0] | (eaddr & 0xfff);
}

static funcaddr_t alloc_func(store_t *S, func_t *func, moduleinst_t *moduleinst) {
    funcinst_t funcinst;

    funcinst.type = &moduleinst->types[func->type];
    funcinst.module = moduleinst;
    funcinst.code   = func;

    return VECTOR_APPEND(&S->funcs, funcinst);
}

static tableaddr_t alloc_table(store_t *S, table_t *table) {
    tableinst_t tableinst;
    
    uint32_t n = table->type.limits.min;
    tableinst.type = table->type;
    VECTOR_NEW(&tableinst.elem, n, n);

    // init with ref.null
    VECTOR_FOR_EACH(elem, &tableinst.elem) {
        *elem = REF_NULL;
    }

    return VECTOR_APPEND(&S->tables, tableinst);
}

static memaddr_t alloc_mem(store_t *S, mem_t *mem) {
    meminst_t meminst;
    
    meminst.type = mem->type;
    meminst.num_pages = mem->type.min;
    for(uint32_t i = 0; i < 4; i++) {
        meminst.table2[i] = NULL;
    }

    return VECTOR_APPEND(&S->mems, meminst);
}

static dataaddr_t alloc_data(store_t *S, data_t *data) {
    datainst_t datainst;

    VECTOR_COPY(&datainst.data, &data->init);

    return VECTOR_APPEND(&S->datas, datainst);
}

error_t exec_expr(store_t *S, expr_t * expr);
static globaladdr_t alloc_global(store_t *S, global_t *global) {
    globalinst_t globalinst;

    globalinst.gt = global->gt;

    exec_expr(S, &global->expr);
    pop_val(S->stack, &globalinst.val);
    
    return VECTOR_APPEND(&S->globals, globalinst);
}

static elemaddr_t alloc_elem(store_t *S, elem_t *elem) {
    eleminst_t eleminst;

    VECTOR_NEW(&eleminst.elem, elem->init.len, elem->init.len);

    for(uint32_t j = 0; j < elem->init.len; j++) {
        expr_t *init = VECTOR_ELEM(&elem->init, j);
        exec_expr(S, init);
        val_t val;
        pop_val(S->stack, &val);
        *VECTOR_ELEM(&eleminst.elem, j) = val.ref;
    }

    return VECTOR_APPEND(&S->elems, eleminst);
}

store_t *new_store(void) {
    // allocate store
    store_t *S = malloc(sizeof(store_t));
    // allocate stack
    new_stack(&S->stack);

    VECTOR_INIT(&S->funcs);
    VECTOR_INIT(&S->tables);
    VECTOR_INIT(&S->mems);
    VECTOR_INIT(&S->globals);
    VECTOR_INIT(&S->elems);
    VECTOR_INIT(&S->datas);

    return S;
}

static error_t invoke_func(store_t *S, funcaddr_t funcaddr);
/*
    In the spec, funcaddr is passed as an argument to invoke. 
    This requires that the caller has access to the moduleinst. 
    Therefore, it takes a pointer to moduleinst as its third argument.
*/
error_t instantiate(store_t *S, module_t *module, externvals_t *externvals, moduleinst_t **inst) {
    __try {
        // allocate space in store
        VECTOR_GROW(&S->funcs, module->funcs.len);
        VECTOR_GROW(&S->tables, module->tables.len);
        VECTOR_GROW(&S->mems, module->mems.len);
        VECTOR_GROW(&S->globals, module->globals.len);
        VECTOR_GROW(&S->elems, module->elems.len);
        VECTOR_GROW(&S->datas, module->datas.len);

        // create new moduleinst
        moduleinst_t *moduleinst = *inst = malloc(sizeof(moduleinst_t));
        moduleinst->types = module->types.elem;
        moduleinst->funcaddrs = malloc(
            sizeof(funcaddr_t) * (module->num_func_imports + module->funcs.len)
        );
        moduleinst->tableaddrs = malloc(
            sizeof(tableaddr_t) * (module->num_table_imports + module->tables.len)
        );
        moduleinst->memaddrs = malloc(sizeof(memaddr_t) * 1);
        moduleinst->dataaddrs = malloc(
            sizeof(dataaddr_t) * module->datas.len
        );
        moduleinst->globaladdrs = malloc(
            sizeof(globaladdr_t) * (module->num_global_imports + module->globals.len)
        );
        moduleinst->elemaddrs = malloc(sizeof(elemaddr_t) * module->elems.len);
        
        // resolve imports (Trust the caller)
        uint32_t funcidx = 0;
        uint32_t tableidx = 0;
        uint32_t memidx = 0;
        uint32_t globalidx = 0;
        uint32_t elemidx = 0, dataidx = 0;

        size_t i = 0;
        VECTOR_FOR_EACH(import, &module->imports) {
            externval_t *externval = VECTOR_ELEM(externvals, i++);

            switch(import->d.kind) {
                case FUNC_IMPORTDESC:
                    moduleinst->funcaddrs[funcidx++] = externval->func;
                    break;
                case TABLE_IMPORTDESC: {
                    moduleinst->tableaddrs[tableidx++] = externval->table;
                    break;
                }
                case MEM_IMPORTDESC: {
                    moduleinst->memaddrs[memidx++] = externval->mem;
                    break;

                }
                case GLOBAL_IMPORTDESC: {
                    moduleinst->globaladdrs[globalidx++] = externval->global;
                    break;
                }
            }
        }

        // alloc funcs
        VECTOR_FOR_EACH(func, &module->funcs) {
            moduleinst->funcaddrs[funcidx] = alloc_func(S, func, moduleinst);
            funcidx++;
        }

        // alloc tables
        VECTOR_FOR_EACH(table, &module->tables) {
            moduleinst->tableaddrs[tableidx] = alloc_table(S, table);
            tableidx++;
        }

        // alloc mems
        VECTOR_FOR_EACH(mem, &module->mems) {
            moduleinst->memaddrs[memidx] = alloc_mem(S, mem);
            memidx++;
        }

        // alloc datas
        VECTOR_FOR_EACH(data, &module->datas) {
            moduleinst->dataaddrs[dataidx] = alloc_data(S, data);
            dataidx++;
        }

        // alloc globals
        frame_t F = {.module = moduleinst, .locals = NULL};
        __throwiferr(push_frame(S->stack, F));

        VECTOR_FOR_EACH(global, &module->globals) {
            moduleinst->globaladdrs[globalidx] = alloc_global(S, global);
            globalidx++;
        }

        // alloc elems
        VECTOR_FOR_EACH(elem, &module->elems) {
            moduleinst->elemaddrs[elemidx] = alloc_elem(S, elem);
            elemidx++;
        }

        // create exportinst
        uint32_t exportidx = 0;
        VECTOR_NEW(&moduleinst->exports, module->exports.len, module->exports.len);
        VECTOR_FOR_EACH(export, &module->exports) {
            exportinst_t *exportinst = VECTOR_ELEM(&moduleinst->exports, exportidx);
            exportinst->name = export->name;
            exportinst->value.kind = export->exportdesc.kind;

            switch(export->exportdesc.kind) {
                case FUNC_EXPORTDESC:
                    exportinst->value.func = moduleinst->funcaddrs[export->exportdesc.idx];
                    break;
                case TABLE_EXPORTDESC:
                    exportinst->value.table = moduleinst->tableaddrs[export->exportdesc.idx];
                    break;
                case MEM_EXPORTDESC:
                    exportinst->value.mem = moduleinst->memaddrs[export->exportdesc.idx];
                    break;
                case GLOBAL_EXPORTDESC:
                    exportinst->value.mem = moduleinst->globaladdrs[export->exportdesc.idx];
                    break;
            }
            exportidx++;
        }

        // init table if elemmode is active
        for(uint32_t i = 0; i < module->elems.len; i++) {
            elem_t *elem = VECTOR_ELEM(&module->elems, i);
            
            if(elem->mode.kind != 0)
                continue;
            
            // exec instruction sequence
            __throwiferr(exec_expr(S, &elem->mode.offset));

            // exec i32.const 0; i32.const n;
            __throwiferr(push_i32(S->stack, 0));
            __throwiferr(push_i32(S->stack, elem->init.len));

            // table.init
            instr_t elem_drop = {
                .op1 = OP_0XFC, .op2 = 0x0D, .x = i, .next = NULL
            };
            instr_t table_init = {
                .op1 = OP_0XFC, .op2 = 0x0C, .x = elem->mode.table, .y = i, .next = &elem_drop
            };
            expr_t expr = &table_init;
            __throwiferr(exec_expr(S, &expr));
        }

        // init memory if datamode is active
        for(uint32_t i = 0; i < module->datas.len; i++) {
            data_t *data = VECTOR_ELEM(&module->datas, i);

            if(data->mode.kind != DATA_MODE_ACTIVE)
                continue;
            
            __throwif(ERR_FAILED, data->mode.memory != 0);

            __throwiferr(exec_expr(S, &data->mode.offset));

            // i32.const 0
            __throwiferr(push_i32(S->stack, 0));
            // i32.const n
            __throwiferr(push_i32(S->stack, data->init.len));

            // memory.init i
            instr_t memory_init = {
                .op1 = OP_0XFC, .op2 = 8, .x = i, .next = NULL
            };
            expr_t expr = &memory_init;
            __throwiferr(exec_expr(S, &expr));
        }

        // exec start function if exists
        if(module->has_start) {
            __throwiferr(invoke_func(S, F.module->funcaddrs[module->start]));
        }

        pop_frame(S->stack, &F);
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
            VECTOR_NEW(&ty->rt2, 1, 1);
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

error_t exec_expr(store_t *S, expr_t *expr) {
    instr_t *ip = *expr;

    stack_t *stack = S->stack;

    // current frame
    frame_t *F = LIST_TAIL(&stack->frames, frame_t, link);

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
            if(ip->op1 == OP_I32_EQZ || (OP_I32_CLZ <= ip->op1 && ip->op1 <= OP_I32_POPCNT) ||
               (OP_I64_EXTEND_I32_S <= ip->op1 && ip->op1 <= OP_I64_EXTEND_I32_U) ||
               (OP_I32_EXTEND8_S <= ip->op1 && ip->op1 <= OP_I32_EXTEND16_S) ||
               (OP_F32_CONVERT_I32_S <= ip->op1 && ip->op1 <= OP_F32_CONVERT_I32_U) ||
               (OP_F64_CONVERT_I32_S <= ip->op1 && ip->op1 <= OP_F64_CONVERT_I32_U) ||
               ip->op1 == OP_F32_REINTERPRET_I32) {
                pop_i32(stack, &lhs_i32);
            }
            if(ip->op1 == OP_I64_EQZ || (OP_I64_CLZ <= ip->op1 && ip->op1 <= OP_I64_POPCNT) ||
               (OP_I64_EXTEND8_S <= ip->op1 && ip->op1 <= OP_I64_EXTEND32_S) ||
               ip->op1 == OP_I32_WRAP_I64 || 
               (OP_F32_CONVERT_I64_S <= ip->op1 && ip->op1 <= OP_F32_CONVERT_I64_U) || 
               (OP_F64_CONVERT_I64_S <= ip->op1 && ip->op1 <= OP_F64_CONVERT_I64_U) ||
               ip->op1 == OP_F64_REINTERPRET_I64) {
                pop_i64(stack, &lhs_i64);
            }
            if((OP_F32_ABS <= ip->op1 && ip->op1 <= OP_F32_SQRT) || 
               (OP_I32_TRUNC_F32_S <= ip->op1 && ip->op1 <= OP_I32_TRUNC_F32_U) ||
               (OP_I64_TRUNC_F32_S <= ip->op1 && ip->op1 <= OP_I64_TRUNC_F32_U) ||
               ip->op1 == OP_F64_PROMOTE_F32 || 
               ip->op1 == OP_I32_REINTERPRET_F32 ||
               (ip->op1 == OP_0XFC && (ip->op2 == 0 || ip->op2 == 1 || ip->op2 == 4 || ip->op2 == 5))) {
                pop_f32(stack, &lhs_f32);
            }
            if((OP_F64_ABS <= ip->op1 && ip->op1 <= OP_F64_SQRT) ||
               (OP_I32_TRUNC_F64_S <= ip->op1 && ip->op1 <= OP_I32_TRUNC_F64_U) || 
               (OP_I64_TRUNC_F64_S <= ip->op1 && ip->op1 <= OP_I64_TRUNC_F64_U) || 
               ip->op1 == OP_F32_DEMOTE_F64 || 
               ip->op1 == OP_I64_REINTERPRET_F64 ||
               (ip->op1 == OP_0XFC && (ip->op2 == 2 || ip->op2 == 3 || ip->op2 == 6 || ip->op2 == 7))) {
                pop_f64(stack, &lhs_f64);
            }
            
            // binary operator
            if((OP_I32_EQ <= ip->op1 && ip->op1 <= OP_I32_GE_U) || 
               (OP_I32_ADD <= ip->op1 && ip->op1 <= OP_I32_ROTR)) {
                pop_i32(stack, &rhs_i32);
                pop_i32(stack, &lhs_i32);
            }
            if((OP_I64_EQ <= ip->op1 && ip->op1 <= OP_I64_GE_U) || 
               (OP_I64_ADD <= ip->op1 && ip->op1 <= OP_I64_ROTR)) {
                pop_i64(stack, &rhs_i64);
                pop_i64(stack, &lhs_i64);
            }
            if((OP_F32_EQ <= ip->op1 && ip->op1 <= OP_F32_GE) ||
               (OP_F32_ADD <= ip->op1 && ip->op1 <= OP_F32_COPYSIGN)) {
                pop_f32(stack, &rhs_f32);
                pop_f32(stack, &lhs_f32);
            }
            if((OP_F64_EQ <= ip->op1 && ip->op1 <= OP_F64_GE) ||
               (OP_F64_ADD <= ip->op1 && ip->op1 <= OP_F64_COPYSIGN)) {
                pop_f64(stack, &rhs_f64);
                pop_f64(stack, &lhs_f64);
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
                    pop_vals_n(stack, ty.rt1.len, &vals);
                    __throwiferr(push_label(stack, L));
                    __throwiferr(push_vals(stack, vals));
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
                    pop_vals_n(stack, ty.rt1.len, &vals);
                    __throwiferr(push_label(stack, L));
                    __throwiferr(push_vals(stack, vals));
                    next_ip = ip->in1;
                    break; 
                }

                case OP_IF: {
                    int32_t c;
                    pop_i32(stack, &c);

                    functype_t ty;
                    expand_F(&ty, ip->bt, F);
                    label_t L = {
                        .arity = ty.rt2.len,
                        .parent = ip,
                        .continuation = &end,
                    };
                    vals_t vals;
                    pop_vals_n(stack, ty.rt1.len, &vals);
                    __throwiferr(push_label(stack, L));
                    __throwiferr(push_vals(stack, vals));

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
                    pop_vals(stack, &vals);

                    // exit instr* with label L
                    label_t l = {.parent = NULL};
                    try_pop_label(stack, &l);
                    __throwiferr(push_vals(stack, vals));
                    next_ip = l.parent != NULL ? l.parent->next : NULL;
                    break;
                }

                case OP_BR_IF: {
                    labelidx_t idx;
                    int32_t c;
                    
                    pop_i32(stack, &c);

                    if(c == 0) {
                        break;
                    }
                    idx = ip->labelidx;
                    goto __br;

                case OP_BR_TABLE:
                    pop_i32(stack, &c);
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
                    label_t *l = LIST_GET_ELEM(&stack->labels, label_t, link, idx);
                    vals_t vals;
                    pop_vals_n(stack, l->arity, &vals);
                    label_t L;
                    for(int i = 0; i <= idx; i++) {
                        vals_t tmp;
                        pop_vals(stack, &tmp);
                        pop_label(stack, &L);
                    }
                    __throwiferr(push_vals(stack, vals));

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
                    pop_i32(stack, &i);
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
                    __throwiferr(invoke_func(S, r));
                    break;
                }

                case OP_DROP: {
                    val_t val;
                    pop_val(stack, &val);
                    break;
                }

                case OP_SELECT:
                case OP_SELECT_T: {
                    val_t v1, v2;
                    int32_t c;
                    pop_i32(stack, &c);
                    pop_val(stack, &v2);
                    pop_val(stack, &v1);
                    if(c != 0)
                        __throwiferr(push_val(stack, v1));
                    else
                        __throwiferr(push_val(stack, v2));
                    break;
                }

                case OP_LOCAL_GET: {
                    localidx_t x = ip->localidx;
                    val_t val = F->locals[x];
                    __throwiferr(push_val(stack, val));
                    break;
                }

                case OP_LOCAL_TEE: {
                    val_t val;
                    pop_val(stack, &val);
                    __throwiferr(push_val(stack, val));
                    __throwiferr(push_val(stack, val));
                }

                case OP_LOCAL_SET: {
                    localidx_t x = ip->localidx;
                    val_t val;
                    pop_val(stack, &val);
                    F->locals[x] = val;
                    break;
                }

                case OP_GLOBAL_GET: {
                    globaladdr_t a = F->module->globaladdrs[ip->globalidx];
                    globalinst_t *glob = VECTOR_ELEM(&S->globals, a);
                    __throwiferr(push_val(stack, glob->val));
                    break;
                }

                case OP_GLOBAL_SET: {
                    val_t val;
                    globaladdr_t a = F->module->globaladdrs[ip->globalidx];
                    globalinst_t *glob = VECTOR_ELEM(&S->globals, a);
                    pop_val(stack, &val);
                    glob->val = val;
                    break;
                }

                case OP_TABLE_GET: {
                    tableaddr_t a = F->module->tableaddrs[ip->x];
                    tableinst_t *tab = VECTOR_ELEM(&S->tables, a);
                    int32_t i;
                    pop_i32(stack, &i);
                    __throwif(ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS, !(i < tab->elem.len));
                    val_t val;
                    val.ref = *VECTOR_ELEM(&tab->elem, i);
                    __throwiferr(push_val(stack, val));
                    break;
                }

                case OP_TABLE_SET: {
                    tableaddr_t a = F->module->tableaddrs[ip->x];
                    tableinst_t *tab = VECTOR_ELEM(&S->tables, a);
                    val_t val;
                    pop_val(stack, &val);
                    int32_t i;
                    pop_i32(stack, &i);
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
                    pop_i32(stack, &i);
                    eaddr_t ea = (uint32_t)i;
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

                    if(ea + n > mem->num_pages * WASM_PAGE_SIZE) {
                        __throw(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS);
                    }

                    // todo: fix this?
                    uint8_t *paddr = (uint8_t *)eaddr_to_paddr(mem, ea);
                    val_t val = {.num.i64 = 0};

                    switch(ip->op1) {
                        case OP_I32_LOAD:
                            val.num.i32 = *(int32_t *)paddr;
                            break;
                        case OP_I32_LOAD8_S:
                            val.num.i32 = (int32_t)*(int8_t *)paddr;
                            break;
                        case OP_I32_LOAD8_U:
                            val.num.i32 = (int32_t)*(uint8_t *)paddr;
                            break;
                        case OP_I32_LOAD16_S:
                            val.num.i32 = (int32_t)*(int16_t *)paddr;
                            break;
                        case OP_I32_LOAD16_U:
                            val.num.i32 = (int32_t)*(uint16_t *)paddr;
                            break;
                        case OP_I64_LOAD:
                            val.num.i64 = *(int64_t *)paddr;
                            break;
                        case OP_I64_LOAD8_S:
                            val.num.i64 = (int64_t)*(int8_t *)paddr;
                            break;
                        case OP_I64_LOAD8_U:
                            val.num.i64 = (int64_t)*(uint8_t *)paddr;
                            break;
                        case OP_I64_LOAD16_S:
                            val.num.i64 = (int64_t)*(int16_t *)paddr;
                            break;
                        case OP_I64_LOAD16_U:
                            val.num.i64 = (int64_t)*(uint16_t *)paddr;
                            break;
                        case OP_I64_LOAD32_S:
                            val.num.i64 = (int64_t)*(int32_t *)paddr;
                            break;
                        case OP_I64_LOAD32_U:
                            val.num.i64 = (int64_t)*(uint32_t *)paddr;
                            break;
                        case OP_F32_LOAD:
                            val.num.f32 = *(float *)paddr;
                            break;
                        case OP_F64_LOAD:
                            val.num.f64 = *(double *)paddr;
                            break;
                    }
                    __throwiferr(push_val(stack, val));
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
                    pop_val(stack, &c);
                    pop_i32(stack, &i);

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

                    if(ea + n > mem->num_pages * WASM_PAGE_SIZE) {
                        __throw(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS);
                    }

                    uint8_t *paddr = (uint8_t *)eaddr_to_paddr(mem, ea);

                    switch(ip->op1) {
                        case OP_I32_STORE:
                            *(int32_t *)paddr = c.num.i32;
                            break;
                        case OP_I64_STORE:
                            *(int64_t *)paddr = c.num.i64;
                            break;
                        case OP_F32_STORE:
                            *(float *)paddr = c.num.f32;
                            break;
                        case OP_F64_STORE:
                            *(double *)paddr = c.num.f64;
                            break;
                        case OP_I32_STORE8:
                            *(uint8_t *)paddr = c.num.i32 & 0xff;
                            break;
                        case OP_I32_STORE16:
                            *(uint16_t *)paddr = c.num.i32 & 0xffff;
                            break;
                        case OP_I64_STORE8:
                            *(uint8_t *)paddr = c.num.i64 & 0xff;
                            break;
                        case OP_I64_STORE16:
                            *(uint16_t *)paddr = c.num.i64 & 0xffff;
                            break;
                        case OP_I64_STORE32:
                            *(uint32_t *)paddr = c.num.i64 & 0xffffffff;
                            break;
                    }
                    break;
                }                
                
                case OP_MEMORY_SIZE: {
                    memaddr_t ma = F->module->memaddrs[0];
                    meminst_t *mem = VECTOR_ELEM(&S->mems, ma);
                    __throwiferr(push_i32(stack, mem->num_pages));
                    break;
                }

                case OP_MEMORY_GROW: {
                    memaddr_t ma = F->module->memaddrs[0];
                    meminst_t *mem = VECTOR_ELEM(&S->mems, ma);

                    int32_t sz = mem->num_pages;
                    int32_t n;
                    pop_i32(stack, &n);

                    if((mem->type.max && n + mem->num_pages > mem->type.max) || \
                        mem->num_pages + n > NUM_PAGE_MAX) {
                        __throwiferr(push_i32(stack, -1));
                    } else {
                        // grow n page(always success)
                        mem->num_pages += n;
                        mem->type.min += n;
                        __throwiferr(push_i32(stack, sz));
                    }
                    break;
                }

                case OP_I32_CONST:
                    __throwiferr(push_i32(stack, ip->c.i32));
                    break;
                
                case OP_I64_CONST:
                    __throwiferr(push_i64(stack, ip->c.i64));
                    break;
                
                case OP_F32_CONST:
                    __throwiferr(push_f32(stack, ip->c.f32));
                    break;
                
                case OP_F64_CONST:
                    __throwiferr(push_f64(stack, ip->c.f64));
                    break;
                
                case OP_I32_EQZ:
                    __throwiferr(push_i32(stack, lhs_i32 == 0));
                    break;
                                    
                case OP_I32_EQ:
                    __throwiferr(push_i32(stack, lhs_i32 == rhs_i32));
                    break;
                
                case OP_I32_NE:
                    __throwiferr(push_i32(stack, lhs_i32 != rhs_i32));
                    break;
                
                case OP_I32_LT_S:
                    __throwiferr(push_i32(stack, lhs_i32 < rhs_i32));
                    break;
                
                case OP_I32_LT_U:
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 < (uint32_t)rhs_i32));
                    break;
                
                case OP_I32_GT_S:
                    __throwiferr(push_i32(stack, lhs_i32 > rhs_i32));
                    break;
                
                case OP_I32_GT_U:
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 > (uint32_t)rhs_i32));
                    break;
                
                case OP_I32_LE_S:
                    __throwiferr(push_i32(stack, lhs_i32 <= rhs_i32));
                    break;
                
                case OP_I32_LE_U:
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 <= (uint32_t)rhs_i32));
                    break;
                
                case OP_I32_GE_S:
                    __throwiferr(push_i32(stack, lhs_i32 >= rhs_i32));
                    break;
                
                case OP_I32_GE_U:
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 >= (uint32_t)rhs_i32));
                    break;
                
                case OP_I64_EQZ:
                    __throwiferr(push_i32(stack, lhs_i64 == 0));
                    break;
                                    
                case OP_I64_EQ:
                    __throwiferr(push_i32(stack, lhs_i64 == rhs_i64));
                    break;
                
                case OP_I64_NE:
                    __throwiferr(push_i32(stack, lhs_i64 != rhs_i64));
                    break;
                
                case OP_I64_LT_S:
                    __throwiferr(push_i32(stack, lhs_i64 < rhs_i64));
                    break;
                
                case OP_I64_LT_U:
                    __throwiferr(push_i32(stack, (uint64_t)lhs_i64 < (uint64_t)rhs_i64));
                    break;
                
                case OP_I64_GT_S:
                    __throwiferr(push_i32(stack, lhs_i64 > rhs_i64));
                    break;
                
                case OP_I64_GT_U:
                    __throwiferr(push_i32(stack, (uint64_t)lhs_i64 > (uint64_t)rhs_i64));
                    break;
                
                case OP_I64_LE_S:
                    __throwiferr(push_i32(stack, lhs_i64 <= rhs_i64));
                    break;
                
                case OP_I64_LE_U:
                    __throwiferr(push_i32(stack, (uint64_t)lhs_i64 <= (uint64_t)rhs_i64));
                    break;
                
                case OP_I64_GE_S:
                    __throwiferr(push_i32(stack, lhs_i64 >= rhs_i64));
                    break;
                
                case OP_I64_GE_U:
                    __throwiferr(push_i32(stack, (uint64_t)lhs_i64 >= (uint64_t)rhs_i64));
                    break;
                
                case OP_F32_EQ:
                    __throwiferr(push_i32(stack, lhs_f32 == rhs_f32));
                    break;
                
                case OP_F32_NE:
                    __throwiferr(push_i32(stack, lhs_f32 != rhs_f32));
                    break;
                
                case OP_F32_LT:
                    __throwiferr(push_i32(stack, lhs_f32 < rhs_f32));
                    break;
                
                case OP_F32_GT:
                    __throwiferr(push_i32(stack, lhs_f32 > rhs_f32));
                    break;
                
                case OP_F32_LE:
                    __throwiferr(push_i32(stack, lhs_f32 <= rhs_f32));
                    break;
                
                case OP_F32_GE:
                    __throwiferr(push_i32(stack, lhs_f32 >= rhs_f32));
                    break;
                
                case OP_F64_EQ:
                    __throwiferr(push_i32(stack, lhs_f64 == rhs_f64));
                    break;
                
                case OP_F64_NE:
                    __throwiferr(push_i32(stack, lhs_f64 != rhs_f64));
                    break;
                
                case OP_F64_LT:
                    __throwiferr(push_i32(stack, lhs_f64 < rhs_f64));
                    break;
                
                case OP_F64_GT:
                    __throwiferr(push_i32(stack, lhs_f64 > rhs_f64));
                    break;
                
                case OP_F64_LE:
                    __throwiferr(push_i32(stack, lhs_f64 <= rhs_f64));
                    break;
                
                case OP_F64_GE:
                    __throwiferr(push_i32(stack, lhs_f64 >= rhs_f64));
                    break;
                
                case OP_I32_CLZ:
                    if(lhs_i32 == 0)
                        __throwiferr(push_i32(stack, 32));
                    else
                        __throwiferr(push_i32(stack, __builtin_clz(lhs_i32)));
                    break;
                    
                case OP_I32_CTZ:
                    __throwiferr(push_i32(stack, __builtin_ctz(lhs_i32)));
                    break;
                
                case OP_I32_POPCNT:
                    __throwiferr(push_i32(stack, __builtin_popcount(lhs_i32)));
                    break;
                
                case OP_I32_ADD:
                    __throwiferr(push_i32(stack, lhs_i32 + rhs_i32));
                    break;
                
                case OP_I32_SUB:
                    __throwiferr(push_i32(stack, lhs_i32 - rhs_i32));
                    break;
                
                case OP_I32_MUL:
                    __throwiferr(push_i32(stack, lhs_i32 * rhs_i32));
                    break;
                
                case OP_I32_DIV_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    __throwif(ERR_TRAP_INTERGET_OVERFLOW, lhs_i32 == INT32_MIN && rhs_i32 == -1);
                    __throwiferr(push_i32(stack, lhs_i32 / rhs_i32));
                    break;
                
                case OP_I32_DIV_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 / (uint32_t)rhs_i32));
                    break;
                
                case OP_I32_REM_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    if(lhs_i32 == INT32_MIN && rhs_i32 == -1) {
                        __throwiferr(push_i32(stack, 0));
                    }
                    else {
                        __throwiferr(push_i32(stack, lhs_i32 % rhs_i32));
                    }
                    break;
                
                case OP_I32_REM_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i32 == 0);
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 % (uint32_t)rhs_i32));
                    break;
                
                case OP_I32_AND:
                    __throwiferr(push_i32(stack, lhs_i32 & rhs_i32));
                    break;
                
                case OP_I32_OR:
                    __throwiferr(push_i32(stack, lhs_i32 | rhs_i32));
                    break;
                
                case OP_I32_XOR:
                    __throwiferr(push_i32(stack, lhs_i32 ^ rhs_i32));
                    break;
                
                case OP_I32_SHL:
                    __throwiferr(push_i32(stack, lhs_i32 << rhs_i32));
                    break;
                
                case OP_I32_SHR_S:
                    __throwiferr(push_i32(stack, lhs_i32 >> rhs_i32));
                    break;
                
                case OP_I32_SHR_U:
                    __throwiferr(push_i32(stack, (uint32_t)lhs_i32 >> (uint32_t)rhs_i32));
                    break;
                
                case OP_I32_ROTL: {
                    uint32_t n = rhs_i32 & 31;
                    __throwiferr(push_i32(stack, ((uint32_t)lhs_i32 << n) | ((uint32_t)lhs_i32 >> ((-n) & 31))));
                    break;
                }

                case OP_I32_ROTR: {
                    uint32_t n = rhs_i32 & 31;
                    __throwiferr(push_i32(stack, ((uint32_t)lhs_i32 >> n) | ((uint32_t)lhs_i32 << ((-n) & 31))));
                    break;
                }

                case OP_I64_CLZ:
                    if(lhs_i64 == 0)
                        __throwiferr(push_i64(stack, 64));
                    else
                        __throwiferr(push_i64(stack, __builtin_clzl(lhs_i64)));
                    break;
                    
                case OP_I64_CTZ:
                    __throwiferr(push_i64(stack, __builtin_ctzl(lhs_i64)));
                    break;
                
                case OP_I64_POPCNT:
                    __throwiferr(push_i64(stack, __builtin_popcountl(lhs_i64)));
                    break;
                
                case OP_I64_ADD:
                    __throwiferr(push_i64(stack, lhs_i64 + rhs_i64));
                    break;
                
                case OP_I64_SUB:
                    __throwiferr(push_i64(stack, lhs_i64 - rhs_i64));
                    break;
                
                case OP_I64_MUL:
                    __throwiferr(push_i64(stack, lhs_i64 * rhs_i64));
                    break;
                
                case OP_I64_DIV_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    __throwif(ERR_TRAP_INTERGET_OVERFLOW, lhs_i64 == INT64_MIN && rhs_i64 == -1);
                    __throwiferr(push_i64(stack, lhs_i64 / rhs_i64));
                    break;
                
                case OP_I64_DIV_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    __throwiferr(push_i64(stack, (uint64_t)lhs_i64 / (uint64_t)rhs_i64));
                    break;
                
                case OP_I64_REM_S:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    if(lhs_i64 == INT64_MIN && rhs_i64 == -1) {
                        __throwiferr(push_i64(stack, 0));
                    }
                    else {
                        __throwiferr(push_i64(stack, lhs_i64 % rhs_i64));
                    }
                    break;
                
                case OP_I64_REM_U:
                    __throwif(ERR_TRAP_INTERGER_DIVIDE_BY_ZERO, rhs_i64 == 0);
                    __throwiferr(push_i64(stack, (uint64_t)lhs_i64 % (uint64_t)rhs_i64));
                    break;
                
                case OP_I64_AND:
                    __throwiferr(push_i64(stack, lhs_i64 & rhs_i64));
                    break;
                
                case OP_I64_OR:
                    __throwiferr(push_i64(stack, lhs_i64 | rhs_i64));
                    break;
                
                case OP_I64_XOR:
                    __throwiferr(push_i64(stack, lhs_i64 ^ rhs_i64));
                    break;
                
                case OP_I64_SHL:
                    __throwiferr(push_i64(stack, lhs_i64 << rhs_i64));
                    break;
                
                case OP_I64_SHR_S:
                    __throwiferr(push_i64(stack, lhs_i64 >> rhs_i64));
                    break;
                
                case OP_I64_SHR_U:
                    __throwiferr(push_i64(stack, (uint64_t)lhs_i64 >> (uint64_t)rhs_i64));
                    break;
                
                case OP_I64_ROTL: {
                    uint64_t n = rhs_i64 & 63;
                    __throwiferr(push_i64( stack, ((uint64_t)lhs_i64 << n) | ((uint64_t)lhs_i64 >> ((-n) & 63))));
                    break;
                }

                case OP_I64_ROTR: {
                    uint64_t n = rhs_i64 & 63;
                    __throwiferr(push_i64(stack, ((uint64_t)lhs_i64 >> n) | ((uint64_t)lhs_i64 << ((-n) & 63))));
                    break;
                }

                case OP_F32_ABS:
                    __throwiferr(push_f32(stack, fabsf(lhs_f32)));
                    break;

                case OP_F32_NEG:
                    __throwiferr(push_f32(stack, -lhs_f32));
                    break;
                
                case OP_F32_CEIL:
                    __throwiferr(push_f32(stack, ceilf(lhs_f32)));
                    break;

                case OP_F32_FLOOR:
                    __throwiferr(push_f32(stack, floorf(lhs_f32)));
                    break;

                case OP_F32_TRUNC:
                    __throwiferr(push_f32(stack, truncf(lhs_f32)));
                    break;
                
                case OP_F32_NEAREST:
                    __throwiferr(push_f32(stack, nearbyintf(lhs_f32)));
                    break;
                
                case OP_F32_SQRT:
                    __throwiferr(push_f32(stack, sqrtf(lhs_f32)));
                    break;
                
                case OP_F32_ADD:
                    __throwiferr(push_f32(stack, lhs_f32 + rhs_f32));
                    break;
                
                case OP_F32_SUB:
                    __throwiferr(push_f32(stack, lhs_f32 - rhs_f32));
                    break;
                
                case OP_F32_MUL:
                    __throwiferr(push_f32(stack, lhs_f32 * rhs_f32));
                    break;
                
                case OP_F32_DIV:
                    __throwiferr(push_f32(stack, lhs_f32 / rhs_f32));
                    break;
                
                case OP_F32_MIN:
                    if(isnan(lhs_f32) || isnan(rhs_f32))
                        __throwiferr(push_f32(stack, NAN));
                    else if(lhs_f32 == 0 && rhs_f32 == 0)
                        __throwiferr(push_f32(stack, signbit(lhs_f32) ? lhs_f32 : rhs_f32));
                    else 
                        __throwiferr(push_f32(stack, lhs_f32 < rhs_f32 ? lhs_f32 : rhs_f32));
                    break;
                
                case OP_F32_MAX:
                    if(isnan(lhs_f32) || isnan(rhs_f32))
                        __throwiferr(push_f32(stack, NAN));
                    else if(lhs_f32 == 0 && rhs_f32 == 0)
                        __throwiferr(push_f32(stack, signbit(lhs_f32) ? rhs_f32 : lhs_f32));
                    else
                        __throwiferr(push_f32(stack, lhs_f32 > rhs_f32 ? lhs_f32 : rhs_f32));
                    break;
                
                case OP_F32_COPYSIGN:
                    __throwiferr(push_f32(stack, copysignf(lhs_f32, rhs_f32)));
                    break;
                
                case OP_F64_ABS:
                    __throwiferr(push_f64(stack, fabs(lhs_f64)));
                    break;
                
                case OP_F64_NEG:
                    __throwiferr(push_f64(stack, -lhs_f64));
                    break;
                
                case OP_F64_CEIL:
                    __throwiferr(push_f64(stack, ceil(lhs_f64)));
                    break;
                
                case OP_F64_FLOOR:
                    __throwiferr(push_f64(stack, floor(lhs_f64)));
                    break;
                
                case OP_F64_TRUNC:
                    __throwiferr(push_f64(stack, trunc(lhs_f64)));
                    break;
                
                case OP_F64_NEAREST:
                    __throwiferr(push_f64(stack, nearbyint(lhs_f64)));
                    break;
                
                case OP_F64_SQRT:
                    __throwiferr(push_f64(stack, sqrt(lhs_f64)));
                    break;
                
                case OP_F64_ADD:
                    __throwiferr(push_f64(stack, lhs_f64 + rhs_f64));
                    break;
                
                case OP_F64_SUB:
                    __throwiferr(push_f64(stack, lhs_f64 - rhs_f64));
                    break;
                
                case OP_F64_MUL:
                    __throwiferr(push_f64(stack, lhs_f64 * rhs_f64));
                    break;
                
                case OP_F64_DIV:
                    __throwiferr(push_f64(stack, lhs_f64 / rhs_f64));
                    break;
                
                case OP_F64_MIN:
                    if(isnan(lhs_f64) || isnan(rhs_f64))
                        __throwiferr(push_f64(stack, NAN));
                    else if(lhs_f64 == 0 && rhs_f64 == 0)
                        __throwiferr(push_f64(stack, signbit(lhs_f64) ? lhs_f64 : rhs_f64));
                    else
                        __throwiferr(push_f64(stack, lhs_f64 < rhs_f64 ? lhs_f64 : rhs_f64));
                    break;
                
                case OP_F64_MAX:
                    if(isnan(lhs_f64) || isnan(rhs_f64))
                        __throwiferr(push_f64(stack, NAN));
                    else if(lhs_f64 == 0 && rhs_f64 == 0)
                        __throwiferr(push_f64(stack, signbit(lhs_f64) ? rhs_f64 : lhs_f64));
                    else
                        __throwiferr(push_f64(stack, lhs_f64 > rhs_f64 ? lhs_f64 : rhs_f64));
                    break;
                
                case OP_F64_COPYSIGN:
                    __throwiferr(push_f64(stack, copysign(lhs_f64, rhs_f64)));
                    break;
                
                case OP_I32_WRAP_I64:
                    __throwiferr(push_i32(stack, lhs_i64 & 0xffffffff));
                    break;

                case OP_I32_TRUNC_F32_S: {
                    __throwiferr(push_i32(stack, I32_TRUNC_F32(lhs_f32)));
                    break;
                }
                
                case OP_I32_TRUNC_F32_U:
                    __throwiferr(push_i32(stack, U32_TRUNC_F32(lhs_f32)));
                    break;
                
                case OP_I32_TRUNC_F64_S:
                    __throwiferr(push_i32(stack, I32_TRUNC_F64(lhs_f64)));
                    break;
                
                case OP_I32_TRUNC_F64_U:
                    __throwiferr(push_i32(stack, U32_TRUNC_F64(lhs_f64)));
                    break;
                
                case OP_I64_EXTEND_I32_S:
                    __throwiferr(push_i64(stack, (int64_t)(int32_t)lhs_i32));
                    break;
                
                case OP_I64_EXTEND_I32_U:
                    __throwiferr(push_i64(stack, (int64_t)(uint32_t)lhs_i32));
                    break;

                case OP_I64_TRUNC_F32_S:
                    __throwiferr(push_i64(stack, I64_TRUNC_F32(lhs_f32)));
                    break;
                
                case OP_I64_TRUNC_F32_U:
                    __throwiferr(push_i64(stack, U64_TRUNC_F32(lhs_f32)));
                    break;
                
                case OP_I64_TRUNC_F64_S:
                    __throwiferr(push_i64(stack, I64_TRUNC_F64(lhs_f64)));
                    break;
                
                case OP_I64_TRUNC_F64_U:
                    __throwiferr(push_i64(stack, U64_TRUNC_F64(lhs_f64)));
                    break;
                
                case OP_F32_CONVERT_I32_S:
                    __throwiferr(push_f32(stack, (float)lhs_i32));
                    break;
                
                case OP_F32_CONVERT_I32_U:
                    __throwiferr(push_f32(stack, (float)(uint32_t)lhs_i32));
                    break;

                case OP_F32_CONVERT_I64_S:
                    __throwiferr(push_f32(stack, (float)lhs_i64));
                    break;
                
                case OP_F32_CONVERT_I64_U:
                    __throwiferr(push_f32(stack, (float)(uint64_t)lhs_i64));
                    break;
                
                case OP_F32_DEMOTE_F64:
                    __throwiferr(push_f32(stack, (float)lhs_f64));
                    break;
                
                case OP_F64_CONVERT_I32_S:
                    __throwiferr(push_f64(stack, (double)lhs_i32));
                    break;

                case OP_F64_CONVERT_I32_U:
                    __throwiferr(push_f64(stack, (double)(uint32_t)lhs_i32));
                    break;

                case OP_F64_CONVERT_I64_S:
                    __throwiferr(push_f64(stack, (double)lhs_i64));
                    break;

                case OP_F64_CONVERT_I64_U:
                    __throwiferr(push_f64(stack, (double)(uint64_t)lhs_i64));
                    break;
                
                case OP_F64_PROMOTE_F32:
                    __throwiferr(push_f64(stack, (double)lhs_f32));
                    break;
                
                // todo: fix this
                case OP_I32_REINTERPRET_F32: {
                    num_t num = {.f32 = lhs_f32};
                    __throwiferr(push_i32(stack, num.i32));
                    break;
                }

                case OP_I64_REINTERPRET_F64: {
                    num_t num = {.f64 = lhs_f64};
                    __throwiferr(push_i64(stack, num.i64));
                    break;
                }
                
                case OP_F32_REINTERPRET_I32: {
                    num_t num = {.i32 = lhs_i32};
                    __throwiferr(push_f32(stack, num.f32));
                    break;
                }

                case OP_F64_REINTERPRET_I64: {
                    num_t num = {.i64 = lhs_i64};
                    __throwiferr(push_f64(stack, num.f64));
                    break;
                }

                case OP_I32_EXTEND8_S:
                    __throwiferr(push_i32(stack, (int32_t)(int8_t)lhs_i32));
                    break;
                
                case OP_I32_EXTEND16_S: 
                    __throwiferr(push_i32(stack, (int32_t)(int16_t)lhs_i32));
                    break;
                
                case OP_I64_EXTEND8_S:
                    __throwiferr(push_i64(stack, (int64_t)(int8_t)lhs_i64));
                    break;
                
                case OP_I64_EXTEND16_S: 
                    __throwiferr(push_i64(stack, (int64_t)(int16_t)lhs_i64));
                    break;
                
                case OP_I64_EXTEND32_S: 
                    __throwiferr(push_i64(stack, (int64_t)(int32_t)lhs_i64));
                    break;
                
                case OP_REF_NULL:
                    __throwiferr(push_val(stack, (val_t){.ref = REF_NULL}));
                    break;
                
                case OP_REF_IS_NULL: {
                    val_t val;
                    pop_val(stack, &val);
                    __throwiferr(push_i32(stack, val.ref == REF_NULL));
                    break;
                }
                
                case OP_0XFC:
                    switch(ip->op2) {
                        case 0x00:
                            __throwiferr(push_i32(stack, I32_TRUNC_SAT_F32(lhs_f32)));
                            break;

                        case 0x01:
                            __throwiferr(push_i32(stack, U32_TRUNC_SAT_F32(lhs_f32)));
                            break;
                        
                        case 0x02:
                            __throwiferr(push_i32(stack, I32_TRUNC_SAT_F64(lhs_f64)));
                            break;

                        case 0x03:
                            __throwiferr(push_i32(stack, U32_TRUNC_SAT_F64(lhs_f64)));
                            break;
                        
                        case 0x04:
                            __throwiferr(push_i64(stack, I64_TRUNC_SAT_F32(lhs_f32)));
                            break;
                        
                        case 0x05:
                            __throwiferr(push_i64(stack, U64_TRUNC_SAT_F32(lhs_f32)));
                            break;

                        case 0x06:
                            __throwiferr(push_i64(stack, I64_TRUNC_SAT_F64(lhs_f64)));
                            break;

                        case 0x07:
                            __throwiferr(push_i64(stack, U64_TRUNC_SAT_F64(lhs_f64)));
                            break;
                        
                        // mememory.init
                        case 0x08: {
                            memaddr_t ma = F->module->memaddrs[0];
                            meminst_t *mem = VECTOR_ELEM(&S->mems, ma);
                            dataaddr_t da = F->module->dataaddrs[ip->x];
                            datainst_t *data = VECTOR_ELEM(&S->datas, da);

                            int32_t n, s, d;
                            pop_i32(stack, &n);
                            pop_i32(stack, &s);
                            pop_i32(stack, &d);

                            // todo: fix this
                            __throwif(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS, s + n > data->data.len || d + n > mem->num_pages * WASM_PAGE_SIZE);
                            
                            while(n--) {
                                byte_t b = *VECTOR_ELEM(&data->data, s);
                                
                                __throwiferr(push_i32(stack, d));
                                __throwiferr(push_i32(stack, (int32_t)b));

                                instr_t i32_store8 = {
                                    .op1 = OP_I32_STORE8, .next = NULL, 
                                    .m = (memarg_t){.offset = 0, .align = 0}
                                };
                                expr_t expr = &i32_store8;
                                exec_expr(S, &expr);
                                s++;
                                d++;
                            }
                            break;
                        }

                        // data.drop
                        case 0x09: {
                            dataaddr_t a = F->module->dataaddrs[ip->x];
                            datainst_t *data = VECTOR_ELEM(&S->datas, a);
                            VECTOR_INIT(&data->data);
                            break;
                        }

                        // memory.copy
                        case 0x0A: {
                            memaddr_t ma = F->module->memaddrs[0];
                            meminst_t *mem = VECTOR_ELEM(&S->mems, ma);
                            int32_t n, s, d;
                            pop_i32(stack, &n);
                            pop_i32(stack, &s);
                            pop_i32(stack, &d);
                            
                            uint64_t ea1 = (uint32_t)s, ea2 = (uint32_t)d;
                            ea1 += (uint32_t)n;
                            ea2 += (uint32_t)n;
                            __throwif(
                                ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS, 
                                ea1 > mem->num_pages * WASM_PAGE_SIZE || \
                                ea2 > mem->num_pages * WASM_PAGE_SIZE
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
                                    __throwiferr(push_i32(stack, d));
                                    __throwiferr(push_i32(stack, s));
                                    exec_expr(S, &expr);
                                    d++;
                                    s++;
                                }
                                else {
                                    __throwiferr(push_i32(stack, d + n - 1));
                                    __throwiferr(push_i32(stack, s + n - 1));
                                    exec_expr(S, &expr);
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
                            pop_i32(stack, &n);
                            pop_i32(stack, &val);
                            pop_i32(stack, &d);
                            uint64_t ea = (uint32_t)d;
                            ea += (uint32_t)n;

                            __throwif(ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS, ea > mem->num_pages * WASM_PAGE_SIZE);

                            while(n--) {
                                __throwiferr(push_i32(stack, d));
                                __throwiferr(push_i32(stack, val));

                                instr_t i32_store8 = {
                                    .op1 = OP_I32_STORE8, .next = NULL, 
                                    .m = (memarg_t){.offset = 0, .align = 0}
                                };
                                expr_t expr = &i32_store8;
                                exec_expr(S, &expr);
                                d++;
                            }
                            break;
                        }

                        // table.init
                        case 0x0C: {
                            while(1) {
                                tableaddr_t ta = F->module->tableaddrs[ip->x];
                                tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);
                                elemaddr_t ea = F->module->elemaddrs[ip->y];
                                eleminst_t *elem = VECTOR_ELEM(&S->elems, ea);
                                int32_t n, s, d;
                                pop_i32(stack, &n);
                                pop_i32(stack, &s);
                                pop_i32(stack, &d);

                                uint64_t idx1 = (uint32_t)s, idx2 = (uint32_t)d;
                                idx1 += (uint32_t)n;
                                idx2 += (uint32_t)n;
                                
                                __throwif(
                                    ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS, 
                                    idx1 > elem->elem.len || idx2 > tab->elem.len
                                );

                                if(n == 0)
                                    break;
                                
                                ref_t *ref = VECTOR_ELEM(&elem->elem, s);
                                __throwiferr(push_i32(stack, d));
                                __throwiferr(push_val(stack, (val_t){.ref = *ref}));
                                instr_t table_set = {
                                    .op1 = OP_TABLE_SET, .next = NULL, 
                                    .x = ip->x
                                };
                                expr_t expr = &table_set;
                                exec_expr(S, &expr);
                                push_i32(stack, d + 1);
                                push_i32(stack, s + 1);
                                push_i32(stack, n - 1);
                            }
                            break;
                        }

                        // elem.drop
                        case 0x0D: {
                            elemaddr_t a = F->module->elemaddrs[ip->x];
                            eleminst_t *elem = VECTOR_ELEM(&S->elems, a);
                            VECTOR_INIT(&elem->elem);
                            break;
                        }

                        // table.copy
                        case 0x0E: {
                            while(1) {
                                tableaddr_t ta_x = F->module->tableaddrs[ip->x];
                                tableinst_t *tab_x = VECTOR_ELEM(&S->tables, ta_x);
                                tableaddr_t ta_y = F->module->tableaddrs[ip->y];
                                tableinst_t *tab_y = VECTOR_ELEM(&S->tables, ta_y);

                                int32_t n, s, d;
                                pop_i32(stack, &n);
                                pop_i32(stack, &s);
                                pop_i32(stack, &d);

                                uint64_t idx1 = (uint32_t)s, idx2 = (uint32_t)d;
                                idx1 += (uint32_t)n;
                                idx2 += (uint32_t)n;

                                __throwif(
                                    ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS, 
                                    idx1 > tab_y->elem.len || idx2 > tab_x->elem.len
                                );

                                instr_t table_set = {
                                    .op1 = OP_TABLE_SET, .x = ip->x, .next = NULL
                                };
                                instr_t table_get = {
                                    .op1 = OP_TABLE_GET, .x = ip->y, .next = &table_set
                                };
                                expr_t expr = &table_get;

                                if(n == 0)
                                    break;
                                
                                if(d <= s) {
                                    __throwiferr(push_i32(stack, d));
                                    __throwiferr(push_i32(stack, s));
                                    exec_expr(S, &expr);
                                    __throwiferr(push_i32(stack, d + 1));
                                    __throwiferr(push_i32(stack, s + 1));
                                }
                                else {
                                    __throwiferr(push_i32(stack, d + n - 1));
                                    __throwiferr(push_i32(stack, s + n - 1));
                                    exec_expr(S, &expr);
                                    __throwiferr(push_i32(stack, d));
                                    __throwiferr(push_i32(stack, s));
                                }
                                push_i32(stack, n - 1);
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
                            pop_i32(stack, &n);
                            pop_val(stack, &val);

                            if(tab->type.limits.max && n + tab->elem.len > tab->type.limits.max) {
                                __throwiferr(push_i32(stack, -1));
                                break;
                            }

                            if(!IS_ERROR(VECTOR_GROW(&tab->elem, n))) {
                                tab->elem.len += n;
                                // init
                                for(int i = sz; i < tab->elem.len; i++) {
                                    *VECTOR_ELEM(&tab->elem, i) = val.ref;
                                }
                                __throwiferr(push_i32(stack, sz));
                            }
                            else {
                                __throwiferr(push_i32(stack, -1));
                            }
                            break;
                        }

                        // table.size
                        case 0x10: {
                            tableaddr_t ta = F->module->tableaddrs[ip->x];
                            tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);
                            __throwiferr(push_i32(stack, tab->elem.len));
                            break;
                        }

                        // table.fill
                        case 0x11: {
                            tableaddr_t ta = F->module->tableaddrs[ip->x];
                            tableinst_t *tab = VECTOR_ELEM(&S->tables, ta);
                            int32_t n, i;
                            val_t val;

                            pop_i32(stack, &n);
                            pop_val(stack, &val);
                            pop_i32(stack, &i);

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
                    __throwiferr(push_val(stack, (val_t){.ref = a}));
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
    __try {
        stack_t *stack = S->stack;

        funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, funcaddr);
        functype_t *functype = funcinst->type;

        // create new frame
        frame_t frame;
        uint32_t num_locals = functype->rt1.len + funcinst->code->locals.len;
        frame.module = funcinst->module;
        frame.locals = calloc(sizeof(val_t), num_locals);

        // pop args
        for(int32_t i = (functype->rt1.len - 1); 0 <= i; i--) {
            pop_val(stack, &frame.locals[i]);
        }
        
        // push activation frame
        frame.arity  = functype->rt2.len;
        __throwiferr(push_frame(stack, frame));

        // create label L
        static instr_t end = {.op1 = OP_END, .next = NULL};
        label_t L = {.arity = functype->rt2.len, .parent = NULL, .continuation = &end};
        // enter instr* with label L
        __throwiferr(push_label(stack, L));

        __throwiferr(exec_expr(S, &funcinst->code->body));

        // return from a function("return" instruction)
        vals_t vals;
        pop_vals_n(stack, frame.arity, &vals);
        pop_while_not_frame(stack);
        pop_frame(stack, &frame);
        __throwiferr(push_vals(stack, vals));
    }
    __catch:
        return err;
}

// The args is a reference to args_t. 
// This is because args is also used to return results.
error_t invoke(store_t *S, funcaddr_t funcaddr, args_t *args) {
    __try {
        stack_t *stack = S->stack;

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
            __throwiferr(push_val(stack, arg->val));
        }

        // invoke func
        __throwiferr(invoke_func(S, funcaddr));

        // reuse args to return results since it is no longer used.
        //free(args->elem);
        VECTOR_NEW(args, functype->rt2.len, functype->rt2.len);
        idx = 0;
        VECTOR_FOR_EACH_REVERSE(ret, args) {
            ret->type = *VECTOR_ELEM(&functype->rt2, idx++);
            pop_val(stack, &ret->val);
        }
    }
    __catch:
        return err;
}