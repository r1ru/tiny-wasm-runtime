#include "parse.h"
#include "print.h"

int parse_module(struct buffer *mod) {
    uint32_t magic      = read_u32(mod);
    uint32_t version    = read_u32(mod);

    INFO("magic = %#x, version = %#x", magic, version);

    while(!eof(mod)) {
        uint8_t id = read_byte(mod);
        uint32_t size = read_u64_leb128(mod);
        
        struct buffer *sec = read_buffer(mod, size);

        INFO("section id = %#x, size = %#x", id, size);
    }
    
    return 0;
}
