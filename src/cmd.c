
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

void print_func(func_t *func) {
    printf("type: %x\n", func->type);
    printf("locals: ");

    VECTOR_FOR_EACH(valtype, &func->locals, valtype_t) {
        printf("%x ", *valtype);
    }
    putchar('\n');
}

void print_mem(mem_t *mem) {
    printf("min: %x, max: %x\n", mem->type.min, mem->type.max);
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
    int fd = open("./build/test/store.0.wasm", O_RDONLY);
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

    puts("[mems]");
    VECTOR_FOR_EACH(mem, &mod->mems, mem_t) {
        print_mem(mem);
    }
    putchar('\n');

    puts("[exports]");
    VECTOR_FOR_EACH(export, &mod->exports, export_t) {
        print_export(export);
    }
    putchar('\n');


    err = validate_module(mod);
    if(IS_ERROR(err))
        PANIC("validation failed: %d", err);
        
    store_t *S;
    err = instantiate(&S, mod);
    if(IS_ERROR(err))
        PANIC("insntiation failed");

    args_t args;
    VECTOR_INIT(&args, 0, arg_t);

    // invoke
    err = invoke(S, 1, &args);
    if(IS_ERROR(err))
        PANIC("invocation failed");
    
    printf("result = %ld\n", VECTOR_ELEM(&args, 0)->val.num.i64);

    // cleanup
    munmap(head, fsize);
    close(fd);
}