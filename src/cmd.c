
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "memory.h"
#include "decode.h"
#include "module.h"
#include "validate.h"
#include "error.h"
#include "exec.h"
#include "print.h"

static void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_type(functype_t *func) {
    printf("args: ");

    VECTOR_FOR_EACH(valtype, &func->rt1, valtype_t) {
        printf("%x ", *valtype);
    }
    putchar('\n');

    printf("returns: ");
    VECTOR_FOR_EACH(valtype, &func->rt2, valtype_t) {
        printf("%x ", *valtype);
    }
    putchar('\n');
}

static const char *op_str[] = {
    [OP_BLOCK]          = "block",
    [OP_LOOP]           = "loop",
    [OP_IF]             = "if",
    [OP_END]            = "end",
    [OP_BR]             = "br",
    [OP_BR_IF]          = "br_if",
    [OP_CALL]           = "call",
    [OP_LOCAL_GET]      = "local.get",
    [OP_LOCAL_SET]      = "local.set",
    [OP_LOCAL_TEE]      = "local.tee",
    [OP_GLOBAL_GET]     = "global.get",
    [OP_GLOBAL_SET]     = "global.set",
    [OP_I32_LOAD]       = "i32.load",
    [OP_I32_STORE]      = "i32.store",
    [OP_I32_CONST]      = "i32.const",
    [OP_I32_EQZ]        = "i32.eqz",
    [OP_I32_EQ]         = "i32.eq",
    [OP_I32_NE]         = "i32.ne",
    [OP_I32_LT_S]       = "i32.lt_s",
    [OP_I32_LT_U]       = "i32.lt_u",
    [OP_I32_GT_S]       = "i32.gt_s",
    [OP_I32_GT_U]       = "i32.gt_u",
    [OP_I32_LE_S]       = "i32.le_s",
    [OP_I32_LE_U]       = "i32.le_u",
    [OP_I32_GE_S]       = "i32.ge_s",
    [OP_I32_GE_U]       = "i32.ge_u",
    [OP_I32_CLZ]        = "i32.clz",
    [OP_I32_CTZ]        = "i32.ctz",
    [OP_I32_POPCNT]     = "i32.popcnt",
    [OP_I32_ADD]        = "i32.add",
    [OP_I32_SUB]        = "i32.sub",
    [OP_I32_MUL]        = "i32.mul",
    [OP_I32_DIV_S]      = "i32.div_s",
    [OP_I32_DIV_U]      = "i32.div_u",
    [OP_I32_REM_S]      = "i32.rem_s",
    [OP_I32_REM_U]      = "i32.rem_u",
    [OP_I32_AND]        = "i32.and",
    [OP_I32_OR]         = "i32.or",
    [OP_I32_XOR]        = "i32.xor",
    [OP_I32_SHL]        = "i32.shl",
    [OP_I32_SHR_S]      = "i32.shr_s",
    [OP_I32_SHR_U]      = "i32.shr_u",
    [OP_I32_ROTL]       = "i32.rotl",
    [OP_I32_ROTR]       = "i32.rotr",
    [OP_I32_EXTEND8_S]  = "i32.extend8_s",
    [OP_I32_EXTEND16_S] = "i32.extend16_s",
};

