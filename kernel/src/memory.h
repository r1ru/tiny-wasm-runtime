#pragma once

// Use libc malloc and free if available.
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);