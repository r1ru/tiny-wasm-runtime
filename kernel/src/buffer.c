#include "buffer.h"
#include "memory.h"

error_t new_buffer(buffer_t **d, uint8_t *head, size_t size) {
    buffer_t *buf = *d = malloc(sizeof(buffer_t));
    
    *buf = (buffer_t) {
        .p      = head,
        .end    = head + size
    };

    return ERR_SUCCESS;
}

bool eof(buffer_t *buf) {
    return buf->p == buf->end;
}

error_t read_buffer(buffer_t **d, size_t size, buffer_t *buf) {
    if(buf->p + size > buf->end)
        return ERR_FAILED;
    
    new_buffer(d, buf->p, size);
    buf->p += size;
    return ERR_SUCCESS;
}

error_t read_byte(uint8_t *d, buffer_t *buf) {
    if(buf->p + 1 > buf->end)
        return ERR_FAILED;

    *d = *buf->p++;
    return ERR_SUCCESS;
}

// read vec(byte)
error_t read_bytes(uint8_t **d, buffer_t *buf) {
    uint32_t n;
    read_u32_leb128(&n, buf);
    
    uint8_t *str = *d =calloc(sizeof(uint8_t), (n + 1));
    
    for(uint32_t i = 0; i < n; i++) {
        read_byte(&str[i], buf);
    }

    return ERR_SUCCESS;
}

error_t read_u32(uint32_t *d, buffer_t *buf) {
    if(buf->p + 4 > buf->end)
        return ERR_FAILED;
    
    *d = *(uint32_t *)buf->p;
    
    buf->p += 4;
    return ERR_SUCCESS;
}

// Little Endian Base 128
error_t read_u32_leb128(uint32_t *d, buffer_t *buf) {
    uint32_t result = 0, shift = 0;
    while(1) {
        uint8_t byte;
        read_byte(&byte, buf);

        result |= (byte & 0b1111111) << shift;
        shift += 7;
        if((0b10000000 & byte) == 0)
            break;
    }
    *d = result;
    return ERR_SUCCESS;
}

error_t read_u64_leb128(uint64_t *d, buffer_t *buf) {
    uint64_t result = 0, shift = 0;
    while(1) {
        uint8_t byte;
        read_byte(&byte, buf);

        result |= (byte & 0b1111111) << shift;
        shift += 7;
        if((0b10000000 & byte) == 0)
            break;
    }
    *d = result;
    return ERR_SUCCESS;
}

error_t read_i32_leb128(int32_t *d, buffer_t *buf) {
    int32_t result = 0, shift = 0;
    while(1) {
        uint8_t byte;
        read_byte(&byte, buf);
        result |= (byte & 0b1111111) << shift;
        shift += 7;
        if((0b10000000 & byte) == 0) {
            if((byte & 0b1000000) != 0)
                result |= ~0 << shift;
            else
                result;
            break;
        }
    }
    *d = result;
    return ERR_SUCCESS;
}

error_t read_i64_leb128(int64_t *d, buffer_t *buf) {
    int64_t result = 0, shift = 0;
    while(1) {
        uint8_t byte;
        read_byte(&byte, buf);
        result |= (byte & 0b1111111) << shift;
        shift += 7;
        if((0b10000000 & byte) == 0) {
            if((byte & 0b1000000) != 0)
                result |= ~0 << shift;
            else
                result;
            break;
        }
    }
    *d = result;
    return ERR_SUCCESS;
}