void print_instr(instr_t *instr) {
    switch(instr->op) {
        case OP_BLOCK:
        case OP_LOOP:
        case OP_IF: {
            printf("%s\n",op_str[instr->op]);
            for(instr_t *i = instr->in1; i; i = i->next) {
                print_instr(i);
            }

            if(instr->op == OP_IF) {
                puts("else");
                for(instr_t *i = instr->in2; i; i = i->next) {
                    print_instr(i);
                }
            }

            break;
        }

        case OP_END:
            printf("%s\n", op_str[instr->op]);
            break;
        
        case OP_BR:
        case OP_BR_IF:
            printf("%s %d\n", op_str[instr->op], instr->labelidx);
            break;
        
        case OP_BR_TABLE:
            printf("br_table");
            VECTOR_FOR_EACH(l, &instr->labels, labelidx_t) {
                printf(" %x", *l);
            }
            printf(" %x\n", instr->default_label);
            break;
        
        case OP_RETURN:
            printf("return\n");
            break;
        
        case OP_CALL:
            printf("%s %d\n", op_str[instr->op], instr->funcidx);
            break;
        
        case OP_CALL_INDIRECT:
            printf("call_indirect %x %x\n",instr->x, instr->y);
            break;

        case OP_DROP:
            printf("drop\n");
            break;
        
        case OP_SELECT:
            printf("select\n");
            break;
        
        case OP_LOCAL_GET:
        case OP_LOCAL_SET:
        case OP_LOCAL_TEE:
            printf("%s %d\n", op_str[instr->op], instr->localidx);
            break;

        case OP_GLOBAL_GET:
        case OP_GLOBAL_SET:
            printf("%s %d\n", op_str[instr->op], instr->globalidx);
            break;
        
        case OP_I32_LOAD:
        case OP_I32_STORE:
            printf("%s %x %x\n", op_str[instr->op], instr->m.align, instr->m.offset);
            break;
        
        case OP_MEMORY_GROW:
            printf("memory.grow 0\n");
            break;
        
        case OP_I32_CONST:
            printf("i32.const %d\n", instr->c.i32);
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

        case OP_I32_EXTEND8_S:
        case OP_I32_EXTEND16_S:
            printf("%s\n", op_str[instr->op]);
            break;
    }
}

void print_func(func_t *func) {
    printf("type: %x\n", func->type);
    printf("locals: ");

    VECTOR_FOR_EACH(valtype, &func->locals, valtype_t) {
        printf("%x ", *valtype);
    }
    putchar('\n');

    puts("body:");
    for(instr_t *i = func->body; i; i = i->next) {
        print_instr(i);
    }
}

static void print_export(export_t *export) {
    printf(
        "%s %x %x\n", 
        export->name, 
        export->exportdesc.kind, 
        export->exportdesc.funcidx
    );
}

int main(int argc, char *argv[]) {
    /*
    if(argc != 2) {
        puts("Usage: ./a.out <*.wasm>");
        exit(EXIT_FAILURE);
    }*/

    //int fd = open(argv[1], O_RDWR);
    int fd = open("./build/test/i32.18.wasm", O_RDONLY);
    if(fd == -1) fatal("open");

    struct stat s;
    if(fstat(fd, &s) == -1) fatal("fstat");

    size_t fsize = s.st_size;
    uint8_t *head = mmap(
        NULL,
        fsize,
        PROT_READ,
        MAP_PRIVATE,
        fd, 
        0
    );
    if(head == MAP_FAILED) fatal("mmap");

    module_t *mod;
    error_t err;

    err = decode_module(&mod, head, fsize);
    if(IS_ERROR(err))
        PANIC("decoding failed");
    
    puts("[types]");
    VECTOR_FOR_EACH(type, &mod->types, functype_t) {
        print_type(type);
    }
    putchar('\n');

    puts("[funcs]");
    VECTOR_FOR_EACH(func, &mod->funcs, func_t) {
        print_func(func);
    }
    putchar('\n');

    puts("[exports]");
    VECTOR_FOR_EACH(export, &mod->exports, export_t) {
        print_export(export);
    }
    putchar('\n');


    err = validate_module(mod);
    if(IS_ERROR(err))
        PANIC("validation failed");

    store_t *S;
    err = instantiate(&S, mod);
    if(IS_ERROR(err))
        PANIC("insntiation failed");

    /*
    args_t args;
    VECTOR_INIT(&args, 2, arg_t);

    int idx = 0;
    VECTOR_FOR_EACH(arg, &args, arg_t) {
        arg->type = TYPE_NUM_I32;
        arg->val.num.i32 = idx++ == 0 ? 1 : 1;
    };

    // invoke ge_u(1,1)
    err = invoke(S, 30, &args);
    if(IS_ERROR(err))
        PANIC("invocation failed");
    
    printf("result = %x\n", VECTOR_ELEM(&args, 0)->val.num.i32); */

    // cleanup
    munmap(head, fsize);
    close(fd);
}