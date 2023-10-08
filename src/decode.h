#pragma once

#include "module.h"
#include "error.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// We use buffer_t to access binary data.
typedef struct {
    uint8_t *p;
    uint8_t *end;
} buffer_t;

static inline bool eof(buffer_t *buf) {
    return buf->p == buf->end;
}

error_t new_buffer(buffer_t **d, uint8_t *head, size_t size);
error_t read_buffer(buffer_t **d, size_t size, buffer_t *buf);
error_t read_byte(uint8_t *d, buffer_t *buf);
error_t read_bytes(uint8_t **d, buffer_t *buf);
error_t read_u32(uint32_t *d, buffer_t *buf);
error_t read_i32(int32_t *d, buffer_t *buf);
error_t read_u32_leb128(uint32_t *d, buffer_t *buf);
error_t read_u64_leb128(uint64_t *d, buffer_t *buf);
error_t read_i32_leb128(int32_t *d, buffer_t *buf);
error_t read_i64_leb128(int64_t *d, buffer_t *buf);

error_t decode_typesec(module_t *mod, buffer_t *buf);
error_t decode_funcsec(module_t *mod, buffer_t *buf);
error_t decode_exportsec(module_t *mod, buffer_t *buf);
error_t decode_codesec(module_t *mod, buffer_t *buf);
error_t decode_module(module_t **mod, uint8_t *image, size_t image_size);