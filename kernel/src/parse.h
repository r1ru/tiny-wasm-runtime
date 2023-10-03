#pragma once

#include "buffer.h"
#include "module.h"

struct section *parse_funcsec(struct buffer *buf);
struct module *parse_module(struct buffer *buf);