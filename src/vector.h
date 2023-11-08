#pragma once

#include "error.h"
#include <stddef.h>

typedef struct {
    size_t  n;
    void    *elem;
} vector_t;

void vector_init(vector_t *vec);
error_t vector_new(vector_t *vec, size_t ent_size, size_t n);
void vector_copy(vector_t *dst, vector_t *src);

// useful macros
#define VECTOR(type)                                                                    \
    struct {                                                                            \
        size_t    n;                                                                    \
        type        *elem;                                                              \
    }
 
#define VECTOR_INIT(vec)                                                                \
    vector_init((vector_t *)vec)

#define VECTOR_NEW(vec, n)                                                              \
    vector_new((vector_t *)vec, sizeof(__typeof__((vec)->elem[0])), n)

#define VECTOR_COPY(dst, src)                                                           \
    vector_copy((vector_t *)dst, (vector_t *)src)

#define VECTOR_FOR_EACH(iter, vec)                                                      \
    for(__typeof__((vec)->elem) iter = &(vec)->elem[0];                                 \
        iter != &(vec)->elem[(vec)->n];                                                 \
        iter++                                                                          \
    )

#define VECTOR_FOR_EACH_REVERSE(iter, vec)                                              \
    for(__typeof__((vec)->elem) iter = &(vec)->elem[(vec)->n - 1];                      \
        (vec)->n != 0 &&  iter != &(vec)->elem[-1];                                     \
        iter--                                                                          \
    )

#define VECTOR_ELEM(vec, idx)                                                           \
    ({                                                                                  \
        size_t __i = (idx);                                                             \
        (vec)->n != 0 && 0 <= __i && __i <= ((vec)->n - 1) ? &(vec)->elem[__i] : NULL;  \
    })

#define VECTOR_CONCAT(dst, src1, src2, type)                                            \
    do {                                                                                \
        VECTOR_NEW((dst), (src1)->n + (src2)->n);                                       \
        memcpy((dst)->elem, (src1)->elem, sizeof(type) * (src1)->n);                    \
        memcpy(&(dst)->elem[(src1)->n], (src2)->elem, sizeof(type) * (src2)->n);        \
    } while(0)
