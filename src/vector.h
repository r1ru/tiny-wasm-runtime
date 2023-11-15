#pragma once

#include "error.h"
#include <stddef.h>

typedef struct {
    size_t  len;
    void    *elem;
} vector_t;

void vector_init(vector_t *vec);
void vector_copy(vector_t *dst, vector_t *src);
error_t vector_new(vector_t *vec, size_t ent_size, size_t n);
error_t vector_concat(vector_t *dst, vector_t *src1, vector_t *src2, size_t ent_size);
error_t vector_grow(vector_t *vec, size_t ent_size, size_t n);

// useful macros
#define VECTOR(type)                                                                    \
    struct {                                                                            \
        size_t    len;                                                                  \
        type      *elem;                                                                \
    }
 
#define VECTOR_INIT(vec)                                                                \
    vector_init((vector_t *)vec)

#define VECTOR_NEW(vec, n)                                                              \
    vector_new((vector_t *)vec, sizeof(__typeof__(*(vec)->elem)), n)

#define VECTOR_COPY(dst, src)                                                           \
    vector_copy((vector_t *)dst, (vector_t *)src)

#define VECTOR_CONCAT(dst, src1, src2)                                                  \
    vector_concat(                                                                      \
        (vector_t *)dst,                                                                \
        (vector_t *)src1,                                                               \
        (vector_t *)src2,                                                               \
        sizeof(__typeof__(*(src1)->elem))                                               \
    )                                                                                   \

#define VECTOR_FOR_EACH(iter, vec)                                                      \
    for(__typeof__((vec)->elem) iter = &(vec)->elem[0];                                 \
        (vec)->len != 0 && iter != &(vec)->elem[(vec)->len];                            \
        iter++                                                                          \
    )

#define VECTOR_FOR_EACH_REVERSE(iter, vec)                                              \
    for(__typeof__((vec)->elem) iter = &(vec)->elem[(vec)->len - 1];                    \
        (vec)->len != 0 &&  iter != &(vec)->elem[-1];                                   \
        iter--                                                                          \
    )

#define VECTOR_ELEM(vec, idx)                                                           \
    ({                                                                                  \
        size_t __i = (idx);                                                             \
        (vec)->len != 0 && __i < (vec)->len ? &(vec)->elem[__i] : NULL;                 \
    })

#define VECTOR_GROW(vec, n)                                                             \
    vector_grow((vector_t *)vec, sizeof(__typeof__(*(vec)->elem)), n)
