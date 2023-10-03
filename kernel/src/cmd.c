
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

static void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void print_func(functype_t *func) {
    printf("args: ");

    VECTOR_FOR_EACH(e, func->rt1, uint8_t) {
        printf("%#x ", *e);
    };
    putchar('\n');

    printf("returns: ");
    VECTOR_FOR_EACH(e, func->rt2, uint8_t) {
        printf("%#x ", *e);
    };
    putchar('\n');
}

static void print_typesec(struct section *sec) {
    puts("[Type Section]");

    VECTOR_FOR_EACH(elem, sec->typeidxes, functype_t) {
        print_func(elem);
    };
}

static void print_funcsec(struct section *sec) {
    puts("[Function Section]");

    VECTOR_FOR_EACH(elem, sec->typeidxes, uint32_t) {
        printf("%#x ", *elem);
    };
    putchar('\n');
}

static void print_instr(instr_t *i) {
    switch(i->op) {
        case OP_END:
            puts("end");
        // todo: add here
    }
}

static void print_code(code_t *code) {
    printf("size: %#x\n", code->size);
    printf("locals: ");

    func_t *func = &code->func;

    VECTOR_FOR_EACH(locals, func->locals, locals_t) {
        printf("(%#x, %#x) ", locals->n, locals->type);
    }
    putchar('\n');

    for(instr_t *i = func->expr; i; i = i -> next) {
        print_instr(i);
    }
}

static void print_codesec(struct section *sec) {
    puts("[Code Section]");
    VECTOR_FOR_EACH(code, sec->codes, code_t) {
        print_code(code);
    };
}

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
    
    struct buffer *buf = new_buffer(head, fsize);
    struct module *mod = parse_module(buf);

    if(mod->known_sections[1])
        print_typesec(mod->known_sections[1]);
    
    if(mod->known_sections[3])
        print_funcsec(mod->known_sections[3]);
    
    if(mod->known_sections[10])
        print_codesec(mod->known_sections[10]);
    
    // cleanup
    munmap(head, fsize);
    close(fd);
}