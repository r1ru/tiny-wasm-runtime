#include "buffer.h"
#include "memory.h"

buffer_t *new_buffer(uint8_t *p, size_t size) {
    buffer_t *buffer = malloc(sizeof(buffer_t));
    
    *buffer = (buffer_t) {
        .p      = p,
        .end    = p + size
    };

    return buffer;
}

bool eof(buffer_t *buf) {
    return buf->p == buf->end;
}

buffer_t *read_buffer(buffer_t *buf, size_t size) {
    buffer_t *new = new_buffer(buf->p, size);
    buf->p += size;
    return new;
}

uint8_t read_byte(buffer_t *buf) {
    if(buf->p + 1 > buf->end)
        return 0;

    return *buf->p++;
}

uint32_t read_u32(buffer_t *buf) {
    if(buf->p + 4 > buf->end)
        return 0;
    
    uint32_t r = *(uint32_t *)buf->p;
    
    buf->p += 4;
    return r;
}

// Little Endian Base 128
uint64_t read_u64_leb128(buffer_t *buf) {
    uint64_t result = 0, shift = 0;
    while(1) {
        uint8_t byte = read_byte(buf); 
        result |= (byte & 0b1111111) << shift;
        shift += 7;
        if((0b10000000 & byte) == 0)
            return result;
    }
}