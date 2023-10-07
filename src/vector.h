#pragma once

#include "memory.h"

// useful macro
#define VECTOR(type)                                \
    struct {                                        \
        uint32_t    n;                              \
        type        *elem;                          \
    }

#define VECTOR_INIT(vec, len, type)                 \
    ({                                              \
        (vec)->n = (len);                           \
        (vec)->elem = malloc(sizeof(type) * (len)); \
    })

#define VECTOR_FOR_EACH(iter, vec, type)            \
    for(type *iter = &(vec)->elem[0];               \
        iter != &(vec)->elem[(vec)->n];             \
        iter++                                      \
    )

#define VECTOR_ELEM(vec, n)                         \
    (&(vec)->elem[n])

#define VECTOR_COPY(dst, src, type)                 \
    do {                                            \
        VECTOR_INIT((dst), (src)->n, type);         \
        int idx = 0;                                \
        VECTOR_FOR_EACH(e, (dst), type){            \
            *e = *VECTOR_ELEM((src), idx++);        \
        }                                           \
    }while(0)
