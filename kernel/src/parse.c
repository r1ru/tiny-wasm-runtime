#include "parse.h"
#include "print.h"
#include "memory.h"

typedef struct section * (*parser_t) (struct buffer *buf);

static parser_t parsers[11] = {
    [1] = parse_typesec,
    [3] = parse_funcsec
};

struct section *parse_typesec(struct buffer *buf) {
    uint32_t n = read_u64_leb128(buf);

    struct section *typesec = malloc(sizeof(struct section));

    VECTOR_INIT(typesec->functypes, n, functype_t);

    VECTOR_FOR_EACH(e1, typesec->functypes, functype_t) {
        // expected to be 0x60
        uint8_t magic = read_byte(buf);

        // parse argment types
        n = read_u64_leb128(buf);
        VECTOR_INIT(e1->rt1, n, uint8_t);

        VECTOR_FOR_EACH(e2, e1->rt1, uint8_t) {
            *e2 = read_byte(buf);
        }

        // pase return types
        n = read_u64_leb128(buf);
        VECTOR_INIT(e1->rt2, n, uint8_t);

        VECTOR_FOR_EACH(e2, e1->rt2, uint8_t) {
            *e2 = read_byte(buf);
        }
    };

    return typesec;
}

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

        if(id <= 11 && parsers[id])
            mod->known_sections[id] = parsers[id](sec);
    }

    return 0;
}
