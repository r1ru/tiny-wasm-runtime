
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
#include "error.h"
#include "exec.h"

static void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void print_type(functype_t *func) {
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
    [OP_BLOCK]      = "block",
    [OP_LOOP]       = "loop",
    [OP_IF]         = "if",
    [OP_END]        = "end",
    [OP_BR]         = "br",
    [OP_BR_IF]      = "br_if",
    [OP_CALL]       = "call",
    [OP_LOCAL_GET]  = "local.get",
    [OP_LOCAL_SET]  = "local.set",
    [OP_I32_CONST]  = "i32.const",
    [OP_I32_EQZ]    = "i32.eqz",
    [OP_I32_LT_S]   = "i32.lt_s",
    [OP_I32_GE_S]   = "i32.ge_s",
    [OP_I32_ADD]    = "i32.add",
    [OP_I32_REM_S]  = "i32.rem_s",
};

static void print_instr(instr_t *instr) {
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
        
        case OP_CALL:
            printf("%s %d\n", op_str[instr->op], instr->funcidx);
            break;
        
        case OP_LOCAL_GET:
        case OP_LOCAL_SET:
            printf("%s %d\n", op_str[instr->op], instr->labelidx);
            break;
        
        case OP_I32_CONST:
            printf("i32.const %d\n", instr->c.i32);
            break;
        
        case OP_I32_EQZ:
        case OP_I32_LT_S:
        case OP_I32_GE_S:
        case OP_I32_ADD:
        case OP_I32_REM_S:
            printf("%s\n", op_str[instr->op]);
            break;
    }
}

static void print_func(func_t *func) {
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
    int fd = open("./wasm/add.wasm", O_RDONLY);
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
    decode_module(&mod, head, fsize);
    
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

    store_t *S;
    error_t err;

    err = instantiate(&S, mod);
    printf("err = %x\n", err);

    args_t args;
    VECTOR_INIT(&args, 2, val_t);
    VECTOR_FOR_EACH(arg, &args, val_t) {
        arg->type      = TYPE_NUM_I32;
        arg->num.int32 = 1;
    };

    // invoke add(1, 1)
    err = invoke(S, 0, &args);
    printf("err = %x\n", err);
    printf("result = %x\n", VECTOR_ELEM(&args, 0)->num.int32);

    // cleanup
    munmap(head, fsize);
    close(fd);
}