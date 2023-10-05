
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "memory.h"
#include "parse.h"
#include "module.h"
#include "vector.h"
#include "error.h"

static void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void print_func(functype_t *func) {
    printf("args: ");

    VECTOR_FOR_EACH(valtype, &func->rt1, uint8_t) {
        printf("%#x ", *valtype);
    };
    putchar('\n');

    printf("returns: ");
    VECTOR_FOR_EACH(valtype, &func->rt2, uint8_t) {
        printf("%#x ", *valtype);
    };
    putchar('\n');
}

static void print_typesec(section_t *sec) {
    puts("[Type Section]");

    VECTOR_FOR_EACH(functype, &sec->functypes, functype_t) {
        print_func(functype);
    };
}

static void print_funcsec(section_t *sec) {
    puts("[Function Section]");

    VECTOR_FOR_EACH(typeidx, &sec->typeidxes, uint32_t) {
        printf("%#x ", *typeidx);
    };
    putchar('\n');
}

static void print_exportsec(section_t *sec) {
    puts("[Export Section]");

    VECTOR_FOR_EACH(export, &sec->exports, export_t) {
        printf(
            "%s %#x %#x\n", 
            export->name, 
            export->exportdesc.kind, 
            export->exportdesc.idx
        );
    }
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
            printf("%s %#x\n",op_str[instr->op], instr->bt.valtype);
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
            printf("%s %#x\n", op_str[instr->op], instr->l);
            break;
        
        case OP_CALL:
        case OP_LOCAL_GET:
        case OP_LOCAL_SET:
            printf("%s %#x\n", op_str[instr->op], instr->idx);
            break;
        
        case OP_I32_CONST:
            printf("i32.const %#x\n", instr->c.i32);
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

static void print_code(code_t *code) {
    printf("size: %#x\n", code->size);
    printf("locals: ");

    func_t *func = &code->func;

    VECTOR_FOR_EACH(locals, &func->locals, locals_t) {
        printf("(%#x, %#x) ", locals->n, locals->type);
    }
    putchar('\n');

    for(instr_t *i = func->expr; i; i = i -> next) {
        print_instr(i);
    }
}

static void print_codesec(section_t *sec) {
    puts("[Code Section]");
    VECTOR_FOR_EACH(code, &sec->codes, code_t) {
        print_code(code);
    };
}

typedef void (*printer_t) (section_t *sec);

printer_t printers[11] = {
    [1]     = print_typesec,
    [3]     = print_funcsec,
    [7]     = print_exportsec,
    [10]    = print_codesec
};

int main(int argc, char *argv[]) {
    if(argc != 2) {
        puts("Usage: ./a.out <*.wasm>");
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDWR);
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

    parse_module(&mod, head, fsize);

    for(int i = 0; i < 11; i++) {
        if(mod->known_sections[i])
            printers[i](mod->known_sections[i]);
    }

    // cleanup
    munmap(head, fsize);
    close(fd);
}