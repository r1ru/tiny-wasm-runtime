#include "parse.h"
#include "print.h"
#include "memory.h"

typedef struct section * (*parser_t) (struct buffer *buf);

static parser_t parsers[11] = {
    [3] = parse_funcsec
};

struct section *parse_funcsec(struct buffer *buf) {
    uint32_t n = read_u64_leb128(buf);

    struct section *funcsec = malloc(sizeof(struct section));

    VECTOR_INIT(funcsec->typeidxes, n, uint32_t);

    VECTOR_FOR_EACH(elem, funcsec->typeidxes, uint32_t) {
        *elem = read_u64_leb128(buf);
    };

    return funcsec;
}

struct module *parse_module(struct buffer *buf) {
    uint32_t magic      = read_u32(buf);
    uint32_t version    = read_u32(buf);

    INFO("magic = %#x, version = %#x", magic, version);

    struct module *mod = malloc(sizeof(struct module));

    while(!eof(buf)) {
        uint8_t id = read_byte(buf);
        uint32_t size = read_u64_leb128(buf);
        
        struct buffer *sec = read_buffer(buf, size);

        INFO("section id = %#x, size = %#x", id, size);

        if(id == 3)
            mod->known_sections[id] = parsers[id](sec);
    }

    return 0;
}
