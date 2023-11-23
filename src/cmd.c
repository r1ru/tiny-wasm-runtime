
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

    VECTOR_FOR_EACH(valtype, &func->rt1) {
        printf("%x ", *valtype);
    }
    putchar('\n');

    printf("returns: ");
    VECTOR_FOR_EACH(valtype, &func->rt2) {
        printf("%x ", *valtype);
    }
    putchar('\n');
}

void print_func(func_t *func) {
    printf("type: %x\n", func->type);
    printf("locals: ");

    VECTOR_FOR_EACH(valtype, &func->locals) {
        printf("%x ", *valtype);
    }
    putchar('\n');
}

void print_table(table_t *table) {
    printf(
        "reftype: %x, min: %x, max: %x\n",
        table->type.reftype,
        table->type.limits.min,
        table->type.limits.max
    );
}

void print_mem(mem_t *mem) {
    printf("min: %x, max: %x\n", mem->type.min, mem->type.max);
}

void print_global(global_t *g) {
    printf("type: %x, mut: %x expr head: %x\n", g->gt.type, g->gt.mut, g->expr->op1);
}

static void print_export(export_t *export) {
    printf(
        "%s %x %x\n", 
        export->name, 
        export->exportdesc.kind, 
        export->exportdesc.idx
    );
}

int main(int argc, char *argv[]) {
    /*
    if(argc != 2) {
        puts("Usage: ./a.out <*.wasm>");
        exit(EXIT_FAILURE);
    }*/

    //int fd = open(argv[1], O_RDWR);
    int fd = open("./build/test/unreached-invalid.1.wasm", O_RDONLY);
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
        PANIC("decoding failed: %d", err);
    
    puts("[types]");
    VECTOR_FOR_EACH(type, &mod->types) {
        print_type(type);
    }
    putchar('\n');

    puts("[funcs]");
    VECTOR_FOR_EACH(func, &mod->funcs) {
        print_func(func);
    }
    putchar('\n');

    puts("[tables]");
    VECTOR_FOR_EACH(table, &mod->tables) {
        print_table(table);
    }
    putchar('\n');
    
    puts("[mems]");
    VECTOR_FOR_EACH(mem, &mod->mems) {
        print_mem(mem);
    }
    putchar('\n');

    puts("[globals]");
    VECTOR_FOR_EACH(g, &mod->globals) {
        print_global(g);
    }
    putchar('\n');

    puts("[exports]");
    VECTOR_FOR_EACH(export, &mod->exports) {
        print_export(export);
    }
    putchar('\n');

    err = validate_module(mod);
    
    if(IS_ERROR(err))
        PANIC("validation failed: %d", err);
    
    instance_t *instance;
    err = instantiate(&instance, mod);
    if(IS_ERROR(err))
        PANIC("insntiation failed");

    args_t args;
    VECTOR_NEW(&args, 1, 1);
    *VECTOR_ELEM(&args, 0) = (arg_t){.type = TYPE_NUM_I64, .val.num.i64 = 200};
    // invoke
    err = invoke(instance, 41, &args);

    if(IS_ERROR(err))
        PANIC("invocation fail: %d", err);

    printf("ret = %d\n", VECTOR_ELEM(&args, 0)->val.num.i32);

    // cleanup
    munmap(head, fsize);
    close(fd);
}