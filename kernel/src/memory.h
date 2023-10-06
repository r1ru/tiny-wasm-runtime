#pragma once

// Use libc malloc and free if available.
void *malloc(size_t size);
void free(void *ptr);