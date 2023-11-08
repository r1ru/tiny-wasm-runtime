#include "vector.h"
#include "exception.h"
#include "memory.h"

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