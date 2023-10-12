#pragma once

#include "memory.h"

// useful macros
#define VECTOR(type)                                                            \
    struct {                                                                    \
        uint32_t    n;                                                          \
        type        *elem;                                                      \
    }

#define VECTOR_INIT(vec, len, type)                                             \
    ({                                                                          \
        (vec)->n = (len);                                                       \
        (vec)->elem = malloc(sizeof(type) * (len));                             \
    })

#define VECTOR_FOR_EACH(iter, vec, type)                                        \
    for(type *iter = &(vec)->elem[0];                                           \
        iter != &(vec)->elem[(vec)->n];                                         \
        iter++                                                                  \
    )

#define VECTOR_ELEM(vec, idx)                                                   \
    ({                                                                          \
        size_t __i = (idx);                                                     \
        0 <= __i && __i <= ((vec)->n - 1) ? &(vec)->elem[__i] : NULL;           \
    })

#define VECTOR_COPY(dst, src, type)                                             \
    do {                                                                        \
        (dst)->n = (src)->n;                                                    \
        (dst)->elem = (src)->elem;                                              \
    } while(0)

#define VECTOR_CONCAT(dst, src1, src2, type)                                    \
    do {                                                                        \
        VECTOR_INIT((dst), (src1)->n + (src2)->n, type);                        \
        memcpy((dst)->elem, (src1)->elem, sizeof(type) * (src1)->n);            \
        memcpy(&(dst)->elem[(src1)->n], (src2)->elem, sizeof(type) * (src2)->n);\
    } while(0)
