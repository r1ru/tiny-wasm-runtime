#include "vector.h"
#include "exception.h"
#include "memory.h"
#include <stdint.h>

void vector_init(vector_t *vec) {
    vec->n = 0;
    vec->elem = NULL;
}

error_t vector_new(vector_t *vec, size_t ent_size, size_t n) {
    __try {
        vec->elem = malloc(ent_size * n);
        __throwif(ERR_FAILED, !vec->elem);
        vec->n = n;        
    }
    __catch:
        return err;
}

void vector_copy(vector_t *dst, vector_t *src) {
    dst->n  = src->n;
    dst->elem = src->elem;
}

error_t vector_concat(vector_t *dst, vector_t *src1, vector_t *src2, size_t ent_size) {
    __try {
        __throwiferr(vector_new(dst, ent_size, src1->n + src2->n));
        memcpy(dst->elem, src1->elem, ent_size * src1->n);
        memcpy(
            (uint8_t *)dst->elem + ent_size * src1->n, 
            src2->elem,
            ent_size * src2->n
        );
    }
    __catch:
        return err;
}