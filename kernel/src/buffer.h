#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
Use this structure whenever you access to binary data. 
This makes the code clearer and reduces the number of function arguments.
*/

typedef struct {
    uint8_t *p;
    uint8_t *end;
} buffer_t;

buffer_t *new_buffer(uint8_t *head, size_t size);
bool eof(buffer_t *buf);
buffer_t *read_buffer(buffer_t *buf, size_t size);
uint8_t read_byte(buffer_t *buf);
uint32_t read_u32(buffer_t *buf);
uint64_t read_u64_leb128(buffer_t *buf);