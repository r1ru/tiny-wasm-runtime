#pragma once

#include <stdint.h>
#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);

/*
Use this structure whenever you access to binary data. 
This makes the code clearer and reduces the number of function arguments.
*/

struct buffer {
    uint8_t *p;
    uint8_t *end;
};

struct buffer *new_buffer(uint8_t *head, size_t size);
uint32_t readu32(struct buffer *buf);