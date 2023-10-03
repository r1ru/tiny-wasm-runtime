#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
Use this structure whenever you access to binary data. 
This makes the code clearer and reduces the number of function arguments.
*/

struct buffer {
    uint8_t *p;
    uint8_t *end;
};

struct buffer *new_buffer(uint8_t *head, size_t size);
bool eof(struct buffer *buf);
struct buffer *read_buffer(struct buffer *buf, size_t size);
uint8_t read_byte(struct buffer *buf);
uint32_t read_u32(struct buffer *buf);
uint64_t read_u64_leb128(struct buffer *buf);