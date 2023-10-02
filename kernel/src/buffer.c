#include "buffer.h"

struct buffer *new_buffer(uint8_t *p, size_t size) {
    struct buffer *buffer = malloc(sizeof(struct buffer));
    
    *buffer = (struct buffer) {
        .p      = p,
        .end    = p + size
    };

    return buffer;
}

uint32_t readu32(struct buffer *buf) {
    if(buf->p + 4 > buf->end)
        return 0;
    
    uint32_t r = *(uint32_t *)buf->p;
    
    buf->p += 4;
    return r;
}