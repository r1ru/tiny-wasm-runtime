#pragma once

#include <stddef.h>

void *malloc(size_t size);

struct vector {
    size_t  n;
    void    *p;   
};

typedef struct vector vector_t;

#define VECTOR_INIT(vec, len, type)             \
    ({                                          \
        vec.n = (len);                          \
        vec.p = malloc(sizeof(type) * (len));   \
    })