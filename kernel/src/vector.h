#pragma once

#include <stddef.h>
#include "memory.h"

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

#define VECTOR_FOR_EACH(elem, vec, type)        \
    for(type *elem = &((type *)vec.p)[0]; elem != &((type *)vec.p)[vec.n]; elem++)
