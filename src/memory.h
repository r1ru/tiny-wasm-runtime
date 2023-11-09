#pragma once

#include <stddef.h>

// Use libc malloc and free if available.
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void *memcpy(void *dest, const void *src, size_t n);