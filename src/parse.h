#pragma once

#include "buffer.h"
#include "module.h"
#include "error.h"

error_t parse_typesec(module_t *mod, buffer_t*buf);
error_t parse_funcsec(module_t *mod, buffer_t *buf);
error_t parse_exportsec(module_t *mod, buffer_t *buf);
error_t parse_codesec(module_t *mod, buffer_t *buf);
error_t parse_module(module_t **mod, uint8_t *image, size_t image_size);