#include "buffer.h"

struct buffer *new_buffer(uint8_t *p, size_t size) {
    struct buffer *buffer = malloc(sizeof(struct buffer));
    
    *buffer = (struct buffer) {
        .p      = p,
        .end    = p + size
    };

    return buffer;
}

bool eof(struct buffer *buf) {
    return buf->p == buf->end;
}

struct buffer *read_buffer(struct buffer *buf, size_t size) {
    struct buffer *new = new_buffer(buf->p, size);
    buf->p += size;
    return new;
}

uint8_t read_byte(struct buffer *buf) {
    if(buf->p + 1 > buf->end)
        return 0;

    return *buf->p++;
}

uint32_t read_u32(struct buffer *buf) {
    if(buf->p + 4 > buf->end)
        return 0;
    
    uint32_t r = *(uint32_t *)buf->p;
    
    buf->p += 4;
    return r;
}

// Little Endian Base 128
uint64_t read_u64_leb128(struct buffer *buf) {
    uint64_t result = 0, shift = 0;
    while(1) {
        uint8_t byte = read_byte(buf); 
        result |= (byte & 0b1111111) << shift;
        shift += 7;
        if((0b10000000 & byte) == 0)
            return result;
    }
}