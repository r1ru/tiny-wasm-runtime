#pragma once

#include "error.h"
#include <stddef.h>

typedef struct {
    size_t  cap;
    size_t  len;
    size_t  ent_size;
    void    *elem;
} vector_t;

// useful macros
#define VECTOR(type)                                                                    \
    struct {                                                                            \
        size_t  cap;                                                                    \
        size_t  len;                                                                    \
        size_t  ent_size;                                                               \
        type    *elem;                                                                  \
    }
 
#define VECTOR_INIT(vec)                                                                \
    vector_init((vector_t *)vec)

#define VECTOR_NEW(vec, c, l)                                                           \
    vector_new((vector_t *)vec, sizeof(__typeof__(*(vec)->elem)), c, l)

#define VECTOR_COPY(dst, src)                                                           \
    vector_copy((vector_t *)dst, (vector_t *)src)

#define VECTOR_CONCAT(dst, src1, src2)                                                  \
    vector_concat(                                                                      \
        (vector_t *)dst,                                                                \
        (vector_t *)src1,                                                               \
        (vector_t *)src2                                                                \
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
    vector_grow((vector_t *)vec, n)

void vector_init(vector_t *vec);
void vector_copy(vector_t *dst, vector_t *src);
error_t vector_new(vector_t *vec, size_t ent_size, size_t cap, size_t len);
error_t vector_concat(vector_t *dst, vector_t *src1, vector_t *src2);
error_t vector_grow(vector_t *vec, size_t len);