#pragma once

#include "buffer.h"
#include "module.h"

section_t *parse_typesec(buffer_t *buf);
section_t *parse_funcsec(buffer_t *buf);
section_t *parse_codesec(buffer_t *buf);
module_t *parse_module(buffer_t *buf);