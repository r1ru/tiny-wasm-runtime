#pragma once

// ref: https://webassembly.github.io/spec/core/

#include <stdint.h>
#include "vector.h"

struct section {
    union {
        // funcsec
        vector_t typeidxes;
    };
};

struct module {
    struct section *known_sections[11];
};