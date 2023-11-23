#include "vector.h"
#include "exception.h"
#include "memory.h"
#include <stdint.h>

void vector_init(vector_t *vec) {
    vec->cap = 0;
    vec->len = 0;
    vec->ent_size = 0;
    vec->elem = NULL;
}

error_t vector_new(vector_t *vec, size_t ent_size, size_t cap, size_t len) {
    __try {
        vec->elem = malloc(ent_size * cap);
        __throwif(ERR_FAILED, !vec->elem);
        vec->cap = cap;
        vec->len = len;
        vec->ent_size = ent_size;        
    }
    __catch:
        return err;
}

void vector_copy(vector_t *dst, vector_t *src) {
    dst->len  = src->len;
    dst->elem = src->elem;
}

error_t vector_concat(vector_t *dst, vector_t *src1, vector_t *src2) {
    __try {
        size_t ent_size = src1->ent_size;
        __throwiferr(vector_new(
            dst, ent_size, src1->cap + src2->cap, src1->len + src2->len
        ));
        memcpy(dst->elem, src1->elem, ent_size * src1->len);
        memcpy(
            (uint8_t *)dst->elem + ent_size * src1->len, 
            src2->elem,
            ent_size * src2->len
        );
    }
    __catch:
        return err;
}

error_t vector_grow(vector_t *vec, size_t n) {
    __try {
        if(n == 0)
            __throw(ERR_SUCCESS);
        
        void *new = realloc(vec->elem, vec->ent_size * (n+ vec->cap));
        __throwif(ERR_FAILED, !new);
        vec->elem = new;
        vec->cap += n;
    }
    __catch:
        return err;
}