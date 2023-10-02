#include "parse.h"
#include "print.h"

void parse_module(struct buffer *mod) {
    uint32_t magic      = readu32(mod);
    uint32_t version    = readu32(mod);

    INFO("magic = %x, version = %x", magic, version);
}
